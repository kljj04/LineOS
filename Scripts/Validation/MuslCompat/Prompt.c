// Scripts/Validation/MuslCompat/Prompt.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#define main LoginProgramMain
#include "../../../UserPrograms/Shell/Login/Login.c"
#undef main
#include <assert.h>

STATIC CONST char *Input;
STATIC UINT64 InputPosition;
STATIC UINT64 ExpectedOutput;
STATIC BOOLEAN FirstRead;

ssize_t read(int FD, void *Buffer, size_t Count)
{
    assert(FD == 0 && Count == 1);
    if (FirstRead)
    {
        assert((UINT64) lseek(1, 0, SEEK_CUR) == ExpectedOutput);
        FirstRead = FALSE;
    }
    if (Input[InputPosition] == 0)
    {
        return 0;
    }
    *(char *) Buffer = Input[InputPosition++];
    return 1;
}

INT32 main(VOID)
{
    FILE *Output = tmpfile();
    assert(Output != NULL && dup2(fileno(Output), 1) == 1);
    STATIC char Storage[4096];
    assert(setvbuf(stdout, Storage, _IOFBF, sizeof(Storage)) == 0);
    char Buffer[64];
    Input = "kljj04\n";
    InputPosition = 0;
    FirstRead = TRUE;
    ExpectedOutput = strlen("Username: ");
    printf("Username: ");
    assert(LoginReadLine(Buffer, sizeof(Buffer), TRUE) == 1 && strcmp(Buffer, "kljj04") == 0);
    ExpectedOutput = (UINT64) lseek(1, 0, SEEK_CUR) + strlen("Password: ");
    Input = "secret\n";
    InputPosition = 0;
    FirstRead = TRUE;
    printf("Password: ");
    assert(LoginReadLine(Buffer, sizeof(Buffer), FALSE) == 1 && strcmp(Buffer, "secret") == 0);
    assert((UINT64) lseek(1, 0, SEEK_CUR) == ExpectedOutput + 1);
    return 0;
}
