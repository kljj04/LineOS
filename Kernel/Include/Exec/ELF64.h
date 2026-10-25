// Kernel/Include/Exec/ELF64.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <LineOS/TypeInfo.h>

BOOLEAN ELF64Validate(CONST VOID *FileData, UINT64 FileSize);

#include <FS/VFS/VFS.h>
#include <Scheduler/TaskTypes.h>

#define ELF64_EXEC_MAX_PAGES 4096U

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
