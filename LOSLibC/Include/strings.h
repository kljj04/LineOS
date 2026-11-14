// LOSLibC/Include/strings.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int strcasecmp(const char *Left, const char *Right);
int strncasecmp(const char *Left, const char *Right, size_t Size);

#ifdef __cplusplus
}
#endif
