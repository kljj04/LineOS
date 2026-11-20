// Scripts/Validation/Locale/Locale.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLocale.h>
#include <locale.h>
#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

STATIC CONST char *LocaleTestPath(CONST char *Path, char Buffer[4096])
{
    if (strncmp(Path, "/etc/", 5) == 0)
    {
        snprintf(Buffer, 4096, "%s/%s", getenv("LOS_TEST_DIRECTORY"), Path + 5);
        return Buffer;
    }
    return Path;
}

int LocaleTestOpen(CONST char *Path, int Flags, ...)
{
    mode_t Mode = 0;
    if ((Flags & O_CREAT) != 0)
    {
        va_list Arguments;
        va_start(Arguments, Flags);
        Mode = (mode_t) va_arg(Arguments, int);
        va_end(Arguments);
    }
    char Buffer[4096];
    return open(LocaleTestPath(Path, Buffer), Flags, Mode);
}

int LocaleTestRename(CONST char *Old, CONST char *New)
{
    char OldBuffer[4096];
    char NewBuffer[4096];
    return rename(LocaleTestPath(Old, OldBuffer), LocaleTestPath(New, NewBuffer));
}

uid_t LocaleTestGetEUID(VOID)
{
    return 0;
}

INT64 LOSRemove(CONST char *Path)
{
    char Buffer[4096];
    return unlink(LocaleTestPath(Path, Buffer));
}

STATIC VOID LocaleTestWrite(CONST char *Name)
{
    int FD = LocaleTestOpen("/etc/locale", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    assert(FD >= 0);
    assert(write(FD, Name, strlen(Name)) == (ssize_t) strlen(Name));
    assert(close(FD) == 0);
}

EXTERN int LocaleCommandMain(int Argc, char **Argv);

int main(VOID)
{
    unsetenv("LANG");
    unsetenv("LC_ALL");
    unsetenv("LC_MESSAGES");
    errno = EDOM;
    LocaleTestWrite("en_US.UTF-8\n");
    assert(LOSLocaleRefresh() != NULL);
    assert(strcmp(setlocale(LC_MESSAGES, NULL), "en_US.UTF-8") == 0);
    assert(getenv("LANG") == NULL);
    assert(setenv("LC_ALL", "", 1) == 0);
    assert(setenv("LC_MESSAGES", "", 1) == 0);
    assert(setenv("LANG", "", 1) == 0);
    LocaleTestWrite("ko_KR.UTF-8\n");
    assert(LOSLocaleRefresh() != NULL);
    assert(strcmp(setlocale(LC_MESSAGES, NULL), "ko_KR.UTF-8") == 0);
    assert(strcmp(LOSGettext("Locale set: %s\n"), "Locale set: %s\n") != 0);
    assert(setenv("LANG", "en_US.UTF-8", 1) == 0);
    assert(LOSLocaleRefresh() != NULL);
    assert(strcmp(setlocale(LC_MESSAGES, NULL), "en_US.UTF-8") == 0);
    assert(setenv("LC_MESSAGES", "ko_KR.UTF-8", 1) == 0);
    assert(LOSLocaleRefresh() != NULL);
    assert(strcmp(setlocale(LC_MESSAGES, NULL), "ko_KR.UTF-8") == 0);
    assert(setenv("LC_ALL", "C", 1) == 0);
    assert(LOSLocaleRefresh() != NULL);
    assert(strcmp(setlocale(LC_MESSAGES, NULL), "C") == 0);
    assert(setenv("LC_ALL", "invalid", 1) == 0);
    assert(LOSLocaleRefresh() == NULL);
    assert(strcmp(setlocale(LC_MESSAGES, NULL), "C") == 0);
    unsetenv("LANG");
    unsetenv("LC_ALL");
    unsetenv("LC_MESSAGES");
    LocaleTestWrite("invalid\n");
    assert(LOSLocaleRefresh() != NULL);
    assert(strcmp(setlocale(LC_MESSAGES, NULL), "C") == 0);
    char *Arguments[] = {"locale", "set", "C.UTF-8", NULL};
    assert(LocaleCommandMain(3, Arguments) == 0);
    char Text[64] = {0};
    int  FD       = LocaleTestOpen("/etc/locale", O_RDONLY);
    assert(FD >= 0 && read(FD, Text, sizeof(Text) - 1) > 0 && close(FD) == 0);
    assert(strcmp(Text, "C.UTF-8\n") == 0);
    Arguments[2] = "not-a-locale";
    assert(LocaleCommandMain(3, Arguments) == 1);
    assert(LOSLocaleNormalize("POSIX") != NULL && strcmp(LOSLocaleNormalize("POSIX"), "C") == 0);
    assert(LOSLocaleNormalize("C;ko_KR.UTF-8") == NULL);
    errno = EDOM;
    assert(LOSLocaleRefresh() != NULL && errno == EDOM);
    puts("Locale refresh, environment precedence, translation and persisted names PASS");
    return 0;
}
