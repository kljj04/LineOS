// Tests/FS/Host.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <FS/VFS/VFSEXT2.h>
#include <FS/EXT2/EXT2Inode.h>
#include <Memory/PMM.h>

STATIC FILE  *Disk;
STATIC UINT64 Now = 1800000000ULL;
STATIC INT32  FailAlloc = -1;
STATIC UINT32 Checks;
STATIC UINT32 DiskWrites;

UINT64 ClockGetUnixTime(VOID)
{
    return Now;
}

VOID *KMemCpy(VOID *Destination, CONST VOID *Source, UINTN Size)
{
    return memcpy(Destination, Source, Size);
}

VOID *KMemSet(VOID *Destination, UINT8 Value, UINTN Size)
{
    return memset(Destination, Value, Size);
}

VOID *KAlloc(UINTN Size)
{
    if (FailAlloc == 0)
    {
        FailAlloc = -1;
        return NULL;
    }

    if (FailAlloc > 0)
    {
        FailAlloc--;
    }

    return malloc(Size);
}

VOID KFree(VOID *Address)
{
    free(Address);
}

VOID *KAllocPagesBelow(UINTN Count, UINT64 Limit)
{
    (VOID) Limit;
    return calloc(Count, 4096);
}

VOID KMemFreePages(VOID *Address, UINTN Count)
{
    (VOID) Count;
    free(Address);
}

UINT64 VirtIOBlockGetCapacity(VIRTIO_BLOCK_DEVICE *Device)
{
    return Device->Capacity;
}

BOOLEAN VirtIOBlockReadSync(VIRTIO_BLOCK_DEVICE *Device, UINT64 Sector, UINT32 Count, VOID *Buffer)
{
    if (Sector >= Device->Capacity || Count > Device->Capacity - Sector)
    {
        abort();
    }

    return fseeko(Disk, Sector * 512, SEEK_SET) == 0 && fread(Buffer, 512, Count, Disk) == Count;
}

BOOLEAN VirtIOBlockWriteSync(VIRTIO_BLOCK_DEVICE *Device, UINT64 Sector, UINT32 Count, CONST VOID *Buffer)
{
    if (Sector >= Device->Capacity || Count > Device->Capacity - Sector)
    {
        abort();
    }

    DiskWrites++;
    return fseeko(Disk, Sector * 512, SEEK_SET) == 0 && fwrite(Buffer, 512, Count, Disk) == Count;
}

BOOLEAN VirtIOBlockFlushSync(VIRTIO_BLOCK_DEVICE *Device)
{
    (VOID) Device;
    return fflush(Disk) == 0 && fsync(fileno(Disk)) == 0;
}

STATIC VOID Check(BOOLEAN Result, CONST char *Name)
{
    Checks++;

    if (!Result)
    {
        fprintf(stderr, "FAIL: %s\n", Name);
        exit(1);
    }
}

#define PATH(Value)    ((CONST CHAR8 *) (Value))
#define OK(Expression) Check((Expression) == VFS_OK, #Expression)

