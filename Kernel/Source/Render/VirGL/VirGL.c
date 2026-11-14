// Kernel/Source/Render/VirGL/VirGL.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Memory/PMM.h>
#include <Render/GPU/VirtIOGPU.h>
#include <Render/VirGL/VirGL.h>

BOOLEAN VirGLGetCapsetInfo(UINT32 Id, VIRGL_CAPSET_INFO *Info)
{
    VIRTIO_GPU_GET_CAPSET_INFO_RESPONSE Response;
    UINT32                              Count;

    if (Info == NULL || (Id != VIRGL_CAPSET_ID && Id != VIRGL2_CAPSET_ID))
    {
        return FALSE;
    }

    Count = VirtIOGPUGetCapsetCount();

    for (UINT32 Index = 0; Index < Count; Index++)
    {
        if (!VirtIOGPUGetCapsetInfo(Index, &Response))
        {
            return FALSE;
        }

        if (Response.CapsetId == Id && Response.CapsetMaxSize != 0)
        {
            Info->Id = Response.CapsetId;
            Info->MaxVersion = Response.CapsetMaxVersion;
            Info->MaxSize = Response.CapsetMaxSize;

            return TRUE;
        }
    }

    return FALSE;
}

BOOLEAN VirGLCreateContext(UINT32 Id, CONST CHAR8 *Name)
{
    VIRTIO_GPU_CTX_CREATE_REQUEST Request = {0};

    if (Id == 0)
    {
        return FALSE;
    }

    Request.Header.Type = VIRTIO_GPU_CMD_CTX_CREATE;
    Request.Header.ContextId = Id;

    if (Name != NULL)
    {
        while (Request.NameLength < sizeof(Request.Name) && Name[Request.NameLength] != 0)
        {
            Request.Name[Request.NameLength] = Name[Request.NameLength];
            Request.NameLength++;
        }
    }

    return VirtIOGPUSendVirGLCommand(&Request, sizeof(Request), L"virgl context create");
}

BOOLEAN VirGLDestroyContext(UINT32 Id)
{
    VIRTIO_GPU_CTRL_HEADER Request = {0};

    if (Id == 0)
    {
        return FALSE;
    }

    Request.Type = VIRTIO_GPU_CMD_CTX_DESTROY;
    Request.ContextId = Id;

    return VirtIOGPUSendVirGLCommand(&Request, sizeof(Request), L"virgl context destroy");
}

BOOLEAN VirGLReadCapset(UINT32 Id, UINT32 Version, VOID *Data, UINT32 Capacity, UINT32 *BytesRead)
{
    VIRGL_CAPSET_INFO Info;

    if (BytesRead == NULL)
    {
        return FALSE;
    }

    *BytesRead = 0;

    if (Data == NULL || !VirGLGetCapsetInfo(Id, &Info) || Version > Info.MaxVersion || Capacity < Info.MaxSize)
    {
        return FALSE;
    }

    return VirtIOGPUReadCapset(Id, Version, Data, Info.MaxSize, BytesRead);
}

#define VIRGL_TEXTURE_2D                       2
#define VIRGL_FORMAT_Z32_FLOAT                 18
#define VIRGL_BIND_DEPTH_STENCIL               1U
#define VIRGL_BIND_RENDER_TARGET               (1U << 1)
#define VIRGL_BIND_SCANOUT                     (1U << 18)
#define VIRGL_OBJECT_SURFACE                   8
#define VIRGL_COMMAND_CREATE_OBJECT            1
#define VIRGL_COMMAND_DESTROY_OBJECT           3
#define VIRGL_COMMAND_SET_FRAMEBUFFER          5
#define VIRGL_COMMAND_CLEAR                    7
#define VIRGL_CLEAR_COLOR0                     (1U << 2)
#define VIRGL_COMMAND(Command, Object, Length) ((Command) | ((Object) << 8) | ((Length) << 16))

STATIC UINT64 NextFenceId;

STATIC BOOLEAN VirGLResourceCommand(UINT32 Command, UINT32 ContextId, UINT32 ResourceId)
{
    VIRTIO_GPU_RESOURCE_REQUEST Request = {0};

    if (ResourceId <= 2)
    {
        return FALSE;
    }

    Request.Header.Type = Command;
    Request.Header.ContextId = ContextId;
    Request.ResourceId = ResourceId;

    return VirtIOGPUSendVirGLCommand(&Request, sizeof(Request), L"virgl resource");
}

