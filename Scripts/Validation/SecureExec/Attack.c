// Scripts/Validation/SecureExec/Attack.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <fcntl.h>
#include <unistd.h>

static void __attribute__((constructor)) Attack(void)
{
    int FD = open("/home/test/preload-hit", O_WRONLY | O_CREAT, 0600);
    if (FD >= 0)
    {
        write(FD, "loaded", 6);
        close(FD);
    }
}
