// UserPrograms/Shell/Commands/rif/rif.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLib/TypeInfo.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <LOS.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define RIF_BUFFER_SIZE      16384
#define RIF_MAX_PATTERN_SIZE 16
#define RIF_MAX_SECTION_NAME 1024
#define RIF_MAX_LITERAL_SIZE 4096
#define RIF_MAX_RO_COUNT     256

typedef struct
{
    UINT8 Data[RIF_MAX_LITERAL_SIZE];
    UINTN Length;
} RIF_RO_OBJECT;

typedef struct
{
    RIF_RO_OBJECT Objects[RIF_MAX_RO_COUNT];
    UINTN         Count;
} RIF_RO_TABLE;

typedef struct
{
    INT32   FD;
    BOOLEAN HasError;
    BOOLEAN HasEOF;
    UINTN   Position;
    UINTN   Length;
    UINT8   Buffer[RIF_BUFFER_SIZE];
} RIF_READER;

typedef struct
{
    INT32 FD;
    UINTN Length;
    UINT8 Buffer[RIF_BUFFER_SIZE];
} RIF_WRITER;

STATIC RIF_READER   RIFReader;
STATIC RIF_WRITER   RIFWriter;
STATIC RIF_RO_TABLE RIFROTable;
STATIC UINT8        RIFInput[RIF_BUFFER_SIZE];
STATIC UINT8        RIFRepeat[RIF_BUFFER_SIZE];

STATIC BOOLEAN DecodeROReference(RIF_READER *Reader, RIF_WRITER *Writer, CONST RIF_RO_TABLE *Table);

STATIC ssize_t ReadBytes(INT32 FD, UINT8 *Buffer, UINTN Length)
{
    ssize_t Count;

    do
    {
        Count = read(FD, Buffer, Length);
    } while (Count < 0 && errno == EINTR);

    return Count;
}

STATIC BOOLEAN RIFReaderReadByte(RIF_READER *Reader, UINT8 *Character)
{
    if (Reader->Position == Reader->Length)
    {
        if (Reader->HasEOF || Reader->HasError)
        {
            return FALSE;
        }

        ssize_t Count = ReadBytes(Reader->FD, Reader->Buffer, sizeof(Reader->Buffer));
        if (Count <= 0)
        {
            Reader->HasEOF   = Count == 0;
            Reader->HasError = Count < 0;
            return FALSE;
        }

        Reader->Position = 0;
        Reader->Length   = (UINTN) Count;
    }

    *Character = Reader->Buffer[Reader->Position++];
    return TRUE;
}

STATIC BOOLEAN RIFReaderUnreadByte(RIF_READER *Reader, UINT8 Character)
{
    if (Reader->Position == 0 || Reader->Buffer[Reader->Position - 1] != Character)
    {
        return FALSE;
    }

    Reader->Position--;
    return TRUE;
}

STATIC BOOLEAN ReadHeader(RIF_READER *Reader)
{
    CONST char Header[] = "RIF1\n";
    UINT8      Character;

    for (UINTN Index = 0; Index < sizeof(Header) - 1; Index++)
    {
        if (!RIFReaderReadByte(Reader, &Character) || Character != (UINT8) Header[Index])
        {
            return FALSE;
        }
    }

    return TRUE;
}

STATIC BOOLEAN ReadSection(RIF_READER *Reader, char *Name, UINTN Capacity)
{
    UINT8 Character;
    UINTN Position = 0;

    if (Reader == NULL || Name == NULL || Capacity < 2)
    {
        return FALSE;
    }

    do
    {
        if (!RIFReaderReadByte(Reader, &Character))
        {
            return FALSE;
        }
    } while (Character == '\n' || Character == '\r');

    if (Character != '[')
    {
        return FALSE;
    }

    for (;;)
    {
        if (!RIFReaderReadByte(Reader, &Character))
        {
            return FALSE;
        }

        if (Character == ']')
        {
            break;
        }

        if (Character == '\n' || Character == '\r' || Character == '\0' || Position + 1 >= Capacity)
        {
            return FALSE;
        }

        Name[Position++] = (char) Character;
    }

    if (Position == 0)
    {
        return FALSE;
    }

    Name[Position] = '\0';

    if (!RIFReaderReadByte(Reader, &Character))
    {
        return FALSE;
    }

    if (Character == '\r')
    {
        if (!RIFReaderReadByte(Reader, &Character))
        {
            return FALSE;
        }
    }

    return Character == '\n';
}

