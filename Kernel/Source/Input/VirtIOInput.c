// Kernel/Source/Input/VirtIOInput.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Input/VirtIOInput.h>
#include <Memory/PMM.h>
#include <PCI/PCI.h>
#include <Arch/AMD64/SpinLock.h>
#include <VirtIO/VirtIOPCI.h>
#include <VirtIO/VirtQueue.h>

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
STATIC SPIN_LOCK                KeyboardQueueLock;
STATIC SPIN_LOCK                TabletQueueLock;
STATIC SPIN_LOCK                KeyEventLock;
STATIC SPIN_LOCK                PointerEventLock;

STATIC CONST CHAR16 *LastError = L"not initialized";

STATIC BOOLEAN BitmapTest(CONST UINT8 *Bitmap, UINT32 Bit)
{
    return (Bitmap[Bit / 8] & (1U << (Bit % 8))) != 0;
}

STATIC UINT8 ReadConfigBitmap(VIRTIO_INPUT_DEVICE_INFO *InputDevice, UINT8 EventType, UINT8 *Bitmap)
{
    VOLATILE VIRTIO_INPUT_CONFIG *Config;
    UINT8                         Size;
    UINT32                        Index;

    Config = (VOLATILE VIRTIO_INPUT_CONFIG *) InputDevice->Device.DeviceConfig;

    Config->Select = VIRTIO_INPUT_CFG_EV_BITS;
    Config->SubSelect = EventType;

    Size = Config->Size;

    if (Size > 128)
    {
        Size = 128;
    }

    KMemSet(Bitmap, 0, 128);

    for (Index = 0; Index < Size; Index++)
    {
        Bitmap[Index] = Config->Bitmap[Index];
    }

    return Size;
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
    UINT16 Index;
    UINT16 Count;

    Count = InputDevice->EventQueue.Size;

    if (Count > VIRTIO_INPUT_EVENT_BUFFER_COUNT)
    {
        Count = VIRTIO_INPUT_EVENT_BUFFER_COUNT;
    }

    InputDevice->EventBufferCount = Count;

    for (Index = 0; Index < Count; Index++)
    {
        KMemSet(&InputDevice->EventBuffers[Index].Event, 0, sizeof(VIRTIO_INPUT_EVENT));

        if (!VirtQueuePostBuffer(&InputDevice->Device, &InputDevice->EventQueue, &InputDevice->EventBuffers[Index].Event, sizeof(VIRTIO_INPUT_EVENT), VIRTQ_DESC_F_WRITE, &InputDevice->EventBuffers[Index].Head))
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
    UINT64 Flags;

    Flags = SpinLockAcquireIRQSave(&KeyEventLock);
    NextIndex = (KeyWriteIndex + 1) % VIRTIO_INPUT_KEY_QUEUE_SIZE;

    if (NextIndex == KeyReadIndex)
    {
        SpinLockReleaseIRQRestore(&KeyEventLock, Flags);
        return;
    }

    KeyEvents[KeyWriteIndex].Code = Code;
    KeyEvents[KeyWriteIndex].Pressed = Pressed;

    KeyWriteIndex = NextIndex;
    SpinLockReleaseIRQRestore(&KeyEventLock, Flags);
}

STATIC VOID PushPointerEvent(VIRTIO_INPUT_DEVICE_INFO *InputDevice)
{
    UINT32 NextIndex;
    UINT64 Flags;

    Flags = SpinLockAcquireIRQSave(&PointerEventLock);
    NextIndex = (PointerWriteIndex + 1) % VIRTIO_INPUT_POINTER_QUEUE_SIZE;

    if (NextIndex == PointerReadIndex)
    {
        SpinLockReleaseIRQRestore(&PointerEventLock, Flags);
        return;
    }

    PointerEvents[PointerWriteIndex].X = InputDevice->AbsX;
    PointerEvents[PointerWriteIndex].Y = InputDevice->AbsY;
    PointerEvents[PointerWriteIndex].LeftButton = InputDevice->LeftButton;
    PointerEvents[PointerWriteIndex].RightButton = InputDevice->RightButton;
    PointerEvents[PointerWriteIndex].WheelButton = InputDevice->WheelButton;
    PointerEvents[PointerWriteIndex].BackButton = InputDevice->BackButton;
    PointerEvents[PointerWriteIndex].ForwardButton = InputDevice->ForwardButton;
    PointerEvents[PointerWriteIndex].Wheel = InputDevice->Wheel;

    PointerWriteIndex = NextIndex;
    SpinLockReleaseIRQRestore(&PointerEventLock, Flags);
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

    if (Event->Type == VIRTIO_INPUT_EV_REL)
    {
        if (Event->Code == VIRTIO_INPUT_REL_WHEEL)
        {
            INT32 Diff = (INT32) Event->Value;
            INT8  Direction = 0;

            if (Diff > 0)
            {
                Direction = 1;
            }
            else if (Diff < 0)
            {
                Direction = -1;
            }

            if (Direction != 0)
            {
                if (InputDevice->WheelDirection != Direction)
                {
                    InputDevice->Wheel = 0;
                    InputDevice->WheelDirection = Direction;
                }

                if (Diff > 0 && InputDevice->Wheel > INT32_MAX - Diff)
                {
                    InputDevice->Wheel = INT32_MAX;
                }
                else if (Diff < 0 && InputDevice->Wheel < INT32_MIN - Diff)
                {
                    InputDevice->Wheel = INT32_MIN;
                }
                else
                {
                    InputDevice->Wheel += Diff;
                }
            }
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
            InputDevice->WheelButton = Event->Value != 0;
        }
        else if (Event->Code == VIRTIO_INPUT_BTN_BACK)
        {
            InputDevice->BackButton = Event->Value != 0;
        }
        else if (Event->Code == VIRTIO_INPUT_BTN_FORWARD)
        {
            InputDevice->ForwardButton = Event->Value != 0;
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
    UINT16 Index;

    for (Index = 0; Index < InputDevice->EventBufferCount; Index++)
    {
        if (InputDevice->EventBuffers[Index].Head == Head)
        {
            return &InputDevice->EventBuffers[Index];
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

STATIC VOID ProcessInputQueue(VIRTIO_INPUT_DEVICE_INFO *InputDevice, SPIN_LOCK *QueueLock)
{
    VIRTIO_INPUT_EVENT_BUFFER *EventBuffer;
    UINT16                     Head;
    UINT32                     Length;
    UINT64                     Flags;

    if (InputDevice == NULL || !InputDevice->Found)
    {
        return;
    }

    Flags = SpinLockAcquireIRQSave(QueueLock);
    AcknowledgeInputInterrupt(InputDevice);

    while (VirtQueuePopUsed(&InputDevice->EventQueue, &Head, &Length))
    {
        EventBuffer = FindEventBuffer(InputDevice, Head);

        if (EventBuffer != NULL && Length >= sizeof(VIRTIO_INPUT_EVENT))
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

        VirtQueueFreeChain(&InputDevice->EventQueue, Head);

        if (EventBuffer == NULL)
        {
            LastError = L"virtio input event buffer missing";
            continue;
        }

        KMemSet(&EventBuffer->Event, 0, sizeof(VIRTIO_INPUT_EVENT));

        if (!VirtQueuePostBuffer(&InputDevice->Device, &InputDevice->EventQueue, &EventBuffer->Event, sizeof(VIRTIO_INPUT_EVENT), VIRTQ_DESC_F_WRITE, &EventBuffer->Head))
        {
            LastError = VirtQueueGetLastError();
            SpinLockReleaseIRQRestore(QueueLock, Flags);
            return;
        }
    }

    LastError = L"ok";
    SpinLockReleaseIRQRestore(QueueLock, Flags);
}

BOOLEAN VirtIOInputInit(VOID)
{
    PCI_DEVICE              *PCIDevice;
    VIRTIO_INPUT_DEVICE_TYPE Type;
    UINT32                   Index;

    KMemSet(&Keyboard, 0, sizeof(VIRTIO_INPUT_DEVICE_INFO));
    KMemSet(&Tablet, 0, sizeof(VIRTIO_INPUT_DEVICE_INFO));
    KMemSet(KeyEvents, 0, sizeof(KeyEvents));
    KMemSet(PointerEvents, 0, sizeof(PointerEvents));
    SpinLockInit(&KeyboardQueueLock);
    SpinLockInit(&TabletQueueLock);
    SpinLockInit(&KeyEventLock);
    SpinLockInit(&PointerEventLock);

    KeyReadIndex = 0;
    KeyWriteIndex = 0;
    PointerReadIndex = 0;
    PointerWriteIndex = 0;

    for (Index = 0; Index < PCIGetDeviceCount(); Index++)
    {
        PCIDevice = PCIGetDevice(Index);

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
    ProcessInputQueue(&Keyboard, &KeyboardQueueLock);
}

VOID VirtIOTabletInterruptHandler(VOID)
{
    ProcessInputQueue(&Tablet, &TabletQueueLock);
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
    BOOLEAN Pending;
    UINT64  Flags;

    Flags = SpinLockAcquireIRQSave(&KeyEventLock);
    Pending = KeyReadIndex != KeyWriteIndex;
    SpinLockReleaseIRQRestore(&KeyEventLock, Flags);

    if (Pending)
    {
        return TRUE;
    }

    Flags = SpinLockAcquireIRQSave(&PointerEventLock);
    Pending = PointerReadIndex != PointerWriteIndex;
    SpinLockReleaseIRQRestore(&PointerEventLock, Flags);

    return Pending;
}

BOOLEAN VirtIOInputGetKeyEvent(VIRTIO_KEY_EVENT *Event)
{
    UINT64 Flags;

    if (Event == NULL)
    {
        return FALSE;
    }

    Flags = SpinLockAcquireIRQSave(&KeyEventLock);
    if (KeyReadIndex == KeyWriteIndex)
    {
        SpinLockReleaseIRQRestore(&KeyEventLock, Flags);
        return FALSE;
    }

    *Event = KeyEvents[KeyReadIndex];
    KeyReadIndex = (KeyReadIndex + 1) % VIRTIO_INPUT_KEY_QUEUE_SIZE;

    SpinLockReleaseIRQRestore(&KeyEventLock, Flags);
    return TRUE;
}

BOOLEAN VirtIOInputGetPointerEvent(VIRTIO_POINTER_EVENT *Event)
{
    UINT64 Flags;

    if (Event == NULL)
    {
        return FALSE;
    }

    Flags = SpinLockAcquireIRQSave(&PointerEventLock);
    if (PointerReadIndex == PointerWriteIndex)
    {
        SpinLockReleaseIRQRestore(&PointerEventLock, Flags);
        return FALSE;
    }

    *Event = PointerEvents[PointerReadIndex];
    PointerReadIndex = (PointerReadIndex + 1) % VIRTIO_INPUT_POINTER_QUEUE_SIZE;

    SpinLockReleaseIRQRestore(&PointerEventLock, Flags);
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
