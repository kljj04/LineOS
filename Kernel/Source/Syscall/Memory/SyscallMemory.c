// Kernel/Source/Syscall/Memory/SyscallMemory.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Memory/SyscallMemory.h>
#include <Syscall/Process/ProcessInternal.h>
#include <Syscall/Memory/Memory.h>
#include <Syscall/Errno.h>
#include <Memory/PMM.h>
#include <Exec/ELF64.h>
#include <Syscall/File/FileInternal.h>
#include <Syscall/SyscallError.h>
#include <Scheduler/LBPWRR.h>

#define MMAP_MAX_LENGTH (64ULL * 1024 * 1024)
#define MMAP_MAX_PAGES  (128ULL * 1024 * 1024 / 4096)

typedef struct MMAP_REGION
{
    struct MMAP_REGION *Next;
    UINT64              Address;
    UINT32              Count;
    BOOLEAN             Owned;
    BOOLEAN             Heap;
    UINT64              Pages[];
} MMAP_REGION;

STATIC VOID MemoryFreePage(TASK *Task, UINT64 Physical)
{
    if (Task->Process->Execution != NULL)
    {
        ELF64_EXECUTION *Execution = Task->Process->Execution;

        for (UINT64 Index = 0; Index < Execution->PageCount; Index++)
        {
            if ((UINT64) Execution->Pages[Index] == Physical)
            {
                Execution->Pages[Index] = Execution->Pages[--Execution->PageCount];
                break;
            }
        }
    }

    KMemFreePages((VOID *) Physical, 1);
}

VOID SyscallMemoryRelease(TASK *Task)
{
    while (Task->Process->Mappings != NULL)
    {
        MMAP_REGION *Region = Task->Process->Mappings;

        Task->Process->Mappings = Region->Next;

        for (UINT32 Index = 0; Region->Owned && Index < Region->Count; Index++)
        {
            if (Region->Pages[Index] != 0)
            {
                MemoryFreePage(Task, Region->Pages[Index]);
            }
        }

        KFree(Region);
    }
}

BOOLEAN SyscallMemoryShared(TASK *Task, UINT64 Address)
{
    if (Address >= VMM_SHARED_USER_START && Address < VMM_SHARED_USER_END)
    {
        return TRUE;
    }

    for (MMAP_REGION *Region = Task->Process->Mappings; Region != NULL; Region = Region->Next)
    {
        if (Address >= Region->Address && Address - Region->Address < (UINT64) Region->Count * 4096)
        {
            return !Region->Owned;
        }
    }

    return FALSE;
}

BOOLEAN SyscallMemoryClone(TASK *Child, TASK *Parent)
{
    for (MMAP_REGION *Region = Parent->Process->Mappings; Region != NULL; Region = Region->Next)
    {
        MMAP_REGION *Copy = KAlloc(sizeof(*Copy) + (UINT64) Region->Count * sizeof(UINT64));

        if (Copy == NULL)
        {
            return FALSE;
        }

        KMemSet(Copy, 0, sizeof(*Copy) + (UINT64) Region->Count * sizeof(UINT64));
        Copy->Address            = Region->Address;
        Copy->Count              = Region->Count;
        Copy->Owned              = Region->Owned;
        Copy->Heap               = Region->Heap;
        Copy->Next               = Child->Process->Mappings;
        Child->Process->Mappings = Copy;

        for (UINT32 Index = 0; Index < Copy->Count; Index++)
        {
            if (Region->Pages[Index] != 0)
            {
                if (!VMMTranslateAddressSpace(Child->Process->AddressSpace, Copy->Address + (UINT64) Index * 4096, &Copy->Pages[Index]))
                {
                    return FALSE;
                }
            }
        }
    }

    return TRUE;
}

