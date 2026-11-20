// Scripts/Validation/SecureExec/Probe.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOS.h>
#include <Syscall/Security/Sudo.h>
#include <sys/auxv.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
#ifdef NORMAL
    return getuid() == 1000 && geteuid() == 1000 && getauxval(AT_SECURE) == 0 && access("/home/test/preload-hit", F_OK) == 0 ? 0 : 1;
#else
    if (getuid() != 1000 || geteuid() != 0 || getauxval(AT_UID) != 1000 || getauxval(AT_EUID) != 0 || getauxval(AT_GID) != 1000 || getauxval(AT_EGID) != 1000 || getauxval(AT_SECURE) != 1)
    {
        return 2;
    }
    for (int Index = 0; Index < 3; Index++)
    {
        if (fcntl(Index, F_GETFD) < 0)
        {
            return 3;
        }
    }
    if (access("/home/test/preload-hit", F_OK) == 0 || LOSSudoCache(SUDO_CACHE_CHECK) < 0)
    {
        return 4;
    }
    return 0;
#endif
}
