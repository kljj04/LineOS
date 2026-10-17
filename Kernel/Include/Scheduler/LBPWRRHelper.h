// kernel/include/scheduler/LBPWRRHelper.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Scheduler/LBPWRRTypes.h>

BOOLEAN LBPWRRTaskIsRunnable(TASK *Task);
BOOLEAN LBPWRRWakeSleepingTasks(LBPWRR_RUNQUEUE *RunQueue);
