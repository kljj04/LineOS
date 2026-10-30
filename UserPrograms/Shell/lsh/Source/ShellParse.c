// UserPrograms/Shell/lsh/Source/ShellParse.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <ShellParse.h>
#include <string.h>
#include <unistd.h>

STATIC BOOLEAN ShellParseCharacter(SHELL_PARSE *Parse, char Character)
{
    if (Parse->Used + 1 >= sizeof(Parse->Storage))
    {
        return FALSE;
    }
    Parse->Active = TRUE;
    Parse->Storage[Parse->Used++] = Character;
    return TRUE;
}

STATIC BOOLEAN ShellParseFinish(SHELL_PARSE *Parse)
{
    if (!Parse->Active)
    {
        return TRUE;
    }
    if (Parse->Count == SHELL_PARSE_TOKENS || Parse->Used >= sizeof(Parse->Storage))
    {
        return FALSE;
    }
    Parse->Storage[Parse->Used++] = 0;
    Parse->Tokens[Parse->Count].Type = SHELL_TOKEN_WORD;
    Parse->Tokens[Parse->Count++].Text = Parse->Storage + Parse->Start;
    Parse->Start = Parse->Used;
    Parse->Active = FALSE;
    return TRUE;
}

STATIC BOOLEAN ShellParseNameStart(char Character)
{
    return (Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z') || Character == '_';
}

STATIC BOOLEAN ShellParseNamePart(char Character)
{
    return ShellParseNameStart(Character) || (Character >= '0' && Character <= '9');
}

STATIC VOID ShellParseNumber(UINT64 Value, char Text[21])
{
    char Digits[20];
    UINTN Count = 0;
    do
    {
        Digits[Count++] = '0' + Value % 10;
        Value /= 10;
    } while (Value != 0);
    UINTN Length = Count;
    for (UINTN Index = 0; Index < Length; Index++)
    {
        Text[Index] = Digits[--Count];
    }
    Text[Length] = 0;
}

STATIC BOOLEAN ShellParseVariable(CONST char **Position, char **Envp, INT32 Status, BOOLEAN Quoted, SHELL_PARSE *Parse)
{
    CONST char *Text = *Position + 1;
    CONST char *Value;
    char Number[21];
    char Name[256];
    UINTN Length = 0;
    BOOLEAN Braces = *Text == '{';
    if (Braces)
    {
        Text++;
    }
    if (*Text == '$' || *Text == '?')
    {
        UINT64 Numeric = *Text == '$' ? (UINT64) getpid() : (UINT64) (Status & 255);
        ShellParseNumber(Numeric, Number);
        Value = Number;
        Text++;
    }
    else if (ShellParseNameStart(*Text))
    {
        while (ShellParseNamePart(*Text))
        {
            if (Length + 1 >= sizeof(Name))
            {
                return FALSE;
            }
            Name[Length++] = *Text++;
        }
        Name[Length] = 0;
        Value = ShellGetEnvironment(Envp, Name);
        if (Value == NULL)
        {
            Value = "";
        }
    }
    else
    {
        if (Braces)
        {
            return FALSE;
        }
        (*Position)++;
        return ShellParseCharacter(Parse, '$');
    }
    if (Braces)
    {
        if (*Text != '}')
        {
            return FALSE;
        }
        Text++;
    }
    *Position = Text;
    while (*Value != 0)
    {
        if (!Quoted && (*Value == ' ' || *Value == '\t' || *Value == '\n'))
        {
            if (!ShellParseFinish(Parse))
            {
                return FALSE;
            }
        }
        else if (!ShellParseCharacter(Parse, *Value))
        {
            return FALSE;
        }
        Value++;
    }
    return TRUE;
}

BOOLEAN ShellParseLine(CONST char *Text, char **Envp, INT32 Status, SHELL_PARSE *Parse)
{
    memset(Parse, 0, sizeof(*Parse));
    char Quote = 0;
    while (*Text != 0)
    {
        char Character = *Text;
        if (Quote == '\'')
        {
            Text++;
            if (Character == '\'')
            {
                Quote = 0;
            }
            else if (!ShellParseCharacter(Parse, Character))
            {
                return FALSE;
            }
            continue;
        }
        if (Character == '\'' && Quote == 0)
        {
            Parse->Active = TRUE;
            Quote = Character;
            Text++;
            continue;
        }
        if (Character == '"')
        {
            Parse->Active = TRUE;
            Quote = Quote == '"' ? 0 : '"';
            Text++;
            continue;
        }
        if (Character == '\\')
        {
            Text++;
            if (*Text == 0)
            {
                return FALSE;
            }
            if (Quote == '"' && *Text != '$' && *Text != '"' && *Text != '\\')
            {
                if (!ShellParseCharacter(Parse, '\\'))
                {
                    return FALSE;
                }
            }
            if (!ShellParseCharacter(Parse, *Text++))
            {
                return FALSE;
            }
            continue;
        }
        if (Character == '$')
        {
            if (!ShellParseVariable(&Text, Envp, Status, Quote != 0, Parse))
            {
                return FALSE;
            }
            continue;
        }
        if (Quote == 0 && (Character == ' ' || Character == '\t'))
        {
            if (!ShellParseFinish(Parse))
            {
                return FALSE;
            }
            Text++;
            continue;
        }
        if (Quote == 0 && Character == '>')
        {
            if (!ShellParseFinish(Parse) || Parse->Count == SHELL_PARSE_TOKENS)
            {
                return FALSE;
            }
            Text++;
            SHELL_TOKEN_TYPE Type = SHELL_TOKEN_TRUNCATE;
            if (*Text == '>')
            {
                Type = SHELL_TOKEN_APPEND;
                Text++;
            }
            Parse->Tokens[Parse->Count].Type = Type;
            Parse->Tokens[Parse->Count++].Text = NULL;
            continue;
        }
        if (!ShellParseCharacter(Parse, *Text++))
        {
            return FALSE;
        }
    }
    return Quote == 0 && ShellParseFinish(Parse);
}
