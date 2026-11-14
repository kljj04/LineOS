// LOSLibC/Source/IO/stream.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

struct FILE
{
    int          FD;
    unsigned int End;
    unsigned int Error;
    unsigned int Allocated;
    char *Memory;
    char **Published;
    size_t *PublishedSize;
    size_t Position;
    size_t Used;
    size_t Capacity;
};

static FILE StandardStreams[] = {{.FD = 0}, {.FD = 1}, {.FD = 2}};

FILE *stdin = &StandardStreams[0];
FILE *stdout = &StandardStreams[1];
FILE *stderr = &StandardStreams[2];

FILE *fopen(const char *Path, const char *Mode)
{
    int Flags;

    switch (*Mode)
    {
        case 'r':
            Flags = O_RDONLY;
            break;
        case 'w':
            Flags = O_WRONLY | O_CREAT | O_TRUNC;
            break;
        case 'a':
            Flags = O_WRONLY | O_CREAT | O_APPEND;
            break;
        default:
            errno = EINVAL;

            return NULL;
    }

    unsigned int Binary = 0;
    unsigned int Update = 0;
    unsigned int Exclusive = 0;

    for (const char *Option = Mode + 1; *Option != 0; Option++)
    {
        if (*Option == 'b' && !Binary)
        {
            Binary = 1;
        }
        else if (*Option == '+' && !Update)
        {
            Update = 1;
            Flags = (Flags & ~O_ACCMODE) | O_RDWR;
        }
        else if (*Option == 'x' && *Mode == 'w' && !Exclusive)
        {
            Exclusive = 1;
            Flags |= O_EXCL;
        }
        else
        {
            errno = EINVAL;

            return NULL;
        }
    }

    FILE *Stream = malloc(sizeof(*Stream));

    if (Stream == NULL)
    {
        return NULL;
    }

    int FD = open(Path, Flags, 0666);

    if (FD < 0)
    {
        free(Stream);

        return NULL;
    }

    *Stream = (FILE) {.FD = FD, .Allocated = 1};

    return Stream;
}

FILE *open_memstream(char **Buffer, size_t *Size)
{
    FILE *Stream = calloc(1, sizeof(*Stream));

    if (Stream == NULL)
    {
        return NULL;
    }

    Stream->Memory = calloc(1, 1);

    if (Stream->Memory == NULL)
    {
        free(Stream);

        return NULL;
    }

    Stream->Published = Buffer;
    Stream->PublishedSize = Size;
    Stream->Capacity = 1;
    Stream->Allocated = 1;

    return Stream;
}

int fclose(FILE *Stream)
{
    int Result = Stream->Published != NULL ? fflush(Stream) : close(Stream->FD);

    if (Stream->Allocated)
    {
        free(Stream);
    }
    else
    {
        Stream->FD = -1;
    }

    return Result < 0 ? EOF : 0;
}

