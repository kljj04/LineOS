// LOSLibC/Include/ctype.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int isdigit(int Character);
int islower(int Character);
int isupper(int Character);
int isspace(int Character);
int isalpha(int Character);
int isalnum(int Character);
int isxdigit(int Character);
int tolower(int Character);
int toupper(int Character);

#ifdef __cplusplus
}
#endif
