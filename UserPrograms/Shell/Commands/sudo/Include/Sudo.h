// UserPrograms/Shell/Commands/sudo/Include/Sudo.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <UserDB/UserDB.h>

BOOLEAN SudoAllowed(CONST char *Username);
BOOLEAN SudoAuthenticate(CONST char *Username, USER_DB_ACCOUNT *Account);
INT32 SudoExecute(INT32 Argc, char **Argv, CONST char *Username);
