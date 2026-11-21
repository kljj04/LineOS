// UserPrograms/Init/Init.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLocale.h>

#include <locale.h>

#include <Userspace/TypeInfo.h>
#include <Asset.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

INT32 main(VOID)
{
    if (LOSLocaleRefresh() == NULL)
    {
        setlocale(LC_ALL, "C");
    }

    if (open("/dev/lsh1", O_RDONLY) != 0 || open("/dev/lsh1", O_WRONLY) != 1 || open("/dev/lsh1", O_WRONLY) != 2)
    {
        return 1;
    }

    INT32 Master = open("/dev/lsh1-master", O_RDWR);

    if (Master != 3)
    {
        puts(LOSGettext("Init: terminal master open failed"));

        return 1;
    }

    pid_t Terminal = fork();

    if (Terminal == 0)
    {
        char *Arguments[]   = {"/sbin/Terminal.lix", NULL};
        char *Environment[] = {NULL};

        execve(Arguments[0], Arguments, Environment);
        puts(LOSGettext("Init: Terminal execve failed"));

        return 127;
    }

    if (Terminal < 0)
    {
        puts(LOSGettext("Init: Terminal fork failed"));

        return 1;
    }

    close(Master);

    pid_t Login = fork();

    if (Login == 0)
    {
        char *Arguments[]   = {"/sbin/Login.lix", NULL};
        char *Environment[] = {NULL};

        execve(Arguments[0], Arguments, Environment);
        puts(LOSGettext("Init: Login execve failed"));

        return 127;
    }

    if (Login < 0)
    {
        puts(LOSGettext("Init: Login fork failed"));

        return 1;
    }

    return 0;
}
