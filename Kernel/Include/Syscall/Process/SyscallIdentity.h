// Kernel/Include/Syscall/Process/SyscallIdentity.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

INT64 SyscallSetUID(UINT64 UID);
INT64 SyscallSetGID(UINT64 GID);
INT64 SyscallGetUID(BOOLEAN Effective);