int main(int Count, char **Args)
{
    VIRTIO_BLOCK_DEVICE Device = {0};
    EXT2_VOLUME         Root = {0};
    EXT2_VOLUME         Other = {0};
    GPT_PARTITION       A = {0};
    GPT_PARTITION       B = {0};
    VFS_REGISTRY        Registry;
    VFS_CONTEXT         Admin;
    VFS_CONTEXT         User;
    VFS_COMMAND         Command;
    VFS_NODE            Node;
    VFS_NODE            Before;
    VFS_DIRECTORY_ENTRY Entry;
    UINT32              RootID;
    UINT32              OtherID;
    UINT32              EFIIID;
    UINT32              Read;
    UINT32              Number;
    UINT64              Cursor;
    EXT2_INODE          Inode;
    CHAR8               Cwd[4096];
    UINT32              Length = 5 * 1024 * 1024 + 173;
    UINT8              *Data = malloc(Length);
    UINT8              *Output = malloc(Length);

    if (Count != 3)
    {
        return 2;
    }

    Disk = fopen(Args[1], "r+b");
    Check(Disk != NULL && Data != NULL && Output != NULL, "host resources");
    A.SectorCount = strtoull(Args[2], NULL, 10);
    B.FirstLBA = A.SectorCount;
    B.SectorCount = A.SectorCount;
    Device.Capacity = A.SectorCount * 2;
    Device.FlushSupported = TRUE;
    Check(EXT2Mount(&Root, &Device, &A, TRUE), "mount root");
    Check(EXT2Mount(&Other, &Device, &B, TRUE), "mount other");
    VFSInit(&Registry);
    OK(VFSRegisterPartition(&Registry, PATH("Root"), 2, &Root, &VFSEXT2Operations, &RootID));
    OK(VFSRegisterPartition(&Registry, PATH("Other"), 3, &Other, &VFSEXT2Operations, &OtherID));
    OK(VFSRegisterPartition(&Registry, PATH("EFI"), 1, NULL, NULL, &EFIIID));
    Check(VFSRegisterPartition(&Registry, PATH("Again"), 2, &Root, &VFSEXT2Operations, &Number) == VFS_EXISTS, "duplicate backend rejected");
    OK(VFSContextInit(&Admin, &Registry, RootID, (VFS_CREDENTIALS){0, 0, TRUE}));
    OK(VFSContextInit(&User, &Registry, RootID, (VFS_CREDENTIALS){1000, 1000, FALSE}));
    OK(VFSGetCwd(&Admin, Cwd, sizeof(Cwd)));
    Check(strcmp((char *) Cwd, "/") == 0, "admin prompt does not gain caret");
    OK(VFSCreate(&Admin, PATH("/A"), TRUE, 0755));
    OK(VFSCreate(&Admin, PATH("/A/B"), TRUE, 0755));
    OK(VFSSymlink(&Admin, PATH("/A/B"), PATH("/S")));
    OK(VFSChangeDirectory(&Admin, PATH("/S/..")));
    OK(VFSGetCwd(&Admin, Cwd, sizeof(Cwd)));
    Check(strcmp((char *) Cwd, "/A") == 0, "dotdot after symlink uses resolved parent");
    OK(VFSChangeDirectory(&Admin, PATH("../../../../")));
    OK(VFSGetCwd(&Admin, Cwd, sizeof(Cwd)));
    Check(strcmp((char *) Cwd, "/") == 0, "dotdot cannot escape proot");
    OK(VFSChangeDirectory(&Admin, PATH("^")));
    OK(VFSGetCwd(&Admin, Cwd, sizeof(Cwd)));
    Check(strcmp((char *) Cwd, "^") == 0, "caret only at aroot");
    Cursor = 0;
    Number = 0;

    do
    {
        OK(VFSReadDirectory(&Admin, PATH("."), &Cursor, &Entry));
        Number += !Entry.End;
    } while (!Entry.End);

    Check(Number == 3, "admin partition listing including reserved EFI");
    Check(VFSChangeDirectory(&Admin, PATH("EFI")) == VFS_UNSUPPORTED, "EFI backend not faked");
    OK(VFSChangeDirectory(&Admin, PATH("Root/A/B")));
    OK(VFSGetCwd(&Admin, Cwd, sizeof(Cwd)));
    Check(strcmp((char *) Cwd, "/A/B") == 0, "partition name omitted inside proot");
    Check(VFSLookup(&User, PATH("^"), &Node) == VFS_NOT_FOUND, "user cannot stat aroot");
    Check(VFSLookup(&User, PATH("^/Root/A"), &Node) == VFS_NOT_FOUND, "user cannot traverse aroot");
    Cursor = 0;
    Check(VFSReadDirectory(&User, PATH("^"), &Cursor, &Entry) == VFS_NOT_FOUND && Cursor == 0 && Entry.Name[0] == 0, "user cannot enumerate aroot");
    Check(VFSSelectPartition(&User, PATH("Other")) == VFS_NOT_FOUND, "user cannot select partitions");
    Check(VFSLookup(&User, PATH("/A/^/.."), &Node) == VFS_NOT_FOUND, "embedded caret rejected");
    OK(VFSSymlink(&Admin, PATH("^/Other"), PATH("/Escape")));
    Check(VFSLookup(&User, PATH("/Escape"), &Node) == VFS_NOT_FOUND, "symlink cannot expose aroot");
    OK(VFSSymlink(&Admin, PATH("Loop"), PATH("/Loop")));
    Check(VFSLookup(&Admin, PATH("/Loop"), &Node) == VFS_LOOP, "symlink loop bounded");
    OK(VFSCreate(&Admin, PATH("/Home"), TRUE, 0777));
    OK(VFSCreate(&User, PATH("/Home/Data"), FALSE, 0640));
    OK(VFSLookup(&User, PATH("/Home/Data"), &Before));
    Check(Before.UID == 1000 && Before.AccessTime == Now && Before.ChangeTime == Now && Before.ModifyTime == Now, "create timestamps and ownership");

    for (UINT32 Index = 0; Index < Length; Index++)
    {
        Data[Index] = (UINT8) (Index * 13U + 7);
    }

    Now += 10;
    OK(VFSWrite(&User, PATH("/Home/Data"), 0, Data, Length));
    OK(VFSLookup(&User, PATH("/Home/Data"), &Node));
    Check(Node.Size == Length && Node.ChangeTime == Now && Node.ModifyTime == Now && Node.AccessTime == Before.AccessTime, "write larger than old 1024-block limit");
    Before = Node;
    Now += 10;
    OK(VFSRead(&User, PATH("/Home/Data"), 0, Output, Length, &Read));
    Check(Read == Length && memcmp(Data, Output, Length) == 0, "large write readback");
    OK(VFSLookup(&User, PATH("/Home/Data"), &Node));
    Check(Node.AccessTime == Now && Node.ChangeTime == Before.ChangeTime && Node.ModifyTime == Before.ModifyTime, "relatime changes atime only");
    Before = Node;
    Now += 10;
    OK(VFSSetMode(&User, PATH("/Home/Data"), 0600));
    OK(VFSLookup(&User, PATH("/Home/Data"), &Node));
    Check(Node.ChangeTime == Now && Node.ModifyTime == Before.ModifyTime, "chmod changes ctime only");
    OK(VFSLink(&User, PATH("/Home/Data"), PATH("/Home/Hard")));
    OK(VFSRename(&User, PATH("/Home/Hard"), PATH("/Home/Moved")));
    OK(VFSRemove(&User, PATH("/Home/Moved")));
    OK(VFSTruncate(&User, PATH("/Home/Data"), 123));
    OK(VFSRead(&User, PATH("/Home/Data"), 0, Output, Length, &Read));
    Check(Read == 123 && memcmp(Data, Output, 123) == 0, "truncate and short read");
    Check(VFSBeginCommand(&User, TRUE, FALSE, &Command) == VFS_PERMISSION, "unapproved elevation denied");
    OK(VFSBeginCommand(&User, TRUE, TRUE, &Command));
    OK(VFSChangeDirectory(&Command.Context, PATH("^/Other")));
    OK(VFSCreate(&Command.Context, PATH("/Else"), TRUE, 0755));
    OK(VFSChangeDirectory(&Command.Context, PATH("/Else")));
    OK(VFSEndCommand(&Command, TRUE));
    Check(User.Partition == OtherID && User.Credentials.UID == 1000 && !User.Credentials.Administrator && !Command.Active, "sudo cd preserves location without elevated credentials");
    OK(VFSGetCwd(&User, Cwd, sizeof(Cwd)));
    Check(strcmp((char *) Cwd, "/Else") == 0, "sudo cd display remains proot path");
    Check(VFSLookup(&User, PATH("/Home/Data"), &Node) == VFS_NOT_FOUND, "slash uses selected partition");
    Check(VFSLookup(&Command.Context, PATH("/"), &Node) == VFS_INVALID, "finished command context invalidated");
    OK(VFSBeginCommand(&User, TRUE, TRUE, &Command));
    Check(VFSRename(&Command.Context, PATH("^/Root/Home/Data"), PATH("^/Other/Else/Data")) == VFS_CROSS_PARTITION, "cross partition rename rejected");
    OK(VFSChangeDirectory(&Command.Context, PATH("^")));
    OK(VFSEndCommand(&Command, TRUE));
    Check(User.AdministratorRoot && !User.Credentials.Administrator, "aroot location persists without privilege");
    OK(VFSGetCwd(&User, Cwd, sizeof(Cwd)));
    Check(strcmp((char *) Cwd, "^") == 0, "aroot location display");
    Check(VFSLookup(&User, PATH("."), &Node) == VFS_NOT_FOUND, "aroot location does not grant access");
    OK(VFSBeginCommand(&User, TRUE, TRUE, &Command));
    OK(VFSChangeDirectory(&Command.Context, PATH("^/Root/Home")));
    OK(VFSEndCommand(&Command, TRUE));
    OK(VFSChangeDirectory(&Admin, PATH("/")));
    Now = 0;
    Check(VFSCreate(&User, PATH("NoClock"), FALSE, 0600) == VFS_CLOCK_ERROR, "unavailable clock fails before mutation");
    Now = (UINT64) UINT32_MAX + 1;
    Check(VFSCreate(&User, PATH("BadClock"), FALSE, 0600) == VFS_CLOCK_ERROR, "EXT2 timestamp overflow rejected");
    Now = 1800000100;
    FailAlloc = 0;
    Check(VFSCreate(&User, PATH("NoMemory"), FALSE, 0600) == VFS_LIMIT, "path allocation failure");
    for (INT32 Failure = 0; Failure < 24; Failure++)
    {
        UINT32    WritesBefore = DiskWrites;
        UINT32    FreeBefore = EXT2Load32(Root.Superblock + 12);
        VFS_ERROR Error;

        FailAlloc = Failure;
        Error = VFSWrite(&User, PATH("Data"), Root.BlockSize * 1500, Data, 173);
        FailAlloc = -1;

        if (Error == VFS_OK)
        {
            OK(VFSTruncate(&User, PATH("Data"), 123));
        }
        else
        {
            Check(Error == VFS_LIMIT && DiskWrites == WritesBefore && EXT2Load32(Root.Superblock + 12) == FreeBefore && Root.Changes == NULL && Root.ChangeBuckets == NULL, "allocation failure rolls back staged hash transaction");
            OK(VFSLookup(&User, PATH("Data"), &Node));
            Check(Node.Size == 123, "failed extension leaves inode unchanged");
        }
    }

    Registry.Busy = 1;
    Check(VFSLookup(&User, PATH("."), &Node) == VFS_BUSY, "concurrent entry fails without spinning");
    Registry.Busy = 0;
    Cursor = 1;
    Check(VFSReadDirectory(&User, PATH("."), &Cursor, &Entry) == VFS_INVALID, "misaligned directory cursor rejected");
    Cursor = 0;
    OK(VFSReadDirectory(&User, PATH("."), &Cursor, &Entry));
    Check(!Entry.End, "real backend enumeration");
    Check(Root.UID == 0 && Root.GID == 0 && Other.UID == 0 && Other.GID == 0, "backend credentials restored");
    Check(EXT2Lookup(&Root, PATH("/Home/Data"), &Number, &Inode), "raw API remains compatible");
    Before.AccessTime = EXT2Load32(Inode.Raw + 8);
    Check(EXT2Unmount(&Root), "unmount root");
    Check(EXT2Mount(&Root, &Device, &A, FALSE), "readonly remount");
    Now += 86401;
    Check(EXT2ReadFile(&Root, Number, 0, Output, 123, &Read) && Read == 123, "readonly read without timestamp write");
    Check(EXT2Lookup(&Root, PATH("/Home/Data"), &Number, &Inode) && EXT2Load32(Inode.Raw + 8) == Before.AccessTime, "readonly atime unchanged");
    Check(EXT2Unmount(&Root) && EXT2Unmount(&Other), "final unmounts");
    fclose(Disk);
    free(Data);
    free(Output);
    printf("PASS %u assertions\n", Checks);
    return 0;
}
