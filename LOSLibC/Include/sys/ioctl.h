// LOSLibC/Include/sys/ioctl.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <Syscall/Device/Ioctl.h>

int ioctl(int FD, unsigned long Request, ...);