STATIC INT64 MemoryMap(UINT64 Address, UINT64 Length, UINT64 Protection, UINT64 Flags, INT64 FD, UINT64 Offset)
{
    TASK      *Task                                    = LBPWRRGetCurrentTask();
    UINT64     Physical                                = 0;
    INT64      Result                                  = -ENOMEM;
    UINT64    *Expected                                = NULL;
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = NULL;
    VFS_NODE   Node                                    = {0};
    if (Task == NULL || Task->Process->AddressSpace == NULL)
    {
        return -ESRCH;
    }
    if (Length == 0 || Length > UINT64_MAX - 4095 || (Offset & 4095) != 0 || (Protection & ~7ULL) != 0 || ((Flags & 3) != MAP_PRIVATE && (Flags & 3) != MAP_SHARED))
    {
        return -EINVAL;
    }
    if ((Flags & ~(MAP_PRIVATE | MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED | MAP_FIXED_NOREPLACE | MAP_DENYWRITE)) != 0)
    {
        return -EOPNOTSUPP;
    }
    if (Offset > INT64_MAX - ((Length + 4095) & ~4095ULL))
    {
        return -EOVERFLOW;
    }
    if (Length > MMAP_MAX_LENGTH)
    {
        return -ENOMEM;
    }
    BOOLEAN Fixed   = (Flags & (MAP_FIXED | MAP_FIXED_NOREPLACE)) != 0;
    BOOLEAN Replace = (Flags & MAP_FIXED) != 0 && (Flags & MAP_FIXED_NOREPLACE) == 0;
    BOOLEAN Owned   = (Flags & MAP_ANONYMOUS) != 0;
    if (Owned && ((Flags & 3) != MAP_PRIVATE || Offset != 0))
    {
        return -EOPNOTSUPP;
    }
    if (!Owned)
    {
        File = FileAcquire(Task, (UINT32) FD);
        if (File == NULL)
        {
            return -EBADF;
        }
        if (File->Device != NULL)
        {
            if (Fixed)
            {
                return -EOPNOTSUPP;
            }
            Result = SyscallMapDevice(FD, Length, Protection, Flags, Offset, &Physical);
            if (Result < 0)
            {
                return Result;
            }
        }
        else
        {
            if (File->Directory)
            {
                return -ENODEV;
            }
            if ((File->Access & FILE_READ) == 0)
            {
                return -EACCES;
            }
            if (Task->Process->SecureExecution && !VFSIsTrustedPath(Task->Process->FileContext, File->Path))
            {
                return -EACCES;
            }
            if ((Flags & 3) != MAP_PRIVATE)
            {
                return -EOPNOTSUPP;
            }
            Result = SyscallVFSError(VFSLookup(Task->Process->FileContext, File->Path, &Node));
            if (Result < 0)
            {
                return Result;
            }
            if (Node.Number != File->Number || Node.Partition != File->Partition || Node.Generation != File->Generation)
            {
                return -ESTALE;
            }
            if ((Node.Mode & VFS_MODE_TYPE) != 0x8000U)
            {
                return -ENODEV;
            }
            Owned = TRUE;
        }
    }
    UINT32 Count = (UINT32) ((Length + 4095) / 4096);
    if (Fixed && ((Address & 4095) != 0 || Address < VMM_MMAP_START || Address >= VMM_MMAP_END || Count > (VMM_MMAP_END - Address) / 4096))
    {
        return (Address & 4095) != 0 ? -EINVAL : -ENOMEM;
    }
    if (Address < VMM_MMAP_START || Address >= VMM_MMAP_END || Count > (VMM_MMAP_END - Address) / 4096)
    {
        Address = VMM_MMAP_START;
    }
    Address &= ~4095ULL;
    while (!Fixed)
    {
        BOOLEAN Found = TRUE;
        for (UINT32 Index = 0; Index < Count; Index++)
        {
            UINT64 Existing;
            if (VMMTranslateAddressSpace(Task->Process->AddressSpace, Address + (UINT64) Index * 4096, &Existing))
            {
                Address += ((UINT64) Index + 1) * 4096;
                Found = FALSE;
                break;
            }
        }
        if (Address >= VMM_MMAP_END || Count > (VMM_MMAP_END - Address) / 4096)
        {
            return -ENOMEM;
        }
        if (Found)
        {
            break;
        }
    }
    UINT64 Used     = 0;
    UINT32 Replaced = 0;
    if (Fixed)
    {
        Expected = KAlloc((UINT64) Count * sizeof(UINT64));
        if (Expected == NULL)
        {
            return -ENOMEM;
        }
        KMemSet(Expected, 0, (UINT64) Count * sizeof(UINT64));
    }
    for (MMAP_REGION *Current = Task->Process->Mappings; Current != NULL; Current = Current->Next)
    {
        for (UINT32 Index = 0; Index < Current->Count; Index++)
        {
            if (Current->Pages[Index] == 0)
            {
                continue;
            }
            Used++;
            UINT64 PageAddress = Current->Address + (UINT64) Index * 4096;
            if (Fixed && PageAddress >= Address && PageAddress - Address < (UINT64) Count * 4096)
            {
                if (!Replace || Current->Heap || !Current->Owned)
                {
                    KFree(Expected);
                    return !Replace ? -EEXIST : -EOPNOTSUPP;
                }
                Expected[(PageAddress - Address) / 4096] = Current->Pages[Index];
                Replaced++;
            }
        }
    }
    if (Fixed)
    {
        for (UINT32 Index = 0; Index < Count; Index++)
        {
            UINT64 Existing;
            if (VMMTranslateAddressSpace(Task->Process->AddressSpace, Address + (UINT64) Index * 4096, &Existing) && Existing != Expected[Index])
            {
                KFree(Expected);
                return Replace ? -EOPNOTSUPP : -EEXIST;
            }
        }
    }
    if (Used > MMAP_MAX_PAGES || Count > MMAP_MAX_PAGES - Used + Replaced)
    {
        KFree(Expected);
        return -ENOMEM;
    }
    MMAP_REGION *Region = KAlloc(sizeof(*Region) + (UINT64) Count * sizeof(UINT64));
    if (Region == NULL)
    {
        KFree(Expected);
        return -ENOMEM;
    }
    KMemSet(Region, 0, sizeof(*Region) + (UINT64) Count * sizeof(UINT64));
    Region->Address = Address;
    Region->Count   = Count;
    Region->Owned   = Owned;
    Result          = -ENOMEM;
    for (UINT32 Index = 0; Index < Count; Index++)
    {
        Region->Pages[Index] = Owned ? (UINT64) KAllocPages(1) : Physical + (UINT64) Index * 4096;
        if (Region->Pages[Index] == 0)
        {
            goto Failed;
        }
        if (Owned)
        {
            KMemSet((VOID *) Region->Pages[Index], 0, 4096);
        }
        if (File != NULL && File->Device == NULL)
        {
            UINT64 Position   = Offset + (UINT64) Index * 4096;
            UINT32 ReadLength = 0;
            UINT32 Size       = Position >= Node.Size ? 0 : Node.Size - Position > 4096 ? 4096 : (UINT32) (Node.Size - Position);
            while (ReadLength < Size)
            {
                UINT32 Bytes = 0;
                Result       = SyscallVFSError(VFSReadChecked(Task->Process->FileContext, File->Path, &Node, Position + ReadLength, (UINT8 *) Region->Pages[Index] + ReadLength, Size - ReadLength, &Bytes));
                if (Result < 0 || Bytes == 0 || Bytes > Size - ReadLength)
                {
                    Result = Result < 0 ? Result : -EIO;
                    goto Failed;
                }
                ReadLength += Bytes;
            }
            Result = -ENOMEM;
        }
    }
    UINT64 PageFlags = ((Protection & PROT_WRITE) != 0 ? VMM_FLAG_WRITABLE : 0) | ((Protection & PROT_EXEC) == 0 ? VMM_FLAG_NO_EXECUTE : 0) | (Protection == PROT_NONE ? VMM_FLAG_NO_ACCESS : 0);
    if (!(Replace ? VMMReplaceUserPages(Task->Process->AddressSpace, Address, Region->Pages, Expected, Count, PageFlags) : VMMMapUserPages(Task->Process->AddressSpace, Address, Region->Pages, Count, PageFlags)))
    {
        goto Failed;
    }
    if (Replace)
    {
        MMAP_REGION **Link = &Task->Process->Mappings;
        while (*Link != NULL)
        {
            MMAP_REGION *Current = *Link;
            BOOLEAN      Empty   = TRUE;
            for (UINT32 Index = 0; Index < Current->Count; Index++)
            {
                UINT64 PageAddress = Current->Address + (UINT64) Index * 4096;
                if (Current->Pages[Index] != 0 && PageAddress >= Address && PageAddress - Address < (UINT64) Count * 4096)
                {
                    MemoryFreePage(Task, Current->Pages[Index]);
                    Current->Pages[Index] = 0;
                }
                Empty = Empty && Current->Pages[Index] == 0;
            }
            if (Empty && !Current->Heap)
            {
                *Link = Current->Next;
                KFree(Current);
            }
            else
            {
                Link = &Current->Next;
            }
        }
    }
    KFree(Expected);
    Region->Next            = Task->Process->Mappings;
    Task->Process->Mappings = Region;
    return (INT64) Address;
Failed:
    for (UINT32 Index = 0; Owned && Index < Count; Index++)
    {
        if (Region->Pages[Index] != 0)
        {
            KMemFreePages((VOID *) Region->Pages[Index], 1);
        }
    }
    KFree(Expected);
    KFree(Region);
    return Result;
}

