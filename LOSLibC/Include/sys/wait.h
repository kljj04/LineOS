// LOSLibC/Include/sys/wait.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <sys/types.h>

#define WNOHANG             1
#define WIFEXITED(Status)   (((Status) & 127) == 0)
#define WEXITSTATUS(Status) (((Status) >> 8) & 255)
#define WIFSIGNALED(Status) (((Status) & 127) != 0 && ((Status) & 127) != 127)
#define WTERMSIG(Status)    ((Status) & 127)

pid_t waitpid(pid_t PID, int *Status, int Options);
pid_t wait(int *Status);