STATIC BOOLEAN VirGLCreateTarget(UINT32 ResourceId, UINT32 Width, UINT32 Height, UINT32 Format, UINT32 Bind, UINT32 Samples)
{
    VIRTIO_GPU_RESOURCE_CREATE_3D_REQUEST Request = {0};

    if (ResourceId <= 2 || Width == 0 || Height == 0)
    {
        return FALSE;
    }

    Request.Header.Type = VIRTIO_GPU_CMD_RESOURCE_CREATE_3D;
    Request.ResourceId = ResourceId;
    Request.Target = VIRGL_TEXTURE_2D;
    Request.Format = Format;
    Request.Bind = Bind;
    Request.Width = Width;
    Request.Height = Height;
    Request.Depth = 1;
    Request.ArraySize = 1;
    Request.Flags = 1;
    Request.Samples = Samples;

    return VirtIOGPUSendVirGLCommand(&Request, sizeof(Request), L"virgl render target");
}

BOOLEAN VirGLCreateRenderTarget(UINT32 ResourceId, UINT32 Width, UINT32 Height)
{
    return VirGLCreateTarget(ResourceId, Width, Height, VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM, VIRGL_BIND_RENDER_TARGET | VIRGL_BIND_SCANOUT, 0);
}

BOOLEAN VirGLCreateDepthTarget(UINT32 ResourceId, UINT32 Width, UINT32 Height)
{
    return VirGLCreateTarget(ResourceId, Width, Height, VIRGL_FORMAT_Z32_FLOAT, VIRGL_BIND_DEPTH_STENCIL, 0);
}

BOOLEAN VirGLCreateMultisampleTarget(UINT32 ResourceId, UINT32 Width, UINT32 Height, UINT32 Samples, BOOLEAN Depth)
{
    if (Samples != 2 && Samples != 4 && Samples != 8)
    {
        return FALSE;
    }

    UINT32 Format = Depth ? VIRGL_FORMAT_Z32_FLOAT : VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
    UINT32 Bind = Depth ? VIRGL_BIND_DEPTH_STENCIL : VIRGL_BIND_RENDER_TARGET;

    return VirGLCreateTarget(ResourceId, Width, Height, Format, Bind, Samples);
}

BOOLEAN VirGLDestroyResource(UINT32 ResourceId)
{
    return VirGLResourceCommand(VIRTIO_GPU_CMD_RESOURCE_UNREF, 0, ResourceId);
}

BOOLEAN VirGLAttachResource(UINT32 ContextId, UINT32 ResourceId)
{
    return ContextId != 0 && VirGLResourceCommand(VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE, ContextId, ResourceId);
}

BOOLEAN VirGLDetachResource(UINT32 ContextId, UINT32 ResourceId)
{
    return ContextId != 0 && VirGLResourceCommand(VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE, ContextId, ResourceId);
}

BOOLEAN VirGLSubmit(UINT32 ContextId, CONST UINT32 *Commands, UINT32 Size)
{
    VIRTIO_GPU_SUBMIT_3D_REQUEST *Request;
    BOOLEAN                       Success;

    if (ContextId == 0 || Commands == NULL || Size == 0 || Size % sizeof(UINT32) != 0 || Size > 0xFFFFFFFFU - sizeof(*Request))
    {
        return FALSE;
    }

    Request = KAlloc(sizeof(*Request) + Size);

    if (Request == NULL)
    {
        return FALSE;
    }

    KMemSet(Request, 0, sizeof(*Request));
    Request->Header.Type = VIRTIO_GPU_CMD_SUBMIT_3D;
    Request->Header.Flags = VIRTIO_GPU_FLAG_FENCE;
    Request->Header.FenceId = __atomic_add_fetch(&NextFenceId, 1, __ATOMIC_RELAXED);
    Request->Header.ContextId = ContextId;
    Request->Size = Size;
    KMemCpy(Request + 1, Commands, Size);
    Success = VirtIOGPUSubmitVirGL(Request, sizeof(*Request) + Size);
    KFree(Request);

    return Success;
}

BOOLEAN VirGLClearRenderTarget(UINT32 ContextId, UINT32 ResourceId, UINT32 SurfaceId, CONST UINT32 *ColorBits)
{
    if (ContextId == 0 || ResourceId <= 2 || SurfaceId == 0 || ColorBits == NULL)
    {
        return FALSE;
    }

    UINT32 Commands[] = {VIRGL_COMMAND(VIRGL_COMMAND_CREATE_OBJECT, VIRGL_OBJECT_SURFACE, 5), SurfaceId, ResourceId, VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM, 0, 0, VIRGL_COMMAND(VIRGL_COMMAND_SET_FRAMEBUFFER, 0, 3), 1, 0, SurfaceId, VIRGL_COMMAND(VIRGL_COMMAND_CLEAR, 0, 8), VIRGL_CLEAR_COLOR0, ColorBits[0], ColorBits[1], ColorBits[2], ColorBits[3], 0, 0, 0, VIRGL_COMMAND(VIRGL_COMMAND_SET_FRAMEBUFFER, 0, 2), 0, 0, VIRGL_COMMAND(VIRGL_COMMAND_DESTROY_OBJECT, VIRGL_OBJECT_SURFACE, 1), SurfaceId};
    return VirGLSubmit(ContextId, Commands, sizeof(Commands));
}

