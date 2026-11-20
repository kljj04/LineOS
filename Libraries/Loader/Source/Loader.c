// Libraries/Loader/Source/Loader.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <ELF/ELF64.h>
#include <Syscall/File/Stat.h>

#define LOADER_OBJECT_LIMIT  64U
#define LOADER_SEGMENT_LIMIT 64U
#define LOADER_PATH_LIMIT    4096U
#define LOADER_FILE_LIMIT    (64ULL * 1024 * 1024)
#define LOADER_PAGE          4096ULL
#define LOADER_DT_LIMIT      64U

typedef struct LOADER_OBJECT
{
    UINT64               Device;
    UINT64               Inode;
    UINT64               Bias;
    UINT64               Mapping;
    UINT64               Size;
    UINT64               Entry;
    ELF64_PROGRAM_HEADER Headers[LOADER_SEGMENT_LIMIT];
    UINT32               HeaderCount;
    UINT64               Tags[LOADER_DT_LIMIT];
    ELF64_SYMBOL        *Symbols;
    CONST CHAR8         *Strings;
    UINT64               StringSize;
    UINT64               SymbolCount;
    UINT64               VersionSymbols;
    UINT64               VersionDefinitions;
    UINT64               VersionNeeds;
    UINT64               GNUHash;
    UINT64               SysVHash;
    CHAR8                Path[LOADER_PATH_LIMIT];
    UINT32               Dependencies[LOADER_OBJECT_LIMIT];
    UINT32               DependencyCount;
    UINT32               InitState;
    UINT64               TLSOffset;
    UINT64               TLSSize;
    UINT64               TLSImage;
    UINT64               TLSFileSize;
} LOADER_OBJECT;

STATIC LOADER_OBJECT Objects[LOADER_OBJECT_LIMIT];
STATIC UINT32        ObjectCount;
STATIC UINT32        InitOrder[LOADER_OBJECT_LIMIT];
STATIC UINT32        InitCount;
STATIC UINT64       *InitialStack;
STATIC UINT64        TLSSize;
STATIC UINT64        TLSAlignment = 16;
STATIC UINT64        TLSRandom[2];

EXTERN VOID LoaderEnter(UINT64 Entry, UINT64 Stack, VOID (*Finalizer)(VOID)) NORETURN;

STATIC INT64 LoaderCall(UINT64 Number, UINT64 A, UINT64 B, UINT64 C, UINT64 D, UINT64 E, UINT64 F)
{
    register UINT64 R10 __asm__("r10") = D;
    register UINT64 R8 __asm__("r8")   = E;
    register UINT64 R9 __asm__("r9")   = F;
    INT64           Result;

    ASM("syscall" : "=a"(Result) : "a"(Number), "D"(A), "S"(B), "d"(C), "r"(R10), "r"(R8), "r"(R9) : "rcx", "r11", "memory");
    return Result;
}

STATIC UINT64 LoaderLength(CONST CHAR8 *Text)
{
    UINT64 Length = 0;

    while (Text[Length] != 0)
    {
        Length++;
    }

    return Length;
}

STATIC BOOLEAN LoaderEqual(CONST CHAR8 *A, CONST CHAR8 *B)
{
    while (*A != 0 && *A == *B)
    {
        A++;
        B++;
    }

    return *A == *B;
}

STATIC VOID LoaderCopy(VOID *Destination, CONST VOID *Source, UINT64 Size)
{
    for (UINT64 Index = 0; Index < Size; Index++)
    {
        ((UINT8 *) Destination)[Index] = ((CONST UINT8 *) Source)[Index];
    }
}

STATIC VOID NORETURN LoaderFail(CONST CHAR8 *Message)
{
    CONST CHAR8 Prefix[] = "ld-lineos: ";
    LoaderCall(1, 2, (UINT64) Prefix, sizeof(Prefix) - 1, 0, 0, 0);
    LoaderCall(1, 2, (UINT64) Message, LoaderLength(Message), 0, 0, 0);
    LoaderCall(1, 2, (UINT64) "\n", 1, 0, 0, 0);
    LoaderCall(231, 127, 0, 0, 0, 0, 0);
    __builtin_trap();
}

STATIC BOOLEAN LoaderRange(UINT64 Offset, UINT64 Size, UINT64 Total)
{
    return Offset <= Total && Size <= Total - Offset;
}

