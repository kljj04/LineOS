// LOSLibC/Include/linux/sched.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <sched.h>

#define CLONE_ARGS_SIZE_VER0 64
#define CLONE_ARGS_SIZE_VER1 80
#define CLONE_ARGS_SIZE_VER2 88

struct clone_args
{
    unsigned long long flags;
    unsigned long long pidfd;
    unsigned long long child_tid;
    unsigned long long parent_tid;
    unsigned long long exit_signal;
    unsigned long long stack;
    unsigned long long stack_size;
    unsigned long long tls;
    unsigned long long set_tid;
    unsigned long long set_tid_size;
    unsigned long long cgroup;
} __attribute__((aligned(8)));