STATIC INT64 MemoryUnmap(UINT64 Address, UINT64 Length)
{
    TASK *Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->Process->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    if ((Address & 4095) != 0 || Length == 0 || Address < VMM_MMAP_START || Address >= VMM_MMAP_END || Length > VMM_MMAP_END - Address)
    {
        return -EINVAL;
    }

    Length = (Length + 4095) & ~4095ULL;

    MMAP_REGION **Link = &Task->Process->Mappings;

    while (*Link != NULL)
    {
        MMAP_REGION *Region = *Link;
        UINT64       Start  = Address > Region->Address ? Address : Region->Address;
        UINT64       End    = Address + Length < Region->Address + (UINT64) Region->Count * 4096 ? Address + Length : Region->Address + (UINT64) Region->Count * 4096;

        if (Start < End)
        {
            UINT32 First = (UINT32) ((Start - Region->Address) / 4096);
            UINT32 Count = (UINT32) ((End - Start) / 4096);

            if (!VMMUnmapUserPages(Task->Process->AddressSpace, Start, Region->Pages + First, Count))
            {
                return -EBUSY;
            }

            for (UINT32 Index = First; Index < First + Count; Index++)
            {
                if (Region->Owned && Region->Pages[Index] != 0)
                {
                    MemoryFreePage(Task, Region->Pages[Index]);
                }

                Region->Pages[Index] = 0;
            }
        }

        BOOLEAN Empty = TRUE;

        for (UINT32 Index = 0; Index < Region->Count; Index++)
        {
            Empty = Empty && Region->Pages[Index] == 0;
        }

        if (Empty)
        {
            *Link = Region->Next;
            KFree(Region);
        }
        else
        {
            Link = &Region->Next;
        }
    }

    return 0;
}

