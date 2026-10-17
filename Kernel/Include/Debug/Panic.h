// kernel/include/debug/panic.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>
#include <Interrupt/IDT.h>

VOID Panic(CONST CHAR16 *Msg);
VOID ExceptionPanic(INTERRUPT_FRAME *Frame);
