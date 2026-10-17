// kernel/include/scheduler/LBPWRRChunk.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Scheduler/LBPWRRTypes.h>

BOOLEAN LBPWRRBuildChunk(LBPWRR_RUNQUEUE *RunQueue);
BOOLEAN LBPWRRModifyChunk(LBPWRR_RUNQUEUE *RunQueue, TASK *Task, UINT8 Priority);