static size_t StreamTransfer(void *Output, const void *Input, size_t Size, size_t Count, FILE *Stream, unsigned int Write)
{
    if (Size == 0 || Count == 0)
    {
        return 0;
    }

    if (Count > (size_t) -1 / Size)
    {
        errno = EOVERFLOW;
        Stream->Error = 1;

        return 0;
    }

    size_t Total = Count * Size;
    if (Stream->Published != NULL)
    {
        if (!Write || Total > (size_t) LONG_MAX - Stream->Position)
        {
            errno = Write ? EOVERFLOW : EBADF;
            Stream->Error = 1;

            return 0;
        }

        size_t End = Stream->Position + Total;

        if (End >= Stream->Capacity)
        {
            size_t Capacity = Stream->Capacity * 2;

            if (Capacity <= End)
            {
                Capacity = End + 1;
            }

            char *Memory = realloc(Stream->Memory, Capacity);

            if (Memory == NULL)
            {
                Stream->Error = 1;

                return 0;
            }

            Stream->Capacity = Capacity;
            Stream->Memory = Memory;
        }

        if (Stream->Position > Stream->Used)
        {
            memset(Stream->Memory + Stream->Used, 0, Stream->Position - Stream->Used);
        }

        memcpy(Stream->Memory + Stream->Position, Input, Total);
        Stream->Position = End;

        if (End > Stream->Used)
        {
            Stream->Used = End;
        }

        Stream->Memory[Stream->Used] = 0;

        return Count;
    }

    size_t Done = 0;

    while (Done < Total && (Write || !Stream->End))
    {
        ssize_t Result = Write ? write(Stream->FD, (const unsigned char *) Input + Done, Total - Done) : read(Stream->FD, (unsigned char *) Output + Done, Total - Done);

        if (Result < 0 && errno == EINTR)
        {
            continue;
        }

        if (Result <= 0)
        {
            if (Result < 0 || Write)
            {
                if (Result == 0)
                {
                    errno = EIO;
                }

                Stream->Error = 1;
            }
            else
            {
                Stream->End = 1;
            }

            break;
        }

        Done += (size_t) Result;
    }

    return Done / Size;
}

size_t fread(void *Buffer, size_t Size, size_t Count, FILE *Stream)
{
    return StreamTransfer(Buffer, NULL, Size, Count, Stream, 0);
}

size_t fwrite(const void *Buffer, size_t Size, size_t Count, FILE *Stream)
{
    return StreamTransfer(NULL, Buffer, Size, Count, Stream, 1);
}

int fseek(FILE *Stream, long Offset, int Whence)
{
    if (Stream->Published != NULL)
    {
        if (Whence != SEEK_SET && Whence != SEEK_CUR && Whence != SEEK_END)
        {
            errno = EINVAL;

            return -1;
        }

        __int128 Position = (__int128) (Whence == SEEK_END ? Stream->Used : Whence == SEEK_CUR ? Stream->Position : 0) + Offset;

        if (Position < 0 || Position > LONG_MAX)
        {
            errno = Position < 0 ? EINVAL : EOVERFLOW;

            return -1;
        }

        Stream->Position = (size_t) Position;
        Stream->End = 0;

        return 0;
    }

    if (lseek(Stream->FD, Offset, Whence) < 0)
    {
        return -1;
    }

    Stream->End = 0;

    return 0;
}

long ftell(FILE *Stream)
{
    return Stream->Published != NULL ? (long) Stream->Position : lseek(Stream->FD, 0, SEEK_CUR);
}

int feof(FILE *Stream)
{
    return Stream->End != 0;
}

int ferror(FILE *Stream)
{
    return Stream->Error != 0;
}

void clearerr(FILE *Stream)
{
    Stream->End = Stream->Error = 0;
}

int fflush(FILE *Stream)
{
    if (Stream != NULL && Stream->Published != NULL)
    {
        *Stream->Published = Stream->Memory;
        *Stream->PublishedSize = Stream->Used;
    }

    return 0;
}

int getc(FILE *Stream)
{
    unsigned char Byte;

    return fread(&Byte, 1, 1, Stream) == 1 ? Byte : EOF;
}

int fputc(int Character, FILE *Stream)
{
    unsigned char Byte = (unsigned char) Character;

    return fwrite(&Byte, 1, 1, Stream) == 1 ? Byte : EOF;
}

int fputs(const char *Text, FILE *Stream)
{
    size_t Length = strlen(Text);

    return fwrite(Text, 1, Length, Stream) == Length ? 0 : EOF;
}

FILE *fdopen(int FD, const char *Mode)
{
    if (FD < 0 || (*Mode != 'r' && *Mode != 'w' && *Mode != 'a'))
    {
        errno = FD < 0 ? EBADF : EINVAL;

        return NULL;
    }

    FILE *Stream = calloc(1, sizeof(*Stream));

    if (Stream != NULL)
    {
        Stream->FD = FD;
        Stream->Allocated = 1;
    }

    return Stream;
}