STATIC INT32 RIFHexValue(UINT8 Character)
{
    if (Character >= '0' && Character <= '9')
    {
        return Character - '0';
    }

    if (Character >= 'a' && Character <= 'f')
    {
        return Character - 'a' + 10;
    }

    if (Character >= 'A' && Character <= 'F')
    {
        return Character - 'A' + 10;
    }

    return -1;
}

STATIC INT32 ReadLiteral(RIF_READER *Reader, UINT8 *Buffer, UINTN Capacity, UINTN *Length)
{
    UINT8 Character;
    UINTN Position = 0;

    if (Reader == NULL || Buffer == NULL || Length == NULL || Capacity == 0)
    {
        return -1;
    }

    *Length = 0;

    if (!RIFReaderReadByte(Reader, &Character) || Character != '\'')
    {
        return -1;
    }

    for (;;)
    {
        if (!RIFReaderReadByte(Reader, &Character))
        {
            return -1;
        }

        if (Character == '\'')
        {
            *Length = Position;
            return 0;
        }

        if (Character == '\\')
        {
            if (!RIFReaderReadByte(Reader, &Character))
            {
                return -1;
            }

            switch (Character)
            {
                case 'n':
                    Character = '\n';
                    break;

                case 'r':
                    Character = '\r';
                    break;

                case 't':
                    Character = '\t';
                    break;

                case '\\':
                    Character = '\\';
                    break;

                case '\'':
                    Character = '\'';
                    break;

                case 'x':
                {
                    UINT8 High;
                    UINT8 Low;

                    if (!RIFReaderReadByte(Reader, &High) || !RIFReaderReadByte(Reader, &Low))
                    {
                        return -1;
                    }

                    INT32 HighValue = RIFHexValue(High);
                    INT32 LowValue  = RIFHexValue(Low);

                    if (HighValue < 0 || LowValue < 0)
                    {
                        return -1;
                    }

                    Character = (UINT8) ((HighValue << 4) | LowValue);
                    break;
                }

                default:
                    return -1;
            }
        }

        if (Position >= Capacity)
        {
            return -1;
        }

        Buffer[Position++] = Character;
    }
}

STATIC BOOLEAN ReadNumber(RIF_READER *Reader, UINT64 *Count, BOOLEAN Required)
{
    UINT8   Character;
    UINT64  Value     = 0;
    BOOLEAN HasDigits = FALSE;

    if (Reader == NULL || Count == NULL)
    {
        return FALSE;
    }

    while (RIFReaderReadByte(Reader, &Character))
    {
        if (Character < '0' || Character > '9')
        {
            if (!RIFReaderUnreadByte(Reader, Character))
            {
                return FALSE;
            }

            break;
        }

        HasDigits = TRUE;

        UINT64 Digit = (UINT64) (Character - '0');

        if (Value > (((UINT64) -1) - Digit) / 10)
        {
            return FALSE;
        }

        Value = Value * 10 + Digit;
    }

    if (Reader->HasError || (Required && !HasDigits))
    {
        return FALSE;
    }

    *Count = HasDigits ? Value : 1;

    return TRUE;
}

STATIC BOOLEAN WriteAll(INT32 FD, CONST UINT8 *Buffer, UINTN Length)
{
    UINTN Position = 0;

    while (Position < Length)
    {
        ssize_t Count = write(FD, Buffer + Position, Length - Position);

        if (Count < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return FALSE;
        }

        if (Count == 0)
        {
            return FALSE;
        }

        Position += (UINTN) Count;
    }

    return TRUE;
}

STATIC BOOLEAN FlushWriter(RIF_WRITER *Writer)
{
    if (!WriteAll(Writer->FD, Writer->Buffer, Writer->Length))
    {
        return FALSE;
    }

    Writer->Length = 0;
    return TRUE;
}