BOOLEAN VirGLPresentRenderTarget(UINT32 ResourceId, UINT32 Width, UINT32 Height)
{
    VIRTIO_GPU_SET_SCANOUT_REQUEST    Scanout = {0};
    VIRTIO_GPU_RESOURCE_FLUSH_REQUEST Flush = {0};
    BOOLEAN                           Success;
    UINT64                            Flags;

    if (ResourceId <= 2 || Width == 0 || Height == 0)
    {
        return FALSE;
    }

    Scanout.Header.Type = VIRTIO_GPU_CMD_SET_SCANOUT;
    Scanout.ResourceId = ResourceId;
    Scanout.Rect.Width = Width;
    Scanout.Rect.Height = Height;
    Flush.Header.Type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    Flush.ResourceId = ResourceId;
    Flush.Rect = Scanout.Rect;

    Flags = VirtIOGPUAcquireRenderLock();
    Success = VirtIOGPUSendVirGLCommand(&Scanout, sizeof(Scanout), L"virgl scanout");

    if (Success)
    {
        VirtIOGPUGetInfo()->ScanoutSet = FALSE;
        Success = VirtIOGPUSendVirGLCommand(&Flush, sizeof(Flush), L"virgl flush");
    }

    VirtIOGPUReleaseRenderLock(Flags);

    return Success;
}

#define VIRGL_BLEND_COLOR_MASK          (0xFU << 27)
#define VIRGL_BLEND_SRC_ALPHA           3U
#define VIRGL_BLEND_ONE_MINUS_SRC_ALPHA 19U
#define VIRGL_BLEND_ONE                 1U
#define VIRGL_BLEND_SOURCE_OVER         (VIRGL_BLEND_COLOR_MASK | 1U | (VIRGL_BLEND_SRC_ALPHA << 4) | (VIRGL_BLEND_ONE_MINUS_SRC_ALPHA << 9) | (VIRGL_BLEND_ONE << 17) | (VIRGL_BLEND_ONE_MINUS_SRC_ALPHA << 22))
#define VIRGL_OBJECT_BLEND              1
#define VIRGL_OBJECT_RASTERIZER         2
#define VIRGL_OBJECT_DSA                3
#define VIRGL_OBJECT_SHADER             4
#define VIRGL_OBJECT_VERTEX_ELEMENTS    5
#define VIRGL_BIND_VERTEX_BUFFER        (1U << 4)
#define VIRGL_FORMAT_R8_UNORM           64
#define VIRGL_FORMAT_RGBA32_FLOAT       31
#define VIRGL_PRIMITIVE_TRIANGLES       4
#define VIRGL_COMMAND_BIND_OBJECT       2
#define VIRGL_COMMAND_SET_VIEWPORT      4
#define VIRGL_COMMAND_SET_VERTICES      6
#define VIRGL_COMMAND_DRAW              8
#define VIRGL_COMMAND_INLINE_WRITE      9
#define VIRGL_COMMAND_BIND_SHADER       31
#define VIRGL_FLOAT_ONE                 0x3F800000U
#define VIRGL_FLOAT_HALF                0x3F000000U

STATIC UINT32 VirGLViewportHalf(UINT32 Value)
{
    UINT32 Exponent = 31U - (UINT32) __builtin_clz(Value);

    return ((Exponent + 126U) << 23) | (((UINT32) (((UINT64) Value << 23) >> Exponent)) & 0x7FFFFFU);
}

BOOLEAN VirGLCreateShader(UINT32 ContextId, UINT32 Handle, UINT32 Stage, CONST CHAR8 *Text, UINT32 Length)
{
    UINT32 Packet[64] = {0};
    if (ContextId == 0 || Handle == 0 || Text == NULL || Length == 0 || Length > sizeof(Packet) - 6 * sizeof(UINT32) || Text[Length - 1] != 0)
    {
        return FALSE;
    }

    UINT32 Words = (Length + 3) / 4;

    Packet[0] = VIRGL_COMMAND(VIRGL_COMMAND_CREATE_OBJECT, VIRGL_OBJECT_SHADER, 5 + Words);
    Packet[1] = Handle;
    Packet[2] = Stage;
    Packet[3] = Length;
    Packet[4] = 32;
    KMemCpy(Packet + 6, Text, Length);

    return VirGLSubmit(ContextId, Packet, (6 + Words) * sizeof(UINT32));
}

