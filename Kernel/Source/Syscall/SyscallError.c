// Kernel/Source/Syscall/SyscallError.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/SyscallError.h>
#include <Memory/PMM.h>

INT64 SyscallVFSError(VFS_ERROR Error)
{
    switch (Error)
    {
        case VFS_OK:
            return 0;

        case VFS_NOT_FOUND:
            return -ENOENT;

        case VFS_PERMISSION:
            return -EACCES;

        case VFS_INVALID:
            return -EINVAL;

        case VFS_UNSUPPORTED:
            return -EOPNOTSUPP;

        case VFS_BUSY:
            return -EBUSY;

        case VFS_READ_ONLY:
            return -EROFS;

        case VFS_EXISTS:
            return -EEXIST;

        case VFS_NO_SPACE:
            return -ENOSPC;

        case VFS_LIMIT:
            return -EFBIG;

        case VFS_LOOP:
            return -ELOOP;

        case VFS_NOT_EMPTY:
            return -ENOTEMPTY;

        case VFS_CROSS_PARTITION:
            return -EXDEV;

        case VFS_NOT_DIRECTORY:
            return -ENOTDIR;

        case VFS_IS_DIRECTORY:
            return -EISDIR;

        case VFS_NAME_TOO_LONG:
            return -ENAMETOOLONG;

        case VFS_NO_MEMORY:
            return -ENOMEM;

        default:
            return -EIO;
    }
}

INT64 SyscallCopyPath(TASK *Task, UINT64 Path, CHAR8 **Name)
{
    *Name = NULL;

    if (Task == NULL || Task->AddressSpace == NULL || Path == 0)
    {
        return -EFAULT;
    }

    CHAR8 *Copy = KAlloc(VFS_MAX_PATH);

    if (Copy == NULL)
    {
        return -ENOMEM;
    }

    for (UINT32 Index = 0; Index < VFS_MAX_PATH; Index++)
    {
        if (Path > UINT64_MAX - Index || !VMMCopyFromUser(Task->AddressSpace, &Copy[Index], Path + Index, 1))
        {
            KFree(Copy);

            return -EFAULT;
        }

        if (Copy[Index] == 0)
        {
            if (Index == 0)
            {
                KFree(Copy);

                return -ENOENT;
            }

            *Name = Copy;

            return 0;
        }
    }

    KFree(Copy);

    return -ENAMETOOLONG;
}
