// Common/LineOS/Ioctl.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#define _IOC(Direction, Type, Number, Size) (((unsigned long) (Direction) << 30) | ((unsigned long) (Size) << 16) | ((unsigned long) (Type) << 8) | (Number))
#define _IO(Type, Number)                   _IOC(0, Type, Number, 0)
#define _IOR(Type, Number, Data)            _IOC(2, Type, Number, sizeof(Data))
#define _IOW(Type, Number, Data)            _IOC(1, Type, Number, sizeof(Data))
#define _IOWR(Type, Number, Data)           _IOC(3, Type, Number, sizeof(Data))
#define _IOC_DIR(Request)                   (((unsigned long) (Request) >> 30) & 3)
#define _IOC_SIZE(Request)                  (((unsigned long) (Request) >> 16) & 0x3FFF)

#define FBIOGET_VSCREENINFO 0x4600UL
#define FBIOGET_FSCREENINFO 0x4602UL

typedef struct
{
    unsigned int Offset;
    unsigned int Length;
    unsigned int MSBRight;
} FB_BITFIELD;

typedef struct
{
    unsigned int XRes;
    unsigned int YRes;
    unsigned int XResVirtual;
    unsigned int YResVirtual;
    unsigned int XOffset;
    unsigned int YOffset;
    unsigned int BitsPerPixel;
    unsigned int Grayscale;
    FB_BITFIELD  Red;
    FB_BITFIELD  Green;
    FB_BITFIELD  Blue;
    FB_BITFIELD  Transp;
    unsigned int NonStd;
    unsigned int Activate;
    unsigned int Height;
    unsigned int Width;
    unsigned int AccelFlags;
    unsigned int PixClock;
    unsigned int LeftMargin;
    unsigned int RightMargin;
    unsigned int UpperMargin;
    unsigned int LowerMargin;
    unsigned int HSyncLen;
    unsigned int VSyncLen;
    unsigned int Sync;
    unsigned int VMode;
    unsigned int Rotate;
    unsigned int ColorSpace;
    unsigned int Reserved[4];
} FB_VAR_SCREENINFO;

typedef struct
{
    char           ID[16];
    unsigned long  SmemStart;
    unsigned int   SmemLen;
    unsigned int   Type;
    unsigned int   TypeAux;
    unsigned int   Visual;
    unsigned short XPanStep;
    unsigned short YPanStep;
    unsigned short YWrapStep;
    unsigned int   LineLength;
    unsigned long  MMIOStart;
    unsigned int   MMIOLen;
    unsigned int   Accel;
    unsigned short Capabilities;
    unsigned short Reserved[2];
} FB_FIX_SCREENINFO;

typedef struct
{
    unsigned long long Buffer;
    unsigned long long Size;
} LOS_FB_PRESENT;

typedef struct
{
    unsigned int       X;
    unsigned int       Y;
    unsigned int       Width;
    unsigned int       Height;
    unsigned long long Buffer;
} LOS_FB_RECT;

#define LOS_FBIO_PRESENT      _IOW('L', 1, LOS_FB_PRESENT)
#define LOS_FBIO_PRESENT_RECT _IOW('L', 2, LOS_FB_RECT)
#define LOS_FBIO_FLUSH        _IO('L', 3)

#define LOS_TIO_WAIT_READ _IO('L', 4)