#define VIRGL_CAPSET_MAX_SAMPLES_WORD 71U
#define VIRGL_COMMAND_BLIT            16U
#define VIRGL_BLIT_RGBA               0xFU

UINT32 VirGLSelectSamples(UINT32 Preferred)
{
    VIRGL_CAPSET_INFO Info;
    UINT32            BytesRead;
    UINT32            Maximum = 0;

    if (Preferred < 2 || !VirGLGetCapsetInfo(VIRGL_CAPSET_ID, &Info) || Info.MaxSize < (VIRGL_CAPSET_MAX_SAMPLES_WORD + 1) * sizeof(UINT32))
    {
        return 1;
    }

    UINT32 *Data = KAlloc(Info.MaxSize);

    if (Data == NULL)
    {
        return 1;
    }

    if (VirGLReadCapset(Info.Id, Info.MaxVersion, Data, Info.MaxSize, &BytesRead) && BytesRead >= (VIRGL_CAPSET_MAX_SAMPLES_WORD + 1) * sizeof(UINT32))
    {
        Maximum = Data[VIRGL_CAPSET_MAX_SAMPLES_WORD];
    }

    KFree(Data);

    while (Preferred > Maximum && Preferred > 1)
    {
        Preferred /= 2;
    }

    return Preferred > 1 ? Preferred : 1;
}

STATIC BOOLEAN VirGLBlitColor(UINT32 ContextId, UINT32 SourceId, UINT32 TargetId, UINT32 Width, UINT32 Height, UINT32 SourceWidth, UINT32 SourceHeight, UINT32 Filter)
{
    if (SourceId <= 2 || TargetId <= 2 || SourceId == TargetId || Width == 0 || Height == 0)
    {
        return FALSE;
    }

    CONST UINT32 Format = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
    CONST UINT32 Commands[] = {VIRGL_COMMAND(VIRGL_COMMAND_BLIT, 0, 21), VIRGL_BLIT_RGBA | (Filter << 8), 0, 0, TargetId, 0, Format, 0, 0, 0, Width, Height, 1, SourceId, 0, Format, 0, 0, 0, SourceWidth, SourceHeight, 1};

    return VirGLSubmit(ContextId, Commands, sizeof(Commands));
}

BOOLEAN VirGLResolveColor(UINT32 ContextId, UINT32 SourceId, UINT32 TargetId, UINT32 Width, UINT32 Height)
{
    return VirGLBlitColor(ContextId, SourceId, TargetId, Width, Height, Width, Height, 0);
}

BOOLEAN VirGLGetAATarget(VIRGL_AA_MODE Mode, UINT32 Width, UINT32 Height, VIRGL_AA_TARGET *Target)
{
    if (Target == NULL || Width == 0 || Height == 0)
    {
        return FALSE;
    }

    UINT32 Scale = 1;

    switch (Mode)
    {
        case VirGLAAMsaa:
        case VirGLAAOff:
            break;

        case VirGLAASsaa4:
            Scale = 2;
            break;

        default:
            return FALSE;
    }

    if (Width > 0x7FFFFFFFU / Scale || Height > 0x7FFFFFFFU / Scale)
    {
        return FALSE;
    }

    Target->Width = Width * Scale;
    Target->Height = Height * Scale;
    Target->Samples = Mode == VirGLAAMsaa ? VirGLSelectSamples(VIRGL_DEFAULT_MSAA_SAMPLES) : 1;

    return TRUE;
}

BOOLEAN VirGLDownsampleColor(UINT32 ContextId, UINT32 SourceId, UINT32 TargetId, UINT32 Width, UINT32 Height, VIRGL_AA_MODE Mode)
{
    VIRGL_AA_TARGET Source;

    if (Mode != VirGLAASsaa4 || !VirGLGetAATarget(Mode, Width, Height, &Source))
    {
        return FALSE;
    }

    return VirGLBlitColor(ContextId, SourceId, TargetId, Width, Height, Source.Width, Source.Height, 1);
}

#define VIRGL_FORMAT_RGBA8_UNORM  67U
#define VIRGL_BIND_SAMPLER_VIEW   (1U << 3)
#define VIRGL_UPLOAD_HEADER_WORDS 12U
#define VIRGL_UPLOAD_MAX_PIXELS   (0xFFFFU - (VIRGL_UPLOAD_HEADER_WORDS - 1U))

BOOLEAN VirGLCreateTexture(UINT32 ResourceId, UINT32 Width, UINT32 Height)
{
    return VirGLCreateTarget(ResourceId, Width, Height, VIRGL_FORMAT_RGBA8_UNORM, VIRGL_BIND_SAMPLER_VIEW, 0);
}

