// UserPrograms/Shell/Login/LoginSession.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LoginSession.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

#define LOGIN_AUTOSTART_PATH      ".config/autostart"
#define LOGIN_AUTOSTART_LIMIT     32U
#define LOGIN_AUTOSTART_LINE_SIZE 4096U

STATIC VOID LoginAutostartDefault(VOID)
{
    struct stat Info;

    if (lstat(LOGIN_AUTOSTART_PATH, &Info) == 0 || errno != ENOENT)
    {
        return;
    }

    if (mkdir(".config", 0700) < 0 && errno != EEXIST)
    {
        perror("Login: .config");

        return;
    }

    INT32 FD = open(LOGIN_AUTOSTART_PATH, O_WRONLY | O_CREAT | O_EXCL, 0600);

    if (FD < 0)
    {
        if (errno != EEXIST)
        {
            perror("Login: autostart");
        }

        return;
    }

    CONST char *Default = "/sys/lwm/lwm.lix\n";
    UINTN       Length = strlen(Default);
    UINTN       Position = 0;
    INT32       Error = 0;

    while (Position < Length)
    {
        ssize_t Count = write(FD, Default + Position, Length - Position);

        if (Count < 0 && errno == EINTR)
        {
            continue;
        }

        if (Count <= 0)
        {
            Error = Count < 0 ? errno : EIO;
            break;
        }

        Position += (UINTN) Count;
    }

    if (close(FD) < 0 && Error == 0)
    {
        Error = errno;
    }

    if (Error != 0)
    {
        remove(LOGIN_AUTOSTART_PATH);
        errno = Error;
        perror("Login: autostart");
    }
}

STATIC BOOLEAN LoginAutostartLaunch(char *Line, INT32 ConfigFD, char **Environment)
{
    UINTN Length = strlen(Line);

    while (Length != 0 && (Line[Length - 1] == ' ' || Line[Length - 1] == '\t' || Line[Length - 1] == '\r'))
    {
        Line[--Length] = 0;
    }

    while (*Line == ' ' || *Line == '\t')
    {
        Line++;
    }

    if (*Line == 0 || *Line == '#')
    {
        return FALSE;
    }

    if (*Line != '/')
    {
        puts("Login: autostart requires an absolute executable path per line");

        return FALSE;
    }

    pid_t Child = fork();

    if (Child < 0)
    {
        perror("Login: autostart fork");

        return FALSE;
    }

    if (Child == 0)
    {
        close(ConfigFD);
        close(0);

        INT32 Input = open("/dev/null", O_RDONLY);

        if (Input != 0)
        {
            puts("Login: cannot prepare autostart standard input");
            exit(126);
        }

        char *Arguments[] = {Line, NULL};

        execve(Line, Arguments, Environment);

        INT32 Error = errno;

        perror(Line);
        exit(Error == ENOENT ? 127 : 126);
    }

    return TRUE;
}

STATIC UINT32 LoginAutostartRead(char **Environment)
{
    LoginAutostartDefault();

    INT32 FD = open(LOGIN_AUTOSTART_PATH, O_RDONLY);

    if (FD < 0)
    {
        if (errno != ENOENT)
        {
            perror("Login: autostart");
        }

        return 0;
    }

    struct stat Info;

    if (fstat(FD, &Info) < 0 || !S_ISREG(Info.st_mode))
    {
        puts("Login: autostart must be a regular file");
        close(FD);

        return 0;
    }

    char    Line[LOGIN_AUTOSTART_LINE_SIZE];
    char    Buffer[1024];
    UINTN   Position = 0;
    UINT32  Live = 0;
    UINT32  Entries = 0;
    BOOLEAN Invalid = FALSE;

    for (;;)
    {
        ssize_t Count = read(FD, Buffer, sizeof(Buffer));

        if (Count < 0 && errno == EINTR)
        {
            continue;
        }

        if (Count < 0)
        {
            perror("Login: autostart read");
            break;
        }

        if (Count == 0)
        {
            if (!Invalid && Position != 0 && Entries < LOGIN_AUTOSTART_LIMIT)
            {
                Line[Position] = 0;
                Live += LoginAutostartLaunch(Line, FD, Environment) ? 1U : 0U;
            }

            break;
        }

        for (ssize_t Index = 0; Index < Count; Index++)
        {
            UINT8 Byte = (UINT8) Buffer[Index];

            if (Byte == '\n')
            {
                if (!Invalid)
                {
                    Line[Position] = 0;

                    if (Entries < LOGIN_AUTOSTART_LIMIT)
                    {
                        BOOLEAN Started = LoginAutostartLaunch(Line, FD, Environment);

                        if (Started)
                        {
                            Live++;
                            Entries++;
                        }
                    }
                }
                else
                {
                    puts("Login: invalid or oversized autostart line skipped");
                }

                Position = 0;
                Invalid = FALSE;
            }
            else if (Byte == 0 || (Byte < 32 && Byte != '\t' && Byte != '\r') || Position + 1 >= sizeof(Line))
            {
                Invalid = TRUE;
            }
            else if (!Invalid)
            {
                Line[Position++] = (char) Byte;
            }
        }
    }

    if (close(FD) < 0)
    {
        perror("Login: autostart close");
    }

    return Live;
}

INT32 LoginSessionRun(char **Environment)
{
    UINT32 Live = LoginAutostartRead(Environment);
    pid_t  Shell = fork();
    INT32  ShellStatus = 1;

    if (Shell == 0)
    {
        char *Arguments[] = {"-lsh", NULL};

        execve("/bin/lsh/lsh.lix", Arguments, Environment);
        perror("Login: shell execve");
        exit(127);
    }

    if (Shell < 0)
    {
        perror("Login: shell fork");
    }
    else
    {
        Live++;
    }

    while (Live != 0)
    {
        INT32 Status;
        pid_t Child = waitpid(-1, &Status, 0);

        if (Child < 0 && errno == EINTR)
        {
            continue;
        }

        if (Child < 0)
        {
            perror("Login: session waitpid");

            return 1;
        }

        Live--;

        if (Child == Shell)
        {
            ShellStatus = WIFEXITED(Status) ? WEXITSTATUS(Status) : 128 + WTERMSIG(Status);
        }
    }

    return ShellStatus;
}
