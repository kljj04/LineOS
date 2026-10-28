// UserPrograms/Shell/Commands/ls/ls.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define LS_BUFFER_SIZE 4096
#define LS_PATH_SIZE   4096

#define LS_TYPE_DIRECTORY 4
#define LS_TYPE_SYMLINK   10

typedef struct
{
    UINT64 Inode;
    INT64  Offset;
    UINT16 RecordLength;
    UINT8  Type;
    CHAR8  Name[];
} LINUX_DIRENT64;

typedef struct
{
    BOOLEAN All;
    BOOLEAN AlmostAll;
    BOOLEAN Classify;
    BOOLEAN Long;
} LS_OPTIONS;

STATIC BOOLEAN LSIsDot(CONST CHAR8 *Name)
{
    return Name[0] == '.' && Name[1] == 0;
}

STATIC BOOLEAN LSIsDotDot(CONST CHAR8 *Name)
{
    return Name[0] == '.' && Name[1] == '.' && Name[2] == 0;
}

STATIC BOOLEAN LSVisible(CONST CHAR8 *Name, CONST LS_OPTIONS *Options)
{
    if (Options->All)
    {
        return TRUE;
    }

    if (Options->AlmostAll)
    {
        return !LSIsDot(Name) && !LSIsDotDot(Name);
    }

    return Name[0] != '.';
}

STATIC VOID LSModeString(UINT32 Mode, CHAR8 Result[11])
{
    Result[0] = '-';

    if (S_ISDIR(Mode))
    {
        Result[0] = 'd';
    }
    else if (S_ISLNK(Mode))
    {
        Result[0] = 'l';
    }
    else if (S_ISCHR(Mode))
    {
        Result[0] = 'c';
    }

    Result[1] = (Mode & 0400U) != 0 ? 'r' : '-';
    Result[2] = (Mode & 0200U) != 0 ? 'w' : '-';
    Result[3] = (Mode & 0100U) != 0 ? 'x' : '-';

    Result[4] = (Mode & 0040U) != 0 ? 'r' : '-';
    Result[5] = (Mode & 0020U) != 0 ? 'w' : '-';
    Result[6] = (Mode & 0010U) != 0 ? 'x' : '-';

    Result[7] = (Mode & 0004U) != 0 ? 'r' : '-';
    Result[8] = (Mode & 0002U) != 0 ? 'w' : '-';
    Result[9] = (Mode & 0001U) != 0 ? 'x' : '-';

    if ((Mode & 04000U) != 0)
    {
        Result[3] = Result[3] == 'x' ? 's' : 'S';
    }

    if ((Mode & 02000U) != 0)
    {
        Result[6] = Result[6] == 'x' ? 's' : 'S';
    }

    if ((Mode & 01000U) != 0)
    {
        Result[9] = Result[9] == 'x' ? 't' : 'T';
    }

    Result[10] = 0;
}

STATIC UINT8 LSTypeFromMode(UINT32 Mode)
{
    if (S_ISDIR(Mode))
    {
        return LS_TYPE_DIRECTORY;
    }

    if (S_ISLNK(Mode))
    {
        return LS_TYPE_SYMLINK;
    }

    return 0;
}

STATIC VOID LSPrintEntry(CONST CHAR8 *Name, UINT8 Type, CONST LS_OPTIONS *Options)
{
    CONST char *Color = "\033[0m";
    CONST char *Suffix = "";

    if (Type == LS_TYPE_DIRECTORY)
    {
        Color = "\033[38;2;110;170;255m";

        if (Options->Classify)
        {
            Suffix = "/";
        }
    }
    else if (Type == LS_TYPE_SYMLINK)
    {
        Color = "\033[38;2;80;210;210m";

        if (Options->Classify)
        {
            Suffix = "@";
        }
    }

    printf("%s%s%s\033[0m\n", Color, (CONST char *) Name, Suffix);
}

