// Common/ELF/ELF64.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

#define ELF64_IDENT_SIZE 16

#define ELF64_MAGIC0 0x7F
#define ELF64_MAGIC1 'E'
#define ELF64_MAGIC2 'L'
#define ELF64_MAGIC3 'F'

#define ELF64_EI_MAG0       0
#define ELF64_EI_MAG1       1
#define ELF64_EI_MAG2       2
#define ELF64_EI_MAG3       3
#define ELF64_EI_CLASS      4
#define ELF64_EI_DATA       5
#define ELF64_EI_VERSION    6
#define ELF64_EI_OSABI      7
#define ELF64_EI_ABIVERSION 8

#define ELF64_CLASS_NONE 0
#define ELF64_CLASS_32   1
#define ELF64_CLASS_64   2

#define ELF64_DATA_NONE 0
#define ELF64_DATA_LSB  1
#define ELF64_DATA_MSB  2

#define ELF64_VERSION_CURRENT 1

#define ELF64_OSABI_SYSV  0
#define ELF64_OSABI_LINUX 3

#define ELF64_ET_NONE 0
#define ELF64_ET_REL  1
#define ELF64_ET_EXEC 2
#define ELF64_ET_DYN  3
#define ELF64_ET_CORE 4

#define ELF64_EM_NONE   0
#define ELF64_EM_X86_64 62

#define ELF64_PT_NULL    0
#define ELF64_PT_LOAD    1
#define ELF64_PT_DYNAMIC 2
#define ELF64_PT_INTERP  3
#define ELF64_PT_NOTE    4
#define ELF64_PT_SHLIB   5
#define ELF64_PT_PHDR    6
#define ELF64_PT_TLS     7

#define ELF64_PT_GNU_EH_FRAME 0x6474E550
#define ELF64_PT_GNU_STACK    0x6474E551
#define ELF64_PT_GNU_RELRO    0x6474E552

#define ELF64_PF_X 0x1
#define ELF64_PF_W 0x2
#define ELF64_PF_R 0x4

#define ELF64_SHT_NULL     0
#define ELF64_SHT_PROGBITS 1
#define ELF64_SHT_SYMTAB   2
#define ELF64_SHT_STRTAB   3
#define ELF64_SHT_RELA     4
#define ELF64_SHT_HASH     5
#define ELF64_SHT_DYNAMIC  6
#define ELF64_SHT_NOTE     7
#define ELF64_SHT_NOBITS   8
#define ELF64_SHT_REL      9
#define ELF64_SHT_DYNSYM   11

#define ELF64_SHF_WRITE     0x1
#define ELF64_SHF_ALLOC     0x2
#define ELF64_SHF_EXECINSTR 0x4

#define ELF64_SHN_UNDEF  0
#define ELF64_SHN_ABS    0xFFF1
#define ELF64_SHN_COMMON 0xFFF2

#define ELF64_STB_LOCAL  0
#define ELF64_STB_GLOBAL 1
#define ELF64_STB_WEAK   2

#define ELF64_STT_NOTYPE  0
#define ELF64_STT_OBJECT  1
#define ELF64_STT_FUNC    2
#define ELF64_STT_SECTION 3
#define ELF64_STT_FILE    4
#define ELF64_STT_COMMON  5
#define ELF64_STT_TLS     6

#define ELF64_STV_DEFAULT   0
#define ELF64_STV_INTERNAL  1
#define ELF64_STV_HIDDEN    2
#define ELF64_STV_PROTECTED 3

#define ELF64_DT_NULL         0
#define ELF64_DT_NEEDED       1
#define ELF64_DT_PLTRELSZ     2
#define ELF64_DT_PLTGOT       3
#define ELF64_DT_HASH         4
#define ELF64_DT_STRTAB       5
#define ELF64_DT_SYMTAB       6
#define ELF64_DT_RELA         7
#define ELF64_DT_RELASZ       8
#define ELF64_DT_RELAENT      9
#define ELF64_DT_STRSZ        10
#define ELF64_DT_SYMENT       11
#define ELF64_DT_INIT         12
#define ELF64_DT_FINI         13
#define ELF64_DT_SONAME       14
#define ELF64_DT_RPATH        15
#define ELF64_DT_SYMBOLIC     16
#define ELF64_DT_REL          17
#define ELF64_DT_RELSZ        18
#define ELF64_DT_RELENT       19
#define ELF64_DT_PLTREL       20
#define ELF64_DT_DEBUG        21
#define ELF64_DT_TEXTREL      22
#define ELF64_DT_JMPREL       23
#define ELF64_DT_BIND_NOW     24
#define ELF64_DT_INIT_ARRAY   25
#define ELF64_DT_FINI_ARRAY   26
#define ELF64_DT_INIT_ARRAYSZ 27
#define ELF64_DT_FINI_ARRAYSZ 28
#define ELF64_DT_RUNPATH      29
#define ELF64_DT_FLAGS        30
#define ELF64_DT_GNU_HASH     0x6FFFFEF5

