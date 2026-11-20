// Scripts/Validation/SecureExec/Guest.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOS.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>

STATIC INT32 Run(CONST char *Path, char **Arguments, INT32 Expected, BOOLEAN Closed, CONST char *Input)
{
    pid_t Child = fork();
    if (Child == 0)
    {
        if (setgid(1000) != 0 || setuid(1000) != 0)
        {
            _exit(125);
        }
        if (Input != NULL)
        {
            INT32 FD = open(Input, O_RDONLY);
            if (FD < 0)
            {
                _exit(124);
            }
            close(0);
            if (fcntl(FD, F_DUPFD, 0) != 0)
            {
                _exit(123);
            }
            close(FD);
        }
        if (Closed)
        {
            close(0);
            close(1);
            close(2);
        }
        char *Environment[] = {"LD_PRELOAD=/home/test/attack.so", "LD_LIBRARY_PATH=/home/test", "LANG=C", NULL};
        execve(Path, Arguments, Environment);
        _exit(Expected == 77 && errno == EACCES && geteuid() == 1000 ? 77 : 126);
    }
    INT32 Status = -1;
    return Child > 0 && waitpid(Child, &Status, 0) == Child && WIFEXITED(Status) && WEXITSTATUS(Status) == Expected ? 0 : -1;
}

#define CHECK(Condition) \
    if (!(Condition)) \
    { \
        Result = __LINE__; \
        goto Done; \
    }

INT32 main(VOID)
{
    INT64 Result = 0;
    for (INT32 Index = 0; Index < 3; Index++)
    {
        CHECK(open("/dev/null", O_RDWR) == Index);
    }
    char *Normal[] = {"/secure-normal.lix", NULL};
    CHECK(Run(Normal[0], Normal, 0, FALSE, NULL) == 0);
    CHECK(LOSRemove("/home/test/preload-hit") == 0);
    char *Probe[] = {"/secure-probe.lix", NULL};
    CHECK(Run(Probe[0], Probe, 0, TRUE, NULL) == 0);
    CHECK(access("/home/test/preload-hit", F_OK) == -1 && errno == ENOENT);
    char *Bad[] = {"/secure-bad-loader.lix", NULL};
    CHECK(Run(Bad[0], Bad, 77, FALSE, NULL) == 0);
    char *Unsafe[] = {"/secure-unsafe.lix", NULL};
    CHECK(Run(Unsafe[0], Unsafe, 127, FALSE, NULL) == 0);
    char *Clear[] = {"/bin/lsh/sudo.lix", "-k", NULL};
    CHECK(Run(Clear[0], Clear, 0, FALSE, NULL) == 0);
    char *Sudo[] = {"/bin/lsh/sudo.lix", "/secure-target.lix", NULL};
    CHECK(Run(Sudo[0], Sudo, 0, FALSE, "/home/test/password") == 0);
    CHECK(access("/secure-target-PASS", F_OK) == 0);
    CHECK(access("/home/test/preload-hit", F_OK) == -1 && errno == ENOENT);
    CHECK(Run(Sudo[0], Sudo, 1, FALSE, "/home/test/wrong-password") == 0);
Done:
    INT32 FD = open(Result == 0 ? "/secure-exec-PASS" : "/secure-exec-FAIL", O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (FD >= 0)
    {
        write(FD, &Result, sizeof(Result));
        INT64 Error = errno;
        write(FD, &Error, sizeof(Error));
        close(FD);
    }
    for (;;)
    {
        sched_yield();
    }
}
