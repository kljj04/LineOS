// Scripts/Validation/SecureExec/Target.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    if (getuid() != 0 || geteuid() != 0)
    {
        return 1;
    }
    int FD = open("/secure-target-PASS", O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (FD < 0)
    {
        return 2;
    }
    close(FD);
    return 0;
}
