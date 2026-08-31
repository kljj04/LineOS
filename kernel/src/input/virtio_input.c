// kernel/src/input/virtio_input.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <input/virtio_input.h>
#include <memory/memory.h>
#include <pci/pci.h>
#include <virtio/virtio_pci.h>
#include <virtio/virtqueue.h>

#define VIRTIO_INPUT_KEYBOARD_VECTOR 65
#define VIRTIO_INPUT_TABLET_VECTOR   66

STATIC VIRTIO_INPUT_DEVICE_INFO Keyboard;
STATIC VIRTIO_INPUT_DEVICE_INFO Tablet;
STATIC VIRTIO_KEY_EVENT         KeyEvents[VIRTIO_INPUT_KEY_QUEUE_SIZE];
STATIC UINT32                   KeyReadIndex;
STATIC UINT32                   KeyWriteIndex;
STATIC VIRTIO_POINTER_EVENT     PointerEvents[VIRTIO_INPUT_POINTER_QUEUE_SIZE];
STATIC UINT32                   PointerReadIndex;
STATIC UINT32                   PointerWriteIndex;

STATIC CONST CHAR16 *LastError = L"not initialized";

STATIC BOOLEAN BitmapTest(CONST UINT8 *Bitmap, UINT32 Bit)
{
    return (Bitmap[Bit / 8] & (1U << (Bit % 8))) != 0;
}

STATIC UINT8 ReadConfigBitmap(VIRTIO_INPUT_DEVICE_INFO *InputDevice, UINT8 EventType, UINT8 *Bitmap)
{
    VOLATILE VIRTIO_INPUT_CONFIG *Config;
    UINT8                         size;
    UINT32                        index;

    Config = (VOLATILE VIRTIO_INPUT_CONFIG *) InputDevice->Device.DeviceConfig;

    Config->Select = VIRTIO_INPUT_CFG_EV_BITS;
    Config->SubSelect = EventType;

    size = Config->Size;

    if (size > 128)
    {
        size = 128;
    }

    KMemSet(Bitmap, 0, 128);

    for (index = 0; index < size; index++)
    {
        Bitmap[index] = Config->Bitmap[index];
    }

    return size;
}

STATIC BOOLEAN ReadABSInfo(VIRTIO_INPUT_DEVICE_INFO *InputDevice, UINT8 Axis, VIRTIO_INPUT_ABS_INFO *Info)
{
    VOLATILE VIRTIO_INPUT_CONFIG *Config;

    Config = (VOLATILE VIRTIO_INPUT_CONFIG *) InputDevice->Device.DeviceConfig;

    Config->Select = VIRTIO_INPUT_CFG_ABS_INFO;
    Config->SubSelect = Axis;

    if (Config->Size < sizeof(VIRTIO_INPUT_ABS_INFO))
    {
        return FALSE;
    }

    Info->Min = Config->Abs.Min;
    Info->Max = Config->Abs.Max;
    Info->Fuzz = Config->Abs.Fuzz;
    Info->Flat = Config->Abs.Flat;
    Info->Resolution = Config->Abs.Resolution;

    return TRUE;
}

STATIC VIRTIO_INPUT_DEVICE_TYPE DetectDeviceType(VIRTIO_INPUT_DEVICE_INFO *InputDevice)
{
    UINT8 KeyBitmap[128];
    UINT8 ABSBitmap[128];
    UINT8 KeySize;
    UINT8 ABSSize;

    KeySize = ReadConfigBitmap(InputDevice, VIRTIO_INPUT_EV_KEY, KeyBitmap);
    ABSSize = ReadConfigBitmap(InputDevice, VIRTIO_INPUT_EV_ABS, ABSBitmap);

    if (ABSSize != 0 && BitmapTest(ABSBitmap, VIRTIO_INPUT_ABS_X) && BitmapTest(ABSBitmap, VIRTIO_INPUT_ABS_Y))
    {
        return VIRTIO_INPUT_DEVICE_TABLET;
    }

    if (KeySize != 0)
    {
        return VIRTIO_INPUT_DEVICE_KEYBOARD;
    }

    return VIRTIO_INPUT_DEVICE_UNKNOWN;
}

