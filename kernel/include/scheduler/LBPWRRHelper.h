// kernel/include/scheduler/LBPWRRHelper.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <scheduler/LBPWRRTypes.h>

BOOLEAN LBPWRRTaskIsRunnable(TASK *task);
BOOLEAN LBPWRRWakeSleepingTasks(LBPWRR_RUNQUEUE *RunQueue);