BOOLEAN VirGLUploadTexture(UINT32 ContextId, UINT32 ResourceId, UINT32 Width, UINT32 Height, CONST VOID *Pixels)
{
    UINT32 *Commands;
    UINT32  TileWidth;
    BOOLEAN Success = TRUE;

    if (ContextId == 0 || ResourceId <= 2 || Width == 0 || Height == 0 || Pixels == NULL || Width > (UINTN) -1 / sizeof(UINT32) / Height)
    {
        return FALSE;
    }

    TileWidth = Width < VIRGL_UPLOAD_MAX_PIXELS ? Width : VIRGL_UPLOAD_MAX_PIXELS;
    Commands = KAlloc((VIRGL_UPLOAD_HEADER_WORDS + TileWidth) * sizeof(*Commands));

    if (Commands == NULL)
    {
        return FALSE;
    }

    for (UINT32 Y = 0; Y < Height && Success; Y++)
    {
        for (UINT32 X = 0; X < Width;)
        {
            UINT32 Count = Width - X;
            if (Count > TileWidth)
            {
                Count = TileWidth;
            }

            UINT32 Header[VIRGL_UPLOAD_HEADER_WORDS] = {VIRGL_COMMAND(VIRGL_COMMAND_INLINE_WRITE, 0, VIRGL_UPLOAD_HEADER_WORDS - 1U + Count), ResourceId, 0, 0, Count * sizeof(UINT32), 0, X, Y, 0, Count, 1, 1};

            KMemCpy(Commands, Header, sizeof(Header));
            KMemCpy(Commands + VIRGL_UPLOAD_HEADER_WORDS, (CONST UINT8 *) Pixels + ((UINTN) Y * Width + X) * sizeof(UINT32), Count * sizeof(UINT32));

            if (!VirGLSubmit(ContextId, Commands, sizeof(Header) + Count * sizeof(UINT32)))
            {
                Success = FALSE;
                break;
            }

            X += Count;
        }
    }

    KFree(Commands);

    return Success;
}

#define VIRGL_OBJECT_SAMPLER_VIEW         6
#define VIRGL_OBJECT_SAMPLER_STATE        7
#define VIRGL_COMMAND_SET_SAMPLER_VIEWS   10
#define VIRGL_COMMAND_BIND_SAMPLER_STATES 18
#define VIRGL_SWIZZLE_RGBA                (1U << 3 | 2U << 6 | 3U << 9)

BOOLEAN VirGLInitTexturedRects(UINT32 ContextId, UINT32 TargetId, UINT32 VerticesId, UINT32 Width, UINT32 Height)
{
    STATIC CONST CHAR8 VertexShader[] = "VERT\nDCL IN[0]\nDCL IN[1]\nDCL OUT[0], POSITION\nDCL OUT[1], GENERIC[0]\nMOV OUT[0], IN[0]\nMOV OUT[1], IN[1]\nEND\n";
    STATIC CONST CHAR8 FragmentShader[] = "FRAG\nDCL IN[0], GENERIC[0], LINEAR\nDCL OUT[0], COLOR\nDCL SAMP[0]\nDCL TEMP[0]\nTEX TEMP[0], IN[0], SAMP[0], 2D\nMOV OUT[0].xyz, TEMP[0]\nMUL OUT[0].w, TEMP[0].wwww, IN[0].zzzz\nEND\n";
    if (ContextId == 0 || TargetId <= 2 || VerticesId <= 2 || TargetId == VerticesId || Width == 0 || Height == 0)
    {
        return FALSE;
    }

    UINT32 HalfWidth = VirGLViewportHalf(Width);
    UINT32 HalfHeight = VirGLViewportHalf(Height);
    UINT32 Commands[] = {VIRGL_COMMAND(VIRGL_COMMAND_CREATE_OBJECT, VIRGL_OBJECT_SURFACE, 5), 1, TargetId, VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM, 0, 0, VIRGL_COMMAND(VIRGL_COMMAND_SET_FRAMEBUFFER, 0, 3), 1, 0, 1, VIRGL_COMMAND(VIRGL_COMMAND_SET_VIEWPORT, 0, 7), 0, HalfWidth, HalfHeight, VIRGL_FLOAT_HALF, HalfWidth, HalfHeight, VIRGL_FLOAT_HALF, VIRGL_COMMAND(VIRGL_COMMAND_CREATE_OBJECT, VIRGL_OBJECT_BLEND, 11), 2, 0, 0, VIRGL_BLEND_SOURCE_OVER, 0, 0, 0, 0, 0, 0, 0, VIRGL_COMMAND(VIRGL_COMMAND_BIND_OBJECT, VIRGL_OBJECT_BLEND, 1), 2, VIRGL_COMMAND(VIRGL_COMMAND_CREATE_OBJECT, VIRGL_OBJECT_RASTERIZER, 9), 3, 0, VIRGL_FLOAT_ONE, 0, 0, 0, 0, 0, 0, VIRGL_COMMAND(VIRGL_COMMAND_BIND_OBJECT, VIRGL_OBJECT_RASTERIZER, 1), 3, VIRGL_COMMAND(VIRGL_COMMAND_CREATE_OBJECT, VIRGL_OBJECT_DSA, 5), 4, 0, 0, 0, 0, VIRGL_COMMAND(VIRGL_COMMAND_BIND_OBJECT, VIRGL_OBJECT_DSA, 1), 4, VIRGL_COMMAND(VIRGL_COMMAND_CREATE_OBJECT, VIRGL_OBJECT_VERTEX_ELEMENTS, 9), 5, 0, 0, 0, VIRGL_FORMAT_RGBA32_FLOAT, 16, 0, 0, VIRGL_FORMAT_RGBA32_FLOAT, VIRGL_COMMAND(VIRGL_COMMAND_BIND_OBJECT, VIRGL_OBJECT_VERTEX_ELEMENTS, 1), 5, VIRGL_COMMAND(VIRGL_COMMAND_SET_VERTICES, 0, 3), 32, 0, VerticesId, VIRGL_COMMAND(VIRGL_COMMAND_BIND_SHADER, 0, 2), 6, 0, VIRGL_COMMAND(VIRGL_COMMAND_BIND_SHADER, 0, 2), 7, 1, VIRGL_COMMAND(VIRGL_COMMAND_CREATE_OBJECT, VIRGL_OBJECT_SAMPLER_STATE, 9), 9, 0, 0, 0, 0, 0, 0, 0, 0, VIRGL_COMMAND(VIRGL_COMMAND_BIND_SAMPLER_STATES, 0, 3), 1, 0, 9};

    return VirGLCreateShader(ContextId, 6, 0, VertexShader, sizeof(VertexShader)) && VirGLCreateShader(ContextId, 7, 1, FragmentShader, sizeof(FragmentShader)) && VirGLSubmit(ContextId, Commands, sizeof(Commands));
}

