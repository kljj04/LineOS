// Libraries/LOSLib/Include/LOS.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <Userspace/TypeInfo.h>

INT64 LOSRemove(CONST char *Path);
INT64 LOSParentChdir(CONST char *Path);
INT64 LOSSudoCache(UINT64 Operation);
