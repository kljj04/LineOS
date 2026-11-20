// Scripts/Validation/MuslCompat/Guest.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Lwm.h>
#include <Syscall/Device/Ioctl.h>
#include <errno.h>
#include <LOS.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

STATIC LWM Manager;

#define CHECK(Condition) \
    if (!(Condition)) \
    { \
        Result = __LINE__; \
        goto Done; \
    }

INT32 main(VOID)
{
    INT64 Result = 0;
    CHECK(syscall(SYS_membarrier, 0, 0) == 0);
    sigset_t Set;
    sigset_t Old;
    sigset_t Current;
    sigemptyset(&Set);
    sigaddset(&Set, SIGUSR1);
    CHECK(sigprocmask(SIG_BLOCK, &Set, &Old) == 0);
    CHECK(sigprocmask(SIG_SETMASK, NULL, &Current) == 0 && sigismember(&Current, SIGUSR1) == 1);
    INT32 FD = open("/sbin/Init.lix", O_RDONLY | O_CLOEXEC);
    CHECK(FD >= 0 && fcntl(FD, F_GETFD) == FD_CLOEXEC);
    INT32 Scratch = open("/compat-unrelated", O_CREAT | O_EXCL | O_WRONLY, 0644);
    CHECK(Scratch >= 0 && close(Scratch) == 0);
    UINT8 Header[4];
    CHECK(pread(FD, Header, sizeof(Header), 0) == sizeof(Header) && memcmp(Header, "\177ELF", 4) == 0);
    VOID *Mapped = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, FD, 0);
    CHECK(Mapped != MAP_FAILED && memcmp(Mapped, Header, 4) == 0 && munmap(Mapped, 4096) == 0);
    INT32 Stale = open("/compat-reuse", O_CREAT | O_EXCL | O_RDWR, 0600);
    CHECK(Stale >= 0 && LOSRemove("/compat-reuse") == 0);
    INT32 Reused = open("/compat-reuse", O_CREAT | O_EXCL | O_RDWR, 0600);
    CHECK(Reused >= 0);
    struct stat Info;
    CHECK(fstat(Stale, &Info) == -1 && errno == ESTALE);
    CHECK(close(Stale) == 0 && close(Reused) == 0);
    INT32 Duplicate = fcntl(FD, F_DUPFD, 12);
    CHECK(Duplicate == 12 && fcntl(Duplicate, F_GETFD) == 0);
    CHECK(fcntl(Duplicate, F_SETFD, FD_CLOEXEC) == 0);
    CHECK(fcntl(FD, F_SETFD, 0) == 0 && fcntl(Duplicate, F_GETFD) == FD_CLOEXEC);
    CHECK((fcntl(FD, F_GETFL) & O_ACCMODE) == O_RDONLY);
    CHECK(fcntl(FD, F_SETFL, O_APPEND) == 0 && (fcntl(Duplicate, F_GETFL) & O_APPEND) != 0);
    pid_t Child = fork();
    if (Child == 0)
    {
        if (sigprocmask(SIG_SETMASK, NULL, &Current) != 0 || sigismember(&Current, SIGUSR1) != 1)
        {
            _exit(10);
        }
        char *Arguments[]   = {"/compat-child.lix", NULL};
        char *Environment[] = {NULL};
        execve(Arguments[0], Arguments, Environment);
        _exit(11);
    }
    INT32 Status = -1;
    CHECK(Child > 0 && waitpid(Child, &Status, 0) == Child && WIFEXITED(Status) && WEXITSTATUS(Status) == 0);
    CHECK(close(FD) == 0 && close(Duplicate) == 0);
    CHECK(sigprocmask(SIG_SETMASK, &Old, NULL) == 0);
    INT32 Master = open("/dev/lsh1-master", O_RDWR);
    CHECK(Master >= 0);
    pid_t Acknowledger = fork();
    if (Acknowledger == 0)
    {
        for (;;)
        {
            INT32 Stop = open("/compat-stop", O_RDONLY);
            if (Stop >= 0)
            {
                close(Stop);
                _exit(0);
            }
            ioctl(Master, LOS_TIO_WAIT_READ, 20);
        }
    }
    CHECK(Acknowledger > 0);
    CHECK(LwmOpen(&Manager));
    CHECK(LwmCompose(&Manager) == 0);
    POINTER_INFO Pointer;
    CHECK(ioctl(Manager.TTY, LOS_TIO_GET_POINTER, &Pointer) == 0);
    UINT64 Compositor = 0;
    CHECK(ioctl(Master, LOS_TIO_GET_COMPOSITOR, &Compositor) == 0 && Compositor != 0);
    LwmClose(&Manager);
    INT32 Stop = open("/compat-stop", O_CREAT | O_WRONLY, 0644);
    CHECK(Stop >= 0);
    close(Stop);
    CHECK(waitpid(Acknowledger, &Status, 0) == Acknowledger && WIFEXITED(Status) && WEXITSTATUS(Status) == 0);
    close(Master);
Done:
    FD = open(Result == 0 ? "/musl-compat-PASS" : "/musl-compat-FAIL", O_WRONLY | O_CREAT | O_EXCL, 0644);
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
