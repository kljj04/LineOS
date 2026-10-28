// Common/LineOS/Memory.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#define PROT_NONE           0
#define PROT_READ           1
#define PROT_WRITE          2
#define PROT_EXEC           4
#define MAP_SHARED          1
#define MAP_PRIVATE         2
#define MAP_FIXED           0x10
#define MAP_ANONYMOUS       0x20
#define MAP_ANON            MAP_ANONYMOUS
#define MAP_FIXED_NOREPLACE 0x100000
#define MAP_FAILED          ((void *) -1)
