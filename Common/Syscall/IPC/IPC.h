// Common/Syscall/IPC/IPC.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#define LOS_IPC_NAME_SIZE    32U
#define LOS_IPC_MESSAGE_SIZE 65536U
#define LOS_IPC_CREATE       0U
#define LOS_IPC_LOOKUP       1U
#define LOS_IPC_BIND         2U
#define LOS_IPC_SEND         3U
#define LOS_IPC_RECEIVE      4U
#define LOS_IPC_CLOSE        5U
#define LOS_IPC_ALIVE        6U

typedef struct
{
    unsigned long long Endpoint;
    unsigned long long Peer;
    unsigned long long Data;
    unsigned int       Size;
    unsigned int       UID;
    unsigned int       PID;
    unsigned int       Reserved;
    char               Name[LOS_IPC_NAME_SIZE];
} LOS_IPC_REQUEST;
