// kernel/include/scheduler/LBPWRRChunk.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <scheduler/LBPWRRTypes.h>

BOOLEAN LBPWRRBuildChunk(LBPWRR_RUNQUEUE *RunQueue);
BOOLEAN LBPWRRRebuildChunk(LBPWRR_RUNQUEUE *RunQueue);
VOID    LBPWRRFlushRebuild(LBPWRR_RUNQUEUE *RunQueue, TASK *CurrentTask);
