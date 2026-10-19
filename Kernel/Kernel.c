// Kernel/Kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Render/TrueType/Print.h>
#include <Render/TrueType/TrueTypeEngine.h>
#include <Render/GPU/VirtIOGPU.h>
#include <Multicore/SMP.h>
#include <Input/VirtIOInput.h>
#include <Input/Input.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>
#include <Arch/AMD64/CPU.h>
#include <Debug/Panic.h>
#include <LineOS/BootInfo.h>
#include <Interrupt/APIC.h>
#include <Interrupt/IDT.h>
#include <Memory/PMM.h>
#include <Timer/HPET.h>
#include <Timer/TSC.h>
#include <Timer/Clock.h>
#include <PCI/PCI.h>
#include <Debug/Debug.h>
#include <Memory/VMM.h>
#include <Disk/VirtIOBlock.h>
#include <FS/EXT2/EXT2.h>
#include <FS/VFS/VFSEXT2.h>

#define SHELL_MAX_COLUMNS 126
#define SHELL_MAX_ROWS    64

typedef struct
{
    VFS_REGISTRY Registry;
    VFS_CONTEXT  Context;
    EXT2_VOLUME  Volume;
    UINT32       Partition;
    UINT32       EFI;
    UINT32       X;
    UINT32       Y;
    UINT32       Width;
    UINT32       Height;
    UINT32       Columns;
    UINT32       Rows;
    UINT32       Row;
    UINT32       Column;
    CHAR8        Text[SHELL_MAX_ROWS][SHELL_MAX_COLUMNS + 1];
    CHAR8        Line[512];
    UINT32       Length;
    CHAR8        History[16][512];
    UINT32       HistoryCount;
    UINT32       Recall;
    UINT8        Buffer[8192];
} SHELL_STATE;

STATIC VOLATILE UINT32 FileSystemTestState;
STATIC UINT64          KernelBootUnix;
STATIC VOLATILE UINT32 KernelRenderBusy;

