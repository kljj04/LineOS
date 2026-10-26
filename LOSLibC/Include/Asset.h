// LOSLibC/Include/Asset.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/Asset.h>

INT64 LOSAssetPublish(CONST char *Name, CONST VOID *Data, UINT64 Size);
INT64 LOSAssetAcquire(CONST char *Name, LOS_ASSET *Asset);
INT64 LOSAssetRelease(LOS_ASSET *Asset);
