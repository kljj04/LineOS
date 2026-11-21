
// UserPrograms/Shell/Commands/nproc/nproc.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <sys/sysinfo.h>
#include <Userspace/TypeInfo.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

BOOLEAN IsInteger(CHAR8 *String)
{
    char *End;
    errno = 0;

    if (String == NULL || String[0] == '\0')
    {
        return FALSE;
    }

    strtol((char *)String, &End, 10);

    return errno != ERANGE && *End == '\0' && End != (char *)String;
}

INT32 main(INT32 Argc, CHAR8 **Argv)
{
    INT32 CPUCount = get_nprocs();

    if (Argc < 2)
    {
        printf("%d\n", CPUCount);
        return 0;
    }
    else if (strcmp(Argv[1], "--all") == 0)
    {
        printf("%d\n", CPUCount);
        return 0;
    }
    else if (strcmp(Argv[1], "--ignore") == 0)
    {
        if (Argc < 3 || IsInteger(Argv[2]) == FALSE)
        {
            fprintf(stderr, "nproc: invalid ignore value\n");
            return 1;
        }

        long Ignore = strtol((char *)Argv[2], NULL, 10);

        if (Ignore < 0 || Ignore >= CPUCount)
        {
            fprintf(stderr, "nproc: invalid ignore value\n");
            return 1;
        }

        printf("%d\n", CPUCount - (INT32)Ignore);
        return 0;
    }

    fprintf(stderr, "nproc: unrecognized argument: %s\n", Argv[1]);
    return 1;
}
