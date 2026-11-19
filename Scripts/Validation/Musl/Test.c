#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/uio.h>

extern char **environ;

static __thread int TLSValue = 73;

int main(void)
{
    int Master = open("/dev/lsh1-master", O_RDWR);
    if (Master < 0)
    {
        return 1;
    }
    int        FD = open("/dev/lsh1", O_WRONLY);
    const char Begin[] = "MUSL: main reached\n";
    write(FD, Begin, sizeof(Begin) - 1);
    struct iovec Vectors[] = {{"MUSL: writev ", 13}, {"PASS\n", 5}};
    if (writev(FD, Vectors, 2) != 18 || writev(FD, Vectors, 0) != 0 || writev(FD, Vectors, 1025) != -1 || errno != EINVAL)
    {
        return 1;
    }
    struct iovec Invalid = {(void *) 1, 1};
    if (writev(FD, &Invalid, 1) != -1 || errno != EFAULT)
    {
        return 1;
    }
    void *Mapping = mmap(NULL, 8192, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (Mapping == MAP_FAILED || write(FD, Mapping, 1) != -1 || errno != EFAULT || mprotect(Mapping, 4096, PROT_READ | PROT_WRITE) != 0)
    {
        write(FD, "MUSL: FAIL PROT_NONE\n", sizeof("MUSL: FAIL PROT_NONE\n") - 1);
        return 1;
    }
    memset(Mapping, 42, 4096);
    if (mprotect(Mapping, 4096, PROT_NONE) != 0 || write(FD, Mapping, 1) != -1 || errno != EFAULT || mprotect(Mapping, 4096, PROT_READ | PROT_WRITE) != 0 || *(unsigned char *) Mapping != 42)
    {
        write(FD, "MUSL: FAIL mprotect\n", sizeof("MUSL: FAIL mprotect\n") - 1);
        return 1;
    }
    pid_t Child = fork();
    if (Child == 0)
    {
        if (mprotect((char *) Mapping + 4096, 4096, PROT_READ | PROT_WRITE) != 0 || *(unsigned char *) Mapping != 42)
        {
            _exit(1);
        }
        _exit(munmap(Mapping, 8192) != 0);
    }
    int Status = -1;
    if (Child < 0 || waitpid(Child, &Status, 0) != Child || Status != 0 || munmap(Mapping, 8192) != 0)
    {
        write(FD, "MUSL: FAIL fork/unmap\n", sizeof("MUSL: FAIL fork/unmap\n") - 1);
        return 1;
    }
    write(FD, "MUSL: PROT_NONE/mprotect/fork/unmap PASS\n", sizeof("MUSL: PROT_NONE/mprotect/fork/unmap PASS\n") - 1);
    char *Buffer = malloc(128);
    if (Buffer == NULL || TLSValue != 73)
    {
        write(FD, "MUSL: FAIL allocation/TLS\n", sizeof("MUSL: FAIL allocation/TLS\n") - 1);
        return 1;
    }
    errno = 0;
    int Result = open("/musl-missing-file", O_RDONLY);
    int Error = errno;
    int Length = snprintf(Buffer, 128, "MUSL: malloc/stdio/TLS PASS; open=%d errno=%d pid=%d\n", Result, Error, getpid());
    write(FD, Buffer, Length);
    free(Buffer);
    if (Result == -1 && Error == ENOENT)
    {
        Child = fork();
        if (Child == 0)
        {
            char *Arguments[] = {"hello", NULL};
            execve("/bin/lsh/hello.lix", Arguments, environ);
            _exit(127);
        }
        if (Child < 0 || waitpid(Child, &Status, 0) != Child || Status != 0)
        {
            write(FD, "MUSL: FAIL hello\n", sizeof("MUSL: FAIL hello\n") - 1);
            return 1;
        }
        write(FD, "MUSL: ALL PASS\n", sizeof("MUSL: ALL PASS\n") - 1);
        return 0;
    }
    write(FD, "MUSL: FAIL errno\n", sizeof("MUSL: FAIL errno\n") - 1);
    return 1;
}