STATIC BOOLEAN PostEventBuffers(VIRTIO_INPUT_DEVICE_INFO *InputDevice)
{
    UINT16 index;
    UINT16 count;

    count = InputDevice->EventQueue.Size;

    if (count > VIRTIO_INPUT_EVENT_BUFFER_COUNT)
    {
        count = VIRTIO_INPUT_EVENT_BUFFER_COUNT;
    }

    InputDevice->EventBufferCount = count;

    for (index = 0; index < count; index++)
    {
        KMemSet(&InputDevice->EventBuffers[index].Event, 0, sizeof(VIRTIO_INPUT_EVENT));

        if (!VirtQueuePostBuffer(&InputDevice->Device, &InputDevice->EventQueue, &InputDevice->EventBuffers[index].Event, sizeof(VIRTIO_INPUT_EVENT), VIRTQ_DESC_F_WRITE, &InputDevice->EventBuffers[index].Head))
        {
            LastError = VirtQueueGetLastError();
            return FALSE;
        }
    }

    return TRUE;
}

STATIC BOOLEAN InitInputDevice(PCI_DEVICE *PCIDevice, VIRTIO_INPUT_DEVICE_INFO *InputDevice)
{
    VIRTIO_INPUT_ABS_INFO XInfo;
    VIRTIO_INPUT_ABS_INFO YInfo;
    UINT8                 Vector;

    KMemSet(InputDevice, 0, sizeof(VIRTIO_INPUT_DEVICE_INFO));

    if (!VirtIOPCIInitDevice(&InputDevice->Device, PCIDevice))
    {
        LastError = VirtIOPCIGetLastError();
        return FALSE;
    }

    if (!VirtIOPCIStartDevice(&InputDevice->Device))
    {
        LastError = VirtIOPCIGetLastError();
        return FALSE;
    }

    InputDevice->Type = DetectDeviceType(InputDevice);

    if (InputDevice->Type == VIRTIO_INPUT_DEVICE_UNKNOWN)
    {
        LastError = L"unknown virtio input device";
        return FALSE;
    }

    if (!VirtQueueInit(&InputDevice->Device, &InputDevice->EventQueue, VIRTIO_INPUT_EVENT_QUEUE, VIRTIO_INPUT_QUEUE_SIZE))
    {
        LastError = VirtQueueGetLastError();
        return FALSE;
    }

    Vector = InputDevice->Type == VIRTIO_INPUT_DEVICE_KEYBOARD ? VIRTIO_INPUT_KEYBOARD_VECTOR : VIRTIO_INPUT_TABLET_VECTOR;

    if (!VirtIOPCIEnableQueueMSIX(&InputDevice->Device, VIRTIO_INPUT_EVENT_QUEUE, Vector))
    {
        LastError = VirtIOPCIGetLastError();
        return FALSE;
    }

    if (InputDevice->Type == VIRTIO_INPUT_DEVICE_TABLET)
    {
        if (!ReadABSInfo(InputDevice, VIRTIO_INPUT_ABS_X, &XInfo) || !ReadABSInfo(InputDevice, VIRTIO_INPUT_ABS_Y, &YInfo))
        {
            LastError = L"tablet abs info missing";
            return FALSE;
        }

        InputDevice->AbsMinX = XInfo.Min;
        InputDevice->AbsMaxX = XInfo.Max;
        InputDevice->AbsMinY = YInfo.Min;
        InputDevice->AbsMaxY = YInfo.Max;
    }

    if (!PostEventBuffers(InputDevice))
    {
        return FALSE;
    }

    VirtIOPCIReadyDevice(&InputDevice->Device);

    InputDevice->Found = TRUE;

    LastError = L"ok";
    return TRUE;
}

STATIC VIRTIO_INPUT_DEVICE_TYPE ProbeInputDevice(PCI_DEVICE *PCIDevice)
{
    VIRTIO_INPUT_DEVICE_INFO InputDevice;
    VIRTIO_INPUT_DEVICE_TYPE Type;

    KMemSet(&InputDevice, 0, sizeof(VIRTIO_INPUT_DEVICE_INFO));

    if (!VirtIOPCIInitDevice(&InputDevice.Device, PCIDevice))
    {
        return VIRTIO_INPUT_DEVICE_UNKNOWN;
    }

    if (!VirtIOPCIStartDevice(&InputDevice.Device))
    {
        return VIRTIO_INPUT_DEVICE_UNKNOWN;
    }

    Type = DetectDeviceType(&InputDevice);

    InputDevice.Device.CommonConfig->DeviceStatus = 0;

    return Type;
}

STATIC VOID PushKeyEvent(UINT16 Code, BOOLEAN Pressed)
{
    UINT32 NextIndex;

    NextIndex = (KeyWriteIndex + 1) % VIRTIO_INPUT_KEY_QUEUE_SIZE;

    if (NextIndex == KeyReadIndex)
    {
        return;
    }

    KeyEvents[KeyWriteIndex].Code = Code;
    KeyEvents[KeyWriteIndex].Pressed = Pressed;

    KeyWriteIndex = NextIndex;
}

