// Libraries/Mesa/Source/src/gallium/winsys/virgl/lineos/VirGLWinsys.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include "VirGLWinsys.h"
#include "VirGLCaps.h"
#include "pipe/p_state.h"
#include <VirGLDevice.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static int MesaLineOSGetVirGLCaps(struct virgl_winsys *Base, struct virgl_drm_caps *Caps)
{
    MESA_LINEOS_VIRGL_WINSYS *Winsys = (MESA_LINEOS_VIRGL_WINSYS *) Base;

    memset(Caps, 0, sizeof(*Caps));
    virgl_ws_fill_new_caps_defaults(Caps);

    int Result = MesaLineOSReadVirGLCaps(Winsys->FD, 2, &Caps->caps, sizeof(Caps->caps));

    if (Result < 0 && errno == EINVAL)
    {
        Result = MesaLineOSReadVirGLCaps(Winsys->FD, 1, &Caps->caps.v1, sizeof(Caps->caps.v1));
    }

    return Result;
}

struct virgl_hw_res
{
    unsigned int Handle;
    unsigned int Size;
    unsigned int References;
    unsigned char Data[];
};

static struct virgl_hw_res *MesaLineOSCreateBuffer(struct virgl_winsys *Base, enum pipe_texture_target Target, const void *Front, uint32_t Format, uint32_t Bind, uint32_t Width, uint32_t Height, uint32_t Depth, uint32_t ArraySize, uint32_t LastLevel, uint32_t Samples, uint32_t Flags, uint32_t Size)
{
    MESA_LINEOS_VIRGL_WINSYS *Winsys = (MESA_LINEOS_VIRGL_WINSYS *) Base;
    (void) Format;
    (void) Bind;
    (void) Flags;

    if (Target != PIPE_BUFFER || Front != NULL || Height != 1 || Depth != 1 || ArraySize != 1 ||
        LastLevel != 0 || Samples > 1 || Width == 0 || Size < Width || Size > LOS_VIRGL_MAX_BUFFER_BYTES)
    {
        errno = EINVAL;

        return NULL;
    }

    struct virgl_hw_res *Resource = calloc(1, sizeof(*Resource) + Size);

    if (Resource == NULL)
    {
        return NULL;
    }

    LOS_VIRGL_BUFFER_CREATE Request = {.Size = Size};

    if (VirGLCreateDeviceBuffer(Winsys->FD, &Request) < 0)
    {
        free(Resource);

        return NULL;
    }

    Resource->Handle = Request.Handle;
    Resource->Size = Size;
    Resource->References = 1;

    return Resource;
}

static void MesaLineOSBufferReference(struct virgl_winsys *Base, struct virgl_hw_res **Destination, struct virgl_hw_res *Source)
{
    MESA_LINEOS_VIRGL_WINSYS *Winsys = (MESA_LINEOS_VIRGL_WINSYS *) Base;
    struct virgl_hw_res *Previous = *Destination;

    if (Source != NULL)
    {
        Source->References++;
    }

    *Destination = Source;

    if (Previous != NULL && --Previous->References == 0)
    {
        VirGLDestroyDeviceBuffer(Winsys->FD, Previous->Handle);
        free(Previous);
    }
}

static void *MesaLineOSMapBuffer(struct virgl_winsys *Base, struct virgl_hw_res *Resource)
{
    (void) Base;

    return Resource->Data;
}

static int MesaLineOSPutBuffer(struct virgl_winsys *Base, struct virgl_hw_res *Resource, const struct pipe_box *Box, uint32_t Stride, uint32_t LayerStride, uint32_t Offset, uint32_t Level)
{
    MESA_LINEOS_VIRGL_WINSYS *Winsys = (MESA_LINEOS_VIRGL_WINSYS *) Base;
    (void) Stride;
    (void) LayerStride;

    if (Level != 0 || Box->x < 0 || Box->y != 0 || Box->z != 0 || Box->height != 1 || Box->depth != 1 ||
        Box->width <= 0 || (uint32_t) Box->x > Resource->Size || (uint32_t) Box->width > Resource->Size - (uint32_t) Box->x ||
        Offset > Resource->Size || (uint32_t) Box->width > Resource->Size - Offset)
    {
        errno = EINVAL;

        return -1;
    }

    uint32_t Remaining = Box->width;
    LOS_VIRGL_BUFFER_WRITE Request = {.Handle = Resource->Handle, .Offset = Box->x, .Address = (unsigned long) (Resource->Data + Offset)};

    while (Remaining != 0)
    {
        Request.Size = Remaining < LOS_VIRGL_MAX_UPLOAD_BYTES ? Remaining : LOS_VIRGL_MAX_UPLOAD_BYTES;

        if (VirGLWriteDeviceBuffer(Winsys->FD, &Request) < 0)
        {
            return -1;
        }

        Remaining -= Request.Size;
        Request.Offset += Request.Size;
        Request.Address += Request.Size;
    }

    return 0;
}