STATIC BOOLEAN WriteBytes(RIF_WRITER *Writer, CONST VOID *Data, UINTN Length)
{
    CONST UINT8 *Buffer = Data;

    while (Length != 0)
    {
        UINTN Space = sizeof(Writer->Buffer) - Writer->Length;
        UINTN Chunk = Length < Space ? Length : Space;
        memcpy(Writer->Buffer + Writer->Length, Buffer, Chunk);
        Writer->Length += Chunk;
        Buffer += Chunk;
        Length -= Chunk;

        if (Writer->Length == sizeof(Writer->Buffer) && !FlushWriter(Writer))
        {
            return FALSE;
        }
    }

    return TRUE;
}

STATIC BOOLEAN WriteRepeatedLiteral(RIF_WRITER *Writer, CONST UINT8 *Buffer, UINTN Length, UINT64 Count)
{
    if (Length == 0 || Count == 0)
    {
        return TRUE;
    }

    if (Count == 1)
    {
        return WriteBytes(Writer, Buffer, Length);
    }

    UINTN Batch = sizeof(RIFRepeat) / Length;
    if (Count < Batch)
    {
        Batch = (UINTN) Count;
    }

    UINTN BatchLength = Batch * Length;
    memcpy(RIFRepeat, Buffer, Length);
    for (UINTN Filled = Length; Filled < BatchLength;)
    {
        UINTN Remaining = BatchLength - Filled;
        UINTN Chunk     = Filled < Remaining ? Filled : Remaining;
        memcpy(RIFRepeat + Filled, RIFRepeat, Chunk);
        Filled += Chunk;
    }

    while (Count >= Batch)
    {
        if (!WriteBytes(Writer, RIFRepeat, BatchLength))
        {
            return FALSE;
        }
        Count -= Batch;
    }

    return WriteBytes(Writer, RIFRepeat, (UINTN) Count * Length);
}

STATIC BOOLEAN DecodeLiteral(RIF_READER *Reader, RIF_WRITER *Writer)
{
    UINT8  Buffer[RIF_MAX_LITERAL_SIZE];
    UINTN  Length = 0;
    UINT64 Count  = 1;

    if (Reader == NULL)
    {
        return FALSE;
    }

    if (ReadLiteral(Reader, Buffer, sizeof(Buffer), &Length) != 0)
    {
        return FALSE;
    }

    if (!ReadNumber(Reader, &Count, FALSE))
    {
        return FALSE;
    }

    if (!WriteRepeatedLiteral(Writer, Buffer, Length, Count))
    {
        return FALSE;
    }

    return TRUE;
}

STATIC BOOLEAN DecodeSection(RIF_READER *Reader, RIF_WRITER *Writer, CONST RIF_RO_TABLE *Table)
{
    UINT8 Character;

    if (Reader == NULL)
    {
        return FALSE;
    }

    for (;;)
    {
        if (!RIFReaderReadByte(Reader, &Character))
        {
            return !Reader->HasError;
        }

        switch (Character)
        {
            case '\'':
            {
                if (!RIFReaderUnreadByte(Reader, Character))
                {
                    return FALSE;
                }

                if (!DecodeLiteral(Reader, Writer))
                {
                    return FALSE;
                }

                break;
            }

            case '[':
            {
                return RIFReaderUnreadByte(Reader, Character);
            }

            case '\n':
            case '\r':
            {
                break;
            }

            case 'r':
            {
                if (!RIFReaderUnreadByte(Reader, Character))
                {
                    return FALSE;
                }

                if (!DecodeROReference(Reader, Writer, Table))
                {
                    return FALSE;
                }

                break;
            }

            default:
            {
                return FALSE;
            }
        }
    }
}

STATIC BOOLEAN RIFAddRO(RIF_RO_TABLE *Table, CONST UINT8 *Data, UINTN Length)
{
    if (Table == NULL || (Data == NULL && Length != 0))
    {
        return FALSE;
    }

    if (Table->Count >= RIF_MAX_RO_COUNT || Length > RIF_MAX_LITERAL_SIZE)
    {
        return FALSE;
    }

    RIF_RO_OBJECT *Object = &Table->Objects[Table->Count];

    if (Length != 0)
    {
        memcpy(Object->Data, Data, Length);
    }

    Object->Length = Length;
    Table->Count++;

    return TRUE;
}

STATIC CONST RIF_RO_OBJECT *RIFGetRO(CONST RIF_RO_TABLE *Table, UINT64 Index)
{
    if (Table == NULL || Index >= Table->Count)
    {
        return NULL;
    }

    return &Table->Objects[Index];
}