STATIC VOID PushPointerEvent(VIRTIO_INPUT_DEVICE_INFO *InputDevice)
{
    UINT32 NextIndex;

    NextIndex = (PointerWriteIndex + 1) % VIRTIO_INPUT_POINTER_QUEUE_SIZE;

    if (NextIndex == PointerReadIndex)
    {
        return;
    }

    PointerEvents[PointerWriteIndex].X = InputDevice->AbsX;
    PointerEvents[PointerWriteIndex].Y = InputDevice->AbsY;
    PointerEvents[PointerWriteIndex].LeftButton = InputDevice->LeftButton;
    PointerEvents[PointerWriteIndex].RightButton = InputDevice->RightButton;
    PointerEvents[PointerWriteIndex].MiddleButton = InputDevice->MiddleButton;

    PointerWriteIndex = NextIndex;
}

STATIC VOID ProcessKeyboardEvent(VIRTIO_INPUT_EVENT *Event)
{
    if (Event->Type != VIRTIO_INPUT_EV_KEY)
    {
        return;
    }

    if (Event->Code >= VIRTIO_INPUT_BTN_LEFT)
    {
        return;
    }

    PushKeyEvent(Event->Code, Event->Value != 0);
}

STATIC VOID ProcessTabletEvent(VIRTIO_INPUT_DEVICE_INFO *InputDevice, VIRTIO_INPUT_EVENT *Event)
{
    if (Event->Type == VIRTIO_INPUT_EV_ABS)
    {
        if (Event->Code == VIRTIO_INPUT_ABS_X)
        {
            InputDevice->AbsX = Event->Value;
        }
        else if (Event->Code == VIRTIO_INPUT_ABS_Y)
        {
            InputDevice->AbsY = Event->Value;
        }

        return;
    }

    if (Event->Type == VIRTIO_INPUT_EV_KEY)
    {
        if (Event->Code == VIRTIO_INPUT_BTN_LEFT)
        {
            InputDevice->LeftButton = Event->Value != 0;
        }
        else if (Event->Code == VIRTIO_INPUT_BTN_RIGHT)
        {
            InputDevice->RightButton = Event->Value != 0;
        }
        else if (Event->Code == VIRTIO_INPUT_BTN_MIDDLE)
        {
            InputDevice->MiddleButton = Event->Value != 0;
        }

        return;
    }

    if (Event->Type == VIRTIO_INPUT_EV_SYN && Event->Code == VIRTIO_INPUT_SYN_REPORT)
    {
        PushPointerEvent(InputDevice);
    }
}

STATIC VIRTIO_INPUT_EVENT_BUFFER *FindEventBuffer(VIRTIO_INPUT_DEVICE_INFO *InputDevice, UINT16 Head)
{
    UINT16 index;

    for (index = 0; index < InputDevice->EventBufferCount; index++)
    {
        if (InputDevice->EventBuffers[index].Head == Head)
        {
            return &InputDevice->EventBuffers[index];
        }
    }

    return NULL;
}

STATIC VOID AcknowledgeInputInterrupt(VIRTIO_INPUT_DEVICE_INFO *InputDevice)
{
    VOLATILE UINT8 *ISRStatus;

    ISRStatus = InputDevice->Device.ISRStatus;

    if (ISRStatus == NULL)
    {
        return;
    }

    if ((*ISRStatus & 0x3) == 0)
    {
        return;
    }
}

STATIC VOID ProcessInputQueue(VIRTIO_INPUT_DEVICE_INFO *InputDevice)
{
    VIRTIO_INPUT_EVENT_BUFFER *EventBuffer;
    UINT16                     head;
    UINT32                     length;

    if (InputDevice == NULL || !InputDevice->Found)
    {
        return;
    }

    AcknowledgeInputInterrupt(InputDevice);

    while (VirtQueuePopUsed(&InputDevice->EventQueue, &head, &length))
    {
        EventBuffer = FindEventBuffer(InputDevice, head);

        if (EventBuffer != NULL && length >= sizeof(VIRTIO_INPUT_EVENT))
        {
            if (InputDevice->Type == VIRTIO_INPUT_DEVICE_KEYBOARD)
            {
                ProcessKeyboardEvent(&EventBuffer->Event);
            }
            else if (InputDevice->Type == VIRTIO_INPUT_DEVICE_TABLET)
            {
                ProcessTabletEvent(InputDevice, &EventBuffer->Event);
            }
        }

        VirtQueueFreeChain(&InputDevice->EventQueue, head);

        if (EventBuffer == NULL)
        {
            LastError = L"virtio input event buffer missing";
            continue;
        }

        KMemSet(&EventBuffer->Event, 0, sizeof(VIRTIO_INPUT_EVENT));

        if (!VirtQueuePostBuffer(&InputDevice->Device, &InputDevice->EventQueue, &EventBuffer->Event, sizeof(VIRTIO_INPUT_EVENT), VIRTQ_DESC_F_WRITE, &EventBuffer->Head))
        {
            LastError = VirtQueueGetLastError();
            return;
        }
    }

    LastError = L"ok";
}