INT64 SyscallMmap(SYSCALL_CONTEXT *Context)
{
    UINT64 Address    = Context->Args.RDI;
    UINT64 Length     = Context->Args.RSI;
    UINT64 Protection = Context->Args.RDX;
    UINT64 Flags      = Context->Args.R10;
    INT64  FD         = (INT64) Context->Args.R8;
    UINT64 Offset     = Context->Args.R9;

    if (!ProcessBegin())
    {
        return -EINTR;
    }

    return ProcessEnd(MemoryMap(Address, Length, Protection, Flags, FD, Offset));
}

INT64 SyscallMunmap(SYSCALL_CONTEXT *Context)
{
    UINT64 Address = Context->Args.RDI;
    UINT64 Length  = Context->Args.RSI;

    if (!ProcessBegin())
    {
        return -EINTR;
    }

    return ProcessEnd(MemoryUnmap(Address, Length));
}

INT64 SyscallMprotect(SYSCALL_CONTEXT *Context)
{
    UINT64 Address    = Context->Args.RDI;
    UINT64 Length     = Context->Args.RSI;
    UINT64 Protection = Context->Args.RDX;

    TASK *Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->Process->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    if ((Address & 4095) != 0 || Length > UINT64_MAX - 4095 || (Protection & ~7ULL) != 0)
    {
        return -EINVAL;
    }

    if (Length == 0)
    {
        return 0;
    }

    UINT64 Count = (Length + 4095) / 4096;

    if (Count > UINT32_MAX || !ProcessBegin())
    {
        return -ENOMEM;
    }

    UINT64 Flags = ((Protection & PROT_WRITE) != 0 ? VMM_FLAG_WRITABLE : 0) | ((Protection & PROT_EXEC) == 0 ? VMM_FLAG_NO_EXECUTE : 0) | (Protection == PROT_NONE ? VMM_FLAG_NO_ACCESS : 0);

    return ProcessEnd(VMMProtectUserPages(Task->Process->AddressSpace, Address, (UINT32) Count, Flags) ? 0 : -EBUSY);
}