STATIC BOOLEAN ReadROSection(RIF_READER *Reader, RIF_RO_TABLE *Table)
{
    UINT8 Buffer[RIF_MAX_LITERAL_SIZE];
    UINT8 Character;
    UINTN Length;

    if (Reader == NULL || Table == NULL)
    {
        return FALSE;
    }

    for (;;)
    {
        if (!RIFReaderReadByte(Reader, &Character))
        {
            return !Reader->HasError;
        }

        if (Character == '[')
        {
            return RIFReaderUnreadByte(Reader, Character);
        }

        if (Character == '\n' || Character == '\r')
        {
            continue;
        }

        if (Character != '\'')
        {
            return FALSE;
        }

        if (!RIFReaderUnreadByte(Reader, Character))
        {
            return FALSE;
        }

        if (ReadLiteral(Reader, Buffer, sizeof(Buffer), &Length) != 0)
        {
            return FALSE;
        }

        if (!RIFAddRO(Table, Buffer, Length))
        {
            return FALSE;
        }

        if (!RIFReaderReadByte(Reader, &Character))
        {
            return !Reader->HasError;
        }

        if (Character == '\r')
        {
            if (!RIFReaderReadByte(Reader, &Character))
            {
                return FALSE;
            }
        }

        if (Character != '\n')
        {
            return FALSE;
        }
    }
}

STATIC BOOLEAN DecodeROReference(RIF_READER *Reader, RIF_WRITER *Writer, CONST RIF_RO_TABLE *Table)
{
    UINT8  Character;
    UINT64 Index;
    UINT64 Count = 1;

    if (Reader == NULL || Table == NULL)
    {
        return FALSE;
    }

    if (!RIFReaderReadByte(Reader, &Character) || Character != 'r')
    {
        return FALSE;
    }

    if (!ReadNumber(Reader, &Index, TRUE))
    {
        return FALSE;
    }

    CONST RIF_RO_OBJECT *Object = RIFGetRO(Table, Index);

    if (Object == NULL)
    {
        return FALSE;
    }

    if (RIFReaderReadByte(Reader, &Character))
    {
        if (Character == 'l')
        {
            if (!ReadNumber(Reader, &Count, TRUE))
            {
                return FALSE;
            }
        }
        else
        {
            if (!RIFReaderUnreadByte(Reader, Character))
            {
                return FALSE;
            }
        }
    }
    else if (Reader->HasError)
    {
        return FALSE;
    }

    return WriteRepeatedLiteral(Writer, Object->Data, Object->Length, Count);
}

STATIC BOOLEAN IsFileName(CONST char *Name)
{
    if (Name[0] == '\0' || strcmp(Name, ".") == 0 || strcmp(Name, "..") == 0 || strcmp(Name, "RO") == 0)
    {
        return FALSE;
    }

    UINTN Length = 0;
    for (; Name[Length] != '\0'; Length++)
    {
        UINT8 Character = (UINT8) Name[Length];
        if (Character < 32 || Character == 127 || Character == '/' || Character == '\\' || Character == '[' || Character == ']')
        {
            return FALSE;
        }
    }

    return Length < RIF_MAX_SECTION_NAME;
}

STATIC BOOLEAN DecodeArchive(RIF_READER *Reader)
{
    char    Name[RIF_MAX_SECTION_NAME];
    UINT8   Character;
    BOOLEAN HasRO   = FALSE;
    BOOLEAN HasFile = FALSE;

    if (!ReadHeader(Reader))
    {
        return FALSE;
    }

    while (RIFReaderReadByte(Reader, &Character))
    {
        if (Character == '\r' || Character == '\n')
        {
            continue;
        }

        if (!RIFReaderUnreadByte(Reader, Character) || !ReadSection(Reader, Name, sizeof(Name)))
        {
            return FALSE;
        }

        if (strcmp(Name, "RO") == 0)
        {
            if (HasRO || HasFile || !ReadROSection(Reader, &RIFROTable))
            {
                return FALSE;
            }
            HasRO = TRUE;
            continue;
        }

        if (!IsFileName(Name))
        {
            return FALSE;
        }

        INT32 FD = open(Name, O_WRONLY | O_CREAT | O_EXCL, 0666);
        if (FD < 0)
        {
            perror(Name);
            return FALSE;
        }

        HasFile          = TRUE;
        RIFWriter.FD     = FD;
        RIFWriter.Length = 0;
        BOOLEAN Success  = DecodeSection(Reader, &RIFWriter, &RIFROTable) && FlushWriter(&RIFWriter);

        if (close(FD) < 0)
        {
            Success = FALSE;
        }
        if (!Success)
        {
            if (LOSRemove(Name) < 0)
            {
                perror(Name);
            }
            printf("rif: failed to extract %s\n", Name);
            return FALSE;
        }
    }

    return HasFile && !Reader->HasError;
}

