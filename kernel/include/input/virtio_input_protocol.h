// kernel/include/input/virtio_input_protocol.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <virtio/virtqueue.h>
#include <lineos/typeinfo.h>

#define VIRTIO_INPUT_CFG_UNSET          0x00
#define VIRTIO_INPUT_CFG_ID_NAME        0x01
#define VIRTIO_INPUT_CFG_ID_SERIAL      0x02
#define VIRTIO_INPUT_CFG_ID_DEVIDS      0x03
#define VIRTIO_INPUT_CFG_PROP_BITS      0x10
#define VIRTIO_INPUT_CFG_EV_BITS        0x11
#define VIRTIO_INPUT_CFG_ABS_INFO       0x12
#define VIRTIO_INPUT_EV_SYN             0x00
#define VIRTIO_INPUT_EV_KEY             0x01
#define VIRTIO_INPUT_EV_REL             0x02
#define VIRTIO_INPUT_EV_ABS             0x03
#define VIRTIO_INPUT_ABS_X              0x00
#define VIRTIO_INPUT_ABS_Y              0x01
#define VIRTIO_INPUT_BTN_LEFT           0x110
#define VIRTIO_INPUT_BTN_RIGHT          0x111
#define VIRTIO_INPUT_BTN_MIDDLE         0x112
#define VIRTIO_INPUT_SYN_REPORT         0x00
#define VIRTIO_INPUT_VENDOR_ID          0x1AF4
#define VIRTIO_INPUT_DEVICE_ID          0x1052
#define VIRTIO_INPUT_EVENT_QUEUE        0
#define VIRTIO_INPUT_QUEUE_SIZE         32
#define VIRTIO_INPUT_EVENT_BUFFER_COUNT 32
#define VIRTIO_INPUT_KEY_QUEUE_SIZE     64
#define VIRTIO_INPUT_POINTER_QUEUE_SIZE 64

typedef struct
{
    UINT16  Code;
    BOOLEAN Pressed;
} VIRTIO_KEY_EVENT;

typedef struct
{
    UINT32  X;
    UINT32  Y;
    BOOLEAN LeftButton;
    BOOLEAN RightButton;
    BOOLEAN MiddleButton;
} VIRTIO_POINTER_EVENT;

typedef struct PACKED
{
    UINT16 Type;
    UINT16 Code;
    UINT32 Value;
} VIRTIO_INPUT_EVENT;

typedef struct PACKED
{
    UINT32 Min;
    UINT32 Max;
    UINT32 Fuzz;
    UINT32 Flat;
    UINT32 Resolution;
} VIRTIO_INPUT_ABS_INFO;

typedef struct PACKED
{
    UINT16 Bustype;
    UINT16 Vendor;
    UINT16 Product;
    UINT16 Version;
} VIRTIO_INPUT_DEVICE_IDS;

typedef struct PACKED
{
    UINT8 Select;
    UINT8 SubSelect;
    UINT8 Size;
    UINT8 Reserved[5];

    union
    {
        CHAR8                   String[128];
        UINT8                   Bitmap[128];
        VIRTIO_INPUT_ABS_INFO   Abs;
        VIRTIO_INPUT_DEVICE_IDS Ids;
    };
} VIRTIO_INPUT_CONFIG;

typedef enum
{
    VIRTIO_INPUT_DEVICE_UNKNOWN,
    VIRTIO_INPUT_DEVICE_KEYBOARD,
    VIRTIO_INPUT_DEVICE_TABLET
} VIRTIO_INPUT_DEVICE_TYPE;

typedef struct
{
    VIRTIO_INPUT_EVENT Event;
    UINT16             Head;
} VIRTIO_INPUT_EVENT_BUFFER;

typedef struct
{
    BOOLEAN                   Found;
    VIRTIO_INPUT_DEVICE_TYPE  Type;
    VIRTIO_PCI_DEVICE         Device;
    VIRTQUEUE                 EventQueue;
    VIRTIO_INPUT_EVENT_BUFFER EventBuffers[VIRTIO_INPUT_EVENT_BUFFER_COUNT];
    UINT16                    EventBufferCount;
    UINT32                    AbsX;
    UINT32                    AbsY;
    UINT32                    AbsMinX;
    UINT32                    AbsMaxX;
    UINT32                    AbsMinY;
    UINT32                    AbsMaxY;
    BOOLEAN                   LeftButton;
    BOOLEAN                   RightButton;
    BOOLEAN                   MiddleButton;
} VIRTIO_INPUT_DEVICE_INFO;