STATIC UINT32 VirGLFixedFloat(INT64 Value)
{
    if (Value == 0)
    {
        return 0;
    }

    UINT32 Sign = Value < 0 ? 0x80000000U : 0;
    UINT64 Magnitude = Value < 0 ? -Value : Value;
    UINT32 Exponent = 63U - (UINT32) __builtin_clzll(Magnitude);
    UINT32 Mantissa = (UINT32) ((Magnitude << 23) >> Exponent);

    return Sign | ((Exponent + 103U) << 23) | (Mantissa & 0x7FFFFFU);
}

BOOLEAN VirGLDrawTexturedRect(UINT32 ContextId, UINT32 VerticesId, UINT32 TextureId, UINT32 TargetWidth, UINT32 TargetHeight, UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT8 Opacity)
{
    if (ContextId == 0 || VerticesId <= 2 || TextureId <= 2 || TextureId == VerticesId || X >= TargetWidth || Y >= TargetHeight || Width == 0 || Height == 0 || Width > TargetWidth - X || Height > TargetHeight - Y)
    {
        return FALSE;
    }

    CONST INT64 Unit = 1LL << 24;
    UINT32      Left = VirGLFixedFloat(2 * Unit * X / TargetWidth - Unit);
    UINT32      Right = VirGLFixedFloat(2 * Unit * (X + Width) / TargetWidth - Unit);
    UINT32      Top = VirGLFixedFloat(Unit - 2 * Unit * Y / TargetHeight);
    UINT32      Bottom = VirGLFixedFloat(Unit - 2 * Unit * (Y + Height) / TargetHeight);
    UINT32      Alpha = VirGLFixedFloat(Unit * Opacity / 255);
    UINT32      Commands[] = {VIRGL_COMMAND(VIRGL_COMMAND_INLINE_WRITE, 0, 59), VerticesId, 0, 0, 0, 0, 0, 0, 0, 192, 1, 1, Left, Top, 0, VIRGL_FLOAT_ONE, 0, 0, Alpha, 0, Right, Top, 0, VIRGL_FLOAT_ONE, VIRGL_FLOAT_ONE, 0, Alpha, 0, Right, Bottom, 0, VIRGL_FLOAT_ONE, VIRGL_FLOAT_ONE, VIRGL_FLOAT_ONE, Alpha, 0, Left, Top, 0, VIRGL_FLOAT_ONE, 0, 0, Alpha, 0, Right, Bottom, 0, VIRGL_FLOAT_ONE, VIRGL_FLOAT_ONE, VIRGL_FLOAT_ONE, Alpha, 0, Left, Bottom, 0, VIRGL_FLOAT_ONE, 0, VIRGL_FLOAT_ONE, Alpha, 0, VIRGL_COMMAND(VIRGL_COMMAND_CREATE_OBJECT, VIRGL_OBJECT_SAMPLER_VIEW, 6), 8, TextureId, VIRGL_FORMAT_RGBA8_UNORM, 0, 0, VIRGL_SWIZZLE_RGBA, VIRGL_COMMAND(VIRGL_COMMAND_SET_SAMPLER_VIEWS, 0, 3), 1, 0, 8, VIRGL_COMMAND(VIRGL_COMMAND_DRAW, 0, 12), 0, 6, VIRGL_PRIMITIVE_TRIANGLES, 0, 1, 0, 0, 0, 0, 0, 5, 0, VIRGL_COMMAND(VIRGL_COMMAND_SET_SAMPLER_VIEWS, 0, 3), 1, 0, 0, VIRGL_COMMAND(VIRGL_COMMAND_DESTROY_OBJECT, VIRGL_OBJECT_SAMPLER_VIEW, 1), 8};

    return VirGLSubmit(ContextId, Commands, sizeof(Commands));
}