STATIC BOOLEAN LoaderContains(LOADER_OBJECT *Object, UINT64 Address, UINT64 Size, UINT32 Flags)
{
    for (UINT32 Index = 0; Index < Object->HeaderCount; Index++)
    {
        ELF64_PROGRAM_HEADER *Header = &Object->Headers[Index];

        if (Header->Type == ELF64_PT_LOAD && (Header->Flags & Flags) == Flags && Address >= Object->Bias + Header->VAddr && LoaderRange(Address - Object->Bias - Header->VAddr, Size, Header->MemSize))
        {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC UINT64 LoaderPointer(LOADER_OBJECT *Object, UINT64 Offset, UINT64 Size)
{
    UINT64 Address;

    if (__builtin_add_overflow(Object->Bias, Offset, &Address) || !LoaderContains(Object, Address, Size, ELF64_PF_R))
    {
        LoaderFail((CONST CHAR8 *) "ELF pointer outside readable segment");
    }

    return Address;
}

STATIC CONST CHAR8 *LoaderString(LOADER_OBJECT *Object, UINT64 Offset)
{
    if (Offset >= Object->StringSize)
    {
        LoaderFail((CONST CHAR8 *) "invalid string offset");
    }

    for (UINT64 Index = Offset; Index < Object->StringSize; Index++)
    {
        if (Object->Strings[Index] == 0)
        {
            return Object->Strings + Offset;
        }
    }

    LoaderFail((CONST CHAR8 *) "unterminated dynamic string");
}

STATIC VOID LoaderParse(LOADER_OBJECT *Object)
{
    BOOLEAN Terminated = FALSE;

    for (UINT32 Index = 0; Index < Object->HeaderCount; Index++)
    {
        ELF64_PROGRAM_HEADER *Header = &Object->Headers[Index];

        if (Header->Type != ELF64_PT_DYNAMIC)
        {
            continue;
        }

        ELF64_DYNAMIC *Dynamic = (VOID *) LoaderPointer(Object, Header->VAddr, Header->MemSize);

        for (UINT64 Entry = 0; Entry < Header->MemSize / sizeof(*Dynamic); Entry++)
        {
            INT64  Tag   = Dynamic[Entry].Tag;
            UINT64 Value = Dynamic[Entry].Data.Value;

            if (Tag == ELF64_DT_TEXTREL || Tag == ELF64_DT_REL || Tag == ELF64_DT_RELSZ)
            {
                LoaderFail((CONST CHAR8 *) "unsupported text or REL relocation");
            }

            if (Tag == ELF64_DT_NULL)
            {
                Terminated = TRUE;
                break;
            }

            if (Tag >= 0 && Tag < LOADER_DT_LIMIT)
            {
                if (Tag != ELF64_DT_NEEDED)
                {
                    Object->Tags[Tag] = Value;
                }
            }
            else if (Tag == ELF64_DT_GNU_HASH)
            {
                Object->GNUHash = Value;
            }
            else if (Tag == 0x6FFFFFF0)
            {
                Object->VersionSymbols = Value;
            }
            else if (Tag == 0x6FFFFFFC)
            {
                Object->VersionDefinitions = Value;
            }
            else if (Tag == 0x6FFFFFFE)
            {
                Object->VersionNeeds = Value;
            }
            else if (Tag == 0x6FFFFFFB)
            {
                if ((Value & ~0x08000001ULL) != 0)
                {
                    LoaderFail((CONST CHAR8 *) "unsupported DT_FLAGS_1");
                }
            }
        }
    }

    if (!Terminated || Object->Tags[ELF64_DT_SYMENT] != sizeof(ELF64_SYMBOL) || Object->Tags[ELF64_DT_STRSZ] == 0 || Object->Tags[ELF64_DT_TEXTREL] != 0 || (Object->Tags[ELF64_DT_FLAGS] & ~8ULL) != 0)
    {
        LoaderFail((CONST CHAR8 *) "unsupported or malformed dynamic section");
    }

    Object->StringSize = Object->Tags[ELF64_DT_STRSZ];
    Object->Strings    = (VOID *) LoaderPointer(Object, Object->Tags[ELF64_DT_STRTAB], Object->StringSize);
    Object->SysVHash   = Object->Tags[ELF64_DT_HASH];

    if (Object->SysVHash != 0)
    {
        UINT32 *Hash        = (VOID *) LoaderPointer(Object, Object->SysVHash, 8);
        Object->SymbolCount = Hash[1];
        LoaderPointer(Object, Object->SysVHash, 8 + ((UINT64) Hash[0] + Hash[1]) * 4);
    }

    if (Object->GNUHash != 0)
    {
        UINT32 *Hash = (VOID *) LoaderPointer(Object, Object->GNUHash, 16);

        if (Hash[0] == 0 || Hash[2] == 0 || Hash[2] > LOADER_FILE_LIMIT / 8)
        {
            LoaderFail((CONST CHAR8 *) "invalid GNU hash");
        }

        UINT64  BucketOffset = Object->GNUHash + 16 + (UINT64) Hash[2] * 8;
        UINT32 *Buckets      = (VOID *) LoaderPointer(Object, BucketOffset, (UINT64) Hash[0] * 4);
        UINT64  ChainOffset  = BucketOffset + (UINT64) Hash[0] * 4;
        UINT64  Maximum      = Hash[1];

        for (UINT32 Bucket = 0; Bucket < Hash[0]; Bucket++)
        {
            UINT64 Symbol = Buckets[Bucket];

            if (Symbol == 0)
            {
                continue;
            }

            if (Symbol < Hash[1])
            {
                LoaderFail((CONST CHAR8 *) "invalid GNU hash bucket");
            }

            for (;; Symbol++)
            {
                UINT32 *Chain = (VOID *) LoaderPointer(Object, ChainOffset + (Symbol - Hash[1]) * 4, 4);

                if (Symbol + 1 > Maximum)
                {
                    Maximum = Symbol + 1;
                }

                if ((*Chain & 1U) != 0)
                {
                    break;
                }
            }
        }

        if (Maximum > Object->SymbolCount)
        {
            Object->SymbolCount = Maximum;
        }
    }

    if (Object->SymbolCount == 0 || Object->SymbolCount > LOADER_FILE_LIMIT / sizeof(ELF64_SYMBOL))
    {
        LoaderFail((CONST CHAR8 *) "missing or invalid symbol hash");
    }

    Object->Symbols = (VOID *) LoaderPointer(Object, Object->Tags[ELF64_DT_SYMTAB], Object->SymbolCount * sizeof(ELF64_SYMBOL));
}

STATIC VOID *LoaderAllocate(UINT64 Size)
{
    INT64 Result = LoaderCall(9, 0, Size, 3, 0x22, (UINT64) -1, 0);

    if (Result < 0)
    {
        LoaderFail((CONST CHAR8 *) "anonymous mmap failed");
    }

    return (VOID *) Result;
}

STATIC BOOLEAN LoaderOpenObject(CONST CHAR8 *Path, LOADER_OBJECT *Object)
{
    INT64 FD = LoaderCall(2, (UINT64) Path, 0, 0, 0, 0, 0);

    if (FD < 0)
    {
        return FALSE;
    }

    STAT_INFO Stat;

    if (LoaderCall(5, FD, (UINT64) &Stat, 0, 0, 0, 0) != 0 || Stat.Size < (INT64) sizeof(ELF64_HEADER) || Stat.Size > (INT64) LOADER_FILE_LIMIT)
    {
        LoaderFail((CONST CHAR8 *) "invalid ELF file size");
    }

    Object->Device = Stat.Device;
    Object->Inode  = Stat.Inode;
    UINT8 *Data    = LoaderAllocate(Stat.Size);
    UINT64 Read    = 0;

    while (Read < (UINT64) Stat.Size)
    {
        INT64 Count = LoaderCall(0, FD, (UINT64) Data + Read, Stat.Size - Read, 0, 0, 0);

        if (Count <= 0)
        {
            LoaderFail((CONST CHAR8 *) "ELF read failed");
        }

        Read += Count;
    }

    LoaderCall(3, FD, 0, 0, 0, 0, 0);
    ELF64_HEADER *Header = (VOID *) Data;

    if (Header->Ident[0] != 0x7F || Header->Ident[1] != 'E' || Header->Ident[2] != 'L' || Header->Ident[3] != 'F' || Header->Ident[4] != 2 || Header->Ident[5] != 1 || Header->Type != ELF64_ET_DYN || Header->Machine != ELF64_EM_X86_64 || Header->Version != 1 || Header->EHSize != sizeof(*Header) || Header->PHEntSize != sizeof(ELF64_PROGRAM_HEADER) || Header->PHNum == 0 || Header->PHNum > LOADER_SEGMENT_LIMIT || !LoaderRange(Header->PHOff, (UINT64) Header->PHNum * sizeof(ELF64_PROGRAM_HEADER), Stat.Size))
    {
        LoaderFail((CONST CHAR8 *) "invalid shared ELF header");
    }

    Object->HeaderCount = Header->PHNum;
    LoaderCopy(Object->Headers, Data + Header->PHOff, Header->PHNum * sizeof(ELF64_PROGRAM_HEADER));
    UINT64 End = 0;

    for (UINT32 Index = 0; Index < Object->HeaderCount; Index++)
    {
        ELF64_PROGRAM_HEADER *Segment = &Object->Headers[Index];

        if (Segment->Type == ELF64_PT_INTERP)
        {
            LoaderFail((CONST CHAR8 *) "shared library has an interpreter");
        }

        if (Segment->Type != ELF64_PT_LOAD)
        {
            continue;
        }

        if (Segment->MemSize < Segment->FileSize || !LoaderRange(Segment->Offset, Segment->FileSize, Stat.Size) || Segment->VAddr > LOADER_FILE_LIMIT || Segment->MemSize > LOADER_FILE_LIMIT - Segment->VAddr || (Segment->VAddr & 4095) != (Segment->Offset & 4095) || (Segment->Flags & ~7U) != 0 || ((Segment->Flags & 6U) != 4U && (Segment->Flags & 6U) != 6U) || (Segment->Flags & 3U) == 3U || (Segment->Align > 1 && ((Segment->Align & (Segment->Align - 1)) != 0 || Segment->Align > LOADER_PAGE)))
        {
            LoaderFail((CONST CHAR8 *) "invalid PT_LOAD");
        }

        UINT64 Start = Segment->VAddr & ~4095ULL;
        UINT64 Top   = (Segment->VAddr + Segment->MemSize + 4095) & ~4095ULL;

        for (UINT32 Previous = 0; Previous < Index; Previous++)
        {
            ELF64_PROGRAM_HEADER *Other = &Object->Headers[Previous];

            if (Other->Type == ELF64_PT_LOAD && Start < ((Other->VAddr + Other->MemSize + 4095) & ~4095ULL) && (Other->VAddr & ~4095ULL) < Top)
            {
                LoaderFail((CONST CHAR8 *) "overlapping PT_LOAD pages");
            }
        }

        if (Top > End)
        {
            End = Top;
        }
    }

    if (End == 0)
    {
        LoaderFail((CONST CHAR8 *) "empty ELF image");
    }

    Object->Mapping = (UINT64) LoaderAllocate(End);
    Object->Size    = End;
    Object->Bias    = Object->Mapping;
    Object->Entry   = Object->Bias + Header->Entry;

    for (UINT64 Index = 0; Index < End; Index++)
    {
        ((UINT8 *) Object->Mapping)[Index] = 0;
    }

    for (UINT32 Index = 0; Index < Object->HeaderCount; Index++)
    {
        ELF64_PROGRAM_HEADER *Segment = &Object->Headers[Index];

        if (Segment->Type == ELF64_PT_LOAD)
        {
            LoaderCopy((VOID *) (Object->Bias + Segment->VAddr), Data + Segment->Offset, Segment->FileSize);
        }
    }

    LoaderCall(11, (UINT64) Data, Stat.Size, 0, 0, 0, 0);
    LoaderCopy(Object->Path, Path, LoaderLength(Path) + 1);
    LoaderParse(Object);
    return TRUE;
}

STATIC BOOLEAN LoaderSearchPath(LOADER_OBJECT *Object, LOADER_OBJECT *Requester, CONST CHAR8 *List, CONST CHAR8 *Name)
{
    while (*List != 0)
    {
        CHAR8  Path[LOADER_PATH_LIMIT];
        UINT64 Length = 0;

        while (*List != 0 && *List != ':')
        {
            CONST CHAR8 Origin[] = "$ORIGIN";
            BOOLEAN     Match    = TRUE;

            for (UINT32 Index = 0; Index < sizeof(Origin) - 1; Index++)
            {
                if (List[Index] == 0 || List[Index] != Origin[Index])
                {
                    Match = FALSE;
                    break;
                }
            }

            if (Match)
            {
                UINT64 End = 0;

                for (UINT64 Index = 0; Requester->Path[Index] != 0; Index++)
                {
                    if (Requester->Path[Index] == '/')
                    {
                        End = Index;
                    }
                }

                if (End == 0 || Length + End >= sizeof(Path))
                {
                    return FALSE;
                }

                LoaderCopy(Path + Length, Requester->Path, End);
                Length += End;
                List += sizeof(Origin) - 1;
            }
            else
            {
                if (Length + 1 >= sizeof(Path))
                {
                    return FALSE;
                }

                Path[Length++] = *List++;
            }
        }

        if (*List == ':')
        {
            List++;
        }

        UINT64 NameLength = LoaderLength(Name);

        if (Length == 0 || Path[0] != '/' || NameLength >= sizeof(Path) - Length - 1)
        {
            continue;
        }

        Path[Length++] = '/';
        LoaderCopy(Path + Length, Name, NameLength + 1);

        for (UINT32 Index = 0; Index + 1 < ObjectCount; Index++)
        {
            if (LoaderEqual(Path, Objects[Index].Path))
            {
                return FALSE;
            }
        }

        if (LoaderOpenObject(Path, Object))
        {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC UINT32 LoaderAdopt(UINT32 Index)
{
    LOADER_OBJECT *Object = &Objects[Index];

    for (UINT32 Other = 1; Other < Index; Other++)
    {
        if (Objects[Other].Device == Object->Device && Objects[Other].Inode == Object->Inode)
        {
            LoaderCall(11, Object->Mapping, Object->Size, 0, 0, 0, 0);
            UINT8 *Bytes = (VOID *) Object;

            for (UINT64 Byte = 0; Byte < sizeof(*Object); Byte++)
            {
                Bytes[Byte] = 0;
            }

            ObjectCount--;
            return Other;
        }
    }

    return Index;
}

STATIC UINT32 LoaderDependency(LOADER_OBJECT *Requester, CONST CHAR8 *Name)
{
    for (UINT32 Index = 0; Index < ObjectCount; Index++)
    {
        LOADER_OBJECT *Object = &Objects[Index];

        if (LoaderEqual(Name, Object->Path) || (Object->Tags[ELF64_DT_SONAME] != 0 && LoaderEqual(Name, LoaderString(Object, Object->Tags[ELF64_DT_SONAME]))))
        {
            return Index;
        }
    }

    if (ObjectCount == LOADER_OBJECT_LIMIT)
    {
        LoaderFail((CONST CHAR8 *) "too many shared libraries");
    }

    UINT32         Index  = ObjectCount++;
    LOADER_OBJECT *Object = &Objects[Index];

    if (Name[0] == '/')
    {
        if (LoaderLength(Name) >= LOADER_PATH_LIMIT || !LoaderOpenObject(Name, Object))
        {
            LoaderFail(Name);
        }

        return LoaderAdopt(Index);
    }

    UINT64 Search = Requester->Tags[ELF64_DT_RUNPATH] != 0 ? Requester->Tags[ELF64_DT_RUNPATH] : Requester->Tags[ELF64_DT_RPATH];

    if (Search != 0 && LoaderSearchPath(Object, Requester, LoaderString(Requester, Search), Name))
    {
        return LoaderAdopt(Index);
    }

    CONST CHAR8 *Directories[] = {(CONST CHAR8 *) "/lib/", (CONST CHAR8 *) "/usr/lib/"};

    for (UINT32 Directory = 0; Directory < 2; Directory++)
    {
        CHAR8  Path[LOADER_PATH_LIMIT];
        UINT64 Prefix = LoaderLength(Directories[Directory]);
        UINT64 Length = LoaderLength(Name);

        if (Prefix + Length >= sizeof(Path))
        {
            LoaderFail((CONST CHAR8 *) "library path too long");
        }

        LoaderCopy(Path, Directories[Directory], Prefix);
        LoaderCopy(Path + Prefix, Name, Length + 1);

        if (LoaderOpenObject(Path, Object))
        {
            return LoaderAdopt(Index);
        }
    }

    LoaderFail(Name);
}

STATIC VOID LoaderDependencies(LOADER_OBJECT *Object)
{
    for (UINT32 Index = 0; Index < Object->HeaderCount; Index++)
    {
        ELF64_PROGRAM_HEADER *Header = &Object->Headers[Index];

        if (Header->Type != ELF64_PT_DYNAMIC)
        {
            continue;
        }

        ELF64_DYNAMIC *Dynamic = (VOID *) LoaderPointer(Object, Header->VAddr, Header->MemSize);

        for (UINT64 Entry = 0; Entry < Header->MemSize / sizeof(*Dynamic) && Dynamic[Entry].Tag != 0; Entry++)
        {
            if (Dynamic[Entry].Tag == ELF64_DT_NEEDED)
            {
                if (Object->DependencyCount == LOADER_OBJECT_LIMIT)
                {
                    LoaderFail((CONST CHAR8 *) "too many dependencies");
                }

                Object->Dependencies[Object->DependencyCount++] = LoaderDependency(Object, LoaderString(Object, Dynamic[Entry].Data.Value));
            }
        }
    }
}

typedef struct LOADER_VERSION_DEF
{
    UINT16 Version;
    UINT16 Flags;
    UINT16 Index;
    UINT16 Count;
    UINT32 Hash;
    UINT32 Aux;
    UINT32 Next;
} LOADER_VERSION_DEF;

typedef struct LOADER_VERSION_AUX
{
    UINT32 Hash;
    UINT16 Flags;
    UINT16 Other;
    UINT32 Name;
    UINT32 Next;
} LOADER_VERSION_AUX;

typedef struct LOADER_VERSION_NEED
{
    UINT16 Version;
    UINT16 Count;
    UINT32 File;
    UINT32 Aux;
    UINT32 Next;
} LOADER_VERSION_NEED;

STATIC CONST CHAR8 *LoaderVersion(LOADER_OBJECT *Object, UINT64 Symbol, BOOLEAN Definition, BOOLEAN *Hidden)
{
    *Hidden = FALSE;

    if (Object->VersionSymbols == 0)
    {
        return NULL;
    }

    UINT16 *Versions = (VOID *) LoaderPointer(Object, Object->VersionSymbols, Object->SymbolCount * 2);
    UINT16  Version  = Versions[Symbol];
    *Hidden          = (Version & 0x8000U) != 0;
    Version &= 0x7FFFU;

    if (Version <= 1)
    {
        return NULL;
    }

    UINT64 Offset = Definition ? Object->VersionDefinitions : Object->VersionNeeds;

    for (UINT32 Entry = 0; Offset != 0 && Entry < 4096; Entry++)
    {
        if (Definition)
        {
            LOADER_VERSION_DEF *Def = (VOID *) LoaderPointer(Object, Offset, sizeof(*Def));

            if (Def->Version != 1 || Def->Count == 0)
            {
                LoaderFail((CONST CHAR8 *) "invalid version definition");
            }

            if ((Def->Index & 0x7FFFU) == Version)
            {
                UINT32 *Aux = (VOID *) LoaderPointer(Object, Offset + Def->Aux, 8);
                return LoaderString(Object, Aux[0]);
            }

            Offset = Def->Next == 0 ? 0 : Offset + Def->Next;
        }
        else
        {
            LOADER_VERSION_NEED *Need = (VOID *) LoaderPointer(Object, Offset, sizeof(*Need));

            if (Need->Version != 1)
            {
                LoaderFail((CONST CHAR8 *) "invalid version requirement");
            }

            LoaderString(Object, Need->File);
            UINT64 AuxOffset = Offset + Need->Aux;

            for (UINT32 Index = 0; Index < Need->Count; Index++)
            {
                LOADER_VERSION_AUX *Aux = (VOID *) LoaderPointer(Object, AuxOffset, sizeof(*Aux));

                if ((Aux->Other & 0x7FFFU) == Version)
                {
                    return LoaderString(Object, Aux->Name);
                }

                if (Aux->Next == 0)
                {
                    break;
                }

                AuxOffset += Aux->Next;
            }

            Offset = Need->Next == 0 ? 0 : Offset + Need->Next;
        }
    }

    LoaderFail((CONST CHAR8 *) "unresolved symbol version index");
}

STATIC ELF64_SYMBOL *LoaderResolve(LOADER_OBJECT *Request, UINT32 Symbol, LOADER_OBJECT **Owner, BOOLEAN Copy)
{
    if (Symbol >= Request->SymbolCount)
    {
        LoaderFail((CONST CHAR8 *) "relocation symbol index outside table");
    }

    ELF64_SYMBOL *Original = &Request->Symbols[Symbol];

    if (!Copy && Original->SHNdx != ELF64_SHN_UNDEF && (ELF64_ST_BIND(Original->Info) == ELF64_STB_LOCAL || ELF64_ST_VISIBILITY(Original->Other) != ELF64_STV_DEFAULT))
    {
        *Owner = Request;
        return Original;
    }

    CONST CHAR8 *Name = LoaderString(Request, Original->Name);
    BOOLEAN      Hidden;
    CONST CHAR8 *Version = LoaderVersion(Request, Symbol, Original->SHNdx != ELF64_SHN_UNDEF && !Copy, &Hidden);

    for (UINT32 Index = Copy ? 1 : 0; Index < ObjectCount; Index++)
    {
        LOADER_OBJECT *Object = &Objects[Index];

        for (UINT64 Entry = 1; Entry < Object->SymbolCount; Entry++)
        {
            ELF64_SYMBOL *Candidate  = &Object->Symbols[Entry];
            UINT8         Bind       = ELF64_ST_BIND(Candidate->Info);
            UINT8         Visibility = ELF64_ST_VISIBILITY(Candidate->Other);

            if (Candidate->SHNdx != ELF64_SHN_UNDEF && (Bind == ELF64_STB_GLOBAL || Bind == ELF64_STB_WEAK) && Visibility != ELF64_STV_HIDDEN && Visibility != ELF64_STV_INTERNAL && LoaderEqual(Name, LoaderString(Object, Candidate->Name)))
            {
                BOOLEAN      CandidateHidden;
                CONST CHAR8 *CandidateVersion = LoaderVersion(Object, Entry, TRUE, &CandidateHidden);

                if ((Version != NULL && (CandidateVersion == NULL || !LoaderEqual(Version, CandidateVersion))) || (Version == NULL && CandidateHidden))
                {
                    continue;
                }

                *Owner = Object;
                return Candidate;
            }
        }
    }

    if (ELF64_ST_BIND(Original->Info) != ELF64_STB_WEAK)
    {
        LoaderFail(Name);
    }

    *Owner = NULL;
    return NULL;
}

VOID *LoaderTLSCreate(VOID)
{
    UINT64  Total      = TLSSize + TLSAlignment + 128 + (ObjectCount + 1) * 16;
    UINT8  *Allocation = LoaderAllocate(Total);
    UINT64  Base       = ((UINT64) Allocation + TLSSize + TLSAlignment - 1) & ~(TLSAlignment - 1);
    UINT64 *TCB        = (VOID *) Base;

    for (UINT64 Index = 0; Index < Total; Index++)
    {
        Allocation[Index] = 0;
    }

    TCB[0]      = Base;
    TCB[1]      = Base + 128;
    TCB[2]      = Base;
    TCB[3]      = (UINT64) Allocation;
    TCB[4]      = Total;
    TCB[5]      = TLSRandom[0] & ~0xFFULL;
    TCB[6]      = TLSRandom[1];
    UINT64 *DTV = (VOID *) TCB[1];
    DTV[0]      = 1;

    for (UINT32 Index = 0; Index < ObjectCount; Index++)
    {
        LOADER_OBJECT *Object = &Objects[Index];

        if (Object->TLSSize != 0)
        {
            UINT64 Address = Base - Object->TLSOffset;
            LoaderCopy((VOID *) Address, (VOID *) Object->TLSImage, Object->TLSFileSize);
            DTV[(Index + 1) * 2] = Address;
        }
    }

    return TCB;
}

VOID LoaderTLSFree(VOID *Pointer)
{
    if (Pointer != NULL)
    {
        UINT64 *TCB = Pointer;
        LoaderCall(11, TCB[3], TCB[4], 0, 0, 0, 0);
    }
}

VOID *LoaderTLSAddress(UINT64 *Index)
{
    UINT64 *TCB;
    ASM("movq %%fs:0, %0" : "=r"(TCB));

    if (Index[0] == 0 || Index[0] > ObjectCount || Index[1] >= Objects[Index[0] - 1].TLSSize)
    {
        LoaderFail((CONST CHAR8 *) "invalid TLS module or offset");
    }

    UINT64 *DTV = (VOID *) TCB[1];
    return (VOID *) (DTV[Index[0] * 2] + Index[1]);
}

STATIC VOID LoaderTLSLayout(VOID)
{
    for (UINT32 Index = 0; Index < ObjectCount; Index++)
    {
        LOADER_OBJECT *Object = &Objects[Index];

        for (UINT32 PH = 0; PH < Object->HeaderCount; PH++)
        {
            ELF64_PROGRAM_HEADER *Header = &Object->Headers[PH];

            if (Header->Type != ELF64_PT_TLS)
            {
                continue;
            }

            UINT64 Align = Header->Align == 0 ? 1 : Header->Align;

            if ((Align & (Align - 1)) != 0 || Align > 4096 || Header->FileSize > Header->MemSize || Header->MemSize > LOADER_FILE_LIMIT || TLSSize > LOADER_FILE_LIMIT - Header->MemSize || Object->TLSSize != 0)
            {
                LoaderFail((CONST CHAR8 *) "unsupported TLS layout");
            }

            UINT64 Skew         = Header->VAddr & (Align - 1);
            TLSSize             = ((TLSSize + Header->MemSize + Skew + Align - 1) & ~(Align - 1)) - Skew;
            Object->TLSOffset   = TLSSize;
            Object->TLSSize     = Header->MemSize;
            Object->TLSFileSize = Header->FileSize;
            Object->TLSImage    = LoaderPointer(Object, Header->VAddr, Header->FileSize);

            if (Align > TLSAlignment)
            {
                TLSAlignment = Align;
            }
        }
    }
}

STATIC VOID LoaderRelocations(LOADER_OBJECT *Object, UINT64 Offset, UINT64 Size, BOOLEAN Resolvers)
{
    if (Size == 0)
    {
        return;
    }

    if (Size % sizeof(ELF64_RELA) != 0)
    {
        LoaderFail((CONST CHAR8 *) "invalid RELA size");
    }

    ELF64_RELA *Relocations = (VOID *) LoaderPointer(Object, Offset, Size);

    for (UINT64 Index = 0; Index < Size / sizeof(*Relocations); Index++)
    {
        ELF64_RELA Relocation  = Relocations[Index];
        UINT32     Type        = ELF64_R_TYPE(Relocation.Info);
        UINT32     SymbolIndex = ELF64_R_SYM(Relocation.Info);

        if (Type == ELF64_R_X86_64_NONE || (Type == ELF64_R_X86_64_IRELATIVE && !Resolvers) || (Resolvers && Type != ELF64_R_X86_64_IRELATIVE && Type != ELF64_R_X86_64_64 && Type != ELF64_R_X86_64_GLOB_DAT && Type != ELF64_R_X86_64_JUMP_SLOT))
        {
            continue;
        }

        UINT64 Target;

        if (__builtin_add_overflow(Object->Bias, Relocation.Offset, &Target) || !LoaderContains(Object, Target, 8, ELF64_PF_W))
        {
            LoaderFail((CONST CHAR8 *) "relocation target outside writable segment");
        }

        UINT64 Value = 0;

        if (Type == ELF64_R_X86_64_RELATIVE || Type == ELF64_R_X86_64_IRELATIVE)
        {
            if (SymbolIndex != 0)
            {
                LoaderFail((CONST CHAR8 *) "relative relocation has a symbol");
            }

            Value = Object->Bias + (UINT64) Relocation.Addend;

            if (Type == ELF64_R_X86_64_IRELATIVE)
            {
                if (!LoaderContains(Object, Value, 1, ELF64_PF_X))
                {
                    LoaderFail((CONST CHAR8 *) "IFUNC resolver outside executable segment");
                }

                Value = ((UINT64 (*)(VOID)) Value)();
            }
        }
        else if (Type == ELF64_R_X86_64_64 || Type == ELF64_R_X86_64_GLOB_DAT || Type == ELF64_R_X86_64_JUMP_SLOT || Type == ELF64_R_X86_64_COPY)
        {
            if (SymbolIndex >= Object->SymbolCount)
            {
                LoaderFail((CONST CHAR8 *) "invalid symbol index");
            }

            CONST CHAR8 *Name    = LoaderString(Object, Object->Symbols[SymbolIndex].Name);
            UINT64       Builtin = LoaderEqual(Name, (CONST CHAR8 *) "__tls_get_addr") ? (UINT64) LoaderTLSAddress : LoaderEqual(Name, (CONST CHAR8 *) "LineOSTLSCreate") ? (UINT64) LoaderTLSCreate : LoaderEqual(Name, (CONST CHAR8 *) "LineOSTLSFree") ? (UINT64) LoaderTLSFree : 0;

            if (Builtin != 0 && Type != ELF64_R_X86_64_COPY)
            {
                Value = Builtin + (Type == ELF64_R_X86_64_64 ? Relocation.Addend : 0);
                LoaderCopy((VOID *) Target, &Value, sizeof(Value));
                continue;
            }

            LOADER_OBJECT *Owner;
            ELF64_SYMBOL  *Symbol = LoaderResolve(Object, SymbolIndex, &Owner, Type == ELF64_R_X86_64_COPY);

            if (Symbol != NULL)
            {
                if (ELF64_ST_TYPE(Symbol->Info) == ELF64_STT_TLS)
                {
                    LoaderFail((CONST CHAR8 *) "TLS symbol in non-TLS relocation");
                }

                BOOLEAN IFUNC = ELF64_ST_TYPE(Symbol->Info) == 10;

                if (IFUNC != Resolvers)
                {
                    continue;
                }

                Value = Symbol->SHNdx == ELF64_SHN_ABS ? Symbol->Value : LoaderPointer(Owner, Symbol->Value, Symbol->Size);

                if (IFUNC)
                {
                    if (!LoaderContains(Owner, Value, 1, ELF64_PF_X))
                    {
                        LoaderFail((CONST CHAR8 *) "IFUNC outside executable segment");
                    }

                    Value = ((UINT64 (*)(VOID)) Value)();
                }
            }

            if (Symbol == NULL && Resolvers)
            {
                continue;
            }

            if (Type == ELF64_R_X86_64_COPY)
            {
                UINT64 Length = Object->Symbols[SymbolIndex].Size;

                if (Symbol == NULL || Symbol->Size < Length || !LoaderContains(Object, Target, Length, ELF64_PF_W))
                {
                    LoaderFail((CONST CHAR8 *) "invalid COPY relocation");
                }

                LoaderCopy((VOID *) Target, (VOID *) Value, Length);
                continue;
            }

            if (Type == ELF64_R_X86_64_64)
            {
                Value += Relocation.Addend;
            }
        }
        else if (Type == ELF64_R_X86_64_DTPMOD64 || Type == ELF64_R_X86_64_DTPOFF64 || Type == ELF64_R_X86_64_TPOFF64)
        {
            LOADER_OBJECT *Owner  = Object;
            ELF64_SYMBOL  *Symbol = SymbolIndex == 0 ? NULL : LoaderResolve(Object, SymbolIndex, &Owner, FALSE);

            if (Owner == NULL || Owner->TLSSize == 0 || (Symbol != NULL && (ELF64_ST_TYPE(Symbol->Info) != ELF64_STT_TLS || !LoaderRange(Symbol->Value, Symbol->Size, Owner->TLSSize))))
            {
                LoaderFail((CONST CHAR8 *) "invalid TLS relocation");
            }

            Value = Type == ELF64_R_X86_64_DTPMOD64 ? (UINT64) (Owner - Objects) + 1 : (Symbol == NULL ? 0 : Symbol->Value) + Relocation.Addend;

            if (Type == ELF64_R_X86_64_TPOFF64)
            {
                Value -= Owner->TLSOffset;
            }
        }
        else
        {
            LoaderFail((CONST CHAR8 *) "unsupported x86_64 relocation");
        }

        LoaderCopy((VOID *) Target, &Value, sizeof(Value));
    }
}

STATIC VOID LoaderProtect(LOADER_OBJECT *Object)
{
    for (UINT32 Index = 0; Index < Object->HeaderCount; Index++)
    {
        ELF64_PROGRAM_HEADER *Header = &Object->Headers[Index];

        if (Header->Type != ELF64_PT_LOAD || Header->MemSize == 0)
        {
            continue;
        }

        UINT64 Start      = (Object->Bias + Header->VAddr) & ~4095ULL;
        UINT64 End        = (Object->Bias + Header->VAddr + Header->MemSize + 4095) & ~4095ULL;
        UINT32 Protection = ((Header->Flags & ELF64_PF_R) != 0 ? 1 : 0) | ((Header->Flags & ELF64_PF_W) != 0 ? 2 : 0) | ((Header->Flags & ELF64_PF_X) != 0 ? 4 : 0);

        if (LoaderCall(10, Start, End - Start, Protection, 0, 0, 0) != 0)
        {
            LoaderFail((CONST CHAR8 *) "segment mprotect failed");
        }
    }
}

STATIC VOID LoaderFunction(LOADER_OBJECT *Object, UINT64 Address)
{
    if (!LoaderContains(Object, Address, 1, ELF64_PF_X))
    {
        LoaderFail((CONST CHAR8 *) "initializer outside executable segment");
    }

    ((VOID (*)(UINT64, UINT64 *, UINT64 *)) Address)(InitialStack[0], InitialStack + 1, InitialStack + InitialStack[0] + 2);
}

STATIC VOID LoaderArray(LOADER_OBJECT *Object, UINT64 Tag, UINT64 SizeTag, BOOLEAN Reverse)
{
    UINT64 Size = Object->Tags[SizeTag];

    if (Size == 0)
    {
        return;
    }

    if (Size % 8 != 0)
    {
        LoaderFail((CONST CHAR8 *) "invalid initializer array");
    }

    UINT64 *Array = (VOID *) LoaderPointer(Object, Object->Tags[Tag], Size);

    for (UINT64 Index = 0; Index < Size / 8; Index++)
    {
        UINT64 Address = Array[Reverse ? Size / 8 - Index - 1 : Index];

        if (Address != 0 && Address != UINT64_MAX)
        {
            BOOLEAN Valid = FALSE;

            for (UINT32 Owner = 0; Owner < ObjectCount; Owner++)
            {
                if (LoaderContains(&Objects[Owner], Address, 1, ELF64_PF_X))
                {
                    LoaderFunction(&Objects[Owner], Address);
                    Valid = TRUE;
                    break;
                }
            }

            if (!Valid)
            {
                LoaderFail((CONST CHAR8 *) "invalid initializer function");
            }
        }
    }
}

STATIC VOID LoaderInitialize(UINT32 Index)
{
    LOADER_OBJECT *Object = &Objects[Index];

    if (Object->InitState != 0)
    {
        return;
    }

    Object->InitState = 1;

    for (UINT32 Dependency = 0; Dependency < Object->DependencyCount; Dependency++)
    {
        LoaderInitialize(Object->Dependencies[Dependency]);
    }

    if (Object->Tags[ELF64_DT_INIT] != 0)
    {
        LoaderFunction(Object, LoaderPointer(Object, Object->Tags[ELF64_DT_INIT], 1));
    }

    LoaderArray(Object, ELF64_DT_INIT_ARRAY, ELF64_DT_INIT_ARRAYSZ, FALSE);
    Object->InitState      = 2;
    InitOrder[InitCount++] = Index;
}

STATIC VOID LoaderFinalize(VOID)
{
    while (InitCount != 0)
    {
        LOADER_OBJECT *Object = &Objects[InitOrder[--InitCount]];
        LoaderArray(Object, ELF64_DT_FINI_ARRAY, ELF64_DT_FINI_ARRAYSZ, TRUE);

        if (Object->Tags[ELF64_DT_FINI] != 0)
        {
            LoaderFunction(Object, LoaderPointer(Object, Object->Tags[ELF64_DT_FINI], 1));
        }
    }
}

VOID NORETURN LoaderStart(UINT64 *Stack)
{
    InitialStack = Stack;
    UINT64 *Aux  = Stack + Stack[0] + 2;

    while (*Aux != 0)
    {
        Aux++;
    }

    Aux++;
    UINT64 Headers     = 0;
    UINT64 HeaderCount = 0;
    UINT64 Entry       = 0;

    for (UINT32 Index = 0; Index < 64 && Aux[0] != 0; Index++, Aux += 2)
    {
        if (Aux[0] == 3)
        {
            Headers = Aux[1];
        }
        else if (Aux[0] == 5)
        {
            HeaderCount = Aux[1];
        }
        else if (Aux[0] == 25)
        {
            LoaderCopy(TLSRandom, (VOID *) Aux[1], sizeof(TLSRandom));
        }
        else if (Aux[0] == 31)
        {
            CONST CHAR8 *Path = (VOID *) Aux[1];

            if (LoaderLength(Path) >= sizeof(Objects[0].Path))
            {
                LoaderFail((CONST CHAR8 *) "executable path too long");
            }

            LoaderCopy(Objects[0].Path, Path, LoaderLength(Path) + 1);
        }
        else if (Aux[0] == 23 && Aux[1] != 0)
        {
            LoaderFail((CONST CHAR8 *) "secure execution is unsupported");
        }
        else if (Aux[0] == 9)
        {
            Entry = Aux[1];
        }
    }

    if (Headers == 0 || HeaderCount == 0 || HeaderCount > LOADER_SEGMENT_LIMIT || Entry == 0)
    {
        LoaderFail((CONST CHAR8 *) "invalid auxiliary vector");
    }

    LOADER_OBJECT *Main = &Objects[0];
    Main->HeaderCount   = HeaderCount;
    LoaderCopy(Main->Headers, (VOID *) Headers, HeaderCount * sizeof(ELF64_PROGRAM_HEADER));
    BOOLEAN FoundBias = FALSE;

    for (UINT32 Index = 0; Index < HeaderCount; Index++)
    {
        ELF64_PROGRAM_HEADER *Header = &Main->Headers[Index];

        if (Header->Type == ELF64_PT_PHDR && Headers >= Header->VAddr)
        {
            Main->Bias = Headers - Header->VAddr;
            FoundBias  = TRUE;
        }
    }

    if (!FoundBias)
    {
        LoaderFail((CONST CHAR8 *) "main ELF lacks PT_PHDR");
    }

    Main->Entry = Entry;
    ObjectCount = 1;
    LoaderParse(Main);

    for (UINT32 Index = 0; Index < ObjectCount; Index++)
    {
        LoaderDependencies(&Objects[Index]);
    }

    LoaderTLSLayout();

    for (UINT32 Index = ObjectCount; Index != 0; Index--)
    {
        LOADER_OBJECT *Object = &Objects[Index - 1];

        if (Object->Tags[ELF64_DT_RELAENT] != 0 && Object->Tags[ELF64_DT_RELAENT] != sizeof(ELF64_RELA))
        {
            LoaderFail((CONST CHAR8 *) "unsupported RELA entry size");
        }

        if (Object->Tags[ELF64_DT_PLTRELSZ] != 0 && Object->Tags[ELF64_DT_PLTREL] != ELF64_DT_RELA)
        {
            LoaderFail((CONST CHAR8 *) "unsupported PLT relocation format");
        }

        LoaderRelocations(Object, Object->Tags[ELF64_DT_RELA], Object->Tags[ELF64_DT_RELASZ], FALSE);
        LoaderRelocations(Object, Object->Tags[ELF64_DT_JMPREL], Object->Tags[ELF64_DT_PLTRELSZ], FALSE);
        LoaderProtect(Object);
    }

    VOID *TCB = LoaderTLSCreate();

    if (LoaderCall(158, 0x1002, (UINT64) TCB, 0, 0, 0, 0) != 0)
    {
        LoaderFail((CONST CHAR8 *) "FS_BASE setup failed");
    }

    for (UINT32 Index = 0; Index < ObjectCount; Index++)
    {
        LOADER_OBJECT *Object = &Objects[Index];
        LoaderRelocations(Object, Object->Tags[ELF64_DT_RELA], Object->Tags[ELF64_DT_RELASZ], TRUE);
        LoaderRelocations(Object, Object->Tags[ELF64_DT_JMPREL], Object->Tags[ELF64_DT_PLTRELSZ], TRUE);
    }

    for (UINT32 Index = 0; Index < ObjectCount; Index++)
    {
        LOADER_OBJECT *Object = &Objects[Index];

        for (UINT32 PH = 0; PH < Object->HeaderCount; PH++)
        {
            ELF64_PROGRAM_HEADER *Header = &Object->Headers[PH];

            if (Header->Type == ELF64_PT_GNU_RELRO)
            {
                UINT64 Start = (Object->Bias + Header->VAddr) & ~4095ULL;
                UINT64 End   = (Object->Bias + Header->VAddr + Header->MemSize) & ~4095ULL;

                if (End > Start && LoaderCall(10, Start, End - Start, 1, 0, 0, 0) != 0)
                {
                    LoaderFail((CONST CHAR8 *) "RELRO mprotect failed");
                }
            }
        }
    }

    LoaderInitialize(0);
    LoaderEnter(Entry, (UINT64) Stack, LoaderFinalize);
}
