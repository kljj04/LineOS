// Kernel/Include/Syscall/File/SyscallOwnership.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

INT64 SyscallChown(UINT64 Path, UINT64 UID, UINT64 GID);
