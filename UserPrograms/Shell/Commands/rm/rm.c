// UserPrograms/Shell/Commands/rm/rm.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLib/TypeInfo.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define RM_PATH_SIZE 4096

typedef struct
{
    BOOLEAN Force;
    BOOLEAN Interactive;
    BOOLEAN InteractiveOnce;
    BOOLEAN Recursive;
    BOOLEAN Directory;
    BOOLEAN Verbose;
    BOOLEAN PreserveRoot;
} RM_OPTIONS;

STATIC VOID RMUsage(VOID)
{
    puts("Usage: rm [OPTION]... FILE...");
    puts("");
    puts("Remove files or directories.");
    puts("");
    puts("  -f              Ignore nonexistent files and arguments");
    puts("  -i              Prompt before every removal");
    puts("  -I              Prompt once before removing multiple files");
    puts("  -r, -R          Remove directories and their contents recursively");
    puts("  -d              Remove empty directories");
    puts("  -v              Explain what is being done");
    puts("      --help      Display this help and exit");
    puts("      --version   Output version information and exit");
    puts("      --          End option processing");
}

STATIC VOID RMVersion(VOID)
{
    puts("rm (LineOS Coreutils) 1.0");
}

STATIC VOID RMError(CONST CHAR8 *Path, INT32 Error)
{
    fprintf(stderr, "rm: cannot remove '%s': %s\n", (CONST char *) Path, strerror(Error));
}

STATIC BOOLEAN RMIsDotPath(CONST CHAR8 *Path)
{
    UINTN Length = strlen((CONST char *) Path);

    while (Length > 1 && Path[Length - 1] == '/')
    {
        Length--;
    }

    UINTN Start = Length;

    while (Start > 0 && Path[Start - 1] != '/')
    {
        Start--;
    }

    UINTN ComponentLength = Length - Start;

    return (ComponentLength == 1 && Path[Start] == '.') ||
           (ComponentLength == 2 && Path[Start] == '.' && Path[Start + 1] == '.');
}

STATIC BOOLEAN RMIsRoot(CONST CHAR8 *Path)
{
    CHAR8 Resolved[RM_PATH_SIZE];

    if (realpath((CONST char *) Path, (char *) Resolved) == NULL)
    {
        return FALSE;
    }

    return strcmp((CONST char *) Resolved, "/") == 0;
}

STATIC BOOLEAN RMConfirm(CONST CHAR8 *Path)
{
    CHAR8 Input[32];

    fprintf(stderr, "rm: remove '%s'? ", (CONST char *) Path);
    fflush(stderr);

    if (fgets((char *) Input, sizeof(Input), stdin) == NULL)
    {
        return FALSE;
    }

    return Input[0] == 'y' || Input[0] == 'Y';
}