STATIC UINTN EscapeByte(UINT8 Character, char *Encoded)
{
    CONST char Hex[] = "0123456789ABCDEF";

    Encoded[0] = '\\';
    switch (Character)
    {
        case '\n':
            Encoded[1] = 'n';
            return 2;
        case '\r':
            Encoded[1] = 'r';
            return 2;
        case '\t':
            Encoded[1] = 't';
            return 2;
        case '\\':
        case '\'':
            Encoded[1] = (char) Character;
            return 2;
    }

    if (Character >= 32 && Character < 127)
    {
        Encoded[0] = (char) Character;
        return 1;
    }

    Encoded[1] = 'x';
    Encoded[2] = Hex[Character >> 4];
    Encoded[3] = Hex[Character & 15];
    return 4;
}

STATIC BOOLEAN EncodeLiteral(RIF_WRITER *Writer, CONST UINT8 *Data, UINTN Length, UINTN Count)
{
    char  Encoded[4];
    char  Digits[20];
    UINTN Position = sizeof(Digits);

    if (!WriteBytes(Writer, "'", 1))
    {
        return FALSE;
    }

    for (UINTN Index = 0; Index < Length; Index++)
    {
        UINTN Width = EscapeByte(Data[Index], Encoded);
        if (!WriteBytes(Writer, Encoded, Width))
        {
            return FALSE;
        }
    }

    if (Count > 1)
    {
        do
        {
            Digits[--Position] = (char) ('0' + Count % 10);
            Count /= 10;
        } while (Count != 0);
    }

    return WriteBytes(Writer, "'", 1) && WriteBytes(Writer, Digits + Position, sizeof(Digits) - Position);
}

STATIC BOOLEAN EncodeBlock(RIF_WRITER *Writer, CONST UINT8 *Data, UINTN Length)
{
    UINTN Literal  = 0;
    UINTN Position = 0;

    while (Position < Length)
    {
        UINTN Pattern = 1;
        UINTN Count   = 1;

        for (; Pattern <= RIF_MAX_PATTERN_SIZE && Pattern * 2 <= Length - Position; Pattern++)
        {
            if (Data[Position] != Data[Position + Pattern] || memcmp(Data + Position, Data + Position + Pattern, Pattern) != 0)
            {
                continue;
            }

            UINTN Width = 0;
            char  Encoded[4];
            for (UINTN Index = 0; Index < Pattern; Index++)
            {
                Width += EscapeByte(Data[Position + Index], Encoded);
            }

            Count = 2;
            while (Pattern <= Length - Position - Pattern * Count && memcmp(Data + Position, Data + Position + Pattern * Count, Pattern) == 0)
            {
                Count++;
            }

            UINTN Digits = 1;
            for (UINTN Value = Count; Value >= 10; Value /= 10)
            {
                Digits++;
            }

            if (Width * (Count - 1) > Digits + 4)
            {
                break;
            }
            Count = 1;
        }

        if (Count > 1)
        {
            if ((Position != Literal && !EncodeLiteral(Writer, Data + Literal, Position - Literal, 1)) || !EncodeLiteral(Writer, Data + Position, Pattern, Count))
            {
                return FALSE;
            }
            Position += Pattern * Count;
            Literal = Position;
        }
        else
        {
            Position++;
            if (Position - Literal == RIF_MAX_LITERAL_SIZE)
            {
                if (!EncodeLiteral(Writer, Data + Literal, Position - Literal, 1))
                {
                    return FALSE;
                }
                Literal = Position;
            }
        }
    }

    return Position == Literal || EncodeLiteral(Writer, Data + Literal, Position - Literal, 1);
}

