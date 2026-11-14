// Libraries/Mesa/Source/src/gallium/winsys/virgl/lineos/VirGLCaps.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include "VirGLCaps.h"
#include <VirGLDevice.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int MesaLineOSReadVirGLCaps(int FD, unsigned int Id, void *Data, size_t Capacity)
{
    if (Data == NULL || Capacity == 0)
    {
        errno = EINVAL;
        return -1;
    }

    LOS_VIRGL_CAPSET_INFO Info = {.Id = Id};

    if (VirGLQueryCapset(FD, &Info) < 0)
    {
        return -1;
    }

    if (Info.MaxSize == 0 || Info.MaxSize > LOS_VIRGL_MAX_CAPSET_BYTES)
    {
        errno = EOVERFLOW;
        return -1;
    }

    void *Buffer = malloc(Info.MaxSize);

    if (Buffer == NULL)
    {
        return -1;
    }

    LOS_VIRGL_CAPSET_READ Request = {.Id = Id, .Capacity = Info.MaxSize, .Address = (unsigned long) Buffer};
    int Result = VirGLFetchCapset(FD, &Request);

    if (Result == 0)
    {
        size_t Size = Request.BytesRead < Capacity ? Request.BytesRead : Capacity;
        memcpy(Data, Buffer, Size);
    }

    free(Buffer);

    return Result;
}