STATIC BOOLEAN RMBuildPath(CONST CHAR8 *Directory, CONST CHAR8 *Name, CHAR8 *Result)
{
    UINTN DirectoryLength = strlen((CONST char *) Directory);
    UINTN NameLength = strlen((CONST char *) Name);
    BOOLEAN Separator = DirectoryLength != 0 && Directory[DirectoryLength - 1] != '/';
    UINTN Required = DirectoryLength + (Separator ? 1 : 0) + NameLength + 1;

    if (Required > RM_PATH_SIZE)
    {
        errno = ENAMETOOLONG;
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

STATIC INT32 RMRemove(CONST CHAR8 *Path, CONST RM_OPTIONS *Options)
{
    struct stat Info;

    if (lstat((CONST char *) Path, &Info) < 0)
    {
        if (Options->Force && errno == ENOENT)
        {
            return 0;
        }

        RMError(Path, errno);
        return 1;
    }

    if (Options->Interactive && !Options->Force && !RMConfirm(Path))
    {
        return 0;
    }

    if (!S_ISDIR(Info.st_mode))
    {
        if (unlink((CONST char *) Path) < 0)
        {
            if (Options->Force && errno == ENOENT)
            {
                return 0;
            }

            RMError(Path, errno);
            return 1;
        }

        if (Options->Verbose)
        {
            printf("removed '%s'\n", (CONST char *) Path);
        }

        return 0;
    }

    if (!Options->Recursive)
    {
        INT32 Result;

        if (Options->Directory)
        {
            Result = rmdir((CONST char *) Path);
        }
        else
        {
            errno = EISDIR;
            Result = -1;
        }

        if (Result < 0)
        {
            RMError(Path, errno);
            return 1;
        }

        if (Options->Verbose)
        {
            printf("removed directory '%s'\n", (CONST char *) Path);
        }

        return 0;
    }

    if (RMIsDotPath(Path))
    {
        fprintf(stderr, "rm: refusing to remove '.' or '..': %s\n", (CONST char *) Path);
        return 1;
    }

    if (Options->PreserveRoot && RMIsRoot(Path))
    {
        fprintf(stderr, "rm: refusing to remove root directory: '%s'\n", (CONST char *) Path);
        return 1;
    }

    DIR *Directory = opendir((CONST char *) Path);

    if (Directory == NULL)
    {
        RMError(Path, errno);
        return 1;
    }

    INT32 Status = 0;

    for (;;)
    {
        errno = 0;
        struct dirent *Entry = readdir(Directory);

        if (Entry == NULL)
        {
            if (errno != 0)
            {
                RMError(Path, errno);
                Status = 1;
            }

            break;
        }

        if (strcmp(Entry->d_name, ".") == 0 || strcmp(Entry->d_name, "..") == 0)
        {
            continue;
        }

        CHAR8 FullPath[RM_PATH_SIZE];

        if (!RMBuildPath(Path, (CONST CHAR8 *) Entry->d_name, FullPath))
        {
            RMError(Path, errno);
            Status = 1;
            continue;
        }

        if (RMRemove(FullPath, Options) != 0)
        {
            Status = 1;
        }
    }

    if (closedir(Directory) < 0)
    {
        RMError(Path, errno);
        Status = 1;
    }

    if (Status != 0)
    {
        return Status;
    }

    if (rmdir((CONST char *) Path) < 0)
    {
        RMError(Path, errno);
        return 1;
    }

    if (Options->Verbose)
    {
        printf("removed directory '%s'\n", (CONST char *) Path);
    }

    return 0;
}

INT32 main(INT32 Argc, CHAR8 **Argv)
{
    RM_OPTIONS Options = {
        .Force = FALSE,
        .Interactive = FALSE,
        .InteractiveOnce = FALSE,
        .Recursive = FALSE,
        .Directory = FALSE,
        .Verbose = FALSE,
        .PreserveRoot = TRUE
    };

    INT32 FirstPath = 1;
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
            FirstPath++;
            EndOptions = TRUE;
            break;
        }

        if (strcmp((CONST char *) Argument, "--help") == 0)
        {
            RMUsage();
            return 0;
        }

        if (strcmp((CONST char *) Argument, "--version") == 0)
        {
            RMVersion();
            return 0;
        }

        if (strcmp((CONST char *) Argument, "--preserve-root") == 0)
        {
            Options.PreserveRoot = TRUE;
            FirstPath++;
            continue;
        }

        if (strcmp((CONST char *) Argument, "--no-preserve-root") == 0)
        {
            Options.PreserveRoot = FALSE;
            FirstPath++;
            continue;
        }

        for (UINTN Index = 1; Argument[Index] != 0; Index++)
        {
            switch (Argument[Index])
            {
                case 'f':
                {
                    Options.Force = TRUE;
                    break;
                }

                case 'i':
                {
                    Options.Interactive = TRUE;
                    Options.InteractiveOnce = FALSE;
                    break;
                }

                case 'I':
                {
                    Options.InteractiveOnce = TRUE;
                    Options.Interactive = FALSE;
                    break;
                }

                case 'r':
                case 'R':
                {
                    Options.Recursive = TRUE;
                    break;
                }

                case 'd':
                {
                    Options.Directory = TRUE;
                    break;
                }

                case 'v':
                {
                    Options.Verbose = TRUE;
                    break;
                }

                default:
                {
                    fprintf(stderr, "rm: invalid option -- '%c'\n", Argument[Index]);
                    RMUsage();
                    return 2;
                }
            }
        }

        FirstPath++;
    }

    if (FirstPath >= Argc)
    {
        if (Options.Force)
        {
            return 0;
        }

        fprintf(stderr, "rm: missing operand\n");
        RMUsage();
        return 1;
    }

    if (Options.InteractiveOnce && !Options.Force)
    {
        if (Argc - FirstPath > 3 || Options.Recursive)
        {
            if (!RMConfirm((CONST CHAR8 *) "the specified files"))
            {
                return 0;
            }
        }
    }

    INT32 Status = 0;

    for (INT32 Index = FirstPath; Index < Argc; Index++)
    {
        if (RMRemove(Argv[Index], &Options) != 0)
        {
            Status = 1;
        }
    }

    return Status;
}
