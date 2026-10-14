// kernel/include/scheduler/loadbalancer.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

VOID LoadBalancerInit(VOID);
VOID LoadBalancerTick(VOID);
VOID LoadBalancerBalance(VOID);