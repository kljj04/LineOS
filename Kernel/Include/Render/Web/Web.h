// Kernel/Include/Render/Web/Web.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

VOID    WebTask(VOID);
BOOLEAN WebLoad(CONST CHAR8 *URL);
VOID    WebRender(VOID);
UINT32  WebGetNodeCount(VOID);
UINT32  WebGetRuleCount(VOID);
