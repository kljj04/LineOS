// kernel/include/scheduler/prr.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>
#include <interrupt/idt.h>

BOOLEAN PRRInit(VOID);
BOOLEAN PRRCreateTask(VOID (*entry)(VOID));
VOID    StartSchedule(VOID);
VOID    Yield(VOID);
VOID    PRRTick(INTERRUPT_FRAME *frame);
UINT64  PRRGetSwitchStack(INTERRUPT_FRAME *frame);
