// UserPrograms/Shell/Commands/stress/stress.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <libintl.h>

#include <locale.h>

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#define STRESS_WORKERS        16
#define STRESS_DURATION       20
#define STRESS_CHECK_INTERVAL 65536

int main(void)
{
    if (setlocale(LC_ALL, "") == NULL)
    {
        setlocale(LC_ALL, "C");
    }

    pid_t    Supervisor;
    pid_t    Pids[STRESS_WORKERS];

    uint32_t Created = 0;
    uint32_t Index;

    Supervisor = fork();

    if (Supervisor < 0)
    {
        printf("%s", gettext("Failed to create stress supervisor.\n"));

        return 1;
    }

    if (Supervisor > 0)
    {
        printf("%s", gettext("Stress test started in background.\n"));
        printf(gettext("Workers: %u | Duration: %u seconds\n"), STRESS_WORKERS, STRESS_DURATION);

        return 0;
    }

    for (Index = 0; Index < STRESS_WORKERS; Index++)
    {
        pid_t Pid = fork();

        if (Pid < 0)
        {
            continue;
        }

        if (Pid == 0)
        {
            struct timespec   StartTime;
            struct timespec   CurrentTime;

            volatile uint64_t Value = 0x123456789ABCDEF0ULL;
            uint32_t          Iterations = 0;

            if (clock_gettime(CLOCK_MONOTONIC, &StartTime) != 0)
            {
                return 1;
            }

            while (1)
            {
                Value ^= Value << 13;
                Value ^= Value >> 7;
                Value ^= Value << 17;

                Iterations++;

                if (Iterations >= STRESS_CHECK_INTERVAL)
                {
                    Iterations = 0;

                    if (clock_gettime(CLOCK_MONOTONIC, &CurrentTime) != 0)
                    {
                        return 1;
                    }

                    if (CurrentTime.tv_sec >= StartTime.tv_sec + STRESS_DURATION)
                    {
                        if (CurrentTime.tv_sec > StartTime.tv_sec + STRESS_DURATION || CurrentTime.tv_nsec >= StartTime.tv_nsec)
                        {
                            break;
                        }
                    }
                }
            }

            return 0;
        }

        Pids[Created++] = Pid;
    }

    for (Index = 0; Index < Created; Index++)
    {
        int Status;

        waitpid(Pids[Index], &Status, 0);
    }

    return 0;
}