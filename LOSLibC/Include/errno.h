// LOSLibC/Include/errno.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <LineOS/Errno.h>

int *__errno_location(void);
#define errno (*__errno_location())
