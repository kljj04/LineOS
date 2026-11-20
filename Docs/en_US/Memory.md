# Memory management

[Documentation index](README.md) · [한국어](../ko_KR/Memory.md) · [English](../en_US/Memory.md)

## Physical allocation and heap

The UEFI memory map initializes the buddy allocator. Physical allocation supports power-of-two page counts and address ceilings. Kernel pages and heap objects are allocated/released separately, using KAlloc/KFree and spinlock synchronization. Kernel memory helpers are KMemCpy, KMemMove and KMemSet.

## Virtual memory

`Kernel/Include/Memory/VMM.h` exposes initialization, address translation, page mapping/unmapping, current CR3 access and SMP shootdown handling. The implementation is in `Kernel/Source/Memory/VMM.c`.

Physical allocation and virtual mapping are separate operations. Reserved boot regions and the kernel image must remain unavailable to the physical allocator. ELF user tasks receive separate address spaces sharing kernel mappings. VMM provides address-space creation, inactive-space mapping and user-copy operations; managed current user roots can create and modify another inactive space. This is a partial process-isolation implementation, not a complete security boundary.

## User address spaces

PROCESS owns the address space shared by its threads. Private ELF, stack, heap and mmap pages are tracked separately from shared framebuffer/assets. Fork eagerly copies private pages without COW. VMM locking and remote TLB shootdowns synchronize updates to shared address spaces. See [userspace ABI](DeviceFS.md) for file mmap, fixed mapping, brk and mprotect limits.
