// UserPrograms/Shell/lsh/Source/ShellVariable.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <ShellParse.h>
#include <string.h>
#include <errno.h>

#define SHELL_VARIABLE_LIMIT 128U

typedef struct
{
    char Name[256];
    char Value[SHELL_PARSE_BYTES];
} SHELL_VARIABLE;

STATIC SHELL_VARIABLE Variables[SHELL_VARIABLE_LIMIT];
STATIC UINTN          VariableCount;

BOOLEAN ShellVariableStart(char Character)
{
    return (Character >= 'A' && Character <= 'Z') || (Character >= 'a' && Character <= 'z') || Character == '_';
}

BOOLEAN ShellVariablePart(char Character)
{
    return ShellVariableStart(Character) || (Character >= '0' && Character <= '9');
}

UINTN ShellAssignmentLength(CONST char *Line)
{
    if (!ShellVariableStart(*Line))
    {
        return 0;
    }

    UINTN Length = 1;

    while (ShellVariablePart(Line[Length]))
    {
        Length++;
    }

    return Line[Length] == '=' ? Length : 0;
}

CONST char *ShellGetVariable(CONST char *Name)
{
    for (UINTN Index = 0; Index < VariableCount; Index++)
    {
        if (strcmp(Name, Variables[Index].Name) == 0)
        {
            return Variables[Index].Value;
        }
    }

    return NULL;
}

INT32 ShellAssign(CONST char *Line, char **Envp, INT32 Status)
{
    UINTN       Length = ShellAssignmentLength(Line);
    SHELL_PARSE Parse;

    if (Length == 0 || Length >= sizeof(Variables[0].Name) || !ShellParseAssignment(Line, Envp, Status, &Parse) || Parse.Count != 1 || Parse.Tokens[0].Type != SHELL_TOKEN_WORD)
    {
        ShellError("assignment", NULL, EINVAL);

        return 2;
    }

    UINTN Index = 0;
    char  Name[256];

    memcpy(Name, Line, Length);
    Name[Length] = 0;

    while (Index < VariableCount && strcmp(Variables[Index].Name, Name) != 0)
    {
        Index++;
    }

    if (Index == SHELL_VARIABLE_LIMIT)
    {
        ShellError("assignment", NULL, ENOSPC);

        return 1;
    }

    strcpy(Variables[Index].Name, Name);
    strcpy(Variables[Index].Value, Parse.Tokens[0].Text + Length + 1);

    if (Index == VariableCount)
    {
        VariableCount++;
    }

    return 0;
}
