// LOSLibC/Include/stdlib.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>

VOID NORETURN exit(int ErrorCode);
extern char **environ;
char         *getenv(const char *Name);
