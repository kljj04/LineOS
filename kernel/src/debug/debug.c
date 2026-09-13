// kernel/src/debug/debug.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <debug/debug.h>
#include <arch/x86_64/cpu.h>

#define DEBUG_PORT 0xE9

typedef struct PACKED DEBUG_DESCRIPTOR_REGISTER
{
    UINT16 Limit;
    UINT64 Base;
} DEBUG_DESCRIPTOR_REGISTER;

STATIC VOID DebugWriteChar(CHAR8 Character)
{
    OUTB(DEBUG_PORT, (UINT8) Character);
}

VOID DebugWrite(CONST char *String)
{
    if (String == NULL)
    {
        return;
    }

    while (*String != 0)
    {
        DebugWriteChar((CHAR8) *String);
        String++;
    }
}

VOID DebugWriteLine(CONST char *String)
{
    DebugWrite(String);
    DebugWrite("\n");
}

VOID DebugWriteWide(CONST CHAR16 *String)
{
    if (String == NULL)
    {
        return;
    }

    while (*String != 0)
    {
        if (*String < 0x80)
        {
            DebugWriteChar((CHAR8) *String);
        }
        else
        {
            DebugWriteChar('?');
        }

        String++;
    }
}

VOID DebugWriteHex(UINT64 Value)
{
    CHAR8 Buffer[19];
    UINTN Index;

    Buffer[0] = '0';
    Buffer[1] = 'x';
    Buffer[18] = 0;

    for (Index = 0; Index < 16; Index++)
    {
        UINT8 Nibble;

        Nibble = (UINT8) ((Value >> ((15 - Index) * 4)) & 0xF);
        Buffer[2 + Index] = (CHAR8) (Nibble < 10 ? '0' + Nibble : 'A' + (Nibble - 10));
    }

    DebugWrite((CONST char *) Buffer);
}

VOID DebugWriteDec(UINT64 Value)
{
    CHAR8 Buffer[21];
    UINTN Index;

    if (Value == 0)
    {
        DebugWriteChar('0');
        return;
    }

    Index = sizeof(Buffer);

    while (Value != 0 && Index > 0)
    {
        Index--;
        Buffer[Index] = (CHAR8) ('0' + (Value % 10));
        Value /= 10;
    }

    while (Index < sizeof(Buffer))
    {
        DebugWriteChar(Buffer[Index]);
        Index++;
    }
}

VOID DebugDumpIretFrame(UINT64 RSP)
{
    UINT64 *Frame;

    Frame = (UINT64 *) RSP;

    DebugWrite("IRET FRAME rsp=");
    DebugWriteHex(RSP);
    DebugWrite(" rip=");
    DebugWriteHex(Frame[0]);
    DebugWrite(" cs=");
    DebugWriteHex(Frame[1]);
    DebugWrite(" rflags=");
    DebugWriteHex(Frame[2]);
    DebugWrite(" frame-rsp=");
    DebugWriteHex(Frame[3]);
    DebugWrite(" ss=");
    DebugWriteHex(Frame[4]);
    DebugWrite("\n");
}

VOID DebugDumpDescriptorState(VOID)
{
    DEBUG_DESCRIPTOR_REGISTER GDTR;
    DEBUG_DESCRIPTOR_REGISTER IDTR;
    UINT16 Cs;
    UINT16 Ss;
    UINT64 *GDT;

    ASM("sgdt %0" : "=m"(GDTR));
    ASM("sidt %0" : "=m"(IDTR));
    ASM("mov %%cs, %0" : "=r"(Cs));
    ASM("mov %%ss, %0" : "=r"(Ss));

    GDT = (UINT64 *) GDTR.Base;

    DebugWrite("DESC cs=");
    DebugWriteHex(Cs);
    DebugWrite(" ss=");
    DebugWriteHex(Ss);
    DebugWrite(" gdtr=");
    DebugWriteHex(GDTR.Base);
    DebugWrite(":");
    DebugWriteHex(GDTR.Limit);
    DebugWrite(" idtr=");
    DebugWriteHex(IDTR.Base);
    DebugWrite(":");
    DebugWriteHex(IDTR.Limit);

    if (GDTR.Base != 0 && GDTR.Limit >= 0x1F)
    {
        DebugWrite(" gdt18=");
        DebugWriteHex(GDT[3]);
    }

    DebugWrite("\n");
}