BOOLEAN VirGLCreateVertexBuffer(UINT32 ResourceId, UINT32 Size)
{
    VIRTIO_GPU_RESOURCE_CREATE_3D_REQUEST Request = {0};

    if (ResourceId <= 2 || Size == 0)
    {
        return FALSE;
    }

    Request.Header.Type = VIRTIO_GPU_CMD_RESOURCE_CREATE_3D;
    Request.ResourceId = ResourceId;
    Request.Format = VIRGL_FORMAT_R8_UNORM;
    Request.Bind = VIRGL_BIND_VERTEX_BUFFER;
    Request.Width = Size;
    Request.Height = 1;
    Request.Depth = 1;
    Request.ArraySize = 1;

    return VirtIOGPUSendVirGLCommand(&Request, sizeof(Request), L"virgl vertex buffer");
}

BOOLEAN VirGLWriteVertexBuffer(UINT32 ContextId, UINT32 ResourceId, UINT32 Offset, CONST VOID *Data, UINT32 Size)
{
    if (ContextId == 0 || ResourceId <= 2 || Data == NULL || Size == 0 || Size > VIRGL_UPLOAD_MAX_PIXELS * sizeof(UINT32) || Offset > 0xFFFFFFFFU - Size)
    {
        return FALSE;
    }

    UINT32  Words = (Size + 3) / 4;
    UINT32  Header[] = {VIRGL_COMMAND(VIRGL_COMMAND_INLINE_WRITE, 0, VIRGL_UPLOAD_HEADER_WORDS - 1 + Words), ResourceId, 0, 0, 0, 0, Offset, 0, 0, Size, 1, 1};
    UINT32 *Commands = KAlloc(sizeof(Header) + Words * sizeof(UINT32));

    if (Commands == NULL)
    {
        return FALSE;
    }

    KMemCpy(Commands, Header, sizeof(Header));
    KMemSet(Commands + VIRGL_UPLOAD_HEADER_WORDS, 0, Words * sizeof(UINT32));
    KMemCpy(Commands + VIRGL_UPLOAD_HEADER_WORDS, Data, Size);
    BOOLEAN Success = VirGLSubmit(ContextId, Commands, sizeof(Header) + Words * sizeof(UINT32));
    KFree(Commands);

    return Success;
}

BOOLEAN VirGLReadColorTarget(UINT32 ContextId, UINT32 ResourceId, UINT32 Width, UINT32 Height, VOID *Pixels, BOOLEAN *CanFree)
{
    VIRTIO_GPU_RESOURCE_ATTACH_BACKING_REQUEST Backing = {0};
    VIRTIO_GPU_RESOURCE_REQUEST Detach = {0};
    struct PACKED
    {
        VIRTIO_GPU_CTRL_HEADER Header;
        UINT32 X, Y, Z, Width, Height, Depth;
        UINT64 Offset;
        UINT32 ResourceId, Level, Stride, LayerStride;
    } Transfer = {0};

    *CanFree = FALSE;
    Backing.Header.Type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    Backing.ResourceId = ResourceId;
    Backing.EntryCount = 1;
    Backing.Entry.Address = (UINT64) Pixels;
    Backing.Entry.Length = Width * Height * 4;

    if (!VirtIOGPUSendVirGLCommand(&Backing, sizeof(Backing), L"color backing"))
    {
        return FALSE;
    }

    Transfer.Header.Type = 0x0206;
    Transfer.Header.ContextId = ContextId;
    Transfer.Header.Flags = VIRTIO_GPU_FLAG_FENCE;
    Transfer.Header.FenceId = __atomic_add_fetch(&NextFenceId, 1, __ATOMIC_RELAXED);
    Transfer.Width = Width;
    Transfer.Height = Height;
    Transfer.Depth = 1;
    Transfer.ResourceId = ResourceId;
    Transfer.Stride = Width * 4;
    Transfer.LayerStride = Backing.Entry.Length;
    BOOLEAN Success = VirtIOGPUSendVirGLCommand(&Transfer, sizeof(Transfer), L"color readback");
    Detach.Header.Type = 0x0107;
    Detach.ResourceId = ResourceId;
    *CanFree = VirtIOGPUSendVirGLCommand(&Detach, sizeof(Detach), L"color detach backing");

    return Success && *CanFree;
}