#define ELF64_R_X86_64_NONE      0
#define ELF64_R_X86_64_64        1
#define ELF64_R_X86_64_PC32      2
#define ELF64_R_X86_64_GOT32     3
#define ELF64_R_X86_64_PLT32     4
#define ELF64_R_X86_64_COPY      5
#define ELF64_R_X86_64_GLOB_DAT  6
#define ELF64_R_X86_64_JUMP_SLOT 7
#define ELF64_R_X86_64_RELATIVE  8
#define ELF64_R_X86_64_GOTPCREL  9
#define ELF64_R_X86_64_32        10
#define ELF64_R_X86_64_32S       11
#define ELF64_R_X86_64_16        12
#define ELF64_R_X86_64_PC16      13
#define ELF64_R_X86_64_8         14
#define ELF64_R_X86_64_PC8       15
#define ELF64_R_X86_64_DTPMOD64  16
#define ELF64_R_X86_64_DTPOFF64  17
#define ELF64_R_X86_64_TPOFF64   18
#define ELF64_R_X86_64_IRELATIVE 37

#define ELF64_R_SYM(Info)          ((UINT32) ((Info) >> 32))
#define ELF64_R_TYPE(Info)         ((UINT32) (Info))
#define ELF64_R_INFO(Symbol, Type) (((UINT64) (Symbol) << 32) | (UINT32) (Type))
#define ELF64_ST_BIND(Info)        ((UINT8) ((Info) >> 4))
#define ELF64_ST_TYPE(Info)        ((UINT8) ((Info) & 0xF))
#define ELF64_ST_INFO(Bind, Type)  ((UINT8) (((Bind) << 4) | ((Type) & 0xF)))
#define ELF64_ST_VISIBILITY(Other) ((UINT8) ((Other) & 0x3))

typedef struct
{
    UINT8  Ident[ELF64_IDENT_SIZE];
    UINT16 Type;
    UINT16 Machine;
    UINT32 Version;
    UINT64 Entry;
    UINT64 PHOff;
    UINT64 SHOff;
    UINT32 Flags;
    UINT16 EHSize;
    UINT16 PHEntSize;
    UINT16 PHNum;
    UINT16 SHEntSize;
    UINT16 SHNum;
    UINT16 SHStrNdx;
} ELF64_HEADER;

typedef struct
{
    UINT32 Type;
    UINT32 Flags;
    UINT64 Offset;
    UINT64 VAddr;
    UINT64 PAddr;
    UINT64 FileSize;
    UINT64 MemSize;
    UINT64 Align;
} ELF64_PROGRAM_HEADER;

typedef struct
{
    UINT32 Name;
    UINT32 Type;
    UINT64 Flags;
    UINT64 Addr;
    UINT64 Offset;
    UINT64 Size;
    UINT32 Link;
    UINT32 Info;
    UINT64 AddrAlign;
    UINT64 EntSize;
} ELF64_SECTION_HEADER;

typedef struct
{
    UINT32 Name;
    UINT8  Info;
    UINT8  Other;
    UINT16 SHNdx;
    UINT64 Value;
    UINT64 Size;
} ELF64_SYMBOL;

typedef struct
{
    UINT64 Offset;
    UINT64 Info;
} ELF64_REL;

typedef struct
{
    UINT64 Offset;
    UINT64 Info;
    INT64  Addend;
} ELF64_RELA;

typedef struct
{
    INT64 Tag;
    union
    {
        UINT64 Value;
        UINT64 Pointer;
    } Data;
} ELF64_DYNAMIC;

STATIC_ASSERT(sizeof(ELF64_HEADER) == 64, "Invalid ELF64 header size");
STATIC_ASSERT(sizeof(ELF64_PROGRAM_HEADER) == 56, "Invalid ELF64 program header size");
STATIC_ASSERT(sizeof(ELF64_SECTION_HEADER) == 64, "Invalid ELF64 section header size");
STATIC_ASSERT(sizeof(ELF64_SYMBOL) == 24, "Invalid ELF64 symbol size");
STATIC_ASSERT(sizeof(ELF64_REL) == 16, "Invalid ELF64 REL size");
STATIC_ASSERT(sizeof(ELF64_RELA) == 24, "Invalid ELF64 RELA size");
STATIC_ASSERT(sizeof(ELF64_DYNAMIC) == 16, "Invalid ELF64 dynamic entry size");
