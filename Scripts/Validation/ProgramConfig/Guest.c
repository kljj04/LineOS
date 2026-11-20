// Scripts/Validation/ProgramConfig/Guest.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Asset.h>
#include <fcntl.h>
#include <sched.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

STATIC INT32 ProgramRun(CONST char *Path, INT32 Expected)
{
    pid_t Child = fork();
    if (Child == 0)
    {
        char *Arguments[] = {(char *) Path, NULL};
        char *Environment[] = {NULL};
        execve(Path, Arguments, Environment);
        _exit(126);
    }
    INT32 Status = -1;
    return Child > 0 && waitpid(Child, &Status, 0) == Child && WIFEXITED(Status) && WEXITSTATUS(Status) == Expected ? 0 : -1;
}

INT32 main(VOID)
{
    INT64 Result = 0;
    LOS_ASSET Asset = {0};
    if (LOSAssetAcquire("Validation/Config", &Asset) != 0 || Asset.Size != 8 || memcmp(Asset.Data, "payload!", 8) != 0 || LOSAssetRelease(&Asset) != 0)
    {
        Result = 1;
    }
    if (Result == 0 && ProgramRun("/cfg-consumer.lix", 0) != 0)
    {
        Result = 2;
    }
    if (Result == 0 && ProgramRun("/cfg-missing.lix", 127) != 0)
    {
        Result = 3;
    }
    if (Result == 0 && ProgramRun("/cfg-denied.lix", 127) != 0)
    {
        Result = 4;
    }
    INT32 FD = open(Result == 0 ? "/program-cfg-PASS" : "/program-cfg-FAIL", O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (FD >= 0)
    {
        write(FD, &Result, sizeof(Result));
        close(FD);
    }
    for (;;)
    {
        sched_yield();
    }
}
