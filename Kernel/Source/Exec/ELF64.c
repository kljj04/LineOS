// Kernel/Source/Exec/ELF64.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Arch/AMD64/CPU.h>
#include <Syscall/File/SyscallFile.h>
#include <ELF/ELF64.h>
#include <Exec/ELF64.h>
#include <Memory/PMM.h>
#include <Debug/Debug.h>
#include <Syscall/SyscallError.h>

STATIC BOOLEAN ELF64RangeValid(UINT64 Offset, UINT64 Length, UINT64 TotalSize)
{
    return Offset <= TotalSize && Length <= TotalSize - Offset;
}

STATIC BOOLEAN ELF64IsPowerOfTwo(UINT64 Value)
{
    return Value != 0 && (Value & (Value - 1)) == 0;
}

STATIC BOOLEAN ELF64ValidateLoadSegment(CONST ELF64_PROGRAM_HEADER *ProgramHeader, UINT64 FileSize)
{
    if (ProgramHeader->FileSize > ProgramHeader->MemSize || ProgramHeader->MemSize == 0)
    {
        return FALSE;
    }

    if (!ELF64RangeValid(ProgramHeader->Offset, ProgramHeader->FileSize, FileSize))
    {
        return FALSE;
    }

    if (ProgramHeader->VAddr > UINT64_MAX - ProgramHeader->MemSize)
    {
        return FALSE;
    }

    if (ProgramHeader->Align > 1)
    {
        if (!ELF64IsPowerOfTwo(ProgramHeader->Align))
        {
            return FALSE;
        }

        if ((ProgramHeader->VAddr % ProgramHeader->Align) != (ProgramHeader->Offset % ProgramHeader->Align))
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOLEAN ELF64Validate(CONST VOID *FileData, UINT64 FileSize)
{
    CONST UINT8         *Data;
    ELF64_HEADER         Header;
    ELF64_PROGRAM_HEADER ProgramHeader;
    UINT64               ProgramHeaderOffset;
    UINT64               ProgramHeaderSize;
    UINT64               SegmentEnd;
    UINT32               LoadCount = 0;
    UINT32               InterpreterCount = 0;
    UINT32               DynamicCount = 0;
    BOOLEAN              EntryExecutable = FALSE;

    if (FileData == NULL || FileSize < sizeof(ELF64_HEADER))
    {
        return FALSE;
    }

    Data = (CONST UINT8 *) FileData;
    KMemCpy(&Header, Data, sizeof(Header));

    if (Header.Ident[ELF64_EI_MAG0] != ELF64_MAGIC0 || Header.Ident[ELF64_EI_MAG1] != ELF64_MAGIC1 || Header.Ident[ELF64_EI_MAG2] != ELF64_MAGIC2 || Header.Ident[ELF64_EI_MAG3] != ELF64_MAGIC3)
    {
        return FALSE;
    }

    if (Header.Ident[ELF64_EI_CLASS] != ELF64_CLASS_64 || Header.Ident[ELF64_EI_DATA] != ELF64_DATA_LSB || Header.Ident[ELF64_EI_VERSION] != ELF64_VERSION_CURRENT)
    {
        return FALSE;
    }

    if (Header.Type != ELF64_ET_EXEC && Header.Type != ELF64_ET_DYN)
    {
        return FALSE;
    }

    if (Header.Machine != ELF64_EM_X86_64 || Header.Version != ELF64_VERSION_CURRENT)
    {
        return FALSE;
    }

    if (Header.EHSize != sizeof(ELF64_HEADER) || Header.PHEntSize != sizeof(ELF64_PROGRAM_HEADER) || Header.PHNum == 0)
    {
        return FALSE;
    }

    ProgramHeaderSize = (UINT64) Header.PHNum * Header.PHEntSize;

    if (Header.PHOff < Header.EHSize || !ELF64RangeValid(Header.PHOff, ProgramHeaderSize, FileSize))
    {
        return FALSE;
    }

    for (UINT32 Index = 0; Index < Header.PHNum; Index++)
    {
        ProgramHeaderOffset = Header.PHOff + (UINT64) Index * Header.PHEntSize;
        KMemCpy(&ProgramHeader, Data + ProgramHeaderOffset, sizeof(ProgramHeader));

        if (ProgramHeader.Type == ELF64_PT_LOAD)
        {
            if (!ELF64ValidateLoadSegment(&ProgramHeader, FileSize))
            {
                return FALSE;
            }

            LoadCount++;

            SegmentEnd = ProgramHeader.VAddr + ProgramHeader.MemSize;

            if ((ProgramHeader.Flags & ELF64_PF_X) != 0 && Header.Entry >= ProgramHeader.VAddr && Header.Entry < SegmentEnd)
            {
                EntryExecutable = TRUE;
            }
        }
        else if (ProgramHeader.Type == ELF64_PT_INTERP)
        {
            if (++InterpreterCount > 1 || ProgramHeader.FileSize < 2 || !ELF64RangeValid(ProgramHeader.Offset, ProgramHeader.FileSize, FileSize))
            {
                return FALSE;
            }

            if (Data[ProgramHeader.Offset + ProgramHeader.FileSize - 1] != 0)
            {
                return FALSE;
            }
        }
        else if (ProgramHeader.Type == ELF64_PT_DYNAMIC)
        {
            if (++DynamicCount > 1 || ProgramHeader.FileSize == 0 || ProgramHeader.FileSize % sizeof(ELF64_DYNAMIC) != 0 || !ELF64RangeValid(ProgramHeader.Offset, ProgramHeader.FileSize, FileSize))
            {
                return FALSE;
            }
        }
    }

    if (LoadCount == 0)
    {
        return FALSE;
    }

    if (Header.Type == ELF64_ET_EXEC && !EntryExecutable)
    {
        return FALSE;
    }

    if (Header.Type == ELF64_ET_DYN && Header.Entry != 0 && !EntryExecutable)
    {
        return FALSE;
    }

    return TRUE;
}

#include <Scheduler/Task.h>
#include <Scheduler/LBPWRR.h>

#define ELF64_EXEC_PAGE_SIZE   4096ULL
#define ELF64_EXEC_USER_END    (1ULL << 47)
#define ELF64_EXEC_STACK       (ELF64_EXEC_USER_END - 17 * ELF64_EXEC_PAGE_SIZE)
#define ELF64_EXEC_STACK_PAGES 16U
#define ELF64_EXEC_MAX_FILE    (64ULL * 1024 * 1024)

STATIC VOID ELF64UnusedEntry(VOID) {}

BOOLEAN ELF64Release(ELF64_EXECUTION *Execution)
{
    if (Execution == NULL)
    {
        return FALSE;
    }

    if (Execution->Task != NULL)
    {
        if (Execution->Task->OnCPU || Execution->Task->Scheduled || (Execution->Task->State != TASK_TERMINATED && Execution->Task->State != TASK_KILLED) || ATOMIC_LOAD(&Execution->Task->Process->ThreadCount, ATOMIC_ACQUIRE) != 1)
        {
            return FALSE;
        }
    }

    if (Execution->AddressSpace != NULL && !VMMDestroyAddressSpace(Execution->AddressSpace))
    {
        return FALSE;
    }

    Execution->AddressSpace = NULL;

    if (Execution->Task != NULL)
    {
        Execution->Task->Process->AddressSpace = NULL;

        if (!TaskDestroy(Execution->Task))
        {
            return FALSE;
        }

        Execution->Task = NULL;
    }

    while (Execution->PageCount != 0)
    {
        KMemFreePages(Execution->Pages[--Execution->PageCount], 1);
    }

    return TRUE;
}

STATIC VOID *ELF64MapOwnedPage(ELF64_EXECUTION *Execution, UINT64 Address, UINT64 Flags)
{
    VOID *Page;

    if (Execution->PageCount == ELF64_EXEC_MAX_PAGES)
    {
        return NULL;
    }

    Page = KAllocPages(1);

    if (Page == NULL)
    {
        return NULL;
    }

    KMemSet(Page, 0, ELF64_EXEC_PAGE_SIZE);

    if (!VMMMapAddressSpacePage(Execution->AddressSpace, Address, (UINT64) Page, VMM_FLAG_USER | Flags))
    {
        KMemFreePages(Page, 1);

        return NULL;
    }

    Execution->Pages[Execution->PageCount++] = Page;

    return Page;
}

STATIC BOOLEAN ELF64SetArguments(ELF64_EXECUTION *Execution, CONST ELF64_ARGUMENTS *Arguments);

STATIC BOOLEAN ELF64MapImage(CONST VOID *FileData, UINT64 FileSize, ELF64_EXECUTION *Execution, UINT64 Bias, UINT64 *Entry, UINT64 *ProgramHeaders)
{
    ELF64_HEADER         Header;
    ELF64_PROGRAM_HEADER Segment;
    ELF64_PROGRAM_HEADER Other;
    CONST UINT8         *Data = FileData;
    UINT64               PageCount = Execution->PageCount + ELF64_EXEC_STACK_PAGES;

    if (!ELF64Validate(FileData, FileSize))
    {
        return FALSE;
    }

    KMemCpy(&Header, Data, sizeof(Header));

    if (Header.Type == ELF64_ET_EXEC)
    {
        Bias = 0;
    }

    if (__builtin_add_overflow(Header.Entry, Bias, Entry))
    {
        return FALSE;
    }

    *ProgramHeaders = 0;

    for (UINT32 Index = 0; Index < Header.PHNum; Index++)
    {
        KMemCpy(&Segment, Data + Header.PHOff + Index * sizeof(Segment), sizeof(Segment));
        if (__builtin_add_overflow(Segment.VAddr, Bias, &Segment.VAddr))
        {
            return FALSE;
        }



        if (Segment.Type != ELF64_PT_LOAD)
        {
            continue;
        }

        if (Segment.VAddr < ELF64_EXEC_PAGE_SIZE || Segment.VAddr >= ELF64_EXEC_STACK - ELF64_EXEC_PAGE_SIZE || Segment.MemSize > ELF64_EXEC_STACK - ELF64_EXEC_PAGE_SIZE - Segment.VAddr || (Segment.Flags & ~7U) != 0 || (Segment.Flags & ELF64_PF_R) == 0 || (Segment.Flags & (ELF64_PF_W | ELF64_PF_X)) == (ELF64_PF_W | ELF64_PF_X) || (Segment.VAddr & 4095) != (Segment.Offset & 4095))
        {
            return FALSE;
        }

        if (Segment.Align > 1 && Bias % Segment.Align != 0)
        {
            return FALSE;
        }

        if (Header.PHOff >= Segment.Offset && Header.PHOff - Segment.Offset <= Segment.FileSize && (UINT64) Header.PHNum * sizeof(Segment) <= Segment.FileSize - (Header.PHOff - Segment.Offset))
        {
            *ProgramHeaders = Segment.VAddr + Header.PHOff - Segment.Offset;
        }

        UINT64 Start = Segment.VAddr & ~4095ULL;
        UINT64 End = (Segment.VAddr + Segment.MemSize + 4095) & ~4095ULL;

        PageCount += (End - Start) / ELF64_EXEC_PAGE_SIZE;

        if (PageCount > ELF64_EXEC_MAX_PAGES)
        {
            return FALSE;
        }

        for (UINT32 Previous = 0; Previous < Index; Previous++)
        {
            KMemCpy(&Other, Data + Header.PHOff + Previous * sizeof(Other), sizeof(Other));
            Other.VAddr += Bias;

            if (Other.Type == ELF64_PT_LOAD && Start < ((Other.VAddr + Other.MemSize + 4095) & ~4095ULL) && (Other.VAddr & ~4095ULL) < End)
            {
                return FALSE;
            }
        }
    }

    Execution->Error = -ENOMEM;

    if (Execution->AddressSpace == NULL && !VMMCreateAddressSpace(&Execution->AddressSpace))
    {
        return FALSE;
    }

    for (UINT32 Index = 0; Index < Header.PHNum; Index++)
    {
        KMemCpy(&Segment, Data + Header.PHOff + Index * sizeof(Segment), sizeof(Segment));
        if (__builtin_add_overflow(Segment.VAddr, Bias, &Segment.VAddr))
        {
            return FALSE;
        }


        if (Segment.Type != ELF64_PT_LOAD)
        {
            continue;
        }

        UINT64 End = (Segment.VAddr + Segment.MemSize + 4095) & ~4095ULL;

        for (UINT64 Address = Segment.VAddr & ~4095ULL; Address < End; Address += ELF64_EXEC_PAGE_SIZE)
        {
            VOID *Page = ELF64MapOwnedPage(Execution, Address, ((Segment.Flags & ELF64_PF_W) != 0 ? VMM_FLAG_WRITABLE : 0) | ((Segment.Flags & ELF64_PF_X) == 0 ? VMM_FLAG_NO_EXECUTE : 0));

            if (Page == NULL)
            {
                return FALSE;
            }

            UINT64 Start = Address < Segment.VAddr ? Segment.VAddr : Address;
            UINT64 CopyEnd = Address + ELF64_EXEC_PAGE_SIZE;

            if (CopyEnd > Segment.VAddr + Segment.FileSize)
            {
                CopyEnd = Segment.VAddr + Segment.FileSize;
            }

            if (Start < CopyEnd)
            {
                KMemCpy((UINT8 *) Page + Start - Address, Data + Segment.Offset + Start - Segment.VAddr, CopyEnd - Start);
            }
        }
    }

    return TRUE;
}

BOOLEAN ELF64Load(CONST VOID *FileData, UINT64 FileSize, ELF64_EXECUTION *Execution)
{
    ELF64_HEADER Header;

    if (Execution == NULL || Execution->Task != NULL || Execution->AddressSpace != NULL || Execution->PageCount != 0)
    {
        return FALSE;
    }

    KMemSet(Execution, 0, sizeof(*Execution));
    Execution->Error = -ENOEXEC;

    if (!ELF64Validate(FileData, FileSize))
    {
        return FALSE;
    }

    KMemCpy(&Header, FileData, sizeof(Header));
    Execution->LinuxStack = Header.Type == ELF64_ET_DYN;
    Execution->ProgramHeaderCount = Header.PHNum;

    if (!ELF64MapImage(FileData, FileSize, Execution, 0x400000000000ULL, &Execution->Entry, &Execution->ProgramHeaders))
    {
        goto Failure;
    }

    for (UINT32 Index = 0; Index < ELF64_EXEC_STACK_PAGES; Index++)
    {
        if (ELF64MapOwnedPage(Execution, ELF64_EXEC_STACK + Index * ELF64_EXEC_PAGE_SIZE, VMM_FLAG_WRITABLE) == NULL)
        {
            goto Failure;
        }
    }

    Execution->Task = TaskCreate(ELF64UnusedEntry);

    if (Execution->Task == NULL)
    {
        goto Failure;
    }

    TASK *Task = Execution->Task;

    Task->Process->AddressSpace = Execution->AddressSpace;
    Task->Process->Execution = Execution;
    Task->UserStackBase = ELF64_EXEC_STACK;
    Task->UserStackSize = ELF64_EXEC_STACK_PAGES * ELF64_EXEC_PAGE_SIZE;
    KMemSet(&Task->Context, 0, sizeof(Task->Context));
    Task->Context.RIP = Execution->Entry;
    Task->Context.CS = GDT_USER_CODE_SELECTOR | 3;
    Task->Context.SS = GDT_USER_DATA_SELECTOR | 3;
    Task->Context.RFLAGS = 0x202;

    if (!ELF64SetArguments(Execution, NULL))
    {
        TaskKill(Task);

        goto Failure;
    }

    Execution->Error = 0;

    return TRUE;

Failure:
    if (Execution->Task != NULL)
    {
        TaskKill(Execution->Task);
    }

    ELF64Release(Execution);

    return FALSE;
}

STATIC BOOLEAN ELF64SetLinuxStack(ELF64_EXECUTION *Execution)
{
    TASK *Task = Execution->Task;
    UINT64 Vector[2 * ELF64_ARG_MAX_COUNT + 64];
    UINT64 Count = 0;
    UINT64 ArgCount = Task->Context.RDI;
    UINT64 Random[2];
    UINT64 Top = Task->Context.RSP;
    UINT64 EnvCount = 0;

    if (!Execution->LinuxStack)
    {
        return TRUE;
    }

    UINT32 EAX;
    UINT32 EBX;
    UINT32 ECX;
    UINT32 EDX;
    CPUID(1, 0, &EAX, &EBX, &ECX, &EDX);

    if ((ECX & (1U << 30)) == 0)
    {
        Execution->Error = -EOPNOTSUPP;
        return FALSE;
    }

    for (UINT32 Index = 0; Index < 2; Index++)
    {
        UINT8 Ready = 0;

        for (UINT32 Attempt = 0; Attempt < 256 && !Ready; Attempt++)
        {
            ASM("rdrand %0; setc %1" : "=r"(Random[Index]), "=qm"(Ready));
        }

        if (!Ready)
        {
            Execution->Error = -EAGAIN;
            return FALSE;
        }
    }

    UINT64 RandomAddress = (Top - sizeof(Random)) & ~15ULL;

    if (!VMMCopyToUser(Execution->AddressSpace, RandomAddress, Random, sizeof(Random)))
    {
        return FALSE;
    }

    Vector[Count++] = ArgCount;

    for (UINT64 Index = 0; Index < ArgCount; Index++)
    {
        if (!VMMCopyFromUser(Execution->AddressSpace, &Vector[Count++], Task->Context.RSI + Index * 8, 8))
        {
            return FALSE;
        }
    }

    UINT64 PathLength = 0;

    while (Execution->ExecPath[PathLength] != 0 && PathLength + 1 < sizeof(Execution->ExecPath))
    {
        PathLength++;
    }

    UINT64 ExecName = RandomAddress - PathLength - 1;

    if (!VMMCopyToUser(Execution->AddressSpace, ExecName, Execution->ExecPath, PathLength + 1))
    {
        return FALSE;
    }
    Vector[Count++] = 0;

    if (Task->Context.RDX != 0)
    {
        for (; EnvCount < ELF64_ARG_MAX_COUNT; EnvCount++)
        {
            UINT64 Pointer;

            if (!VMMCopyFromUser(Execution->AddressSpace, &Pointer, Task->Context.RDX + EnvCount * 8, 8))
            {
                return FALSE;
            }

            if (Pointer == 0)
            {
                break;
            }

            Vector[Count++] = Pointer;
        }
    }

    Vector[Count++] = 0;
    CONST UINT64 Aux[] = {3, Execution->ProgramHeaders, 4, sizeof(ELF64_PROGRAM_HEADER), 5, Execution->ProgramHeaderCount, 6, 4096, 7, Execution->InterpreterBase, 9, Execution->Entry, 11, Task->Process->RealUID, 12, Task->Process->RealUID, 13, Task->Process->RealGID, 14, Task->Process->RealGID, 23, 0, 25, RandomAddress, 31, ExecName, 0, 0};
    KMemCpy(Vector + Count, Aux, sizeof(Aux));
    Count += sizeof(Aux) / sizeof(Aux[0]);
    UINT64 Stack = (ExecName - Count * 8) & ~15ULL;

    if (!VMMCopyToUser(Execution->AddressSpace, Stack, Vector, Count * 8))
    {
        return FALSE;
    }

    Task->Context.RSP = Stack;
    Task->Context.RDX = 0;
    KMemCpy((VOID *) Task->RSP, &Task->Context, sizeof(Task->Context));
    return TRUE;
}

STATIC BOOLEAN ELF64SetArguments(ELF64_EXECUTION *Execution, CONST ELF64_ARGUMENTS *Arguments)
{
    TASK  *Task = Execution->Task;
    UINT64 Pointers[ELF64_ARG_MAX_COUNT + 1] = {0};
    UINT32 Count = Arguments == NULL ? 0 : Arguments->Count;
    UINT32 Bytes = Arguments == NULL ? 0 : Arguments->Bytes;
    UINT32 Position = 0;
    UINT64 Strings;
    UINT64 Vector;

    if (Count > ELF64_ARG_MAX_COUNT || Bytes > ELF64_ARG_MAX_BYTES)
    {
        return FALSE;
    }

    Strings = ELF64_EXEC_STACK + Task->UserStackSize - 56 - Bytes;
    Vector = (Strings - (Count + 1) * sizeof(UINT64)) & ~7ULL;

    for (UINT32 Index = 0; Index < Count; Index++)
    {
        UINT32 Length = 0;

        if (Arguments->Offsets[Index] != Position)
        {
            return FALSE;
        }

        Pointers[Index] = Strings + Position;
        do
        {
            if (Position == Bytes || Length++ == ELF64_ARG_MAX_LENGTH)
            {
                return FALSE;
            }
        } while (Arguments->Data[Position++] != 0);
    }

    if (Position != Bytes || !VMMCopyToUser(Execution->AddressSpace, Vector, Pointers, (Count + 1) * sizeof(UINT64)) || (Bytes != 0 && !VMMCopyToUser(Execution->AddressSpace, Strings, Arguments->Data, Bytes)))
    {
        return FALSE;
    }

    Task->Context.RDI = Count;
    Task->Context.RSI = Vector;
    Task->Context.RSP = (Vector & ~15ULL) - 8;
    KMemCpy((VOID *) Task->RSP, &Task->Context, sizeof(Task->Context));

    return TRUE;
}

STATIC BOOLEAN ELF64LoadInterpreter(VFS_CONTEXT *Context, CONST VOID *Data, UINT64 Size, ELF64_EXECUTION *Execution)
{
    CONST ELF64_HEADER *Header = Data;

    for (UINT32 Index = 0; Index < Header->PHNum; Index++)
    {
        ELF64_PROGRAM_HEADER Segment;
        KMemCpy(&Segment, (CONST UINT8 *) Data + Header->PHOff + Index * sizeof(Segment), sizeof(Segment));

        if (Segment.Type != ELF64_PT_INTERP)
        {
            continue;
        }

        if (Segment.FileSize > 4096 || !ELF64RangeValid(Segment.Offset, Segment.FileSize, Size))
        {
            return FALSE;
        }

        CONST CHAR8 *Path = (CONST CHAR8 *) Data + Segment.Offset;

        if (Path[0] != '/' || Path[Segment.FileSize - 1] != 0)
        {
            return FALSE;
        }

        for (UINT64 Position = 0; Position + 1 < Segment.FileSize; Position++)
        {
            if (Path[Position] == 0)
            {
                return FALSE;
            }
        }

        VFS_NODE Node;
        UINT32 ReadLength;

        Execution->Error = SyscallVFSError(VFSLookup(Context, Path, &Node));

        if (Execution->Error != 0)
        {
            return FALSE;
        }

        if (Node.Size < sizeof(ELF64_HEADER) || Node.Size > ELF64_EXEC_MAX_FILE || (Node.Mode & VFS_MODE_TYPE) != 0x8000 || (Node.Mode & 0111) == 0)
        {
            Execution->Error = -ENOEXEC;
            return FALSE;
        }

        VOID *Image = KAlloc(Node.Size);

        if (Image == NULL)
        {
            Execution->Error = -ENOMEM;
            return FALSE;
        }

        Execution->Error = SyscallVFSError(VFSRead(Context, Path, 0, Image, (UINT32) Node.Size, &ReadLength));
        BOOLEAN Loaded = Execution->Error == 0 && ReadLength == Node.Size && ELF64Validate(Image, Node.Size);
        ELF64_HEADER Interpreter;

        if (Loaded)
        {
            KMemCpy(&Interpreter, Image, sizeof(Interpreter));
            Loaded = Interpreter.Type == ELF64_ET_DYN;

            for (UINT32 PH = 0; Loaded && PH < Interpreter.PHNum; PH++)
            {
                ELF64_PROGRAM_HEADER Program;
                KMemCpy(&Program, (UINT8 *) Image + Interpreter.PHOff + PH * sizeof(Program), sizeof(Program));
                Loaded = Program.Type != ELF64_PT_INTERP;
            }
        }

        UINT64 Entry = 0;
        UINT64 ProgramHeaders = 0;
        Execution->InterpreterBase = 0x400100000000ULL;
        Loaded = Loaded && ELF64MapImage(Image, Node.Size, Execution, Execution->InterpreterBase, &Entry, &ProgramHeaders);
        KFree(Image);

        if (!Loaded)
        {
            Execution->Error = -ENOEXEC;
            return FALSE;
        }

        Execution->LinuxStack = TRUE;
        Execution->Task->Context.RIP = Entry;
        return TRUE;
    }

    return TRUE;
}

STATIC BOOLEAN ELF64ExecuteProgram(VFS_CONTEXT *Context, CONST CHAR8 *Path, CONST ELF64_ARGUMENTS *Arguments, ELF64_EXECUTION *Execution, BOOLEAN AssetPublisher, BOOLEAN Schedule)
{
    VFS_NODE Node;
    VOID    *Data;
    UINT32   ReadLength;
    BOOLEAN  Loaded;

    if (Context == NULL || Path == NULL || Execution == NULL)
    {
        return FALSE;
    }

    Execution->Error = SyscallVFSError(VFSLookup(Context, Path, &Node));

    if (Execution->Error != 0)
    {
        return FALSE;
    }

    Execution->Error = -EACCES;

    if ((Node.Mode & VFS_MODE_TYPE) != 0x8000 || (Node.Mode & 0111) == 0)
    {
        return FALSE;
    }

    if (Node.Size == 0 || Node.Size > ELF64_EXEC_MAX_FILE)
    {
        Execution->Error = -ENOEXEC;

        return FALSE;
    }

    UINT16 ExecuteBit = Context->Credentials.UID == Node.UID ? 0100 : (Context->Credentials.GID == Node.GID ? 0010 : 0001);

    if (!Context->Credentials.Administrator && (Node.Mode & ExecuteBit) == 0)
    {
        DebugWriteLine("ELF64Execute: execute permission denied");

        return FALSE;
    }

    Data = KAlloc(Node.Size);

    if (Data == NULL)
    {
        Execution->Error = -ENOMEM;
        DebugWriteLine("ELF64Execute: file allocation failed");

        return FALSE;
    }

    Execution->Error = SyscallVFSError(VFSRead(Context, Path, 0, Data, (UINT32) Node.Size, &ReadLength));
    Loaded = Execution->Error == 0 && ReadLength == Node.Size;

    if (Loaded)
    {
        Loaded = ELF64Load(Data, Node.Size, Execution);

        if (Loaded)
        {
            Loaded = ELF64LoadInterpreter(Context, Data, Node.Size, Execution);

            if (!Loaded)
            {
                TaskKill(Execution->Task);
                ELF64Release(Execution);
            }
        }
    }
    else if (Execution->Error == 0)
    {
        Execution->Error = -EIO;
    }

    KFree(Data);

    if (!Loaded)
    {
        DebugWriteLine("ELF64Execute: read or load failed");

        return FALSE;
    }

    if (!ELF64SetArguments(Execution, Arguments))
    {
        TaskKill(Execution->Task);
        ELF64Release(Execution);

        return FALSE;
    }

    Execution->Task->Process->FileContext = KAlloc(sizeof(VFS_CONTEXT));

    if (Execution->Task->Process->FileContext == NULL)
    {
        Execution->Error = -ENOMEM;
        DebugWriteLine("ELF64Execute: context allocation failed");
        TaskKill(Execution->Task);
        ELF64Release(Execution);

        return FALSE;
    }

    KMemCpy(Execution->Task->Process->FileContext, Context, sizeof(VFS_CONTEXT));

    CONST CHAR8 *ProgramName = Path;

    for (CONST CHAR8 *Position = Path; *Position != 0; Position++)
    {
        if (*Position == '/')
        {
            ProgramName = Position + 1;
        }
    }

    UINTN NameLength = 0;

    while (ProgramName[NameLength] != 0 && NameLength + 1 < sizeof(Execution->Task->Process->Name))
    {
        Execution->Task->Process->Name[NameLength] = ProgramName[NameLength];
        NameLength++;
    }

    Execution->Task->Process->Name[NameLength] = 0;
    Execution->Task->Process->RealUID = Context->Credentials.UID;
    Execution->Task->Process->RealGID = Context->Credentials.GID;

    if (Execution->LinuxStack && (Node.Mode & 06000U) != 0)
    {
        Execution->Error = -EOPNOTSUPP;
        TaskKill(Execution->Task);
        ELF64Release(Execution);
        return FALSE;
    }

    if ((Node.Mode & 04000U) != 0)
    {
        Execution->Task->Process->FileContext->Credentials.UID = Node.UID;
        Execution->Task->Process->FileContext->Credentials.Administrator = Node.UID == 0;
    }

    UINT64 PathLength = 0;

    while (Path[PathLength] != 0 && PathLength + 1 < sizeof(Execution->ExecPath))
    {
        Execution->ExecPath[PathLength] = Path[PathLength];
        PathLength++;
    }

    Execution->ExecPath[PathLength] = 0;

    if (!ELF64SetLinuxStack(Execution))
    {
        TaskKill(Execution->Task);
        ELF64Release(Execution);
        return FALSE;
    }

    Execution->Task->Process->AssetPublisher = AssetPublisher;

    if (!Schedule)
    {
        return TRUE;
    }

    if (!SyscallFilesInherit(Execution->Task, AssetPublisher ? NULL : LBPWRRGetCurrentTask()) || !LBPWRRAddTask(Execution->Task))
    {
        Execution->Error = -EAGAIN;
        DebugWriteLine("ELF64Execute: scheduling failed");
        TaskKill(Execution->Task);
        ELF64Release(Execution);

        return FALSE;
    }

    return TRUE;
}

BOOLEAN ELF64Execute(VFS_CONTEXT *Context, CONST CHAR8 *Path, ELF64_EXECUTION *Execution)
{
    return ELF64ExecuteProgram(Context, Path, NULL, Execution, FALSE, TRUE);
}

BOOLEAN ELF64ExecuteInit(VFS_CONTEXT *Context, CONST CHAR8 *Path, ELF64_EXECUTION *Execution)
{
    return ELF64ExecuteProgram(Context, Path, NULL, Execution, TRUE, TRUE);
}

BOOLEAN ELF64ExecuteArguments(VFS_CONTEXT *Context, CONST CHAR8 *Path, CONST ELF64_ARGUMENTS *Arguments, ELF64_EXECUTION *Execution)
{
    return ELF64ExecuteProgram(Context, Path, Arguments, Execution, FALSE, TRUE);
}

BOOLEAN ELF64PrepareArguments(VFS_CONTEXT *Context, CONST CHAR8 *Path, CONST ELF64_ARGUMENTS *Arguments, ELF64_EXECUTION *Execution)
{
    return ELF64ExecuteProgram(Context, Path, Arguments, Execution, FALSE, FALSE);
}

BOOLEAN ELF64SetEnvironment(ELF64_EXECUTION *Execution, CONST ELF64_ARGUMENTS *Environment)
{
    TASK  *Task = Execution->Task;
    UINT64 Pointers[ELF64_ARG_MAX_COUNT + 1] = {0};
    UINT32 Count = Environment->Count;
    UINT32 Bytes = Environment->Bytes;

    if (Count > ELF64_ARG_MAX_COUNT || Bytes > ELF64_ARG_MAX_BYTES)
    {
        return FALSE;
    }

    UINT64 Strings = Task->Context.RSP - Bytes;
    UINT64 Vector = (Strings - (Count + 1) * sizeof(UINT64)) & ~7ULL;

    for (UINT32 Index = 0; Index < Count; Index++)
    {
        Pointers[Index] = Strings + Environment->Offsets[Index];
    }

    if (!VMMCopyToUser(Execution->AddressSpace, Vector, Pointers, (Count + 1) * sizeof(UINT64)) || (Bytes != 0 && !VMMCopyToUser(Execution->AddressSpace, Strings, Environment->Data, Bytes)))
    {
        return FALSE;
    }

    Task->Context.RDX = Vector;
    Task->Context.RSP = (Vector & ~15ULL) - 8;
    KMemCpy((VOID *) Task->RSP, &Task->Context, sizeof(Task->Context));

    return ELF64SetLinuxStack(Execution);
}
