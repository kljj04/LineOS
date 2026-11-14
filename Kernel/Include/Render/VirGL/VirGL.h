// Kernel/Include/Render/VirGL/VirGL.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

#define VIRGL_CAPSET_ID  1
#define VIRGL2_CAPSET_ID 2

#define VIRGL_DEFAULT_MSAA_SAMPLES 8U

typedef enum
{
    VirGLAAMsaa,
    VirGLAASsaa4,
    VirGLAAOff
} VIRGL_AA_MODE;

typedef struct
{
    UINT32 Width;
    UINT32 Height;
    UINT32 Samples;
} VIRGL_AA_TARGET;

BOOLEAN VirGLGetAATarget(VIRGL_AA_MODE Mode, UINT32 Width, UINT32 Height, VIRGL_AA_TARGET *Target);
BOOLEAN VirGLDownsampleColor(UINT32 ContextId, UINT32 SourceId, UINT32 TargetId, UINT32 Width, UINT32 Height, VIRGL_AA_MODE Mode);

typedef struct
{
    UINT32 Id;
    UINT32 MaxVersion;
    UINT32 MaxSize;
} VIRGL_CAPSET_INFO;

BOOLEAN VirGLGetCapsetInfo(UINT32 Id, VIRGL_CAPSET_INFO *Info);
BOOLEAN VirGLCreateContext(UINT32 Id, CONST CHAR8 *Name);
BOOLEAN VirGLDestroyContext(UINT32 Id);
BOOLEAN VirGLReadCapset(UINT32 Id, UINT32 Version, VOID *Data, UINT32 Capacity, UINT32 *BytesRead);
BOOLEAN VirGLCreateRenderTarget(UINT32 ResourceId, UINT32 Width, UINT32 Height);
BOOLEAN VirGLDestroyResource(UINT32 ResourceId);
BOOLEAN VirGLAttachResource(UINT32 ContextId, UINT32 ResourceId);
BOOLEAN VirGLDetachResource(UINT32 ContextId, UINT32 ResourceId);
BOOLEAN VirGLClearRenderTarget(UINT32 ContextId, UINT32 ResourceId, UINT32 SurfaceId, CONST UINT32 *ColorBits);
BOOLEAN VirGLPresentRenderTarget(UINT32 ResourceId, UINT32 Width, UINT32 Height);
BOOLEAN VirGLCreateDepthTarget(UINT32 ResourceId, UINT32 Width, UINT32 Height);
BOOLEAN VirGLCreateTexture(UINT32 ResourceId, UINT32 Width, UINT32 Height);
BOOLEAN VirGLUploadTexture(UINT32 ContextId, UINT32 ResourceId, UINT32 Width, UINT32 Height, CONST VOID *Pixels);
BOOLEAN VirGLInitTexturedRects(UINT32 ContextId, UINT32 TargetId, UINT32 VerticesId, UINT32 Width, UINT32 Height);
BOOLEAN VirGLDrawTexturedRect(UINT32 ContextId, UINT32 VerticesId, UINT32 TextureId, UINT32 TargetWidth, UINT32 TargetHeight, UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT8 Opacity);
UINT32  VirGLSelectSamples(UINT32 Preferred);
BOOLEAN VirGLCreateMultisampleTarget(UINT32 ResourceId, UINT32 Width, UINT32 Height, UINT32 Samples, BOOLEAN Depth);
BOOLEAN VirGLResolveColor(UINT32 ContextId, UINT32 SourceId, UINT32 TargetId, UINT32 Width, UINT32 Height);
BOOLEAN VirGLCreateShader(UINT32 ContextId, UINT32 Handle, UINT32 Stage, CONST CHAR8 *Text, UINT32 Length);
BOOLEAN VirGLSubmit(UINT32 ContextId, CONST UINT32 *Commands, UINT32 Size);
BOOLEAN VirGLCreateVertexBuffer(UINT32 ResourceId, UINT32 Size);
BOOLEAN VirGLWriteVertexBuffer(UINT32 ContextId, UINT32 ResourceId, UINT32 Offset, CONST VOID *Data, UINT32 Size);
BOOLEAN VirGLReadColorTarget(UINT32 ContextId, UINT32 ResourceId, UINT32 Width, UINT32 Height, VOID *Pixels, BOOLEAN *CanFree);
BOOLEAN VirGLDrawPositionTriangle(UINT32 ContextId, UINT32 TargetId, UINT32 VerticesId, UINT32 Width, UINT32 Height);
