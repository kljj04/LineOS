// Kernel/Source/Exec/ELF64.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <ELF/ELF64.h>
#include <Exec/ELF64.h>
#include <Memory/PMM.h>
#include <Debug/Debug.h>

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
        if (Execution->Task->OnCPU || Execution->Task->Scheduled || (Execution->Task->State != TASK_TERMINATED && Execution->Task->State != TASK_KILLED))
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
        Execution->Task->AddressSpace = NULL;
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

BOOLEAN ELF64Load(CONST VOID *FileData, UINT64 FileSize, ELF64_EXECUTION *Execution)
{
    ELF64_HEADER         Header;
    ELF64_PROGRAM_HEADER Segment;
    ELF64_PROGRAM_HEADER Other;
    CONST UINT8         *Data = FileData;
    UINT64               PageCount = ELF64_EXEC_STACK_PAGES;

    if (Execution == NULL || Execution->Task != NULL || Execution->AddressSpace != NULL || Execution->PageCount != 0)
    {
        return FALSE;
    }

    KMemSet(Execution, 0, sizeof(*Execution));
    if (!ELF64Validate(FileData, FileSize))
    {
        return FALSE;
    }

    KMemCpy(&Header, Data, sizeof(Header));
    if (Header.Type != ELF64_ET_EXEC)
    {
        return FALSE;
    }

    for (UINT32 Index = 0; Index < Header.PHNum; Index++)
    {
        KMemCpy(&Segment, Data + Header.PHOff + Index * sizeof(Segment), sizeof(Segment));
        if (Segment.Type == ELF64_PT_INTERP || Segment.Type == ELF64_PT_DYNAMIC || Segment.Type == ELF64_PT_TLS)
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
            if (Other.Type == ELF64_PT_LOAD && Start < ((Other.VAddr + Other.MemSize + 4095) & ~4095ULL) && (Other.VAddr & ~4095ULL) < End)
            {
                return FALSE;
            }
        }
    }

    if (!VMMCreateAddressSpace(&Execution->AddressSpace))
    {
        return FALSE;
    }

    for (UINT32 Index = 0; Index < Header.PHNum; Index++)
    {
        KMemCpy(&Segment, Data + Header.PHOff + Index * sizeof(Segment), sizeof(Segment));
        if (Segment.Type != ELF64_PT_LOAD)
        {
            continue;
        }
        UINT64 End = (Segment.VAddr + Segment.MemSize + 4095) & ~4095ULL;
        for (UINT64 Address = Segment.VAddr & ~4095ULL; Address < End; Address += ELF64_EXEC_PAGE_SIZE)
        {
            VOID *Page = ELF64MapOwnedPage(Execution, Address, (Segment.Flags & ELF64_PF_W) != 0 ? VMM_FLAG_WRITABLE : 0);
            if (Page == NULL)
            {
                goto Failure;
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
    Task->AddressSpace = Execution->AddressSpace;
    Task->UserStackBase = ELF64_EXEC_STACK;
    Task->UserStackSize = ELF64_EXEC_STACK_PAGES * ELF64_EXEC_PAGE_SIZE;
    KMemSet(&Task->Context, 0, sizeof(Task->Context));
    Task->Context.RIP = Header.Entry;
    Task->Context.CS = 0x23;
    Task->Context.SS = 0x2B;
    Task->Context.RFLAGS = 0x202;
    Task->Context.RSP = ELF64_EXEC_STACK + Task->UserStackSize - 56;
    KMemCpy((VOID *) Task->RSP, &Task->Context, sizeof(Task->Context));
    return TRUE;

Failure:
    ELF64Release(Execution);
    return FALSE;
}

BOOLEAN ELF64Execute(VFS_CONTEXT *Context, CONST CHAR8 *Path, ELF64_EXECUTION *Execution)
{
    VFS_NODE Node;
    VOID    *Data;
    UINT32   ReadLength;
    BOOLEAN  Loaded;

    if (Context == NULL || Path == NULL || Execution == NULL || VFSLookup(Context, Path, &Node) != VFS_OK || (Node.Mode & VFS_MODE_TYPE) != 0x8000 || (Node.Mode & 0111) == 0 || Node.Size == 0 || Node.Size > ELF64_EXEC_MAX_FILE)
    {
        DebugWriteLine("ELF64Execute: lookup or file metadata rejected");
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
        DebugWriteLine("ELF64Execute: file allocation failed");
        return FALSE;
    }
    Loaded = VFSRead(Context, Path, 0, Data, (UINT32) Node.Size, &ReadLength) == VFS_OK && ReadLength == Node.Size && ELF64Load(Data, Node.Size, Execution);
    KFree(Data);
    if (!Loaded)
    {
        DebugWriteLine("ELF64Execute: read or load failed");
        return FALSE;
    }
    Execution->Task->FileContext = KAlloc(sizeof(VFS_CONTEXT));
    if (Execution->Task->FileContext == NULL)
    {
        DebugWriteLine("ELF64Execute: context allocation failed");
        TaskKill(Execution->Task);
        ELF64Release(Execution);
        return FALSE;
    }
    KMemCpy(Execution->Task->FileContext, Context, sizeof(VFS_CONTEXT));

    if (!LBPWRRAddTask(Execution->Task))
    {
        DebugWriteLine("ELF64Execute: scheduling failed");
        TaskKill(Execution->Task);
        ELF64Release(Execution);
        return FALSE;
    }
    return TRUE;
}
