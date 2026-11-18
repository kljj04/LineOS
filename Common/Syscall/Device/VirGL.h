// Common/Syscall/Device/VirGL.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/Device/Ioctl.h>

#define LOS_VIRGL_PATH             "/dev/virgl0"
#define LOS_VIRGL_MAX_CAPSET_BYTES 65536U

typedef struct
{
    unsigned int Id;
    unsigned int MaxVersion;
    unsigned int MaxSize;
} LOS_VIRGL_CAPSET_INFO;

typedef struct
{
    unsigned int  Id;
    unsigned int  Version;
    unsigned int  Capacity;
    unsigned int  BytesRead;
    unsigned long Address;
} LOS_VIRGL_CAPSET_READ;

#define LOS_VIRGL_GET_CAPSET_INFO _IOWR('G', 0, LOS_VIRGL_CAPSET_INFO)
#define LOS_VIRGL_READ_CAPSET     _IOWR('G', 1, LOS_VIRGL_CAPSET_READ)
#define LOS_VIRGL_CREATE_CONTEXT  _IO('G', 2)

#define LOS_VIRGL_BUFFER_LIMIT     16U
#define LOS_VIRGL_MAX_BUFFER_BYTES (4U * 1024U * 1024U)
#define LOS_VIRGL_MAX_UPLOAD_BYTES 65536U

typedef struct
{
    unsigned int Size;
    unsigned int Handle;
} LOS_VIRGL_BUFFER_CREATE;

typedef struct
{
    unsigned int  Handle;
    unsigned int  Offset;
    unsigned int  Size;
    unsigned int  Reserved;
    unsigned long Address;
} LOS_VIRGL_BUFFER_WRITE;

#define LOS_VIRGL_CREATE_BUFFER  _IOWR('G', 3, LOS_VIRGL_BUFFER_CREATE)
#define LOS_VIRGL_WRITE_BUFFER   _IOW('G', 4, LOS_VIRGL_BUFFER_WRITE)
#define LOS_VIRGL_DESTROY_BUFFER _IOW('G', 5, unsigned int)

#define LOS_VIRGL_MAX_COMMAND_WORDS (65U * 1024U)

typedef struct
{
    unsigned long Address;
    unsigned int  Words;
    unsigned int  Reserved;
} LOS_VIRGL_SUBMIT;

#define LOS_VIRGL_SUBMIT_COMMANDS _IOW('G', 6, LOS_VIRGL_SUBMIT)

typedef struct
{
    unsigned int Width;
    unsigned int Height;
    unsigned int Handle;
} LOS_VIRGL_TARGET_CREATE;

#define LOS_VIRGL_CREATE_TARGET _IOWR('G', 7, LOS_VIRGL_TARGET_CREATE)

typedef struct
{
    unsigned int  Handle;
    unsigned int  Capacity;
    unsigned long Address;
} LOS_VIRGL_TARGET_READ;

#define LOS_VIRGL_READ_TARGET _IOW('G', 8, LOS_VIRGL_TARGET_READ)
