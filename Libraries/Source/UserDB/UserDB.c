// Libraries/Source/UserDB/UserDB.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <libintl.h>

#include <UserDB/UserDB.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

#define USER_DB_CAPACITY  65536U
#define USER_DB_MAX_USERS 128U
#define USER_DB_LINE_SIZE 512U
#define USER_DB_PATH      "/etc/userdb"
#define USER_DB_TEMP      "/etc/userdb.tmp"
#define USER_DB_LOCK      "/etc/userdb.lock"

typedef struct
{
    UINTN  Start;
    UINTN  End;
    UINT32 UID;
    UINT32 GID;
    char   Name[USER_DB_NAME_MAX + 1];
} USER_DB_RECORD;

STATIC char           Database[USER_DB_CAPACITY + 1];
STATIC char           Replacement[USER_DB_CAPACITY + 1];
STATIC USER_DB_RECORD Records[USER_DB_MAX_USERS];
STATIC UINTN          DatabaseLength;
STATIC UINT32         RecordCount;

BOOLEAN UserDBValidateName(CONST char *Username)
{
    UINTN Length = 0;

    if (Username == NULL || (Username[0] >= '0' && Username[0] <= '9'))
    {
        return FALSE;
    }

    while (Username[Length] != 0)
    {
        char Character = Username[Length];

        if (Length == USER_DB_NAME_MAX || !((Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z') || (Character >= '0' && Character <= '9') || Character == '_' || Character == '-'))
        {
            return FALSE;
        }

        Length++;
    }

    return Length != 0;
}

STATIC BOOLEAN UserDBNumber(CONST char *Text, UINT32 *Number)
{
    UINT32 Value = 0;

    if (*Text == 0)
    {
        return FALSE;
    }

    while (*Text != 0)
    {
        if (*Text < '0' || *Text > '9' || Value > (0xFFFFFFFFU - (UINT32) (*Text - '0')) / 10U)
        {
            return FALSE;
        }

        Value = Value * 10U + (UINT32) (*Text++ - '0');
    }

    *Number = Value;

    return TRUE;
}

STATIC BOOLEAN UserDBParse(VOID)
{
    UINTN   Position = 0;
    BOOLEAN RootFound = FALSE;

    RecordCount = 0;

    while (Position < DatabaseLength)
    {
        char   Line[USER_DB_LINE_SIZE];
        char  *Fields[5];
        UINT32 FieldCount = 1;
        UINTN  Start = Position;
        UINTN  Length = 0;

        if (RecordCount == USER_DB_MAX_USERS)
        {
            return FALSE;
        }

        while (Position < DatabaseLength && Database[Position] != '\n')
        {
            UINT8 Byte = (UINT8) Database[Position++];

            if (Byte < 32 || Byte > 126 || Length + 1 >= sizeof(Line))
            {
                return FALSE;
            }

            Line[Length++] = (char) Byte;
        }

        if (Position < DatabaseLength)
        {
            Position++;
        }

        if (Length == 0)
        {
            return FALSE;
        }

        Line[Length] = 0;
        Fields[0] = Line;

        for (UINTN Index = 0; Index < Length; Index++)
        {
            if (Line[Index] == ':')
            {
                if (FieldCount == 5)
                {
                    return FALSE;
                }

                Line[Index] = 0;
                Fields[FieldCount++] = Line + Index + 1;
            }
        }

        USER_DB_RECORD *Record = &Records[RecordCount];

        if (FieldCount != 5 || !UserDBValidateName(Fields[0]) || !UserDBNumber(Fields[1], &Record->UID) || !UserDBNumber(Fields[2], &Record->GID) || !UserDBValidatePasswordFields(Fields[3], Fields[4]))
        {
            return FALSE;
        }

        BOOLEAN Root = strcmp(Fields[0], "root") == 0;

        if ((Root && (Record->UID != 0 || Record->GID != 0 || RootFound)) || (!Root && Record->UID == 0))
        {
            return FALSE;
        }

        RootFound = RootFound || Root;

        for (UINT32 Index = 0; Index < RecordCount; Index++)
        {
            if (strcmp(Records[Index].Name, Fields[0]) == 0 || Records[Index].UID == Record->UID)
            {
                return FALSE;
            }
        }

        strcpy(Record->Name, Fields[0]);
        Record->Start = Start;
        Record->End = Position;
        RecordCount++;
    }

    return RecordCount != 0 && RootFound;
}

STATIC USER_DB_RESULT UserDBLoad(VOID)
{
    INT32 FD = open(USER_DB_PATH, O_RDONLY);

    DatabaseLength = 0;
    RecordCount = 0;

    if (FD < 0)
    {
        return USER_DB_UNAVAILABLE;
    }

    USER_DB_RESULT Result = USER_DB_OK;

    for (;;)
    {
        ssize_t Length = read(FD, Database + DatabaseLength, sizeof(Database) - DatabaseLength);

        if (Length < 0 || (UINTN) Length > sizeof(Database) - DatabaseLength)
        {
            Result = USER_DB_IO;
            break;
        }

        if (Length == 0)
        {
            break;
        }

        DatabaseLength += (UINTN) Length;

        if (DatabaseLength > USER_DB_CAPACITY)
        {
            Result = USER_DB_LIMIT;
            break;
        }
    }

    if (close(FD) != 0)
    {
        Result = USER_DB_IO;
    }

    if (Result == USER_DB_OK && !UserDBParse())
    {
        Result = USER_DB_INVALID;
    }

    return Result;
}

STATIC BOOLEAN UserDBWriteAll(INT32 FD, CONST char *Text, UINTN Length)
{
    UINTN Offset = 0;

    while (Offset < Length)
    {
        ssize_t Written = write(FD, Text + Offset, Length - Offset);

        if (Written <= 0 || (UINTN) Written > Length - Offset)
        {
            return FALSE;
        }

        Offset += (UINTN) Written;
    }

    return TRUE;
}

STATIC BOOLEAN UserDBPublishNames(CONST char *Text, UINTN Length)
{
    int FD = open("/etc/passwd.tmp", O_WRONLY | O_CREAT | O_EXCL, 0644U);

    if (FD < 0)
    {
        return FALSE;
    }

    BOOLEAN Result = TRUE;
    UINTN   Position = 0;

    while (Position < Length && Result)
    {
        UINTN Start = Position;
        UINTN Fields[3];
        UINTN Count = 0;

        while (Position < Length && Text[Position] != '\n')
        {
            if (Text[Position] == ':' && Count < 3)
            {
                Fields[Count++] = Position;
            }

            Position++;
        }

        if (Count != 3)
        {
            Result = FALSE;
            break;
        }

        Result = UserDBWriteAll(FD, Text + Start, Fields[0] - Start) && UserDBWriteAll(FD, ":x:", 3) && UserDBWriteAll(FD, Text + Fields[0] + 1, Fields[2] - Fields[0] - 1) && UserDBWriteAll(FD, "::/home/", 8) && UserDBWriteAll(FD, Text + Start, Fields[0] - Start) && UserDBWriteAll(FD, ":/bin/lsh.lix\n", sizeof(":/bin/lsh.lix\n") - 1);

        if (Position < Length)
        {
            Position++;
        }
    }

    if (close(FD) != 0)
    {
        Result = FALSE;
    }

    if (Result && rename("/etc/passwd.tmp", "/etc/passwd") == 0)
    {
        return TRUE;
    }

    remove("/etc/passwd.tmp");

    return FALSE;
}

STATIC USER_DB_RESULT UserDBSave(UINTN Length, BOOLEAN Initial)
{
    CONST char *Path = Initial ? USER_DB_PATH : USER_DB_TEMP;
    INT32       FD = open(Path, O_WRONLY | O_CREAT | O_EXCL, 0600U);

    if (FD < 0)
    {
        return USER_DB_IO;
    }

    BOOLEAN Written = UserDBWriteAll(FD, Replacement, Length);
    INT32   Closed = close(FD);

    if (!Written || Closed != 0)
    {
        if (remove(Path) != 0)
        {
            return USER_DB_CLEANUP;
        }

        return USER_DB_IO;
    }

    if (!Initial && rename(USER_DB_TEMP, USER_DB_PATH) != 0)
    {
        return remove(USER_DB_TEMP) == 0 ? USER_DB_IO : USER_DB_CLEANUP;
    }

    return UserDBPublishNames(Replacement, Length) ? USER_DB_OK : USER_DB_COMMITTED_CLEANUP;
}

STATIC UINTN UserDBDecimal(char *Text, UINT32 Value)
{
    char  Digits[10];
    UINTN Count = 0;

    do
    {
        Digits[Count++] = (char) ('0' + Value % 10);
        Value /= 10;
    } while (Value != 0);

    for (UINTN Index = 0; Index < Count; Index++)
    {
        Text[Index] = Digits[Count - Index - 1];
    }

    return Count;
}

STATIC USER_DB_RESULT UserDBUnlock(INT32 LockFD, USER_DB_RESULT Result)
{
    INT32 Closed = close(LockFD);
    INT32 Removed = remove(USER_DB_LOCK);

    if (Closed != 0 || Removed != 0)
    {
        return Result == USER_DB_OK ? USER_DB_COMMITTED_CLEANUP : USER_DB_CLEANUP;
    }

    return Result;
}

USER_DB_RESULT UserDBAdd(CONST char *Username, CONST char *Salt, CONST char *Hash)
{
    if (!UserDBValidateName(Username))
    {
        return USER_DB_INVALID_NAME;
    }

    if (!UserDBValidatePasswordFields(Salt, Hash))
    {
        return USER_DB_INVALID;
    }

    if (strcmp(Username, "root") == 0)
    {
        mkdir("/etc", 0755U);
    }

    INT32 LockFD = open(USER_DB_LOCK, O_WRONLY | O_CREAT | O_EXCL, 0600U);

    if (LockFD < 0)
    {
        return USER_DB_LOCKED;
    }

    USER_DB_RESULT Result = UserDBLoad();
    BOOLEAN        Initial = Result == USER_DB_UNAVAILABLE;

    if (Initial && strcmp(Username, "root") != 0)
    {
        return UserDBUnlock(LockFD, USER_DB_ROOT_REQUIRED);
    }

    if (!Initial && Result != USER_DB_OK)
    {
        return UserDBUnlock(LockFD, Result);
    }

    UINT32 UID = Initial ? 0 : 1000;

    if (!Initial)
    {
        for (UINT32 Index = 0; Index < RecordCount; Index++)
        {
            if (strcmp(Records[Index].Name, Username) == 0)
            {
                return UserDBUnlock(LockFD, USER_DB_EXISTS);
            }
        }

        if (strcmp(Username, "root") == 0)
        {
            return UserDBUnlock(LockFD, USER_DB_ROOT_PROTECTED);
        }

        if (RecordCount == USER_DB_MAX_USERS)
        {
            return UserDBUnlock(LockFD, USER_DB_LIMIT);
        }

        for (;; UID++)
        {
            BOOLEAN Used = FALSE;

            for (UINT32 Index = 0; Index < RecordCount; Index++)
            {
                Used = Used || Records[Index].UID == UID || Records[Index].GID == UID;
            }

            if (!Used)
            {
                break;
            }
        }
    }

    UINTN Length = Initial ? 0 : DatabaseLength;

    memcpy(Replacement, Database, Length);

    if (Length != 0 && Replacement[Length - 1] != '\n')
    {
        Replacement[Length++] = '\n';
    }

    UINTN Required = strlen(Username) + strlen(Salt) + strlen(Hash) + 25;

    if (Length > USER_DB_CAPACITY || Required > USER_DB_CAPACITY - Length)
    {
        return UserDBUnlock(LockFD, USER_DB_LIMIT);
    }

    strcpy(Replacement + Length, Username);
    Length += strlen(Username);
    Replacement[Length++] = ':';
    Length += UserDBDecimal(Replacement + Length, UID);
    Replacement[Length++] = ':';
    Length += UserDBDecimal(Replacement + Length, UID);
    Replacement[Length++] = ':';
    strcpy(Replacement + Length, Salt);
    Length += strlen(Salt);
    Replacement[Length++] = ':';
    strcpy(Replacement + Length, Hash);
    Length += strlen(Hash);
    Replacement[Length++] = '\n';
    Result = UserDBSave(Length, Initial);

    return UserDBUnlock(LockFD, Result);
}

USER_DB_RESULT UserDBDelete(CONST char *Username)
{
    if (!UserDBValidateName(Username))
    {
        return USER_DB_INVALID_NAME;
    }

    if (strcmp(Username, "root") == 0)
    {
        return USER_DB_ROOT_PROTECTED;
    }

    INT32 LockFD = open(USER_DB_LOCK, O_WRONLY | O_CREAT | O_EXCL, 0600U);

    if (LockFD < 0)
    {
        return USER_DB_LOCKED;
    }

    USER_DB_RESULT Result = UserDBLoad();

    if (Result != USER_DB_OK)
    {
        return UserDBUnlock(LockFD, Result);
    }

    for (UINT32 Index = 0; Index < RecordCount; Index++)
    {
        if (strcmp(Records[Index].Name, Username) == 0)
        {
            UINTN Start = Records[Index].Start;
            UINTN End = Records[Index].End;

            memcpy(Replacement, Database, Start);
            memcpy(Replacement + Start, Database + End, DatabaseLength - End);
            Result = UserDBSave(Start + DatabaseLength - End, FALSE);

            return UserDBUnlock(LockFD, Result);
        }
    }

    return UserDBUnlock(LockFD, USER_DB_NOT_FOUND);
}

CONST char *UserDBResultText(USER_DB_RESULT Result)
{
    switch (Result)
    {
        case USER_DB_OK:
            return gettext("account database updated");

        case USER_DB_INVALID_NAME:
            return gettext("invalid username");

        case USER_DB_UNAVAILABLE:
            return gettext("cannot open account database");

        case USER_DB_INVALID:
            return gettext("invalid account database or password encoding; no recovery overwrite performed");

        case USER_DB_EXISTS:
            return gettext("user already exists");

        case USER_DB_NOT_FOUND:
            return gettext("user not found");

        case USER_DB_ROOT_REQUIRED:
            return gettext("initialize root first");

        case USER_DB_ROOT_PROTECTED:
            return gettext("root account is protected");

        case USER_DB_LIMIT:
            return gettext("account database limit reached");

        case USER_DB_LOCKED:
            return gettext("cannot acquire account database lock");

        case USER_DB_IO:
            return gettext("account database I/O failed");

        case USER_DB_CLEANUP:
            return gettext("update failed; lock or temporary file cleanup may be required");

        case USER_DB_COMMITTED_CLEANUP:
            return gettext("account updated; lock cleanup required; do not repeat the update");

        default:
            return gettext("unknown account database error");
    }
}

STATIC VOID UserDBClearSnapshot(VOID)
{
    for (UINTN Index = 0; Index < sizeof(Database); Index++)
    {
        ((VOLATILE char *) Database)[Index] = 0;
    }

    for (UINTN Index = 0; Index < sizeof(Records); Index++)
    {
        ((VOLATILE UINT8 *) Records)[Index] = 0;
    }

    DatabaseLength = 0;
    RecordCount = 0;
}

USER_DB_RESULT UserDBCheck(VOID)
{
    USER_DB_RESULT Result = UserDBLoad();

    if (Result == USER_DB_OK && !UserDBPublishNames(Database, DatabaseLength))
    {
        Result = USER_DB_IO;
    }

    UserDBClearSnapshot();

    return Result;
}

STATIC VOID UserDBCopyAccount(CONST USER_DB_RECORD *Record, USER_DB_ACCOUNT *Account)
{
    UINTN  Position = Record->Start;
    UINT32 Separators = 0;

    while (Position < Record->End && Separators < 3)
    {
        if (Database[Position++] == ':')
        {
            Separators++;
        }
    }

    memcpy(Account->Salt, Database + Position, USER_DB_SALT_TEXT_SIZE - 1);
    Account->Salt[USER_DB_SALT_TEXT_SIZE - 1] = 0;
    Position += USER_DB_SALT_TEXT_SIZE;

    UINTN Length = Record->End - Position;

    if (Length != 0 && Database[Record->End - 1] == '\n')
    {
        Length--;
    }

    memcpy(Account->Hash, Database + Position, Length);
    Account->Hash[Length] = 0;
    Account->UID = Record->UID;
    Account->GID = Record->GID;
}

USER_DB_RESULT UserDBLookup(CONST char *Username, USER_DB_ACCOUNT *Account)
{
    if (Account == NULL)
    {
        return USER_DB_INVALID;
    }

    memset(Account, 0, sizeof(*Account));

    if (!UserDBValidateName(Username))
    {
        return USER_DB_INVALID_NAME;
    }

    USER_DB_RESULT Result = UserDBLoad();

    if (Result == USER_DB_OK)
    {
        Result = USER_DB_NOT_FOUND;

        for (UINT32 Index = 0; Index < RecordCount; Index++)
        {
            USER_DB_RECORD *Record = &Records[Index];

            if (strcmp(Record->Name, Username) != 0)
            {
                continue;
            }

            UserDBCopyAccount(Record, Account);
            Result = USER_DB_OK;
            break;
        }
    }

    UserDBClearSnapshot();

    return Result;
}

USER_DB_RESULT UserDBLookupUID(UINT32 UID, char Username[USER_DB_NAME_MAX + 1], USER_DB_ACCOUNT *Account)
{
    if (Username == NULL || Account == NULL)
    {
        return USER_DB_INVALID;
    }

    Username[0] = 0;
    memset(Account, 0, sizeof(*Account));

    USER_DB_RESULT Result = UserDBLoad();

    if (Result == USER_DB_OK)
    {
        Result = USER_DB_NOT_FOUND;

        for (UINT32 Index = 0; Index < RecordCount; Index++)
        {
            if (Records[Index].UID == UID)
            {
                strcpy(Username, Records[Index].Name);
                UserDBCopyAccount(&Records[Index], Account);
                Result = USER_DB_OK;
                break;
            }
        }
    }

    UserDBClearSnapshot();

    return Result;
}
