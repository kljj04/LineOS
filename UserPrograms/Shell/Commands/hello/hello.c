// UserPrograms/Shell/Commands/hello/hello.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

static _Thread_local int ThreadValue = 0;

static void *Worker(void *Arg)
{
    int *Result = (int *) Arg;

    ThreadValue = 200;
    errno       = ERANGE;

    *Result = ThreadValue + 50;

    return NULL;
}

static int CompareInt(const void *A, const void *B)
{
    int X = *(const int *) A;
    int Y = *(const int *) B;

    return (X > Y) - (X < Y);
}

int main(void)
{
    printf("=== MUSL LIBC TEST ===\n");

    printf("[1] stdio: ");
    printf("PASS\n");

    printf("[2] memory: ");

    int *Array = malloc(sizeof(int) * 4);

    if (Array == NULL)
    {
        printf("FAIL: malloc: %s\n", strerror(errno));
        return 1;
    }

    Array[0] = 40;
    Array[1] = 10;
    Array[2] = 30;
    Array[3] = 20;

    qsort(Array, 4, sizeof(int), CompareInt);

    if (Array[0] != 10 || Array[1] != 20 || Array[2] != 30 || Array[3] != 40)
    {
        printf("FAIL: qsort\n");
        free(Array);
        return 1;
    }

    free(Array);
    printf("PASS\n");

    printf("[3] string: ");

    char Buffer[64];

    snprintf(Buffer, sizeof(Buffer), "LineOS %d", 2026);

    if (strcmp(Buffer, "LineOS 2026") != 0)
    {
        printf("FAIL: snprintf/strcmp\n");
        return 1;
    }

    printf("PASS\n");

    printf("[4] conversion: ");

    char *End;
    long  Value = strtol("123456", &End, 10);

    if (Value != 123456 || *End != '\0')
    {
        printf("FAIL: strtol\n");
        return 1;
    }

    printf("PASS\n");

    printf("[5] TLS: ");

    ThreadValue = 100;

    if (ThreadValue != 100)
    {
        printf("FAIL: TLS initialization\n");
        return 1;
    }

    printf("PASS\n");

    printf("[6] pthread: ");

    pthread_t Thread;
    int       Result = 0;

    errno = EDOM;

    int PthreadError = pthread_create(&Thread, NULL, Worker, &Result);

    if (PthreadError != 0)
    {
        printf("FAIL: pthread_create: %d (%s)\n", PthreadError, strerror(PthreadError));
        return 1;
    }

    PthreadError = pthread_join(Thread, NULL);

    if (PthreadError != 0)
    {
        printf("FAIL: pthread_join: %d (%s)\n", PthreadError, strerror(PthreadError));
        return 1;
    }

    if (Result != 250 || ThreadValue != 100)
    {
        printf("FAIL: TLS isolation\n");
        return 1;
    }

    if (errno != EDOM)
    {
        printf("FAIL: errno isolation\n");
        return 1;
    }

    printf("PASS\n");

    printf("[7] errno: ");

    char Invalid[] = "invalid";

    errno = 0;
    Value = strtol(Invalid, &End, 10);

    if (Value != 0 || End != Invalid)
    {
        printf("FAIL: invalid conversion\n");
        return 1;
    }

    errno = 0;
    Value = strtol("999999999999999999999999999999999999999999", &End, 10);

    if (errno != ERANGE)
    {
        printf("FAIL: ERANGE: %d (expected %d)\n", errno, ERANGE);
        return 1;
    }

    printf("PASS\n");

    printf("[8] memfd + sealing: ");

    int MemFd = memfd_create("LineOS-Seal-Test", MFD_CLOEXEC | MFD_ALLOW_SEALING);

    if (MemFd < 0)
    {
        if (errno == ENOSYS)
        {
            printf("UNSUPPORTED (ENOSYS)\n");
        }
        else
        {
            printf("FAIL: memfd_create: %d (%s)\n", errno, strerror(errno));
            return 1;
        }
    }
    else
    {
        const char Payload[] = "LineOS Memory File";

        ssize_t Written = write(MemFd, Payload, sizeof(Payload));

        if (Written != sizeof(Payload))
        {
            printf("FAIL: write: %s\n", strerror(errno));
            close(MemFd);
            return 1;
        }

        int RequiredSeals = F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;

        if (fcntl(MemFd, F_ADD_SEALS, RequiredSeals) < 0)
        {
            printf("FAIL: F_ADD_SEALS: %d (%s)\n", errno, strerror(errno));
            close(MemFd);
            return 1;
        }

        int CurrentSeals = fcntl(MemFd, F_GET_SEALS);

        if (CurrentSeals < 0 || (CurrentSeals & RequiredSeals) != RequiredSeals)
        {
            printf("FAIL: F_GET_SEALS\n");
            close(MemFd);
            return 1;
        }

        errno = 0;

        Written = write(MemFd, Payload, sizeof(Payload));

        if (Written != -1 || errno != EPERM)
        {
            printf("FAIL: write seal enforcement: %d (%s)\n", errno, strerror(errno));
            close(MemFd);
            return 1;
        }

        errno = 0;

        if (ftruncate(MemFd, 0) != -1 || errno != EPERM)
        {
            printf("FAIL: shrink seal enforcement: %d (%s)\n", errno, strerror(errno));
            close(MemFd);
            return 1;
        }

        close(MemFd);

        printf("PASS\n");
    }

    printf("\nMUSL: ALL TESTS COMPLETED\n");

    return 0;
}
