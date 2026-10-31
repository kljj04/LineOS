// LOSLibC/Source/Runtime/environment.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdlib.h>
#include <string.h>

char **environ;

char *getenv(const char *Name)
{
    if (Name == NULL || *Name == 0 || strchr(Name, '=') != NULL || environ == NULL)
    {
        return NULL;
    }

    size_t Length = strlen(Name);

    for (size_t Index = 0; environ[Index] != NULL; Index++)
    {
        if (strncmp(environ[Index], Name, Length) == 0 && environ[Index][Length] == '=')
        {
            return environ[Index] + Length + 1;
        }
    }

    return NULL;
}