BOOLEAN VirGLDrawPositionTriangle(UINT32 ContextId, UINT32 TargetId, UINT32 VerticesId, UINT32 Width, UINT32 Height)
{
    STATIC CONST CHAR8 VertexShader[] = "VERT\nDCL IN[0]\nDCL OUT[0], POSITION\nMOV OUT[0], IN[0]\nEND\n";
    STATIC CONST CHAR8 FragmentShader[] = "FRAG\nDCL OUT[0], COLOR\nIMM[0] FLT32 {1.0, 1.0, 1.0, 1.0}\nMOV OUT[0], IMM[0]\nEND\n";
    if (ContextId == 0 || TargetId <= 2 || VerticesId <= 2 || TargetId == VerticesId || Width == 0 || Height == 0)
    {
        return FALSE;
    }

    UINT32 HalfWidth = VirGLViewportHalf(Width);
    UINT32 HalfHeight = VirGLViewportHalf(Height);
    UINT32 Commands[] = {VIRGL_COMMAND(VIRGL_COMMAND_CREATE_OBJECT, VIRGL_OBJECT_SURFACE, 5), 1, TargetId, VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM, 0, 0,
        VIRGL_COMMAND(VIRGL_COMMAND_SET_FRAMEBUFFER, 0, 3), 1, 0, 1,
        VIRGL_COMMAND(VIRGL_COMMAND_SET_VIEWPORT, 0, 7), 0, HalfWidth, HalfHeight, VIRGL_FLOAT_HALF, HalfWidth, HalfHeight, VIRGL_FLOAT_HALF,
        VIRGL_COMMAND(VIRGL_COMMAND_CREATE_OBJECT, VIRGL_OBJECT_BLEND, 11), 2, 0, 0, VIRGL_BLEND_COLOR_MASK, 0, 0, 0, 0, 0, 0, 0,
        VIRGL_COMMAND(VIRGL_COMMAND_BIND_OBJECT, VIRGL_OBJECT_BLEND, 1), 2,
        VIRGL_COMMAND(VIRGL_COMMAND_CREATE_OBJECT, VIRGL_OBJECT_RASTERIZER, 9), 3, 0, VIRGL_FLOAT_ONE, 0, 0, 0, 0, 0, 0,
        VIRGL_COMMAND(VIRGL_COMMAND_BIND_OBJECT, VIRGL_OBJECT_RASTERIZER, 1), 3,
        VIRGL_COMMAND(VIRGL_COMMAND_CREATE_OBJECT, VIRGL_OBJECT_DSA, 5), 4, 0, 0, 0, 0,
        VIRGL_COMMAND(VIRGL_COMMAND_BIND_OBJECT, VIRGL_OBJECT_DSA, 1), 4,
        VIRGL_COMMAND(VIRGL_COMMAND_CREATE_OBJECT, VIRGL_OBJECT_VERTEX_ELEMENTS, 5), 5, 0, 0, 0, VIRGL_FORMAT_RGBA32_FLOAT,
        VIRGL_COMMAND(VIRGL_COMMAND_BIND_OBJECT, VIRGL_OBJECT_VERTEX_ELEMENTS, 1), 5,
        VIRGL_COMMAND(VIRGL_COMMAND_SET_VERTICES, 0, 3), 16, 0, VerticesId,
        VIRGL_COMMAND(VIRGL_COMMAND_BIND_SHADER, 0, 2), 6, 0,
        VIRGL_COMMAND(VIRGL_COMMAND_BIND_SHADER, 0, 2), 7, 1,
        VIRGL_COMMAND(VIRGL_COMMAND_DRAW, 0, 12), 0, 3, VIRGL_PRIMITIVE_TRIANGLES, 0, 1, 0, 0, 0, 0, 0, 2, 0};

    return VirGLCreateShader(ContextId, 6, 0, VertexShader, sizeof(VertexShader)) && VirGLCreateShader(ContextId, 7, 1, FragmentShader, sizeof(FragmentShader)) && VirGLSubmit(ContextId, Commands, sizeof(Commands));
}
