// Kernel/Include/FS/VFS/VFS.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <FS/VFS/VFSTypes.h>

VOID      VFSInit(VFS_REGISTRY *Registry);
VFS_ERROR VFSRegisterPartition(VFS_REGISTRY *Registry, CONST CHAR8 *Name, UINT32 GPTIndex, VOID *Backend, CONST VFS_BACKEND_OPERATIONS *Operations, UINT32 *Partition);
VFS_ERROR VFSContextInit(VFS_CONTEXT *Context, VFS_REGISTRY *Registry, UINT32 Partition, VFS_CREDENTIALS Credentials);
VFS_ERROR VFSBeginCommand(VFS_CONTEXT *Session, BOOLEAN Elevate, BOOLEAN Authorized, VFS_COMMAND *Command);
VFS_ERROR VFSEndCommand(VFS_COMMAND *Command, BOOLEAN KeepLocation);
VFS_ERROR VFSGetCwd(CONST VFS_CONTEXT *Context, CHAR8 *Buffer, UINT32 Capacity);
VFS_ERROR VFSSelectPartition(VFS_CONTEXT *Context, CONST CHAR8 *Name);
VFS_ERROR VFSChangeDirectory(VFS_CONTEXT *Context, CONST CHAR8 *Path);
VFS_ERROR VFSLookup(VFS_CONTEXT *Context, CONST CHAR8 *Path, VFS_NODE *Node);
VFS_ERROR VFSReadDirectory(VFS_CONTEXT *Context, CONST CHAR8 *Path, UINT64 *Cursor, VFS_DIRECTORY_ENTRY *Entry);
VFS_ERROR VFSRead(VFS_CONTEXT *Context, CONST CHAR8 *Path, UINT64 Offset, VOID *Buffer, UINT32 Length, UINT32 *ReadLength);
VFS_ERROR VFSWrite(VFS_CONTEXT *Context, CONST CHAR8 *Path, UINT64 Offset, CONST VOID *Buffer, UINT32 Length);
VFS_ERROR VFSCreate(VFS_CONTEXT *Context, CONST CHAR8 *Path, BOOLEAN Directory, UINT16 Mode);
VFS_ERROR VFSRemove(VFS_CONTEXT *Context, CONST CHAR8 *Path);
VFS_ERROR VFSRename(VFS_CONTEXT *Context, CONST CHAR8 *OldPath, CONST CHAR8 *NewPath);
VFS_ERROR VFSTruncate(VFS_CONTEXT *Context, CONST CHAR8 *Path, UINT64 Size);
VFS_ERROR VFSLink(VFS_CONTEXT *Context, CONST CHAR8 *Source, CONST CHAR8 *Destination);
VFS_ERROR VFSSymlink(VFS_CONTEXT *Context, CONST CHAR8 *Target, CONST CHAR8 *Path);
VFS_ERROR VFSSetMode(VFS_CONTEXT *Context, CONST CHAR8 *Path, UINT16 Mode);
VFS_ERROR VFSSetOwner(VFS_CONTEXT *Context, CONST CHAR8 *Path, UINT32 UID, UINT32 GID);

VFS_ERROR VFSTouch(VFS_CONTEXT *Context, CONST CHAR8 *Path);

VFS_ERROR VFSLookupNoFollow(VFS_CONTEXT *Context, CONST CHAR8 *Path, VFS_NODE *Node);
