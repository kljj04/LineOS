// Kernel/Include/Debug/Panic.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>
#include <Interrupt/IDT.h>

BOOLEAN PanicHandleNMI(VOID);
VOID    Panic(CONST CHAR16 *Msg);
VOID    ExceptionPanic(INTERRUPT_FRAME *Frame);