STATIC VOID LSPrintLongEntry(CONST CHAR8 *Name, CONST struct stat *Info, CONST LS_OPTIONS *Options)
{
    CHAR8 Mode[11];

    LSModeString((UINT32) Info->st_mode, Mode);

    printf(
        "%s %lu %u %u %ld %lu ",
        (CONST char *) Mode,
        Info->st_nlink,
        Info->st_uid,
        Info->st_gid,
        Info->st_size,
        Info->st_mtime
    );

    LSPrintEntry(Name, LSTypeFromMode((UINT32) Info->st_mode), Options);
}

STATIC BOOLEAN LSBuildPath(CONST CHAR8 *Directory, CONST CHAR8 *Name, CHAR8 *Result, UINTN Capacity)
{
    UINTN DirectoryLength = strlen((CONST char *) Directory);
    UINTN NameLength = strlen((CONST char *) Name);
    BOOLEAN Separator = DirectoryLength != 0 && Directory[DirectoryLength - 1] != '/';
    UINTN Required = DirectoryLength + (Separator ? 1 : 0) + NameLength + 1;

    if (Required > Capacity)
    {
        return FALSE;
    }

    memcpy(Result, Directory, DirectoryLength);

    UINTN Position = DirectoryLength;

    if (Separator)
    {
        Result[Position++] = '/';
    }

    memcpy(Result + Position, Name, NameLength);
    Result[Position + NameLength] = 0;

    return TRUE;
}

