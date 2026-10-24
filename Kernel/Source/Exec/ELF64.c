// Kernel/Source/Exec/ELF64.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <ELF/ELF64.h>
#include <Exec/ELF64.h>
#include <Memory/PMM.h>

STATIC BOOLEAN ELF64RangeValid(UINT64 Offset, UINT64 Length, UINT64 TotalSize)
{
    return Offset <= TotalSize && Length <= TotalSize - Offset;
}

STATIC BOOLEAN ELF64IsPowerOfTwo(UINT64 Value)
{
    return Value != 0 && (Value & (Value - 1)) == 0;
}

STATIC BOOLEAN ELF64ValidateLoadSegment(CONST ELF64_PROGRAM_HEADER *ProgramHeader, UINT64 FileSize)
{
    if (ProgramHeader->FileSize > ProgramHeader->MemSize || ProgramHeader->MemSize == 0)
    {
        return FALSE;
    }

    if (!ELF64RangeValid(ProgramHeader->Offset, ProgramHeader->FileSize, FileSize))
    {
        return FALSE;
    }

    if (ProgramHeader->VAddr > UINT64_MAX - ProgramHeader->MemSize)
    {
        return FALSE;
    }

    if (ProgramHeader->Align > 1)
    {
        if (!ELF64IsPowerOfTwo(ProgramHeader->Align))
        {
            return FALSE;
        }

        if ((ProgramHeader->VAddr % ProgramHeader->Align) != (ProgramHeader->Offset % ProgramHeader->Align))
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOLEAN ELF64Validate(CONST VOID *FileData, UINT64 FileSize)
{
    CONST UINT8 *Data;
    ELF64_HEADER Header;
    ELF64_PROGRAM_HEADER ProgramHeader;
    UINT64 ProgramHeaderOffset;
    UINT64 ProgramHeaderSize;
    UINT64 SegmentEnd;
    UINT32 LoadCount = 0;
    UINT32 InterpreterCount = 0;
    UINT32 DynamicCount = 0;
    BOOLEAN EntryExecutable = FALSE;

    if (FileData == NULL || FileSize < sizeof(ELF64_HEADER))
    {
        return FALSE;
    }

    Data = (CONST UINT8 *) FileData;
    KMemCpy(&Header, Data, sizeof(Header));

    if (Header.Ident[ELF64_EI_MAG0] != ELF64_MAGIC0 ||
        Header.Ident[ELF64_EI_MAG1] != ELF64_MAGIC1 ||
        Header.Ident[ELF64_EI_MAG2] != ELF64_MAGIC2 ||
        Header.Ident[ELF64_EI_MAG3] != ELF64_MAGIC3)
    {
        return FALSE;
    }

    if (Header.Ident[ELF64_EI_CLASS] != ELF64_CLASS_64 ||
        Header.Ident[ELF64_EI_DATA] != ELF64_DATA_LSB ||
        Header.Ident[ELF64_EI_VERSION] != ELF64_VERSION_CURRENT)
    {
        return FALSE;
    }

    if (Header.Type != ELF64_ET_EXEC && Header.Type != ELF64_ET_DYN)
    {
        return FALSE;
    }

    if (Header.Machine != ELF64_EM_X86_64 || Header.Version != ELF64_VERSION_CURRENT)
    {
        return FALSE;
    }

    if (Header.EHSize != sizeof(ELF64_HEADER) ||
        Header.PHEntSize != sizeof(ELF64_PROGRAM_HEADER) ||
        Header.PHNum == 0)
    {
        return FALSE;
    }

    ProgramHeaderSize = (UINT64) Header.PHNum * Header.PHEntSize;

    if (Header.PHOff < Header.EHSize ||
        !ELF64RangeValid(Header.PHOff, ProgramHeaderSize, FileSize))
    {
        return FALSE;
    }

    for (UINT32 Index = 0; Index < Header.PHNum; Index++)
    {
        ProgramHeaderOffset = Header.PHOff + (UINT64) Index * Header.PHEntSize;
        KMemCpy(&ProgramHeader, Data + ProgramHeaderOffset, sizeof(ProgramHeader));

        if (ProgramHeader.Type == ELF64_PT_LOAD)
        {
            if (!ELF64ValidateLoadSegment(&ProgramHeader, FileSize))
            {
                return FALSE;
            }

            LoadCount++;

            SegmentEnd = ProgramHeader.VAddr + ProgramHeader.MemSize;

            if ((ProgramHeader.Flags & ELF64_PF_X) != 0 &&
                Header.Entry >= ProgramHeader.VAddr &&
                Header.Entry < SegmentEnd)
            {
                EntryExecutable = TRUE;
            }
        }
        else if (ProgramHeader.Type == ELF64_PT_INTERP)
        {
            if (++InterpreterCount > 1 ||
                ProgramHeader.FileSize < 2 ||
                !ELF64RangeValid(ProgramHeader.Offset, ProgramHeader.FileSize, FileSize))
            {
                return FALSE;
            }

            if (Data[ProgramHeader.Offset + ProgramHeader.FileSize - 1] != 0)
            {
                return FALSE;
            }
        }
        else if (ProgramHeader.Type == ELF64_PT_DYNAMIC)
        {
            if (++DynamicCount > 1 ||
                ProgramHeader.FileSize == 0 ||
                ProgramHeader.FileSize % sizeof(ELF64_DYNAMIC) != 0 ||
                !ELF64RangeValid(ProgramHeader.Offset, ProgramHeader.FileSize, FileSize))
            {
                return FALSE;
            }
        }
    }

    if (LoadCount == 0)
    {
        return FALSE;
    }

    if (Header.Type == ELF64_ET_EXEC && !EntryExecutable)
    {
        return FALSE;
    }

    if (Header.Type == ELF64_ET_DYN && Header.Entry != 0 && !EntryExecutable)
    {
        return FALSE;
    }

    return TRUE;
}