INT64 SyscallBrk(SYSCALL_CONTEXT *Context)
{
    UINT64 Address = Context->Args.RDI;

    TASK *Task = LBPWRRGetCurrentTask();
    if (Task == NULL || Task->Process->AddressSpace == NULL)
    {
        return 0;
    }
    PROCESS *Process = Task->Process;
    if (!ProcessBegin())
    {
        return (INT64) Process->Break;
    }
    UINT64 Previous = Process->Break;
    if (Address < Process->BreakBase || Address - Process->BreakBase > MMAP_MAX_LENGTH || Process->BreakBase == 0 || Address >= (1ULL << 47) - 4095)
    {
        return ProcessEnd((INT64) Previous);
    }
    UINT64        Start = (Previous + 4095) & ~4095ULL;
    UINT64        End   = (Address + 4095) & ~4095ULL;
    MMAP_REGION **Link  = &Process->Mappings;
    while (*Link != NULL && !(*Link)->Heap)
    {
        Link = &(*Link)->Next;
    }
    MMAP_REGION *Region = *Link;
    if (End > Start)
    {
        UINT32 Count    = (UINT32) ((End - Process->BreakBase) / 4096);
        UINT32 OldCount = Region == NULL ? 0 : Region->Count;
        UINT64 Used     = 0;
        for (MMAP_REGION *Current = Process->Mappings; Current != NULL; Current = Current->Next)
        {
            for (UINT32 Index = 0; Index < Current->Count; Index++)
            {
                Used += Current->Pages[Index] != 0;
            }
        }
        if (Used > MMAP_MAX_PAGES || Count - OldCount > MMAP_MAX_PAGES - Used)
        {
            return ProcessEnd((INT64) Previous);
        }
        MMAP_REGION *New = KAlloc(sizeof(*New) + (UINT64) Count * sizeof(UINT64));
        if (New == NULL)
        {
            return ProcessEnd((INT64) Previous);
        }
        KMemSet(New, 0, sizeof(*New) + (UINT64) Count * sizeof(UINT64));
        New->Address = Process->BreakBase;
        New->Count   = Count;
        New->Owned   = TRUE;
        New->Heap    = TRUE;
        New->Next    = Region == NULL ? NULL : Region->Next;
        if (Region != NULL)
        {
            KMemCpy(New->Pages, Region->Pages, (UINT64) OldCount * sizeof(UINT64));
        }
        UINT32 Index;
        for (Index = OldCount; Index < Count; Index++)
        {
            UINT64 Existing;
            if (VMMTranslateAddressSpace(Process->AddressSpace, New->Address + (UINT64) Index * 4096, &Existing))
            {
                break;
            }
            New->Pages[Index] = (UINT64) KAllocPages(1);
            if (New->Pages[Index] == 0)
            {
                break;
            }
            KMemSet((VOID *) New->Pages[Index], 0, 4096);
        }
        if (Index != Count || !VMMMapUserPages(Process->AddressSpace, Start, New->Pages + OldCount, Count - OldCount, VMM_FLAG_WRITABLE | VMM_FLAG_NO_EXECUTE))
        {
            for (UINT32 Page = OldCount; Page < Count; Page++)
            {
                if (New->Pages[Page] != 0)
                {
                    KMemFreePages((VOID *) New->Pages[Page], 1);
                }
            }
            KFree(New);
            return ProcessEnd((INT64) Previous);
        }
        *Link = New;
        KFree(Region);
    }
    else if (End < Start)
    {
        UINT32 Count = (UINT32) ((End - Process->BreakBase) / 4096);
        if (Region == NULL || !VMMUnmapUserPages(Process->AddressSpace, End, Region->Pages + Count, Region->Count - Count))
        {
            return ProcessEnd((INT64) Previous);
        }
        for (UINT32 Index = Count; Index < Region->Count; Index++)
        {
            MemoryFreePage(Task, Region->Pages[Index]);
        }
        Region->Count = Count;
    }
    Process->Break = Address;
    return ProcessEnd((INT64) Address);
}
