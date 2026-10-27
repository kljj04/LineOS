// Kernel/Include/Exec/ELF64.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <LineOS/TypeInfo.h>

BOOLEAN ELF64Validate(CONST VOID *FileData, UINT64 FileSize);

#include <FS/VFS/VFS.h>
#include <Scheduler/TaskTypes.h>

#define ELF64_EXEC_MAX_PAGES 262144U
#define ELF64_ARG_MAX_COUNT  64U
#define ELF64_ARG_MAX_LENGTH 4096U
#define ELF64_ARG_MAX_BYTES  16384U

typedef struct
{
    UINT32 Count;
    UINT32 Bytes;
    UINT32 Offsets[ELF64_ARG_MAX_COUNT];
    CHAR8  Data[ELF64_ARG_MAX_BYTES];
} ELF64_ARGUMENTS;

typedef struct
{
    TASK              *Task;
    VMM_ADDRESS_SPACE *AddressSpace;
    UINT64             PageCount;
    VOID              *Pages[ELF64_EXEC_MAX_PAGES];
} ELF64_EXECUTION;

BOOLEAN ELF64Load(CONST VOID *FileData, UINT64 FileSize, ELF64_EXECUTION *Execution);
BOOLEAN ELF64Execute(VFS_CONTEXT *Context, CONST CHAR8 *Path, ELF64_EXECUTION *Execution);
BOOLEAN ELF64Release(ELF64_EXECUTION *Execution);
BOOLEAN ELF64ExecuteArguments(VFS_CONTEXT *Context, CONST CHAR8 *Path, CONST ELF64_ARGUMENTS *Arguments, ELF64_EXECUTION *Execution);

BOOLEAN ELF64ExecuteInit(VFS_CONTEXT *Context, CONST CHAR8 *Path, ELF64_EXECUTION *Execution);
