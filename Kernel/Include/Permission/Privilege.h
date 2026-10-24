// Kernel/Include/Permission/Privilege.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

#define PRIVILEGE_KERNEL 0
#define PRIVILEGE_USER   3

UINT8   PrivilegeGetCPL(VOID);
BOOLEAN PrivilegeInit(VOID);
BOOLEAN PrivilegeInitCPU(VOID);
BOOLEAN PrivilegeSetKernelStack(UINT64 StackTop);
BOOLEAN PrivilegeEnterUserMode(UINT64 EntryPoint, UINT64 StackTop);