STATIC CONST char *FileName(CONST char *Path)
{
    CONST char *Name = Path;
    for (; *Path != '\0'; Path++)
    {
        if (*Path == '/')
        {
            Name = Path + 1;
        }
    }

    return Name;
}

STATIC BOOLEAN EncodeFile(RIF_WRITER *Writer, CONST char *Path)
{
    CONST char *Name = FileName(Path);
    INT32       FD   = open(Path, O_RDONLY);

    if (FD < 0)
    {
        perror(Path);
        return FALSE;
    }

    BOOLEAN Success = WriteBytes(Writer, "[", 1) && WriteBytes(Writer, Name, strlen(Name)) && WriteBytes(Writer, "]\n", 2);
    while (Success)
    {
        UINTN Length = 0;
        while (Length < sizeof(RIFInput))
        {
            ssize_t Count = ReadBytes(FD, RIFInput + Length, sizeof(RIFInput) - Length);
            if (Count < 0)
            {
                perror(Path);
                Success = FALSE;
                break;
            }
            if (Count == 0)
            {
                break;
            }
            Length += (UINTN) Count;
        }

        if (!Success || Length == 0)
        {
            break;
        }
        Success = EncodeBlock(Writer, RIFInput, Length);
        if (Length < sizeof(RIFInput))
        {
            break;
        }
    }

    if (close(FD) < 0)
    {
        perror(Path);
        return FALSE;
    }

    return Success && WriteBytes(Writer, "\n", 1);
}

STATIC BOOLEAN EncodeArchive(CONST char *Archive, INT32 FileCount, char **Paths)
{
    for (INT32 Index = 0; Index < FileCount; Index++)
    {
        CONST char *Name = FileName(Paths[Index]);
        if (!IsFileName(Name))
        {
            printf("rif: invalid file name %s\n", Paths[Index]);
            return FALSE;
        }

        struct stat Info;
        if (stat(Paths[Index], &Info) < 0)
        {
            perror(Paths[Index]);
            return FALSE;
        }
        if (!S_ISREG(Info.st_mode))
        {
            printf("rif: expected a regular file: %s\n", Paths[Index]);
            return FALSE;
        }

        for (INT32 Previous = 0; Previous < Index; Previous++)
        {
            if (strcmp(Name, FileName(Paths[Previous])) == 0)
            {
                printf("rif: duplicate file name %s\n", Name);
                return FALSE;
            }
        }
    }

    INT32 FD = open(Archive, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (FD < 0)
    {
        perror(Archive);
        return FALSE;
    }

    RIFWriter.FD    = FD;
    BOOLEAN Success = WriteBytes(&RIFWriter, "RIF1\n", 5);

    for (INT32 Index = 0; Success && Index < FileCount; Index++)
    {
        Success = EncodeFile(&RIFWriter, Paths[Index]);
    }

    Success = Success && FlushWriter(&RIFWriter);
    if (close(FD) < 0)
    {
        Success = FALSE;
    }
    if (!Success && LOSRemove(Archive) < 0)
    {
        perror(Archive);
    }

    return Success;
}

INT32 main(INT32 Argc, char **Argv)
{
    if (Argc >= 4 && strcmp(Argv[1], "-c") == 0)
    {
        if (EncodeArchive(Argv[2], Argc - 3, Argv + 3))
        {
            return 0;
        }

        printf("rif: failed to create %s\n", Argv[2]);
        return 1;
    }

    if (Argc == 3 && (strcmp(Argv[1], "-d") == 0 || strcmp(Argv[1], "x") == 0))
    {
        INT32 FD = open(Argv[2], O_RDONLY);
        if (FD < 0)
        {
            perror(Argv[2]);
            return 1;
        }

        RIFReader.FD    = FD;
        BOOLEAN Success = DecodeArchive(&RIFReader);
        if (close(FD) < 0)
        {
            Success = FALSE;
        }
        if (Success)
        {
            return 0;
        }

        printf("rif: failed to extract %s\n", Argv[2]);
        return 1;
    }

    puts("Usage: rif -c ARCHIVE FILE...\n       rif -d ARCHIVE");
    return 2;
}
