// Kernel/Include/FS/EXT2/EXT2Directory.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <FS/EXT2/EXT2Types.h>

BOOLEAN EXT2FindEntry(EXT2_VOLUME *Volume, EXT2_INODE *Directory, CONST CHAR8 *Name, UINT32 *Number);
BOOLEAN EXT2AddEntry(EXT2_VOLUME *Volume, EXT2_INODE *Directory, CONST CHAR8 *Name, UINT32 Number, UINT8 Type);
BOOLEAN EXT2Resolve(EXT2_VOLUME *Volume, CONST CHAR8 *Path, UINT32 *Number, EXT2_INODE *Inode);
BOOLEAN EXT2CreateEntry(EXT2_VOLUME *Volume, UINT32 Parent, CONST CHAR8 *Name, BOOLEAN Directory, UINT16 Permissions, UINT32 Time, UINT32 *Number);

BOOLEAN EXT2RemoveEntry(EXT2_VOLUME *Volume, UINT32 Parent, CONST CHAR8 *Name, UINT32 Time);
BOOLEAN EXT2RenameEntry(EXT2_VOLUME *Volume, UINT32 OldParent, CONST CHAR8 *OldName, UINT32 NewParent, CONST CHAR8 *NewName, UINT32 Time);
BOOLEAN EXT2LinkEntry(EXT2_VOLUME *Volume, UINT32 Parent, CONST CHAR8 *Name, UINT32 Number, UINT32 Time);
BOOLEAN EXT2SymlinkEntry(EXT2_VOLUME *Volume, UINT32 Parent, CONST CHAR8 *Name, CONST CHAR8 *Target, UINT32 Time, UINT32 *Number);
BOOLEAN EXT2ReadLinkData(EXT2_VOLUME *Volume, EXT2_INODE *Inode, CHAR8 *Buffer, UINT32 Capacity, UINT32 *Length);

BOOLEAN EXT2ValidateDirectories(EXT2_VOLUME *Volume);
