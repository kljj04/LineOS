// LOSLibC/Include/errno.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <Syscall/Errno.h>

int *__errno_location(void);
#define errno (*__errno_location())
