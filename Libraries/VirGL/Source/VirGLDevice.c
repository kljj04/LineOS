// Libraries/VirGL/Source/VirGLDevice.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <VirGLDevice.h>
#include <fcntl.h>
#include <sys/ioctl.h>

int VirGLOpenDevice(VOID)
{
    return open(LOS_VIRGL_PATH, O_RDONLY);
}

int VirGLQueryCapset(int FD, LOS_VIRGL_CAPSET_INFO *Info)
{
    return ioctl(FD, LOS_VIRGL_GET_CAPSET_INFO, (VOID *) Info);
}

int VirGLFetchCapset(int FD, LOS_VIRGL_CAPSET_READ *Request)
{
    return ioctl(FD, LOS_VIRGL_READ_CAPSET, (VOID *) Request);
}

int VirGLCreateDeviceContext(int FD)
{
    return ioctl(FD, LOS_VIRGL_CREATE_CONTEXT);
}

int VirGLCreateDeviceBuffer(int FD, LOS_VIRGL_BUFFER_CREATE *Request)
{
    return ioctl(FD, LOS_VIRGL_CREATE_BUFFER, (VOID *) Request);
}

int VirGLWriteDeviceBuffer(int FD, LOS_VIRGL_BUFFER_WRITE *Request)
{
    return ioctl(FD, LOS_VIRGL_WRITE_BUFFER, (VOID *) Request);
}

int VirGLDestroyDeviceBuffer(int FD, unsigned int Handle)
{
    return ioctl(FD, LOS_VIRGL_DESTROY_BUFFER, (VOID *) &Handle);
}

int VirGLSubmitDeviceCommands(int FD, const unsigned int *Commands, unsigned int Words)
{
    LOS_VIRGL_SUBMIT Request = {.Address = (unsigned long) Commands, .Words = Words};

    return ioctl(FD, LOS_VIRGL_SUBMIT_COMMANDS, (VOID *) &Request);
}

int VirGLCreateDeviceTarget(int FD, LOS_VIRGL_TARGET_CREATE *Request)
{
    return ioctl(FD, LOS_VIRGL_CREATE_TARGET, (VOID *) Request);
}
