// Libraries/VirGL/Include/VirGLDevice.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Userspace/TypeInfo.h>
#include <Syscall/Device/VirGL.h>

int VirGLOpenDevice(VOID);
int VirGLQueryCapset(int FD, LOS_VIRGL_CAPSET_INFO *Info);
int VirGLFetchCapset(int FD, LOS_VIRGL_CAPSET_READ *Request);
int VirGLCreateDeviceContext(int FD);
int VirGLCreateDeviceBuffer(int FD, LOS_VIRGL_BUFFER_CREATE *Request);
int VirGLWriteDeviceBuffer(int FD, LOS_VIRGL_BUFFER_WRITE *Request);
int VirGLDestroyDeviceBuffer(int FD, unsigned int Handle);
int VirGLSubmitDeviceCommands(int FD, const unsigned int *Commands, unsigned int Words);
int VirGLCreateDeviceTarget(int FD, LOS_VIRGL_TARGET_CREATE *Request);