BOOLEAN VirtIOInputInit(VOID)
{
    PCI_DEVICE              *PCIDevice;
    VIRTIO_INPUT_DEVICE_TYPE Type;
    UINT32                   index;

    KMemSet(&Keyboard, 0, sizeof(VIRTIO_INPUT_DEVICE_INFO));
    KMemSet(&Tablet, 0, sizeof(VIRTIO_INPUT_DEVICE_INFO));
    KMemSet(KeyEvents, 0, sizeof(KeyEvents));
    KMemSet(PointerEvents, 0, sizeof(PointerEvents));

    KeyReadIndex = 0;
    KeyWriteIndex = 0;
    PointerReadIndex = 0;
    PointerWriteIndex = 0;

    for (index = 0; index < PCIGetDeviceCount(); index++)
    {
        PCIDevice = PCIGetDevice(index);

        if (PCIDevice == NULL)
        {
            continue;
        }

        if (PCIDevice->VendorId != VIRTIO_INPUT_VENDOR_ID || PCIDevice->DeviceId != VIRTIO_INPUT_DEVICE_ID)
        {
            continue;
        }

        Type = ProbeInputDevice(PCIDevice);

        if (Type == VIRTIO_INPUT_DEVICE_KEYBOARD && !Keyboard.Found)
        {
            if (!InitInputDevice(PCIDevice, &Keyboard))
            {
                continue;
            }
        }
        else if (Type == VIRTIO_INPUT_DEVICE_TABLET && !Tablet.Found)
        {
            if (!InitInputDevice(PCIDevice, &Tablet))
            {
                continue;
            }
        }
    }

    if (!Keyboard.Found && !Tablet.Found)
    {
        LastError = L"virtio input devices not found";
        return FALSE;
    }

    LastError = L"ok";
    return TRUE;
}

VOID VirtIOKeyboardInterruptHandler(VOID)
{
    ProcessInputQueue(&Keyboard);
}

VOID VirtIOTabletInterruptHandler(VOID)
{
    ProcessInputQueue(&Tablet);
}

BOOLEAN VirtIOInputIsKeyboardAvailable(VOID)
{
    return Keyboard.Found;
}

BOOLEAN VirtIOInputIsTabletAvailable(VOID)
{
    return Tablet.Found;
}

BOOLEAN VirtIOInputHasPendingEvent(VOID)
{
    return KeyReadIndex != KeyWriteIndex || PointerReadIndex != PointerWriteIndex;
}

BOOLEAN VirtIOInputGetKeyEvent(VIRTIO_KEY_EVENT *Event)
{
    if (Event == NULL)
    {
        return FALSE;
    }

    if (KeyReadIndex == KeyWriteIndex)
    {
        return FALSE;
    }

    *Event = KeyEvents[KeyReadIndex];
    KeyReadIndex = (KeyReadIndex + 1) % VIRTIO_INPUT_KEY_QUEUE_SIZE;

    return TRUE;
}

BOOLEAN VirtIOInputGetPointerEvent(VIRTIO_POINTER_EVENT *Event)
{
    if (Event == NULL)
    {
        return FALSE;
    }

    if (PointerReadIndex == PointerWriteIndex)
    {
        return FALSE;
    }

    *Event = PointerEvents[PointerReadIndex];
    PointerReadIndex = (PointerReadIndex + 1) % VIRTIO_INPUT_POINTER_QUEUE_SIZE;

    return TRUE;
}

CONST CHAR16 *VirtIOInputGetLastError(VOID)
{
    return LastError;
}

BOOLEAN VirtIOInputGetTabletRange(UINT32 *MinX, UINT32 *MaxX, UINT32 *MinY, UINT32 *MaxY)
{
    if (!Tablet.Found)
    {
        return FALSE;
    }

    *MinX = Tablet.AbsMinX;
    *MaxX = Tablet.AbsMaxX;
    *MinY = Tablet.AbsMinY;
    *MaxY = Tablet.AbsMaxY;

    return TRUE;
}