STATIC BOOLEAN LSValidEntry(CONST LINUX_DIRENT64 *Entry, UINTN Remaining)
{
    UINTN HeaderSize = offsetof(LINUX_DIRENT64, Name);

    if (Remaining < HeaderSize + 1)
    {
        return FALSE;
    }

    if (Entry->RecordLength < HeaderSize + 1 ||
        Entry->RecordLength > Remaining)
    {
        return FALSE;
    }

    UINTN NameCapacity = Entry->RecordLength - HeaderSize;

    for (UINTN Index = 0; Index < NameCapacity; Index++)
    {
        if (Entry->Name[Index] == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC INT32 LSListDirectory(CONST CHAR8 *Path, CONST LS_OPTIONS *Options)
{
    UINT8 Buffer[LS_BUFFER_SIZE];
    INT32 Status = 0;
    INT32 FD = open((CONST char *) Path, O_RDONLY);

    if (FD < 0)
    {
        printf("ls: cannot open '%s'\n", (CONST char *) Path);
        return 1;
    }

    for (;;)
    {
        INT64 Count = getdents64(FD, Buffer, sizeof(Buffer));

        if (Count == 0)
        {
            break;
        }

        if (Count < 0)
        {
            printf("ls: cannot read '%s'\n", (CONST char *) Path);
            Status = 1;
            break;
        }

        if ((UINT64) Count > sizeof(Buffer))
        {
            printf("ls: invalid directory data in '%s'\n", (CONST char *) Path);
            Status = 1;
            break;
        }

        UINTN Position = 0;

        while (Position < (UINTN) Count)
        {
            LINUX_DIRENT64 *Entry = (LINUX_DIRENT64 *) (Buffer + Position);

            if (!LSValidEntry(Entry, (UINTN) Count - Position))
            {
                printf("ls: invalid directory entry in '%s'\n", (CONST char *) Path);
                Status = 1;
                break;
            }

            if (Entry->Inode != 0 && LSVisible(Entry->Name, Options))
            {
                if (Options->Long)
                {
                    CHAR8 FullPath[LS_PATH_SIZE];
                    struct stat Info;

                    if (!LSBuildPath(Path, Entry->Name, FullPath, sizeof(FullPath)))
                    {
                        printf("ls: path too long: '%s'\n", (CONST char *) Entry->Name);
                        Status = 1;
                    }
                    else if (lstat((CONST char *) FullPath, &Info) < 0)
                    {
                        printf("ls: cannot access '%s'\n", (CONST char *) FullPath);
                        Status = 1;
                    }
                    else
                    {
                        LSPrintLongEntry(Entry->Name, &Info, Options);
                    }
                }
                else
                {
                    LSPrintEntry(Entry->Name, Entry->Type, Options);
                }
            }

            Position += Entry->RecordLength;
        }

        if (Status != 0)
        {
            break;
        }
    }

    if (close(FD) < 0)
    {
        printf("ls: cannot close '%s'\n", (CONST char *) Path);
        Status = 1;
    }

    return Status;
}

STATIC INT32 LSListPath(CONST CHAR8 *Path, CONST LS_OPTIONS *Options)
{
    struct stat Info;

    if (lstat((CONST char *) Path, &Info) < 0)
    {
        printf("ls: cannot access '%s'\n", (CONST char *) Path);
        return 1;
    }

    if (!S_ISDIR(Info.st_mode))
    {
        CONST CHAR8 *Name = (CONST CHAR8 *) strrchr((CONST char *) Path, '/');

        if (Name == NULL)
        {
            Name = Path;
        }
        else
        {
            Name++;
        }

        if (Options->Long)
        {
            LSPrintLongEntry(Name, &Info, Options);
        }
        else
        {
            LSPrintEntry(Name, LSTypeFromMode(Info.st_mode), Options);
        }

        return 0;
    }

    return LSListDirectory(Path, Options);
}

STATIC VOID LSUsage(VOID)
{
    puts("Usage: ls [-aAlF1] [PATH ...]");
    puts("  -a  Show all entries");
    puts("  -A  Show hidden entries except . and ..");
    puts("  -l  Use a long listing format");
    puts("  -1  Print one entry per line");
    puts("  -F  Append / to directories and @ to symlinks");
}

VOID _Start(INT32 Argc, CHAR8 **Argv)
{
    LS_OPTIONS Options = {0};
    INT32 FirstPath = 1;
    INT32 Status = 0;
    BOOLEAN EndOptions = FALSE;

    while (FirstPath < Argc)
    {
        CONST CHAR8 *Argument = Argv[FirstPath];

        if (EndOptions || Argument[0] != '-' || Argument[1] == 0)
        {
            break;
        }

        if (strcmp((CONST char *) Argument, "--") == 0)
        {
            EndOptions = TRUE;
            FirstPath++;
            break;
        }

        if (strcmp((CONST char *) Argument, "--help") == 0)
        {
            LSUsage();
            exit(0);
        }

        for (UINTN Index = 1; Argument[Index] != 0; Index++)
        {
            switch (Argument[Index])
            {
                case 'a':
                {
                    Options.All = TRUE;
                    Options.AlmostAll = FALSE;
                    break;
                }

                case 'A':
                {
                    Options.AlmostAll = TRUE;
                    Options.All = FALSE;
                    break;
                }

                case 'l':
                {
                    Options.Long = TRUE;
                    break;
                }

                case '1':
                {
                    break;
                }

                case 'F':
                {
                    Options.Classify = TRUE;
                    break;
                }

                default:
                {
                    printf("ls: invalid option -- '%c'\n", Argument[Index]);
                    LSUsage();
                    exit(2);
                }
            }
        }

        FirstPath++;
    }

    if (FirstPath == Argc)
    {
        exit(LSListPath((CONST CHAR8 *) ".", &Options));
    }

    UINT32 PathCount = (UINT32) (Argc - FirstPath);

    for (INT32 Index = FirstPath; Index < Argc; Index++)
    {
        CONST CHAR8 *Path = Argv[Index];

        if (PathCount > 1)
        {
            if (Index != FirstPath)
            {
                puts("");
            }

            printf("%s:\n", (CONST char *) Path);
        }

        if (LSListPath(Path, &Options) != 0)
        {
            Status = 1;
        }
    }

    exit(Status);
}