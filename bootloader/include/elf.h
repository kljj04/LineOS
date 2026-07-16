// elf.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Uefi.h>

#define ELF_MAGIC32 0x464C457F
#define PT_LOAD 1

typedef struct
{
    UINT8  Ident[16];
    UINT16 Type;
    UINT16 Machine;
    UINT32 Version;
    UINT64 Entry;
    UINT64 ProgramHeaderOffset;
    UINT64 SectionHeaderOffset;
    UINT32 Flags;
    UINT16 HeaderSize;
    UINT16 ProgramHeaderSize;
    UINT16 ProgramHeaderCount;
    UINT16 SectionHeaderSize;
    UINT16 SectionHeaderCount;
    UINT16 SectionHeaderStringIndex;
} ELF64_HEADER;

typedef struct
{
    UINT32 Type;
    UINT32 Flags;
    UINT64 Offset;
    UINT64 VirtualAddress;
    UINT64 PhysicalAddress;
    UINT64 FileSize;
    UINT64 MemorySize;
    UINT64 Align;
} ELF64_PROGRAM_HEADER;

typedef struct
{
    UINT64 Base;
    UINT64 Entry;
    UINT64 Size;
} LINEOS_KERNEL;

BOOLEAN LoadKernel(void);

extern LINEOS_KERNEL Kernel;