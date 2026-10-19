// Tests/FS/ShellHost.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#define main FileSystemHostMain
#include "Host.c"
#undef main
#include "../../Kernel/Kernel.c"

VOID DebugWrite(CONST char *Text)
{
    (VOID) Text;
}

VOID DebugWriteLine(CONST char *Text)
{
    (VOID) Text;
}

STATIC VOID RunShell(SHELL_STATE *Shell, CONST char *Text, VFS_ERROR Expected)
{
    CHAR8 Line[512];
    Check(strlen(Text) < sizeof(Line), "shell test input bounds");
    strcpy((char *) Line, Text);
    Check(ShellExecute(Shell, Line) == Expected, Text);
}

int main(int Count, char **Args)
{
    SHELL_STATE        *Shell = calloc(1, sizeof(*Shell));
    VFS_CONTEXT         Admin;
    VIRTIO_BLOCK_DEVICE Device = {0};
    GPT_PARTITION       Partition = {0};
    VFS_NODE            Node;
    UINT32              Read;
    CHAR8               Bytes[256];

    if (Count != 2)
    {
        return 2;
    }

    Disk = fopen(Args[1], "r+b");
    Check(Disk != NULL && Shell != NULL, "shell resources");
    fseeko(Disk, 0, SEEK_END);
    Device.Capacity = ftello(Disk) / 512;
    Device.FlushSupported = TRUE;
    Partition.SectorCount = Device.Capacity;
    Check(EXT2Mount(&Shell->Volume, &Device, &Partition, TRUE), "shell mount");
    Shell->Columns = 80;
    Shell->Rows = 24;
    KernelBootUnix = Now;
    VFSInit(&Shell->Registry);
    OK(VFSRegisterPartition(&Shell->Registry, PATH("Root"), 2, &Shell->Volume, &VFSEXT2Operations, &Shell->Partition));
    OK(VFSRegisterPartition(&Shell->Registry, PATH("EFI"), 1, NULL, NULL, &Shell->EFI));
    OK(VFSContextInit(&Admin, &Shell->Registry, Shell->Partition, (VFS_CREDENTIALS){0, 0, TRUE}));
    OK(VFSCreate(&Admin, PATH("/Home"), TRUE, 0777));
    OK(VFSContextInit(&Shell->Context, &Shell->Registry, Shell->Partition, (VFS_CREDENTIALS){1000, 1000, FALSE}));
    OK(VFSChangeDirectory(&Shell->Context, PATH("/Home")));
    RunShell(Shell, "write hello 'hello world'", VFS_OK);
    RunShell(Shell, "append hello ' + EXT2'", VFS_OK);
    OK(VFSRead(&Shell->Context, PATH("hello"), 0, Bytes, sizeof(Bytes), &Read));
    Check(Read == 18 && memcmp(Bytes, "hello world + EXT2", 18) == 0, "shell append content");
    RunShell(Shell, "cp hello copy", VFS_OK);
    RunShell(Shell, "mv copy moved", VFS_OK);
    RunShell(Shell, "ln -s hello alias", VFS_OK);
    RunShell(Shell, "cat alias", VFS_OK);
    RunShell(Shell, "cp hello alias", VFS_INVALID);
    RunShell(Shell, "cp hello hello", VFS_INVALID);
    RunShell(Shell, "truncate 5 moved", VFS_OK);
    OK(VFSLookup(&Shell->Context, PATH("moved"), &Node));
    Check(Node.Size == 5, "shell truncate content size");
    RunShell(Shell, "stat moved", VFS_OK);
    RunShell(Shell, "hexdump hello", VFS_OK);
    RunShell(Shell, "chmod 000 hello", VFS_OK);
    RunShell(Shell, "cat hello", VFS_PERMISSION);
    RunShell(Shell, "cp hello moved", VFS_PERMISSION);
    OK(VFSRead(&Shell->Context, PATH("moved"), 0, Bytes, sizeof(Bytes), &Read));
    Check(Read == 5 && memcmp(Bytes, "hello", 5) == 0, "cp permission failure preserved destination");
    RunShell(Shell, "sudo cat hello", VFS_OK);
    RunShell(Shell, "chmod 600 hello", VFS_OK);
    Now++;
    RunShell(Shell, "touch hello", VFS_OK);
    OK(VFSLookup(&Shell->Context, PATH("hello"), &Node));
    Check(Node.AccessTime == Now && Node.ModifyTime == Now && Node.ChangeTime == Now, "touch timestamp meanings");
    RunShell(Shell, "mkdir demo", VFS_OK);
    RunShell(Shell, "rm demo", VFS_INVALID);
    RunShell(Shell, "rmdir hello", VFS_INVALID);
    RunShell(Shell, "rmdir demo", VFS_OK);
    RunShell(Shell, "rm alias", VFS_OK);
    RunShell(Shell, "rm moved", VFS_OK);
    RunShell(Shell, "ls ^", VFS_NOT_FOUND);
    RunShell(Shell, "sudo ls ^", VFS_OK);
    RunShell(Shell, "sudo cd ^", VFS_OK);
    Check(Shell->Context.AdministratorRoot && !Shell->Context.Credentials.Administrator, "sudo cd aroot preserves location without privilege");
    RunShell(Shell, "pwd", VFS_OK);
    RunShell(Shell, "ls", VFS_NOT_FOUND);
    RunShell(Shell, "sudo ls", VFS_OK);
    RunShell(Shell, "cd /", VFS_OK);
    RunShell(Shell, "sudo cd ^/Root/Home", VFS_OK);
    Check(!Shell->Context.Credentials.Administrator && Shell->Context.Credentials.UID == 1000, "sudo shell command drops privilege");
    RunShell(Shell, "sudo chown 2000 2000 hello", VFS_OK);
    RunShell(Shell, "cat hello", VFS_PERMISSION);
    RunShell(Shell, "sudo chown 1000 1000 hello", VFS_OK);
    RunShell(Shell, "write 'with space' 'quoted text'", VFS_OK);
    RunShell(Shell, "write broken 'unterminated", VFS_INVALID);
    RunShell(Shell, "echo a b c d e f g h i j k l", VFS_INVALID);
    RunShell(Shell, "echo test", VFS_OK);
    RunShell(Shell, "whoami", VFS_OK);
    RunShell(Shell, "sudo whoami", VFS_OK);
    RunShell(Shell, "pwd", VFS_OK);
    RunShell(Shell, "help", VFS_OK);
    RunShell(Shell, "time", VFS_OK);
    RunShell(Shell, "uptime", VFS_OK);
    RunShell(Shell, "history", VFS_OK);
    RunShell(Shell, "sync", VFS_OK);
    RunShell(Shell, "clear", VFS_OK);
    Check(ShellKeyCharacter((KEYBOARD_STATUS){30, TRUE, 0}) == 'a', "US keyboard letter");
    Check(ShellKeyCharacter((KEYBOARD_STATUS){30, TRUE, 1}) == 'A', "shift letter");
    Check(ShellKeyCharacter((KEYBOARD_STATUS){30, TRUE, 3}) == 'a', "caps plus shift");
    Check(ShellKeyCharacter((KEYBOARD_STATUS){7, TRUE, 1}) == '^', "caret key");
    Check(EXT2Unmount(&Shell->Volume), "shell unmount");
    fclose(Disk);
    free(Shell);
    printf("PASS %u shell assertions\n", Checks);
    return 0;
}
