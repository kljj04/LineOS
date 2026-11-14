// LOSLibC/Source/String/ctype.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <ctype.h>

int isdigit(int Character)
{
    return Character >= '0' && Character <= '9';
}

int islower(int Character)
{
    return Character >= 'a' && Character <= 'z';
}

int isupper(int Character)
{
    return Character >= 'A' && Character <= 'Z';
}

int isspace(int Character)
{
    return Character == ' ' || (Character >= '\t' && Character <= '\r');
}

int isalpha(int Character)
{
    return islower(Character) || isupper(Character);
}

int isalnum(int Character)
{
    return isalpha(Character) || isdigit(Character);
}

int isxdigit(int Character)
{
    return isdigit(Character) || (tolower(Character) >= 'a' && tolower(Character) <= 'f');
}

int tolower(int Character)
{
    return isupper(Character) ? Character + ('a' - 'A') : Character;
}

int toupper(int Character)
{
    return islower(Character) ? Character - ('a' - 'A') : Character;
}