STATIC BOOLEAN KernelRenderEnter(VOID)
{
    UINT32 Expected = 0;
    return __atomic_compare_exchange_n(&KernelRenderBusy, &Expected, 1, FALSE, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

STATIC VOID KernelRenderLeave(VOID)
{
    __atomic_store_n(&KernelRenderBusy, 0, __ATOMIC_RELEASE);
}

STATIC BOOLEAN ShellIs(CONST CHAR8 *A, CONST char *B)
{
    while (*A != 0 && *A == (CHAR8) *B)
    {
        A++;
        B++;
    }

    return *A == (CHAR8) *B;
}

STATIC VOID ShellWrite(SHELL_STATE *Shell, CONST CHAR8 *Text)
{
    DebugWrite((CONST char *) Text);

    while (*Text != 0)
    {
        CHAR8 Ch = *Text++;

        if (Ch == '\r')
        {
            continue;
        }

        if (Ch == '\n' || Shell->Column >= Shell->Columns)
        {
            Shell->Column = 0;
            Shell->Row++;

            if (Shell->Row >= Shell->Rows)
            {
                for (UINT32 Index = 1; Index < Shell->Rows; Index++)
                {
                    KMemCpy(Shell->Text[Index - 1], Shell->Text[Index], SHELL_MAX_COLUMNS + 1);
                }

                Shell->Row = Shell->Rows - 1;
            }

            KMemSet(Shell->Text[Shell->Row], 0, SHELL_MAX_COLUMNS + 1);

            if (Ch == '\n')
            {
                continue;
            }
        }

        Shell->Text[Shell->Row][Shell->Column++] = Ch >= 32 && Ch < 127 ? Ch : '.';
        Shell->Text[Shell->Row][Shell->Column] = 0;
    }
}

STATIC VOID ShellNumber(SHELL_STATE *Shell, UINT64 Value, UINT32 Base)
{
    CHAR8        Text[65];
    CHAR8        Output[65];
    UINT32       Length = 0;
    CONST CHAR8 *Digits = (CONST CHAR8 *) "0123456789abcdef";

    do
    {
        Text[Length++] = Digits[Value % Base];
        Value /= Base;
    } while (Value != 0);

    for (UINT32 Index = 0; Index < Length; Index++)
    {
        Output[Index] = Text[Length - Index - 1];
    }

    Output[Length] = 0;
    ShellWrite(Shell, Output);
}

STATIC BOOLEAN ShellUnsigned(CONST CHAR8 *Text, UINT32 Base, UINT64 *Value)
{
    *Value = 0;

    if (*Text == 0)
    {
        return FALSE;
    }

    while (*Text != 0)
    {
        UINT32 Digit = *Text++ - '0';

        if (Digit >= Base || *Value > (UINT64_MAX - Digit) / Base)
        {
            return FALSE;
        }

        *Value = *Value * Base + Digit;
    }

    return TRUE;
}

STATIC INT32 ShellArguments(CHAR8 *Line, CHAR8 **Arguments)
{
    UINT32 Read = 0;
    UINT32 Write = 0;
    INT32  Count = 0;

    while (Line[Read] != 0)
    {
        CHAR8 Quote = 0;

        while (Line[Read] == ' ' || Line[Read] == '\t')
        {
            Read++;
        }

        if (Line[Read] == 0)
        {
            break;
        }

        if (Count == 12)
        {
            return -1;
        }

        Arguments[Count++] = Line + Write;

        while (Line[Read] != 0)
        {
            CHAR8 Ch = Line[Read];

            if (Quote == 0 && (Ch == ' ' || Ch == '\t'))
            {
                break;
            }

            Read++;

            if ((Ch == '\'' || Ch == '"') && (Quote == 0 || Quote == Ch))
            {
                Quote = Quote == 0 ? Ch : 0;
                continue;
            }

            if (Ch == '\\' && Quote != '\'')
            {
                if (Line[Read] == 0)
                {
                    return -1;
                }

                Ch = Line[Read++];
            }

            Line[Write++] = Ch;
        }

        if (Quote != 0)
        {
            return -1;
        }

        while (Line[Read] == ' ' || Line[Read] == '\t')
        {
            Read++;
        }

        Line[Write++] = 0;
    }

    return Count;
}

STATIC VFS_ERROR ShellExecute(SHELL_STATE *Shell, CHAR8 *Line)
{
    CHAR8       *Arguments[12];
    INT32        Count;
    VFS_COMMAND  Command;
    VFS_CONTEXT *Context;
    VFS_ERROR    Error = VFS_OK;
    VFS_NODE     Node;
    VFS_NODE     Other;
    UINT64       Number;
    UINT64       Second;
    BOOLEAN      KeepLocation = FALSE;
    BOOLEAN      Elevated;

    DebugWrite("[SHELL] ");
    DebugWriteLine((CONST char *) Line);
    Count = ShellArguments(Line, Arguments);

    if (Count < 0)
    {
        return VFS_INVALID;
    }

    if (Count == 0)
    {
        return VFS_OK;
    }

    Elevated = ShellIs(Arguments[0], "sudo");

    if (Elevated)
    {
        for (INT32 Index = 1; Index < Count; Index++)
        {
            Arguments[Index - 1] = Arguments[Index];
        }

        if (--Count == 0)
        {
            return VFS_INVALID;
        }
    }

    Error = VFSBeginCommand(&Shell->Context, Elevated, TRUE, &Command);

    if (Error != VFS_OK)
    {
        return Error;
    }

    Context = &Command.Context;

    if (ShellIs(Arguments[0], "help"))
    {
        ShellWrite(Shell, (CONST CHAR8 *) "help pwd cd ls stat cat hexdump\nmkdir touch write append cp mv rm rmdir\nln [-s] truncate chmod chown\necho time uptime whoami sync clear history\nsudo <command> (local debug authorization)\nwrite PATH TEXT; quote spaces with \"...\"\nchmod OCTAL PATH; chown UID GID PATH\ntruncate SIZE PATH; ln -s TARGET PATH\n/ = selected root; sudo ls ^ lists roots\nsudo cd ^/Root/PATH keeps cwd, drops privilege\n");
    }
    else if (ShellIs(Arguments[0], "pwd") && Count == 1)
    {
        Error = VFSGetCwd(Context, Shell->Buffer, sizeof(Shell->Buffer));

        if (Error == VFS_OK)
        {
            ShellWrite(Shell, Shell->Buffer);
            ShellWrite(Shell, (CONST CHAR8 *) "\n");
        }
    }
    else if (ShellIs(Arguments[0], "cd") && Count <= 2)
    {
        Error = VFSChangeDirectory(Context, Count == 2 ? Arguments[1] : (CONST CHAR8 *) "/");
        KeepLocation = Error == VFS_OK;
    }
    else if (ShellIs(Arguments[0], "ls") && Count <= 2)
    {
        VFS_DIRECTORY_ENTRY Entry;
        UINT64              Cursor = 0;

        do
        {
            Error = VFSReadDirectory(Context, Count == 2 ? Arguments[1] : (CONST CHAR8 *) ".", &Cursor, &Entry);

            if (Error == VFS_OK && !Entry.End)
            {
                ShellWrite(Shell, Entry.Name);
                ShellWrite(Shell, (CONST CHAR8 *) ((Entry.Node.Mode & VFS_MODE_TYPE) == VFS_MODE_DIRECTORY ? "/\n" : "\n"));
            }
        } while (Error == VFS_OK && !Entry.End);
    }
    else if (ShellIs(Arguments[0], "stat") && Count == 2)
    {
        Error = VFSLookup(Context, Arguments[1], &Node);

        if (Error == VFS_OK)
        {
            ShellWrite(Shell, (CONST CHAR8 *) "inode=");
            ShellNumber(Shell, Node.Number, 10);
            ShellWrite(Shell, (CONST CHAR8 *) " bytes=");
            ShellNumber(Shell, Node.Size, 10);
            ShellWrite(Shell, (CONST CHAR8 *) " mode=");
            ShellNumber(Shell, Node.Mode, 8);
            ShellWrite(Shell, (CONST CHAR8 *) "\nuid=");
            ShellNumber(Shell, Node.UID, 10);
            ShellWrite(Shell, (CONST CHAR8 *) " gid=");
            ShellNumber(Shell, Node.GID, 10);
            ShellWrite(Shell, (CONST CHAR8 *) "\natime=");
            ShellNumber(Shell, Node.AccessTime, 10);
            ShellWrite(Shell, (CONST CHAR8 *) " ctime=");
            ShellNumber(Shell, Node.ChangeTime, 10);
            ShellWrite(Shell, (CONST CHAR8 *) " mtime=");
            ShellNumber(Shell, Node.ModifyTime, 10);
            ShellWrite(Shell, (CONST CHAR8 *) "\n");
        }
    }
    else if ((ShellIs(Arguments[0], "cat") || ShellIs(Arguments[0], "hexdump")) && Count == 2)
    {
        UINT64  Offset = 0;
        UINT32  Read;
        BOOLEAN Hex = ShellIs(Arguments[0], "hexdump");
        UINT64  Limit = Hex ? 256 : 65536;

        do
        {
            UINT32 Want = Hex ? 16 : 1024;
            Error = VFSRead(Context, Arguments[1], Offset, Shell->Buffer, Want, &Read);

            if (Error != VFS_OK || Read == 0)
            {
                break;
            }

            if (Hex)
            {
                for (UINT32 Index = 0; Index < Read; Index++)
                {
                    CHAR8 Byte[4] = {"0123456789abcdef"[Shell->Buffer[Index] >> 4], "0123456789abcdef"[Shell->Buffer[Index] & 15], ' ', 0};
                    ShellWrite(Shell, Byte);
                }

                ShellWrite(Shell, (CONST CHAR8 *) "\n");
            }
            else
            {
                for (UINT32 Index = 0; Index < Read; Index++)
                {
                    if (Shell->Buffer[Index] == 0)
                    {
                        Shell->Buffer[Index] = '.';
                    }
                }

                Shell->Buffer[Read] = 0;
                ShellWrite(Shell, Shell->Buffer);
            }

            Offset += Read;
        } while (Offset < Limit);

        ShellWrite(Shell, (CONST CHAR8 *) (Offset >= Limit ? "\n[display limit reached]\n" : "\n"));
    }
    else if (ShellIs(Arguments[0], "mkdir") && Count == 2)
    {
        Error = VFSCreate(Context, Arguments[1], TRUE, 0755);
    }
    else if (ShellIs(Arguments[0], "touch") && Count == 2)
    {
        Error = VFSLookup(Context, Arguments[1], &Node);
        Error = Error == VFS_NOT_FOUND ? VFSCreate(Context, Arguments[1], FALSE, 0644) : Error == VFS_OK ? VFSTouch(Context, Arguments[1]) : Error;
    }
    else if ((ShellIs(Arguments[0], "write") || ShellIs(Arguments[0], "append")) && Count >= 3)
    {
        UINT64 Offset = 0;
        Error = VFSLookup(Context, Arguments[1], &Node);

        if (Error == VFS_NOT_FOUND)
        {
            Error = VFSCreate(Context, Arguments[1], FALSE, 0644);
        }
        else if (Error == VFS_OK)
        {
            if (ShellIs(Arguments[0], "append"))
            {
                Offset = Node.Size;
            }
            else
            {
                Error = VFSTruncate(Context, Arguments[1], 0);
            }
        }

        for (INT32 Index = 2; Error == VFS_OK && Index < Count; Index++)
        {
            UINT32 Length = 0;

            while (Arguments[Index][Length] != 0)
            {
                Length++;
            }

            if (Index != 2)
            {
                Error = VFSWrite(Context, Arguments[1], Offset++, " ", 1);
            }

            if (Error == VFS_OK)
            {
                Error = VFSWrite(Context, Arguments[1], Offset, Arguments[Index], Length);
            }

            Offset += Length;
        }
    }
    else if (ShellIs(Arguments[0], "cp") && Count == 3)
    {
        Error = VFSLookup(Context, Arguments[1], &Node);

        if (Error == VFS_OK)
        {
            UINT32    ProbeRead;
            VFS_ERROR Destination;

            if ((Node.Mode & VFS_MODE_TYPE) != 0x8000)
            {
                Error = VFS_INVALID;
            }
            else
            {
                Error = VFSRead(Context, Arguments[1], 0, Shell->Buffer, 0, &ProbeRead);
            }

            Destination = Error == VFS_OK ? VFSLookup(Context, Arguments[2], &Other) : Error;

            if (Destination == VFS_OK && Node.Number == Other.Number && Node.Partition == Other.Partition)
            {
                Error = VFS_INVALID;
            }
            else if (Destination == VFS_NOT_FOUND)
            {
                Error = VFSCreate(Context, Arguments[2], FALSE, 0644);
            }
            else if (Destination == VFS_OK)
            {
                Error = VFSTruncate(Context, Arguments[2], 0);
            }
            else
            {
                Error = Destination;
            }
        }

        Number = 0;

        while (Error == VFS_OK)
        {
            UINT32 Read;
            Error = VFSRead(Context, Arguments[1], Number, Shell->Buffer, sizeof(Shell->Buffer), &Read);

            if (Error != VFS_OK || Read == 0)
            {
                break;
            }

            Error = VFSWrite(Context, Arguments[2], Number, Shell->Buffer, Read);
            Number += Read;
        }
    }
    else if (ShellIs(Arguments[0], "mv") && Count == 3)
    {
        Error = VFSRename(Context, Arguments[1], Arguments[2]);
    }
    else if ((ShellIs(Arguments[0], "rm") || ShellIs(Arguments[0], "rmdir")) && Count == 2)
    {
        Error = VFSLookupNoFollow(Context, Arguments[1], &Node);

        if (Error == VFS_OK)
        {
            BOOLEAN Directory = (Node.Mode & VFS_MODE_TYPE) == VFS_MODE_DIRECTORY;
            Error = Directory != ShellIs(Arguments[0], "rmdir") ? VFS_INVALID : VFSRemove(Context, Arguments[1]);
        }
    }
    else if (ShellIs(Arguments[0], "ln") && Count == 3)
    {
        Error = VFSLink(Context, Arguments[1], Arguments[2]);
    }
    else if (ShellIs(Arguments[0], "ln") && Count == 4 && ShellIs(Arguments[1], "-s"))
    {
        Error = VFSSymlink(Context, Arguments[2], Arguments[3]);
    }
    else if (ShellIs(Arguments[0], "truncate") && Count == 3 && ShellUnsigned(Arguments[1], 10, &Number))
    {
        Error = VFSTruncate(Context, Arguments[2], Number);
    }
    else if (ShellIs(Arguments[0], "chmod") && Count == 3 && ShellUnsigned(Arguments[1], 8, &Number) && Number <= 07777)
    {
        Error = VFSSetMode(Context, Arguments[2], (UINT16) Number);
    }
    else if (ShellIs(Arguments[0], "chown") && Count == 4 && ShellUnsigned(Arguments[1], 10, &Number) && ShellUnsigned(Arguments[2], 10, &Second) && Number <= UINT32_MAX && Second <= UINT32_MAX)
    {
        Error = VFSSetOwner(Context, Arguments[3], (UINT32) Number, (UINT32) Second);
    }
    else if (ShellIs(Arguments[0], "echo"))
    {
        for (INT32 Index = 1; Index < Count; Index++)
        {
            ShellWrite(Shell, Arguments[Index]);
            ShellWrite(Shell, (CONST CHAR8 *) (Index + 1 == Count ? "\n" : " "));
        }
    }
    else if (ShellIs(Arguments[0], "whoami") && Count == 1)
    {
        ShellWrite(Shell, (CONST CHAR8 *) "uid=");
        ShellNumber(Shell, Context->Credentials.UID, 10);
        ShellWrite(Shell, (CONST CHAR8 *) " gid=");
        ShellNumber(Shell, Context->Credentials.GID, 10);
        ShellWrite(Shell, (CONST CHAR8 *) (Context->Credentials.Administrator ? " administrator\n" : " user\n"));
    }
    else if (ShellIs(Arguments[0], "time") && Count == 1)
    {
        ShellWrite(Shell, (CONST CHAR8 *) "UTC Unix seconds: ");
        ShellNumber(Shell, ClockGetUnixTime(), 10);
        ShellWrite(Shell, (CONST CHAR8 *) "\n");
    }
    else if (ShellIs(Arguments[0], "uptime") && Count == 1)
    {
        UINT64 Current = ClockGetUnixTime();
        ShellWrite(Shell, (CONST CHAR8 *) "Boot elapsed seconds: ");
        ShellNumber(Shell, Current >= KernelBootUnix ? Current - KernelBootUnix : 0, 10);
        ShellWrite(Shell, (CONST CHAR8 *) "\n");
    }
    else if (ShellIs(Arguments[0], "sync") && Count == 1)
    {
        Error = VirtIOBlockFlushSync(Shell->Volume.Device) ? VFS_OK : VFS_IO;
    }
    else if (ShellIs(Arguments[0], "clear") && Count == 1)
    {
        KMemSet(Shell->Text, 0, sizeof(Shell->Text));
        Shell->Row = 0;
        Shell->Column = 0;
    }
    else if (ShellIs(Arguments[0], "history") && Count == 1)
    {
        for (UINT32 Index = 0; Index < Shell->HistoryCount; Index++)
        {
            ShellWrite(Shell, Shell->History[Index]);
            ShellWrite(Shell, (CONST CHAR8 *) "\n");
        }
    }
    else
    {
        Error = VFS_INVALID;
    }

    {
        VFS_ERROR EndError = VFSEndCommand(&Command, KeepLocation && Error == VFS_OK);

        if (Error == VFS_OK)
        {
            Error = EndError;
        }
    }

    return Error;
}

STATIC CHAR8 ShellKeyCharacter(KEYBOARD_STATUS Event)
{
    STATIC CONST CHAR8 Normal[58] = {0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '};
    STATIC CONST CHAR8 Shifted[58] = {0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, 0, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0, 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '};
    CHAR8              Ch;

    if (!Event.Pressed || Event.KeyCode >= 58 || (Event.Modifiers & 4) != 0)
    {
        return 0;
    }

    Ch = Normal[Event.KeyCode];

    if (Ch >= 'a' && Ch <= 'z')
    {
        return ((Event.Modifiers & 1) != 0) != ((Event.Modifiers & 2) != 0) ? Shifted[Event.KeyCode] : Ch;
    }

    return (Event.Modifiers & 1) != 0 ? Shifted[Event.KeyCode] : Ch;
}

STATIC BOOLEAN ShellRender(SHELL_STATE *Shell)
{
    CHAR16 Text[512];
    CHAR8  Cwd[4096];
    UINT32 Length;

    if (!KernelRenderEnter())
    {
        return FALSE;
    }

    FillRect(Shell->X, Shell->Y, Shell->Width, Shell->Height, 0x080C12);
    KPrint(L"LineOS debug shell", Shell->X + 12, Shell->Y + 38, 0x80C0FF, 32, PRETENDARD);

    for (UINT32 Row = 0; Row < Shell->Rows; Row++)
    {
        for (Length = 0; Shell->Text[Row][Length] != 0 && Length < SHELL_MAX_COLUMNS; Length++)
        {
            Text[Length] = Shell->Text[Row][Length];
        }

        Text[Length] = 0;
        KPrint(L"%s", Shell->X + 12, Shell->Y + 82 + Row * 38, 0xE0E8F0, 32, JETBRAINS_MONO, Text);
    }

    if (VFSGetCwd(&Shell->Context, Cwd, sizeof(Cwd)) == VFS_OK)
    {
        Length = 0;

        while (Cwd[Length] != 0 && Length < Shell->Columns / 2)
        {
            Text[Length] = Cwd[Length];
            Length++;
        }

        Text[Length++] = '>';
        Text[Length++] = ' ';

        UINT32 Start = Shell->Length > Shell->Columns - Length ? Shell->Length - (Shell->Columns - Length) : 0;

        for (UINT32 Index = Start; Index < Shell->Length && Length < Shell->Columns; Index++)
        {
            Text[Length++] = Shell->Line[Index];
        }

        Text[Length] = 0;
        KPrint(L"%s", Shell->X + 12, Shell->Y + 82 + Shell->Rows * 38, 0x80FFB0, 32, JETBRAINS_MONO, Text);
    }

    VirtIOGPUTransferRect(Shell->X, Shell->Y, Shell->Width, Shell->Height);
    VirtIOGPUFlushRect(Shell->X, Shell->Y, Shell->Width, Shell->Height);
    KernelRenderLeave();
    return TRUE;
}

STATIC VOID KernelShellTask(VOID)
{
    SHELL_STATE    *Shell;
    KEYBOARD_STATUS Event;
    VFS_ERROR       Error;
    VFS_NODE        Node;
    VFS_CONTEXT     Admin;
    BOOLEAN         NeedsRender = TRUE;

    while (__atomic_load_n(&FileSystemTestState, __ATOMIC_ACQUIRE) == 0)
    {
        TaskSleepMls(100);
    }

    if (FileSystemTestState != 1)
    {
        DebugWriteLine("[SHELL] disabled: filesystem test failed");

        for (;;)
        {
            TaskSleepMls(1000);
        }
    }

    Shell = KAlloc(sizeof(*Shell));

    if (Shell == NULL)
    {
        Panic(L"Shell allocation failed");
    }

    KMemSet(Shell, 0, sizeof(*Shell));
    if (VirtIOGPUGetFrameBufferWidth() < 640 || VirtIOGPUGetFrameBufferHeight() < 900)
    {
        Panic(L"Display too small for shell");
    }

    Shell->X = VirtIOGPUGetFrameBufferWidth() >= 2200 ? 1440 : 20;
    Shell->Y = Shell->X == 20 ? 630 : 180;
    Shell->Width = VirtIOGPUGetFrameBufferWidth() - Shell->X - 20;
    Shell->Height = VirtIOGPUGetFrameBufferHeight() - Shell->Y - 20;
    Shell->Columns = (Shell->Width - 24) / 20;
    Shell->Rows = (Shell->Height - 132) / 38;

    if (Shell->Columns > SHELL_MAX_COLUMNS)
    {
        Shell->Columns = SHELL_MAX_COLUMNS;
    }

    if (Shell->Rows > SHELL_MAX_ROWS)
    {
        Shell->Rows = SHELL_MAX_ROWS;
    }

    if (Shell->Columns < 10 || Shell->Rows < 2)
    {
        Panic(L"Display too small for shell");
    }

    VFSInit(&Shell->Registry);
    Error = VFSMountEXT2(&Shell->Registry, (CONST CHAR8 *) "Root", &Shell->Volume, VirtIOBlockGetDevice(), 2, TRUE, &Shell->Partition);

    if (Error != VFS_OK || VFSRegisterPartition(&Shell->Registry, (CONST CHAR8 *) "EFI", 1, NULL, NULL, &Shell->EFI) != VFS_OK || VFSContextInit(&Admin, &Shell->Registry, Shell->Partition, (VFS_CREDENTIALS){0, 0, TRUE}) != VFS_OK)
    {
        Panic(L"Shell filesystem initialization failed");
    }

    Error = VFSLookup(&Admin, (CONST CHAR8 *) "/Home", &Node);

    if (Error == VFS_NOT_FOUND)
    {
        Error = VFSCreate(&Admin, (CONST CHAR8 *) "/Home", TRUE, 0755);
    }

    if (Error != VFS_OK)
    {
        Panic(L"Shell home directory unavailable");
    }

    Error = VFSLookup(&Admin, (CONST CHAR8 *) "/Home/User", &Node);

    if (Error == VFS_NOT_FOUND)
    {
        Error = VFSCreate(&Admin, (CONST CHAR8 *) "/Home/User", TRUE, 0700);

        if (Error == VFS_OK)
        {
            Error = VFSSetOwner(&Admin, (CONST CHAR8 *) "/Home/User", 1000, 1000);
        }
    }

    if (Error != VFS_OK || VFSContextInit(&Shell->Context, &Shell->Registry, Shell->Partition, (VFS_CREDENTIALS){1000, 1000, FALSE}) != VFS_OK)
    {
        Panic(L"Shell user context unavailable");
    }

    if (VFSChangeDirectory(&Shell->Context, (CONST CHAR8 *) "/Home/User") != VFS_OK)
    {
        ShellWrite(Shell, (CONST CHAR8 *) "Home inaccessible; starting at /\n");
    }

    while (InputGetKeyboardEvent(&Event))
    {
    }

    ShellWrite(Shell, (CONST CHAR8 *) "Local debug console; no login authentication.\nType help. Files use the real EXT2 disk.\n");
    DebugWriteLine("[SHELL] READY");
    NeedsRender = !ShellRender(Shell);

    for (;;)
    {
        BOOLEAN Changed = FALSE;

        while (InputGetKeyboardEvent(&Event))
        {
            CHAR8 Ch;

            if (!Event.Pressed)
            {
                continue;
            }

            if ((Event.Modifiers & 4) != 0 && (Event.KeyCode == 46 || Event.KeyCode == 38 || Event.KeyCode == 22))
            {
                if (Event.KeyCode == 38)
                {
                    KMemSet(Shell->Text, 0, sizeof(Shell->Text));
                    Shell->Row = 0;
                    Shell->Column = 0;
                }

                Shell->Length = 0;
                Shell->Line[0] = 0;
                Changed = TRUE;
                continue;
            }

            if (Event.KeyCode == 103 || Event.KeyCode == 108)
            {
                if (Event.KeyCode == 103 && Shell->Recall > 0)
                {
                    Shell->Recall--;
                }
                else if (Event.KeyCode == 108 && Shell->Recall < Shell->HistoryCount)
                {
                    Shell->Recall++;
                }

                KMemSet(Shell->Line, 0, sizeof(Shell->Line));

                if (Shell->Recall < Shell->HistoryCount)
                {
                    KMemCpy(Shell->Line, Shell->History[Shell->Recall], sizeof(Shell->Line));
                }

                Shell->Length = 0;

                while (Shell->Line[Shell->Length] != 0)
                {
                    Shell->Length++;
                }

                Changed = TRUE;
                continue;
            }

            if (Event.KeyCode == 14)
            {
                if (Shell->Length != 0)
                {
                    Shell->Line[--Shell->Length] = 0;
                    Changed = TRUE;
                }

                continue;
            }

            if (Event.KeyCode == 28)
            {
                if (Shell->Length != 0)
                {
                    if (Shell->HistoryCount == 16)
                    {
                        KMemMove(Shell->History, Shell->History + 1, 15 * sizeof(Shell->History[0]));
                        Shell->HistoryCount--;
                    }

                    KMemCpy(Shell->History[Shell->HistoryCount++], Shell->Line, sizeof(Shell->Line));
                    Shell->Recall = Shell->HistoryCount;
                    ShellWrite(Shell, (CONST CHAR8 *) "> ");
                    ShellWrite(Shell, Shell->Line);
                    ShellWrite(Shell, (CONST CHAR8 *) "\n");
                    Error = ShellExecute(Shell, Shell->Line);

                    if (Error != VFS_OK)
                    {
                        STATIC CONST CHAR8 *Errors[] = {(CONST CHAR8 *) "OK", (CONST CHAR8 *) "not found", (CONST CHAR8 *) "permission denied", (CONST CHAR8 *) "invalid command/arguments", (CONST CHAR8 *) "unsupported", (CONST CHAR8 *) "busy", (CONST CHAR8 *) "I/O error", (CONST CHAR8 *) "read only", (CONST CHAR8 *) "already exists", (CONST CHAR8 *) "no space", (CONST CHAR8 *) "limit/memory", (CONST CHAR8 *) "link loop", (CONST CHAR8 *) "directory not empty", (CONST CHAR8 *) "cross partition", (CONST CHAR8 *) "clock unavailable"};
                        ShellWrite(Shell, (CONST CHAR8 *) "error: ");
                        ShellWrite(Shell, Errors[Error < sizeof(Errors) / sizeof(Errors[0]) ? Error : VFS_IO]);
                        ShellWrite(Shell, (CONST CHAR8 *) "\n");
                    }

                    DebugWrite("[SHELL RESULT] ");
                    DebugWriteDec(Error);
                    DebugWriteLine("");
                }

                Shell->Length = 0;
                Shell->Line[0] = 0;
                Changed = TRUE;
                continue;
            }

            Ch = ShellKeyCharacter(Event);

            if (Ch != 0 && Shell->Length + 1 < sizeof(Shell->Line))
            {
                Shell->Line[Shell->Length++] = Ch;
                Shell->Line[Shell->Length] = 0;
                Changed = TRUE;
            }
        }

        if (Changed || NeedsRender)
        {
            NeedsRender = !ShellRender(Shell);
        }

        TaskSleepMls(8);
    }
}

STATIC VOID InputKeyboardTask(VOID)
{
    InputKeyboardHandler();
}

STATIC VOID InputMouseTask(VOID)
{
    InputMouseHandler();
}

#define INPUT_TEST_INTERVAL_MS 8

#define INPUT_PANEL_X      20
#define INPUT_PANEL_Y      190
#define INPUT_PANEL_WIDTH  1100
#define INPUT_PANEL_HEIGHT 430

STATIC VOID InputTestTask(VOID)
{
    KEYBOARD_STATUS Keyboard;
    KEYBOARD_STATUS LastKeyboard = {0};
    MOUSE_STATUS    Mouse;
    MOUSE_STATUS    LastMouse = {0};

    BOOLEAN FirstFrame = TRUE;
    BOOLEAN KeyboardChanged;
    BOOLEAN MouseChanged;
    BOOLEAN ButtonsChanged;

    while (TRUE)
    {
        Keyboard = GetKeyboardStatus();
        Mouse = GetMouseStatus();

        KeyboardChanged = FirstFrame || Keyboard.KeyCode != LastKeyboard.KeyCode || Keyboard.Pressed != LastKeyboard.Pressed;

        MouseChanged = FirstFrame || Mouse.X != LastMouse.X || Mouse.Y != LastMouse.Y;

        ButtonsChanged = FirstFrame || Mouse.LeftButton != LastMouse.LeftButton || Mouse.RightButton != LastMouse.RightButton || Mouse.MiddleButton != LastMouse.MiddleButton || Mouse.Wheel != LastMouse.Wheel || Mouse.BackButton != LastMouse.BackButton || Mouse.ForwardButton != LastMouse.ForwardButton;

        if ((KeyboardChanged || MouseChanged || ButtonsChanged) && KernelRenderEnter())
        {
            FillRect(INPUT_PANEL_X, INPUT_PANEL_Y, INPUT_PANEL_WIDTH, INPUT_PANEL_HEIGHT, 0x101010);

            KPrint(L"KEYBOARD", 40, 240, 0x80C0FF, 44, PRETENDARD);
            KPrint(L"Key Code: 0x%X", 40, 290, 0xFFFFFF, 36, JETBRAINS_MONO, Keyboard.KeyCode);
            KPrint(L"Pressed: %u", 40, 330, 0xFFFFFF, 36, JETBRAINS_MONO, Keyboard.Pressed ? 1 : 0);

            KPrint(L"MOUSE", 40, 410, 0x80C0FF, 44, PRETENDARD);
            KPrint(L"X: %u", 40, 460, 0xFFFFFF, 36, JETBRAINS_MONO, Mouse.X);
            KPrint(L"Y: %u", 40, 500, 0xFFFFFF, 36, JETBRAINS_MONO, Mouse.Y);

            KPrint(L"Left: %u", 40, 540, 0xFFFFFF, 34, JETBRAINS_MONO, Mouse.LeftButton ? 1 : 0);
            KPrint(L"Right: %u", 300, 540, 0xFFFFFF, 34, JETBRAINS_MONO, Mouse.RightButton ? 1 : 0);
            KPrint(L"Middle: %u", 600, 540, 0xFFFFFF, 34, JETBRAINS_MONO, Mouse.MiddleButton ? 1 : 0);
            KPrint(L"Wheel: %d", 40, 580, 0xFFFFFF, 34, JETBRAINS_MONO, Mouse.Wheel);
            KPrint(L"Back: %u", 300, 580, 0xFFFFFF, 34, JETBRAINS_MONO, Mouse.BackButton ? 1 : 0);
            KPrint(L"Forward: %u", 600, 580, 0xFFFFFF, 34, JETBRAINS_MONO, Mouse.ForwardButton ? 1 : 0);

            VirtIOGPUTransferRect(INPUT_PANEL_X, INPUT_PANEL_Y, INPUT_PANEL_WIDTH, INPUT_PANEL_HEIGHT);
            VirtIOGPUFlushRect(INPUT_PANEL_X, INPUT_PANEL_Y, INPUT_PANEL_WIDTH, INPUT_PANEL_HEIGHT);

            LastKeyboard = Keyboard;
            LastMouse = Mouse;
            FirstFrame = FALSE;
            KernelRenderLeave();
        }

        UpdateMouseCursor();

        TaskSleepMls(INPUT_TEST_INTERVAL_MS);
    }
}

STATIC VOID DiskTestFlush(VOID)
{
    VirtIOGPUTransferRect(20, 630, 1400, 420);
    VirtIOGPUFlushRect(20, 630, 1400, 420);
}

typedef struct PACKED
{
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 HeaderCRC32;
    UINT32 Reserved;
    UINT64 CurrentLBA;
    UINT64 BackupLBA;
    UINT64 FirstUsableLBA;
    UINT64 LastUsableLBA;
    UINT8  DiskGUID[16];
    UINT64 PartitionEntryLBA;
    UINT32 NumberOfPartitionEntries;
    UINT32 SizeOfPartitionEntry;
    UINT32 PartitionEntryArrayCRC32;
    UINT8  Reserved2[420];
} GPT_HEADER;

typedef struct PACKED
{
    UINT8  TypeGUID[16];
    UINT8  UniqueGUID[16];
    UINT64 FirstLBA;
    UINT64 LastLBA;
    UINT64 Attributes;
    CHAR16 Name[36];
} GPT_PARTITION_ENTRY;

#define GPT_SIGNATURE       0x5452415020494645ULL
#define EXT2_MAGIC          0xEF53
#define DISK_TEST_DMA_LIMIT 0x100000000ULL
#define DISK_TEST_TIMEOUT   10000000U

STATIC VOID *DiskTestBuffer;

STATIC BOOLEAN DiskTestReadSector(UINT64 Sector)
{
    VIRTIO_BLOCK_DEVICE  *Device;
    VIRTIO_BLOCK_REQUEST *Request;

    Device = VirtIOBlockGetDevice();

    if (Device == NULL || DiskTestBuffer == NULL)
    {
        return FALSE;
    }

    Request = NULL;

    if (!VirtIOBlockReadAsync(Device, Sector, 1, DiskTestBuffer, &Request))
    {
        KPrint(L"Read submit failed: LBA %u", 40, 760, 0xFF6060, 32, JETBRAINS_MONO, (UINT32) Sector);
        return FALSE;
    }

    if (!VirtIOBlockWait(Device, Request, DISK_TEST_TIMEOUT))
    {
        KPrint(L"Read failed: LBA %u", 40, 790, 0xFF6060, 32, JETBRAINS_MONO, (UINT32) Sector);

        if (Request->State == VIRTIO_BLOCK_REQUEST_FAILED || Request->State == VIRTIO_BLOCK_REQUEST_COMPLETED)
        {
            VirtIOBlockReleaseRequest(Device, Request);
        }

        return FALSE;
    }

    VirtIOBlockReleaseRequest(Device, Request);
    return TRUE;
}

STATIC VOID VirtIOBlockTestTask(VOID)
{
    VIRTIO_BLOCK_DEVICE *Device;
    GPT_HEADER          *GPTHeader;
    GPT_PARTITION_ENTRY *PartitionEntry;
    UINT64               Capacity;
    UINT64               PartitionEntrySector;
    UINT64               PartitionStartLBA;
    UINT16               EXT2Magic;
    EXT2_VOLUME          Volume = {0};
    EXT2_INODE           Inode;
    UINT8               *WriteBuffer = NULL;
    UINT8               *ReadBuffer = NULL;
    CHAR8                TestPath[] = "/LineOSTest0000";
    CHAR8                FilePath[] = "/LineOSTest0000/Payload.bin";
    CHAR8                LinkPath[] = "/LineOSTest0000/Link";
    UINT32               DirectoryNumber;
    UINT32               FileNumber;
    UINT32               ScratchNumber;
    UINT32               SymlinkNumber;
    UINT32               FoundNumber;
    UINT32               ReadLength;
    UINT32               DataLength;
    UINT32               TestIndex;
    UINT64               SparseOffset;
    CONST CHAR16        *Stage = L"Storage read";
    BOOLEAN              TestPassed = FALSE;
    VFS_REGISTRY        *Registry = NULL;
    VFS_CONTEXT         *VFSUser = NULL;
    VFS_CONTEXT         *VFSAdmin = NULL;
    VFS_COMMAND         *Command = NULL;
    VFS_NODE             VFSNode;
    VFS_DIRECTORY_ENTRY  VFSEntry;
    UINT64               DirectoryCursor;
    UINT32               RootPartition;
    UINT32               EFIPartition;

    while (!KernelRenderEnter())
    {
        TaskSleepMls(1);
    }

    KPrint(L"VirtIO-BLK Test", 40, 680, 0x80C0FF, 40, PRETENDARD);

    Device = VirtIOBlockGetDevice();

    if (Device == NULL)
    {
        KPrint(L"Block device unavailable", 40, 720, 0xFF6060, 32, PRETENDARD);

        goto TestFinished;
    }

    Capacity = VirtIOBlockGetCapacity(Device);

    KPrint(L"Capacity: %u sectors", 40, 720, 0xFFFFFF, 32, JETBRAINS_MONO, (UINT32) Capacity);

    DiskTestBuffer = KAllocPagesBelow(1, DISK_TEST_DMA_LIMIT);

    if (DiskTestBuffer == NULL)
    {
        KPrint(L"DMA buffer allocation failed", 40, 760, 0xFF6060, 32, PRETENDARD);

        goto TestFinished;
    }

    KMemSet(DiskTestBuffer, 0, 4096);

    if (!DiskTestReadSector(1))
    {
        goto TestFinished;
    }

    GPTHeader = (GPT_HEADER *) DiskTestBuffer;

    if (GPTHeader->Signature != GPT_SIGNATURE || GPTHeader->HeaderSize < 92 || GPTHeader->HeaderSize > 512)
    {
        KPrint(L"Invalid GPT header", 40, 760, 0xFF6060, 32, PRETENDARD);
        goto TestFinished;
    }

    KPrint(L"GPT header: OK", 40, 760, 0x80FF80, 32, PRETENDARD);

    if (GPTHeader->NumberOfPartitionEntries < 2 || GPTHeader->SizeOfPartitionEntry != sizeof(GPT_PARTITION_ENTRY))
    {
        KPrint(L"Unsupported GPT entry layout", 40, 790, 0xFF6060, 32, PRETENDARD);
        goto TestFinished;
    }

    PartitionEntrySector = GPTHeader->PartitionEntryLBA + sizeof(GPT_PARTITION_ENTRY) / 512;

    if (PartitionEntrySector >= Capacity || !DiskTestReadSector(PartitionEntrySector))
    {
        KPrint(L"GPT entries read failed", 40, 790, 0xFF6060, 32, PRETENDARD);
        goto TestFinished;
    }

    PartitionEntry = (GPT_PARTITION_ENTRY *) ((UINT8 *) DiskTestBuffer + sizeof(GPT_PARTITION_ENTRY));

    if (PartitionEntry->FirstLBA == 0 || PartitionEntry->FirstLBA >= Capacity || PartitionEntry->LastLBA < PartitionEntry->FirstLBA || PartitionEntry->LastLBA >= Capacity)
    {
        KPrint(L"Invalid root partition", 40, 820, 0xFF6060, 32, PRETENDARD);
        goto TestFinished;
    }

    PartitionStartLBA = PartitionEntry->FirstLBA;

    KPrint(L"Root start LBA: %u", 40, 790, 0xFFFFFF, 32, JETBRAINS_MONO, (UINT32) PartitionStartLBA);

    if (PartitionStartLBA > Capacity - 3 || !DiskTestReadSector(PartitionStartLBA + 2))
    {
        KPrint(L"EXT2 superblock read failed", 40, 820, 0xFF6060, 32, PRETENDARD);
        goto TestFinished;
    }

    EXT2Magic = *(UINT16 *) ((UINT8 *) DiskTestBuffer + 56);

    if (EXT2Magic != EXT2_MAGIC)
    {
        KPrint(L"EXT2 magic invalid: 0x%X", 40, 850, 0xFF6060, 32, JETBRAINS_MONO, (UINT32) EXT2Magic);
        goto TestFinished;
    }

    KPrint(L"EXT2 superblock: OK", 40, 820, 0x80FF80, 32, PRETENDARD);
    KPrint(L"Storage read test passed!", 40, 860, 0x80FF80, 40, PRETENDARD);
    DiskTestFlush();

    FillRect(20, 630, 1400, 420, 0x101010);
    KPrint(L"EXT2 filesystem test", 40, 680, 0x80C0FF, 40, PRETENDARD);
    DebugWriteLine("[EXT2 TEST] begin");
    DiskTestFlush();

    Stage = L"Mount GPT p2";

    if (!EXT2MountP2(&Volume, Device, TRUE))
    {
        goto TestFinished;
    }

    KPrint(L"GPT p2 mounted: %u-byte blocks", 40, 720, 0x80FF80, 32, JETBRAINS_MONO, Volume.BlockSize);
    DebugWriteLine("[EXT2 TEST] writable mount passed");
    DiskTestFlush();

    Stage = L"Root directory lookup";

    if (!EXT2Lookup(&Volume, (CONST CHAR8 *) "/", &FoundNumber, &Inode) || FoundNumber != EXT2_ROOT_INODE || (EXT2Load16(Inode.Raw) & EXT2_MODE_TYPE) != EXT2_MODE_DIRECTORY)
    {
        goto TestFinished;
    }

    Stage = L"Find unused test directory";

    for (TestIndex = 0; TestIndex < 10000; TestIndex++)
    {
        TestPath[11] = (CHAR8) ('0' + TestIndex / 1000);
        TestPath[12] = (CHAR8) ('0' + TestIndex / 100 % 10);
        TestPath[13] = (CHAR8) ('0' + TestIndex / 10 % 10);
        TestPath[14] = (CHAR8) ('0' + TestIndex % 10);

        if (!EXT2Lookup(&Volume, TestPath, &FoundNumber, &Inode))
        {
            if (Volume.Error != EXT2_ERROR_NOT_FOUND)
            {
                goto TestFinished;
            }

            break;
        }
    }

    if (TestIndex == 10000)
    {
        goto TestFinished;
    }

    KMemCpy(FilePath, TestPath, sizeof(TestPath) - 1);

    Stage = L"UTC clock validation";

    if (ClockGetUnixTime() == 0 || ClockGetUnixTime() > UINT32_MAX)
    {
        goto TestFinished;
    }

    Stage = L"Create directory and file";

    if (!EXT2Create(&Volume, EXT2_ROOT_INODE, TestPath + 1, TRUE, 0755, (UINT32) ClockGetUnixTime(), &DirectoryNumber) || !EXT2Create(&Volume, DirectoryNumber, (CONST CHAR8 *) "Payload.bin", FALSE, 0644, (UINT32) ClockGetUnixTime(), &FileNumber))
    {
        goto TestFinished;
    }

    if (!EXT2Lookup(&Volume, FilePath, &FoundNumber, &Inode) || FoundNumber != FileNumber)
    {
        goto TestFinished;
    }

    KPrint(L"Root lookup, directory and file creation: OK", 40, 755, 0x80FF80, 32, JETBRAINS_MONO);
    DebugWrite("[EXT2 TEST] created ");
    DebugWriteLine((CONST char *) FilePath);
    DiskTestFlush();

    Stage = L"Allocate test buffers";
    DataLength = Volume.BlockSize * 14 + 173;
    WriteBuffer = KAlloc(DataLength);
    ReadBuffer = KAlloc(DataLength);

    if (WriteBuffer == NULL || ReadBuffer == NULL)
    {
        goto TestFinished;
    }

    for (UINT32 Index = 0; Index < DataLength; Index++)
    {
        WriteBuffer[Index] = (UINT8) (Index * 17U + Index / 511);
    }

    Stage = L"Direct and single-indirect write/read";

    if (!EXT2WriteFile(&Volume, FileNumber, 0, WriteBuffer, DataLength, (UINT32) ClockGetUnixTime()) || !EXT2ReadFile(&Volume, FileNumber, 0, ReadBuffer, DataLength, &ReadLength) || ReadLength != DataLength)
    {
        goto TestFinished;
    }

    for (UINT32 Index = 0; Index < DataLength; Index++)
    {
        if (ReadBuffer[Index] != WriteBuffer[Index])
        {
            goto TestFinished;
        }
    }

    KPrint(L"Direct + single-indirect: %u bytes verified", 40, 790, 0x80FF80, 32, JETBRAINS_MONO, DataLength);
    DebugWriteLine("[EXT2 TEST] direct and single-indirect data verified");
    DiskTestFlush();

    Stage = L"Double-indirect sparse write/read";
    SparseOffset = (12ULL + Volume.BlockSize / 4) * Volume.BlockSize;

    if (!EXT2WriteFile(&Volume, FileNumber, SparseOffset + 37, WriteBuffer, 173, (UINT32) ClockGetUnixTime()) || !EXT2ReadFile(&Volume, FileNumber, SparseOffset - Volume.BlockSize, ReadBuffer, Volume.BlockSize + 210, &ReadLength) || ReadLength != Volume.BlockSize + 210)
    {
        goto TestFinished;
    }

    for (UINT32 Index = 0; Index < Volume.BlockSize + 37; Index++)
    {
        if (ReadBuffer[Index] != 0)
        {
            goto TestFinished;
        }
    }

    for (UINT32 Index = 0; Index < 173; Index++)
    {
        if (ReadBuffer[Volume.BlockSize + 37 + Index] != WriteBuffer[Index])
        {
            goto TestFinished;
        }
    }

    KPrint(L"Double-indirect data + zero-filled hole: OK", 40, 825, 0x80FF80, 32, JETBRAINS_MONO);
    DebugWriteLine("[EXT2 TEST] double-indirect sparse data verified");
    DiskTestFlush();

    Stage = L"EOF and overflow rejection";

    if (!EXT2ReadFile(&Volume, FileNumber, SparseOffset + 200, ReadBuffer, 173, &ReadLength) || ReadLength != 10)
    {
        goto TestFinished;
    }

    for (UINT32 Index = 0; Index < ReadLength; Index++)
    {
        if (ReadBuffer[Index] != WriteBuffer[163 + Index])
        {
            goto TestFinished;
        }
    }

    if (!EXT2ReadFile(&Volume, FileNumber, SparseOffset + 210, ReadBuffer, 1, &ReadLength) || ReadLength != 0 || EXT2WriteFile(&Volume, FileNumber, UINT64_MAX, WriteBuffer, 1, (UINT32) ClockGetUnixTime()) || Volume.Error != EXT2_ERROR_LIMIT)
    {
        goto TestFinished;
    }

    Stage = L"Duplicate name rejection";

    if (EXT2Create(&Volume, DirectoryNumber, (CONST CHAR8 *) "Payload.bin", FALSE, 0644, (UINT32) ClockGetUnixTime(), &FoundNumber) || Volume.Error != EXT2_ERROR_EXISTS)
    {
        goto TestFinished;
    }

    KPrint(L"EOF, overflow and duplicate rejection: OK", 40, 860, 0x80FF80, 32, JETBRAINS_MONO);
    DebugWriteLine("[EXT2 TEST] EOF and invalid operations verified");
    DiskTestFlush();

    Stage = L"Truncate, links, rename and deletion";

    if (!EXT2Create(&Volume, DirectoryNumber, (CONST CHAR8 *) "Scratch.bin", FALSE, 0600, (UINT32) ClockGetUnixTime(), &ScratchNumber) || !EXT2WriteFile(&Volume, ScratchNumber, 0, WriteBuffer, DataLength, (UINT32) ClockGetUnixTime()) || !EXT2Truncate(&Volume, ScratchNumber, Volume.BlockSize + 17, (UINT32) ClockGetUnixTime()) || !EXT2Truncate(&Volume, ScratchNumber, DataLength, (UINT32) ClockGetUnixTime()) || !EXT2ReadFile(&Volume, ScratchNumber, 0, ReadBuffer, DataLength, &ReadLength) || ReadLength != DataLength)
    {
        goto TestFinished;
    }

    for (UINT32 Index = 0; Index < DataLength; Index++)
    {
        if (ReadBuffer[Index] != (Index < Volume.BlockSize + 17 ? WriteBuffer[Index] : 0))
        {
            goto TestFinished;
        }
    }

    if (!EXT2Link(&Volume, DirectoryNumber, (CONST CHAR8 *) "Alias.bin", ScratchNumber, (UINT32) ClockGetUnixTime()) || !EXT2Remove(&Volume, DirectoryNumber, (CONST CHAR8 *) "Scratch.bin", (UINT32) ClockGetUnixTime()) || !EXT2Rename(&Volume, DirectoryNumber, (CONST CHAR8 *) "Alias.bin", DirectoryNumber, (CONST CHAR8 *) "Moved.bin", (UINT32) ClockGetUnixTime()) || !EXT2Symlink(&Volume, DirectoryNumber, (CONST CHAR8 *) "Link", (CONST CHAR8 *) "Moved.bin", (UINT32) ClockGetUnixTime(), &SymlinkNumber))
    {
        goto TestFinished;
    }

    KMemCpy(LinkPath, TestPath, sizeof(TestPath) - 1);

    if (!EXT2ReadLink(&Volume, SymlinkNumber, (CHAR8 *) ReadBuffer, DataLength, &ReadLength) || ReadLength != 9 || !EXT2Lookup(&Volume, LinkPath, &FoundNumber, &Inode) || FoundNumber != ScratchNumber)
    {
        goto TestFinished;
    }

    Stage = L"Owner and permission checks";

    if (!EXT2SetOwner(&Volume, ScratchNumber, 1000, 1000, (UINT32) ClockGetUnixTime()) || !EXT2SetMode(&Volume, ScratchNumber, 0600, (UINT32) ClockGetUnixTime()) || !EXT2SetCredentials(&Volume, 2000, 2000))
    {
        goto TestFinished;
    }

    if (EXT2WriteFile(&Volume, ScratchNumber, 0, WriteBuffer, 1, (UINT32) ClockGetUnixTime()) || Volume.Error != EXT2_ERROR_PERMISSION)
    {
        goto TestFinished;
    }

    if (!EXT2SetCredentials(&Volume, 0, 0) || !EXT2Remove(&Volume, DirectoryNumber, (CONST CHAR8 *) "Link", (UINT32) ClockGetUnixTime()) || !EXT2Remove(&Volume, DirectoryNumber, (CONST CHAR8 *) "Moved.bin", (UINT32) ClockGetUnixTime()))
    {
        goto TestFinished;
    }

    if (EXT2Lookup(&Volume, LinkPath, &FoundNumber, &Inode) || Volume.Error != EXT2_ERROR_NOT_FOUND)
    {
        goto TestFinished;
    }

    KPrint(L"Truncate, links, rename, deletion, permissions: OK", 40, 895, 0x80FF80, 32, JETBRAINS_MONO);
    DebugWriteLine("[EXT2 TEST] truncate, links, rename, deletion and permissions passed");
    DiskTestFlush();

    Stage = L"VFS context and administrator root";
    Registry = KAlloc(sizeof(*Registry));
    VFSUser = KAlloc(sizeof(*VFSUser));
    VFSAdmin = KAlloc(sizeof(*VFSAdmin));
    Command = KAlloc(sizeof(*Command));

    if (Registry == NULL || VFSUser == NULL || VFSAdmin == NULL || Command == NULL)
    {
        goto TestFinished;
    }

    VFSInit(Registry);

    if (VFSRegisterPartition(Registry, (CONST CHAR8 *) "Root", 2, &Volume, &VFSEXT2Operations, &RootPartition) != VFS_OK || VFSRegisterPartition(Registry, (CONST CHAR8 *) "EFI", 1, NULL, NULL, &EFIPartition) != VFS_OK || VFSContextInit(VFSAdmin, Registry, RootPartition, (VFS_CREDENTIALS){0, 0, TRUE}) != VFS_OK || VFSContextInit(VFSUser, Registry, RootPartition, (VFS_CREDENTIALS){1000, 1000, FALSE}) != VFS_OK)
    {
        goto TestFinished;
    }

    if (VFSGetCwd(VFSAdmin, (CHAR8 *) ReadBuffer, DataLength) != VFS_OK || ReadBuffer[0] != '/' || ReadBuffer[1] != 0 || VFSLookup(VFSUser, (CONST CHAR8 *) "^", &VFSNode) != VFS_NOT_FOUND)
    {
        goto TestFinished;
    }

    DirectoryCursor = 0;

    if (VFSReadDirectory(VFSUser, (CONST CHAR8 *) "^", &DirectoryCursor, &VFSEntry) != VFS_NOT_FOUND || DirectoryCursor != 0 || VFSEntry.Name[0] != 0 || VFSBeginCommand(VFSUser, TRUE, FALSE, Command) != VFS_PERMISSION)
    {
        goto TestFinished;
    }

    if (VFSBeginCommand(VFSUser, TRUE, TRUE, Command) != VFS_OK || VFSChangeDirectory(&Command->Context, (CONST CHAR8 *) "^") != VFS_OK || VFSGetCwd(&Command->Context, (CHAR8 *) ReadBuffer, DataLength) != VFS_OK || ReadBuffer[0] != '^' || ReadBuffer[1] != 0)
    {
        goto TestFinished;
    }

    DirectoryCursor = 0;

    if (VFSReadDirectory(&Command->Context, (CONST CHAR8 *) ".", &DirectoryCursor, &VFSEntry) != VFS_OK || VFSEntry.End || VFSChangeDirectory(&Command->Context, (CONST CHAR8 *) "Root") != VFS_OK || VFSChangeDirectory(&Command->Context, TestPath) != VFS_OK || VFSEndCommand(Command, TRUE) != VFS_OK || VFSUser->Credentials.Administrator || VFSUser->Credentials.UID != 1000)
    {
        goto TestFinished;
    }

    if (VFSGetCwd(VFSUser, (CHAR8 *) ReadBuffer, DataLength) != VFS_OK)
    {
        goto TestFinished;
    }

    for (UINT32 Index = 0; Index < sizeof(TestPath); Index++)
    {
        if (ReadBuffer[Index] != TestPath[Index])
        {
            goto TestFinished;
        }
    }

    if (VFSRead(VFSUser, (CONST CHAR8 *) "Payload.bin", 0, ReadBuffer, DataLength, &ReadLength) != VFS_OK || ReadLength != DataLength)
    {
        goto TestFinished;
    }

    for (UINT32 Index = 0; Index < DataLength; Index++)
    {
        if (ReadBuffer[Index] != WriteBuffer[Index])
        {
            goto TestFinished;
        }
    }

    if (VFSLookup(VFSUser, (CONST CHAR8 *) "Payload.bin", &VFSNode) != VFS_OK || VFSNode.ChangeTime == 0 || VFSNode.ModifyTime == 0 || VFSNode.AccessTime == 0 || VFSSelectPartition(VFSAdmin, (CONST CHAR8 *) "EFI") != VFS_UNSUPPORTED)
    {
        goto TestFinished;
    }

    DebugWrite("[CLOCK EXT2] mtime=");
    DebugWriteDec(VFSNode.ModifyTime);
    DebugWrite(" now=");
    DebugWriteDec(ClockGetUnixTime());
    DebugWriteLine("");

    DebugWriteLine("[VFS TEST] PASS: PRoot, hidden ARoot, command credentials, persistent cwd, UTC timestamps");

    Stage = L"Unmount and remount read-only";

    if (!EXT2Unmount(&Volume) || !EXT2MountP2(&Volume, Device, FALSE) || !EXT2Lookup(&Volume, FilePath, &FoundNumber, &Inode) || FoundNumber != FileNumber || !EXT2ReadFile(&Volume, FileNumber, 0, ReadBuffer, DataLength, &ReadLength) || ReadLength != DataLength)
    {
        goto TestFinished;
    }

    for (UINT32 Index = 0; Index < DataLength; Index++)
    {
        if (ReadBuffer[Index] != WriteBuffer[Index])
        {
            goto TestFinished;
        }
    }

    if (EXT2WriteFile(&Volume, FileNumber, 0, WriteBuffer, 1, (UINT32) ClockGetUnixTime()) || Volume.Error != EXT2_ERROR_READ_ONLY)
    {
        goto TestFinished;
    }

    KPrint(L"Remount persistence + read-only rejection: OK", 40, 930, 0x80FF80, 32, JETBRAINS_MONO);
    DebugWriteLine("[EXT2 TEST] remount persistence and read-only checks passed");
    TestPassed = TRUE;

TestFinished:
    if (!TestPassed)
    {
        KPrint(L"FAIL: %s (EXT2 error %u)", 40, 970, 0xFF6060, 32, JETBRAINS_MONO, Stage, (UINT32) Volume.Error);
        DebugWrite("[EXT2 TEST] FAIL: ");
        DebugWriteWide(Stage);
        DebugWrite(" error=");
        DebugWriteDec(Volume.Error);
        DebugWriteLine("");
    }

    if (Volume.Mounted && !Volume.Poisoned)
    {
        if (!EXT2Unmount(&Volume))
        {
            TestPassed = FALSE;
            KPrint(L"FAIL: final unmount (error %u)", 40, 970, 0xFF6060, 32, JETBRAINS_MONO, (UINT32) Volume.Error);
            DebugWriteLine("[EXT2 TEST] FAIL: final unmount");
        }
    }

    if (Command != NULL)
    {
        KFree(Command);
    }

    if (VFSUser != NULL)
    {
        KFree(VFSUser);
    }

    if (VFSAdmin != NULL)
    {
        KFree(VFSAdmin);
    }

    if (Registry != NULL)
    {
        KFree(Registry);
    }

    if (WriteBuffer != NULL)
    {
        KFree(WriteBuffer);
    }

    if (ReadBuffer != NULL)
    {
        KFree(ReadBuffer);
    }

    if (DiskTestBuffer != NULL)
    {
        KMemFreePages(DiskTestBuffer, 1);
        DiskTestBuffer = NULL;
    }

    if (TestPassed)
    {
        KPrint(L"EXT2 TEST PASSED - retained /LineOSTest%c%c%c%c", 40, 970, 0x80FF80, 32, JETBRAINS_MONO, (UINT32) TestPath[11], (UINT32) TestPath[12], (UINT32) TestPath[13], (UINT32) TestPath[14]);
        DebugWriteLine("[EXT2 TEST] PASS");
    }

    DiskTestFlush();
    KernelRenderLeave();
    __atomic_store_n(&FileSystemTestState, TestPassed ? 1 : 2, __ATOMIC_RELEASE);

    for (;;)
    {
        TaskSleepMls(1000);
    }
}

VOID ClockTestTask(VOID)
{
    CLOCK_DATE_TIME DateTime;

    while (TRUE)
    {
        if (ClockGetDateTime(&DateTime, 540) && KernelRenderEnter())
        {
            FillRect(690, 65, 420, 55, 0x101010);

            KPrint(L"%04u-%02u-%02u %02u:%02u:%02u", 700, 100, 0xFFFFFFFF, 28, JETBRAINS_MONO, (UINT32) DateTime.Year, (UINT32) DateTime.Month, (UINT32) DateTime.Day, (UINT32) DateTime.Hour, (UINT32) DateTime.Minute, (UINT32) DateTime.Second);

            VirtIOGPUTransferRect(690, 65, 420, 55);
            VirtIOGPUFlushRect(690, 65, 420, 55);
            KernelRenderLeave();
        }

        TaskSleepMls(1000);
    }
}

VOID InitKernel(LINEOS_BOOT_INFO *BootInfo)
{
    VIRTIO_GPU_INFO *GPU;

    CLI();
    CPUEnableSSE();

    DebugWrite("[VMM] Firmware CR3=");
    DebugWriteHex(VMMGetCurrentCR3());
    DebugWriteLine("");

    if (!KMemoryInit(BootInfo))
    {
        DebugWriteLine("[VMM] PMM init FAILED");
        HLT();
    }

    DebugWriteLine("[VMM] PMM init OK; cloning page tables");

    if (!VMMInit())
    {
        DebugWriteLine("[VMM] Owned CR3 init FAILED");
        HLT();
    }

    DebugWrite("[VMM] Owned CR3=");
    DebugWriteHex(VMMGetCurrentCR3());
    DebugWriteLine("");

    KHeapInit();

    TrueTypeInit();
    PCIInit(BootInfo);

    VirtIOGPUInit();

    GPU = VirtIOGPUGetInfo();

    VirtIOGPUCreateFrameBuffer(GPU->DisplayInfo.Displays[0].Rect.Width, GPU->DisplayInfo.Displays[0].Rect.Height);

    GDTInitCurrentCPU();

    IDTInit();
    IDTLoad();

    LAPICInit();
    HPETInit(BootInfo);
    TSCCalibrate();
    TSCDeadlineInit(0x40);
    ClockInit();

    DebugWriteLine("[VMM] Starting SMP; TLB IPI=70 (0x46), ISR70");

    if (!SMPInit(BootInfo))
    {
        DebugWriteLine("[VMM] SMP init FAILED; mapping changes disabled");
    }

    KernelBootUnix = ClockGetUnixTime();

    LBPWRRInit();

    VirtIOBlockInit();
    VirtIOInputInit();
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    TASK   *KeyboardTask;
    TASK   *MouseTask;
    TASK   *TestTask;
    TASK   *DiskTask;
    TASK   *ClockTask;
    TASK   *ShellTask;
    UINT64  CR0;
    UINT64  CR4;
    UINT64  PhysAddr;
    UINT32  OnlineCount = 0;
    BOOLEAN TranslationValid;

    InitKernel(BootInfo);

    ASM("mov %%cr0, %0" : "=r"(CR0));
    ASM("mov %%cr4, %0" : "=r"(CR4));

    DebugWrite("[VMM] CR0=");
    DebugWriteHex(CR0);
    DebugWrite(" CR4=");
    DebugWriteHex(CR4);
    DebugWrite(" EFER=");
    DebugWriteHex(ReadMSR(0xC0000080));
    DebugWriteLine("");
    DebugWrite("[VMM] Current CR3=");
    DebugWriteHex(VMMGetCurrentCR3());
    DebugWrite(" Boot CR3=");
    DebugWriteHex(SMPGetBootCR3());
    DebugWrite(" TSC Hz=");
    DebugWriteDec(TSCGetFrequency());
    DebugWriteLine("");

    for (UINT32 Index = 0; Index < SMPGetCPUCount(); Index++)
    {
        CPU_INFO *CPU = SMPGetCPU(Index);

        if (CPU != NULL && __atomic_load_n(&CPU->Online, __ATOMIC_ACQUIRE))
        {
            OnlineCount++;
        }
    }

    DebugWrite("[VMM] SMP ready=");
    DebugWriteDec(SMPIsReady());
    DebugWrite(" Online CPUs=");
    DebugWriteDec(OnlineCount);
    DebugWrite("/");
    DebugWriteDec(SMPGetCPUCount());
    DebugWriteLine("");

    TranslationValid = VMMTranslate((UINT64) KMain, &PhysAddr);

    if (TranslationValid)
    {
        DebugWrite("[VMM] KMain VA=");
        DebugWriteHex((UINT64) KMain);
        DebugWrite(" PA=");
        DebugWriteHex(PhysAddr);
        DebugWriteLine("");
    }
    else
    {
        DebugWriteLine("[VMM] KMain translation FAILED");
    }

    FillScreen(0x101010);

    KPrint(L"LineOS VirtIO Input Test", 40, 80, 0xFFFFFF, 56, PRETENDARD);

    KPrint(L"Keyboard: %u", 40, 140, 0x80FF80, 36, JETBRAINS_MONO, VirtIOInputIsKeyboardAvailable() ? 1 : 0);
    KPrint(L"Tablet: %u", 40, 180, 0x80FF80, 36, JETBRAINS_MONO, VirtIOInputIsTabletAvailable() ? 1 : 0);

    KPrint(L"VMM / SMP", 1140, 240, 0x80C0FF, 32, PRETENDARD);
    KPrint(L"Ready %u  CPUs %u/%u", 1140, 275, SMPIsReady() ? 0x80FF80 : 0xFF6060, 20, JETBRAINS_MONO, (UINT32) SMPIsReady(), OnlineCount, SMPGetCPUCount());
    KPrint(L"IPI 70 / 0x46 / ISR70", 1140, 305, 0xFFFFFF, 20, JETBRAINS_MONO);
    KPrint(L"CR3  %016llX", 1140, 335, 0xFFFFFF, 20, JETBRAINS_MONO, VMMGetCurrentCR3());
    KPrint(L"BOOT %016llX", 1140, 365, 0xFFFFFF, 20, JETBRAINS_MONO, SMPGetBootCR3());
    KPrint(L"CR0  %016llX", 1140, 395, 0xFFFFFF, 20, JETBRAINS_MONO, CR0);
    KPrint(L"CR4  %016llX", 1140, 425, 0xFFFFFF, 20, JETBRAINS_MONO, CR4);
    KPrint(L"EFER %016llX", 1140, 455, 0xFFFFFF, 20, JETBRAINS_MONO, ReadMSR(0xC0000080));
    KPrint(L"TSC %llu Hz", 1140, 485, 0xFFFFFF, 20, JETBRAINS_MONO, TSCGetFrequency());
    KPrint(L"VA   %016llX", 1140, 515, 0xFFFFFF, 20, JETBRAINS_MONO, (UINT64) KMain);

    if (TranslationValid)
    {
        KPrint(L"PA   %016llX", 1140, 545, 0x80FF80, 20, JETBRAINS_MONO, PhysAddr);
    }
    else
    {
        KPrint(L"Translation FAILED", 1140, 545, 0xFF6060, 20, JETBRAINS_MONO);
    }

    VirtIOGPUFlush();

    KeyboardTask = TaskCreate(InputKeyboardTask);
    MouseTask = TaskCreate(InputMouseTask);
    TestTask = TaskCreate(InputTestTask);
    DiskTask = TaskCreate(VirtIOBlockTestTask);
    ClockTask = TaskCreate(ClockTestTask);
    ShellTask = TaskCreate(KernelShellTask);

    TaskSetPriority(ClockTask, 6);

    if (KeyboardTask == NULL || MouseTask == NULL || TestTask == NULL || DiskTask == NULL || ClockTask == NULL || ShellTask == NULL)
    {
        Panic(L"Task creation failed");
    }

    if (!LBPWRRAddTask(KeyboardTask))
    {
        Panic(L"Failed to add keyboard task");
    }

    if (!LBPWRRAddTask(MouseTask))
    {
        Panic(L"Failed to add mouse task");
    }

    if (!LBPWRRAddTask(TestTask))
    {
        Panic(L"Failed to add input test task");
    }

    if (!LBPWRRAddTask(DiskTask))
    {
        Panic(L"Failed to add disk test task");
    }

    if (!LBPWRRAddTask(ClockTask))
    {
        Panic(L"Failed to add clock task");
    }

    if (!LBPWRRAddTask(ShellTask))
    {
        Panic(L"Failed to add shell task");
    }


    LBPWRRStart();

    HLT();
}

VOID APMain(UINT32 CPUID)
{
    CPU_INFO *CPU;

    CLI();

    CPU = SMPGetCPU(CPUID);

    GDTInitCurrentCPU();
    CPUEnableSSE();
    IDTLoad();

    if (!LAPICInitCurrentCPU())
    {
        HLT();
    }

    if (!TSCDeadlineInit(0x40))
    {
        HLT();
    }

    if (CPU != NULL)
    {
        __atomic_store_n(&CPU->Online, TRUE, __ATOMIC_RELEASE);
    }

    CompilerBarrier();

    for (;;)
    {
        LBPWRRJoin();
        STIHLT();
    }
}
