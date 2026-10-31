// LOSLibC/Source/Time/strftime.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <time.h>
#include <locale.h>
#include <string.h>
#include <stdint.h>

STATIC CONST char *Weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
STATIC CONST char *FullWeekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
STATIC CONST char *KoreanWeekdays[] = {"일", "월", "화", "수", "목", "금", "토"};
STATIC CONST char *KoreanFullWeekdays[] = {"일요일", "월요일", "화요일", "수요일", "목요일", "금요일", "토요일"};
STATIC CONST char *Months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
STATIC CONST char *FullMonths[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
STATIC CONST char *KoreanMonths[] = {"1월", "2월", "3월", "4월", "5월", "6월", "7월", "8월", "9월", "10월", "11월", "12월"};

STATIC BOOLEAN TimeAppend(char *Buffer, size_t Capacity, size_t *Length, CONST char *Text)
{
    size_t Size = strlen(Text);

    if (*Length >= Capacity || Size >= Capacity - *Length)
    {
        return FALSE;
    }

    memcpy(Buffer + *Length, Text, Size);
    *Length += Size;
    Buffer[*Length] = 0;

    return TRUE;
}

STATIC VOID TimeNumber(char Buffer[64], INT64 Value, UINT32 Width, char Padding)
{
    char    Digits[32];
    UINT32  Count = 0;
    BOOLEAN Negative = Value < 0;
    UINT64  Absolute = Negative ? (UINT64) (-(Value + 1)) + 1 : (UINT64) Value;

    do
    {
        Digits[Count++] = '0' + Absolute % 10;
        Absolute /= 10;
    } while (Absolute != 0);

    UINT32 Length = 0;

    if (Negative && Padding == '0')
    {
        Buffer[Length++] = '-';
    }

    while (Length + Count + (Negative && Padding != '0' ? 1U : 0U) < Width)
    {
        Buffer[Length++] = Padding;
    }

    if (Negative && Padding != '0')
    {
        Buffer[Length++] = '-';
    }

    while (Count != 0)
    {
        Buffer[Length++] = Digits[--Count];
    }

    Buffer[Length] = 0;
}

size_t strftime(char *Buffer, size_t Capacity, const char *Format, const struct tm *Time)
{
    if (Buffer == NULL || Capacity == 0 || Format == NULL || Time == NULL)
    {
        return 0;
    }

    BOOLEAN Korean = strcmp(setlocale(LC_TIME, NULL), "ko_KR.UTF-8") == 0;
    size_t  Length = 0;

    Buffer[0] = 0;

    for (size_t Index = 0; Format[Index] != 0; Index++)
    {
        char        Text[256];
        CONST char *Value = Text;

        if (Format[Index] != '%')
        {
            Text[0] = Format[Index];
            Text[1] = 0;
        }
        else
        {
            Index++;

            char    Padding = '0';
            BOOLEAN Unpadded = FALSE;

            if (Format[Index] == '-' || Format[Index] == '_' || Format[Index] == '0')
            {
                Unpadded = Format[Index] == '-';
                Padding = Format[Index] == '_' ? ' ' : '0';
                Index++;
            }

            UINT32 Width = 0;

            while (Format[Index] >= '0' && Format[Index] <= '9')
            {
                Width = Width * 10 + (UINT32) (Format[Index++] - '0');

                if (Width > 32)
                {
                    return 0;
                }
            }

            if (Format[Index] == 'E' || Format[Index] == 'O')
            {
                Index++;
            }

            INT64       Number = 0;
            UINT32      DefaultWidth = 2;
            BOOLEAN     Numeric = TRUE;
            CONST char *Nested = NULL;

            switch (Format[Index])
            {
                case 'a':
                case 'A':
                {
                    if (Time->tm_wday < 0 || Time->tm_wday > 6)
                    {
                        return 0;
                    }

                    Value = Korean ? (Format[Index] == 'a' ? KoreanWeekdays[Time->tm_wday] : KoreanFullWeekdays[Time->tm_wday]) : (Format[Index] == 'a' ? Weekdays[Time->tm_wday] : FullWeekdays[Time->tm_wday]);
                    Numeric = FALSE;
                    break;
                }

                case 'b':
                case 'B':
                case 'h':
                {
                    if (Time->tm_mon < 0 || Time->tm_mon > 11)
                    {
                        return 0;
                    }

                    Value = Korean ? KoreanMonths[Time->tm_mon] : (Format[Index] == 'B' ? FullMonths[Time->tm_mon] : Months[Time->tm_mon]);
                    Numeric = FALSE;
                    break;
                }

                case 'Y':
                    Number = (INT64) Time->tm_year + 1900;
                    DefaultWidth = 4;
                    break;

                case 'y':
                    Number = ((INT64) Time->tm_year + 1900) % 100;
                    break;

                case 'C':
                    Number = ((INT64) Time->tm_year + 1900) / 100;
                    break;

                case 'm':
                    Number = Time->tm_mon + 1;
                    break;

                case 'd':
                    Number = Time->tm_mday;
                    break;

                case 'e':
                    Number = Time->tm_mday;
                    Padding = ' ';
                    break;

                case 'H':
                    Number = Time->tm_hour;
                    break;

                case 'I':
                    Number = Time->tm_hour % 12;

                    if (Number == 0)
                    {
                        Number = 12;
                    }

                    break;

                case 'M':
                    Number = Time->tm_min;
                    break;

                case 'S':
                    Number = Time->tm_sec;
                    break;

                case 'j':
                    Number = Time->tm_yday + 1;
                    DefaultWidth = 3;
                    break;

                case 'w':
                    Number = Time->tm_wday;
                    DefaultWidth = 1;
                    break;

                case 'u':
                    Number = Time->tm_wday == 0 ? 7 : Time->tm_wday;
                    DefaultWidth = 1;
                    break;

                case 'p':
                    Value = Korean ? (Time->tm_hour < 12 ? "오전" : "오후") : (Time->tm_hour < 12 ? "AM" : "PM");
                    Numeric = FALSE;
                    break;

                case 'Z':
                    Value = Time->tm_zone == NULL ? "" : Time->tm_zone;
                    Numeric = FALSE;
                    break;

                case 'z':
                {
                    INT64  Offset = Time->tm_gmtoff;
                    UINT64 Absolute = Offset < 0 ? (UINT64) (-(Offset + 1)) + 1 : (UINT64) Offset;

                    if (Absolute / 3600 > 99)
                    {
                        return 0;
                    }

                    Text[0] = Offset < 0 ? '-' : '+';
                    TimeNumber(Text + 1, (INT64) (Absolute / 3600), 2, '0');
                    TimeNumber(Text + 3, (INT64) (Absolute / 60 % 60), 2, '0');
                    Numeric = FALSE;
                    break;
                }

                case 'c':
                    Nested = Korean ? "%Y. %m. %d. (%a) %T" : "%a %b %e %T %Y";
                    break;

                case 'x':
                    Nested = Korean ? "%Y. %m. %d." : "%m/%d/%y";
                    break;

                case 'X':
                case 'T':
                    Nested = "%H:%M:%S";
                    break;

                case 'F':
                    Nested = "%Y-%m-%d";
                    break;

                case 'D':
                    Nested = "%m/%d/%y";
                    break;

                case 'R':
                    Nested = "%H:%M";
                    break;

                case 'r':
                    Nested = "%I:%M:%S %p";
                    break;

                case '%':
                    Value = "%";
                    Numeric = FALSE;
                    break;

                case 'n':
                    Value = "\n";
                    Numeric = FALSE;
                    break;

                case 't':
                    Value = "\t";
                    Numeric = FALSE;
                    break;

                default:
                    return 0;
            }

            if (Nested != NULL)
            {
                if (strftime(Text, sizeof(Text), Nested, Time) == 0)
                {
                    return 0;
                }
            }
            else if (Numeric)
            {
                TimeNumber(Text, Number, Unpadded ? 0 : (Width != 0 ? Width : DefaultWidth), Padding);
            }
        }

        if (!TimeAppend(Buffer, Capacity, &Length, Value))
        {
            Buffer[0] = 0;

            return 0;
        }
    }

    return Length;
}
