// Scripts/Validation/MuslCompat/Child.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <errno.h>
#include <fcntl.h>
#include <signal.h>

int main(void)
{
    sigset_t Current;
    if (sigprocmask(SIG_SETMASK, 0, &Current) != 0 || sigismember(&Current, SIGUSR1) != 1)
    {
        return 1;
    }
    if (fcntl(12, F_GETFD) != -1 || errno != EBADF)
    {
        return 2;
    }
    return 0;
}
