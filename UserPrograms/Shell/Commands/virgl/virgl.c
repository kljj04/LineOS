// UserPrograms/Shell/Commands/virgl/virgl.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <VirGLDevice.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

STATIC UINT8 CapsetData[LOS_VIRGL_MAX_CAPSET_BYTES];

STATIC BOOLEAN VirGLTestBuffer(int FD)
{
    CONST UINT8             Data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    LOS_VIRGL_BUFFER_CREATE Buffer = {0};
    LOS_VIRGL_BUFFER_WRITE  Write = {0};

    Buffer.Size = 64;

    if (VirGLCreateDeviceBuffer(FD, &Buffer) < 0)
    {
        perror("virgl buffer create");

        return FALSE;
    }

    Write.Handle = Buffer.Handle;
    Write.Offset = 4;
    Write.Size = sizeof(Data);
    Write.Address = (UINTN) Data;
    BOOLEAN Success = VirGLWriteDeviceBuffer(FD, &Write) == 0;

    if (!Success)
    {
        perror("virgl buffer upload");
    }

    if (VirGLDestroyDeviceBuffer(FD, Buffer.Handle) < 0)
    {
        perror("virgl buffer destroy");

        return FALSE;
    }

    if (Success)
    {
        puts("VirGL buffer create/upload/destroy OK");
    }

    return Success;
}

int main(VOID)
{
    int     FD = VirGLOpenDevice();
    int     Status = 0;
    BOOLEAN Found = FALSE;

    if (FD < 0)
    {
        perror(LOS_VIRGL_PATH);

        return 1;
    }

    for (UINT32 Id = 1; Id <= 2; Id++)
    {
        LOS_VIRGL_CAPSET_INFO Info = {0};
        Info.Id = Id;

        if (VirGLQueryCapset(FD, &Info) < 0)
        {
            if (errno != EINVAL)
            {
                perror("virgl capset");
                Status = 1;
            }

            continue;
        }

        LOS_VIRGL_CAPSET_READ Request = {0};
        Request.Id = Id;
        Request.Version = Info.MaxVersion;
        Request.Capacity = sizeof(CapsetData);
        Request.Address = (UINTN) CapsetData;

        if (VirGLFetchCapset(FD, &Request) < 0)
        {
            perror("virgl capset read");
            Status = 1;

            continue;
        }

        Found = TRUE;
        printf("%s version %u, %u bytes read (max %u)\n", Id == 1 ? "VirGL" : "VirGL2", Info.MaxVersion, Request.BytesRead, Info.MaxSize);
    }

    if (Found)
    {
        if (VirGLCreateDeviceContext(FD) < 0)
        {
            perror("virgl context");
            Status = 1;
        }
        else
        {
            puts("VirGL context created");

            if (!VirGLTestBuffer(FD))
            {
                Status = 1;
            }
        }
    }

    if (close(FD) < 0)
    {
        perror("virgl close");
        Status = 1;
    }

    if (!Found)
    {
        puts("virgl: no readable capsets");

        return 1;
    }

    return Status;
}
