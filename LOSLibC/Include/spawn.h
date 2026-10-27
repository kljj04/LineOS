// LOSLibC/Include/spawn.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <sys/types.h>

// spawn supplies argc = 0 and argv[0] = NULL.
// spawnv copies a NULL-terminated vector verbatim, including caller-supplied argv[0].
// NULL Argv means no arguments. Limits: 64 strings, 4096 bytes per string including
// NUL, 16384 string bytes total. Both calls return a PID or -1 without setting errno.
pid_t spawn(const char *Path);
pid_t spawnv(const char *Path, const char *const Argv[]);
