// UserPrograms/Shell/lsh/Source/IME.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <IME.h>

typedef struct
{
    INT32 First;
    INT32 Second;
    INT32 Combined;
} IME_PAIR;

STATIC CONST IME_PAIR Vowels[] = {{8, 0, 9}, {8, 1, 10}, {8, 20, 11}, {9, 20, 10}, {13, 4, 14}, {13, 5, 15}, {13, 20, 16}, {14, 20, 15}, {18, 20, 19}};
STATIC CONST IME_PAIR Finals[] = {{1, 19, 3}, {4, 22, 5}, {4, 27, 6}, {8, 1, 9}, {8, 16, 10}, {8, 17, 11}, {8, 19, 12}, {8, 25, 13}, {8, 26, 14}, {8, 27, 15}, {17, 19, 18}};
STATIC CONST INT32    LeadFinals[] = {1, 2, 4, 7, 0, 8, 16, 17, 0, 19, 20, 21, 22, 0, 23, 24, 25, 26, 27};
STATIC CONST INT32    FinalLeads[] = {-1, 0, 1, -1, 2, -1, -1, 3, 5, -1, -1, -1, -1, -1, -1, -1, 6, 7, -1, 9, 10, 11, 12, 14, 15, 16, 17, 18};
STATIC CONST UINT32   LeadJamo[] = {0x3131, 0x3132, 0x3134, 0x3137, 0x3138, 0x3139, 0x3141, 0x3142, 0x3143, 0x3145, 0x3146, 0x3147, 0x3148, 0x3149, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E};

VOID IMEReset(IME_STATE *State)
{
    State->Lead = -1;
    State->Vowel = -1;
    State->Final = 0;
}

UINT32 IMECodepoint(CONST IME_STATE *State)
{
    if (State->Lead >= 0 && State->Vowel >= 0)
    {
        return 0xAC00U + (UINT32) ((State->Lead * 21 + State->Vowel) * 28 + State->Final);
    }

    if (State->Lead >= 0)
    {
        return LeadJamo[State->Lead];
    }

    return State->Vowel >= 0 ? 0x314FU + (UINT32) State->Vowel : 0;
}

STATIC INT32 IMECombine(CONST IME_PAIR *Pairs, UINTN Count, INT32 First, INT32 Second)
{
    for (UINTN Index = 0; Index < Count; Index++)
    {
        if (Pairs[Index].First == First && Pairs[Index].Second == Second)
        {
            return Pairs[Index].Combined;
        }
    }

    return -1;
}

STATIC INT32 IMELead(UINT8 Byte)
{
    switch (Byte)
    {
        case 'r':
            return 0;

        case 'R':
            return 1;

        case 's':
        case 'S':
            return 2;

        case 'e':
            return 3;

        case 'E':
            return 4;

        case 'f':
        case 'F':
            return 5;

        case 'a':
        case 'A':
            return 6;

        case 'q':
            return 7;

        case 'Q':
            return 8;

        case 't':
            return 9;

        case 'T':
            return 10;

        case 'd':
        case 'D':
            return 11;

        case 'w':
            return 12;

        case 'W':
            return 13;

        case 'c':
        case 'C':
            return 14;

        case 'z':
        case 'Z':
            return 15;

        case 'x':
        case 'X':
            return 16;

        case 'v':
        case 'V':
            return 17;

        case 'g':
        case 'G':
            return 18;

        default:
            return -1;
    }
}

STATIC INT32 IMEVowel(UINT8 Byte)
{
    switch (Byte)
    {
        case 'k':
        case 'K':
            return 0;

        case 'o':
            return 1;

        case 'i':
        case 'I':
            return 2;

        case 'O':
            return 3;

        case 'j':
        case 'J':
            return 4;

        case 'p':
            return 5;

        case 'u':
        case 'U':
            return 6;

        case 'P':
            return 7;

        case 'h':
        case 'H':
            return 8;

        case 'y':
        case 'Y':
            return 12;

        case 'n':
        case 'N':
            return 13;

        case 'b':
        case 'B':
            return 17;

        case 'm':
        case 'M':
            return 18;

        case 'l':
        case 'L':
            return 20;

        default:
            return -1;
    }
}

BOOLEAN IMEFeed(IME_STATE *State, UINT8 Byte, UINT32 *Committed)
{
    INT32 Lead = IMELead(Byte);
    INT32 Vowel = IMEVowel(Byte);
    INT32 Combined;

    *Committed = 0;

    if (Lead < 0 && Vowel < 0)
    {
        return FALSE;
    }

    if (Lead >= 0)
    {
        if (State->Lead >= 0 && State->Vowel >= 0 && LeadFinals[Lead] != 0)
        {
            if (State->Final == 0)
            {
                State->Final = LeadFinals[Lead];

                return TRUE;
            }

            Combined = IMECombine(Finals, sizeof(Finals) / sizeof(Finals[0]), State->Final, LeadFinals[Lead]);

            if (Combined >= 0)
            {
                State->Final = Combined;

                return TRUE;
            }
        }

        *Committed = IMECodepoint(State);
        IMEReset(State);
        State->Lead = Lead;

        return TRUE;
    }

    if (State->Vowel < 0)
    {
        State->Vowel = Vowel;

        return TRUE;
    }

    if (State->Final != 0)
    {
        INT32 NextFinal = State->Final;

        State->Final = 0;

        for (UINTN Index = 0; Index < sizeof(Finals) / sizeof(Finals[0]); Index++)
        {
            if (Finals[Index].Combined == NextFinal)
            {
                State->Final = Finals[Index].First;
                NextFinal = Finals[Index].Second;
                break;
            }
        }

        *Committed = IMECodepoint(State);
        State->Lead = FinalLeads[NextFinal];
        State->Vowel = Vowel;
        State->Final = 0;

        return TRUE;
    }

    Combined = IMECombine(Vowels, sizeof(Vowels) / sizeof(Vowels[0]), State->Vowel, Vowel);

    if (Combined >= 0)
    {
        State->Vowel = Combined;

        return TRUE;
    }

    *Committed = IMECodepoint(State);
    IMEReset(State);
    State->Vowel = Vowel;

    return TRUE;
}

VOID IMEBackspace(IME_STATE *State)
{
    if (State->Final != 0)
    {
        for (UINTN Index = 0; Index < sizeof(Finals) / sizeof(Finals[0]); Index++)
        {
            if (Finals[Index].Combined == State->Final)
            {
                State->Final = Finals[Index].First;

                return;
            }
        }

        State->Final = 0;

        return;
    }

    if (State->Vowel >= 0)
    {
        for (UINTN Index = 0; Index < sizeof(Vowels) / sizeof(Vowels[0]); Index++)
        {
            if (Vowels[Index].Combined == State->Vowel)
            {
                State->Vowel = Vowels[Index].First;

                return;
            }
        }

        State->Vowel = -1;

        return;
    }

    State->Lead = -1;
}
