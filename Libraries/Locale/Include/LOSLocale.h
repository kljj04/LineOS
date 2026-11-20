// Libraries/Locale/Include/LOSLocale.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLib/TypeInfo.h>

CONST char *LOSLocaleNormalize(CONST char *Name);
char       *LOSLocaleRefresh(VOID);
char       *LOSGettext(const char *Message);
