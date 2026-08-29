// kernel/include/debug/panic.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>
#include <interrupt/idt.h>

VOID Panic(CONST CHAR16 *msg);
VOID ExceptionPanic(INTERRUPT_FRAME *frame);