typedef struct
{
    struct virgl_cmd_buf Base;
    struct virgl_winsys *Winsys;
    struct virgl_hw_res *Resources[LOS_VIRGL_BUFFER_LIMIT];
    uint32_t Capacity;
    uint32_t Count;
    bool Failed;
    uint32_t Words[];
} MESA_LINEOS_COMMAND_BUFFER;

static struct virgl_cmd_buf *MesaLineOSCreateCommands(struct virgl_winsys *Base, uint32_t Size)
{
    if (Size == 0 || Size > VIRGL_MAX_CMDBUF_DWORDS)
    {
        errno = EINVAL;

        return NULL;
    }

    MESA_LINEOS_COMMAND_BUFFER *Commands = calloc(1, sizeof(*Commands) + Size * sizeof(uint32_t));

    if (Commands == NULL)
    {
        return NULL;
    }

    Commands->Winsys = Base;
    Commands->Capacity = Size;
    Commands->Base.buf = Commands->Words;

    return &Commands->Base;
}

static void MesaLineOSDestroyCommands(struct virgl_cmd_buf *Base)
{
    MESA_LINEOS_COMMAND_BUFFER *Commands = (MESA_LINEOS_COMMAND_BUFFER *) Base;

    for (uint32_t Index = 0; Index < Commands->Count; Index++)
    {
        MesaLineOSBufferReference(Commands->Winsys, &Commands->Resources[Index], NULL);
    }

    free(Commands);
}

static bool MesaLineOSResourceReferenced(struct virgl_winsys *Base, struct virgl_cmd_buf *Buffer, struct virgl_hw_res *Resource)
{
    MESA_LINEOS_COMMAND_BUFFER *Commands = (MESA_LINEOS_COMMAND_BUFFER *) Buffer;
    (void) Base;

    for (uint32_t Index = 0; Index < Commands->Count; Index++)
    {
        if (Commands->Resources[Index] == Resource)
        {
            return true;
        }
    }

    return false;
}

static void MesaLineOSEmitResource(struct virgl_winsys *Base, struct virgl_cmd_buf *Buffer, struct virgl_hw_res *Resource, bool WriteHandle)
{
    MESA_LINEOS_COMMAND_BUFFER *Commands = (MESA_LINEOS_COMMAND_BUFFER *) Buffer;

    if (Commands->Failed || (WriteHandle && Buffer->cdw >= Commands->Capacity))
    {
        Commands->Failed = true;

        return;
    }

    if (!MesaLineOSResourceReferenced(Base, Buffer, Resource))
    {
        if (Commands->Count == LOS_VIRGL_BUFFER_LIMIT)
        {
            Commands->Failed = true;

            return;
        }

        MesaLineOSBufferReference(Base, &Commands->Resources[Commands->Count++], Resource);
    }

    if (WriteHandle)
    {
        Buffer->buf[Buffer->cdw++] = Resource->Handle;
    }
}

static uint32_t MesaLineOSBufferSize(struct virgl_winsys *Base, struct virgl_hw_res *Resource)
{
    (void) Base;

    return Resource->Size;
}

static int MesaLineOSSubmitCommands(struct virgl_winsys *Base, struct virgl_cmd_buf *Buffer, struct pipe_fence_handle **Fence)
{
    MESA_LINEOS_VIRGL_WINSYS *Winsys = (MESA_LINEOS_VIRGL_WINSYS *) Base;
    MESA_LINEOS_COMMAND_BUFFER *Commands = (MESA_LINEOS_COMMAND_BUFFER *) Buffer;
    int Result = 0;

    if (Fence != NULL)
    {
        *Fence = NULL;
    }

    if (Commands->Failed || Buffer->cdw > Commands->Capacity)
    {
        errno = EINVAL;
        Result = -1;
    }
    else if (Buffer->cdw != 0)
    {
        Result = VirGLSubmitDeviceCommands(Winsys->FD, Buffer->buf, Buffer->cdw);
    }

    for (uint32_t Index = 0; Index < Commands->Count; Index++)
    {
        MesaLineOSBufferReference(Base, &Commands->Resources[Index], NULL);
    }

    Commands->Count = 0;
    Commands->Failed = false;
    Buffer->cdw = 0;

    return Result;
}

void MesaLineOSInitVirGLCaps(MESA_LINEOS_VIRGL_WINSYS *Winsys, int FD)
{
    Winsys->FD = FD;
    Winsys->Base.get_caps = MesaLineOSGetVirGLCaps;
    Winsys->Base.resource_create = MesaLineOSCreateBuffer;
    Winsys->Base.resource_reference = MesaLineOSBufferReference;
    Winsys->Base.resource_map = MesaLineOSMapBuffer;
    Winsys->Base.transfer_put = MesaLineOSPutBuffer;
    Winsys->Base.resource_get_storage_size = MesaLineOSBufferSize;
    Winsys->Base.cmd_buf_create = MesaLineOSCreateCommands;
    Winsys->Base.cmd_buf_destroy = MesaLineOSDestroyCommands;
    Winsys->Base.res_is_referenced = MesaLineOSResourceReferenced;
    Winsys->Base.emit_res = MesaLineOSEmitResource;
    Winsys->Base.submit_cmd = MesaLineOSSubmitCommands;
}
