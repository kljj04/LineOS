# Memory management

[Documentation index](README.md) · [한국어](../ko_KR/Memory.md) · [English](../en_US/Memory.md)

## Memory Management

- Buddy physical memory allocator
- Power-of-two physical page allocation
- Physical allocations below an address limit
- Kernel page allocation and release
- Kernel heap allocator
- `KAlloc()` / `KFree()`
- Spinlock synchronization primitives
- Basic freestanding memory functions:
    - `KMemCpy`
    - `KMemMove`
    - `KMemSet`

## Virtual memory

`Kernel/Include/Memory/VMM.h` exposes initialization, address translation, page mapping/unmapping, current CR3 access and SMP shootdown handling. The implementation is in `Kernel/Source/Memory/VMM.c`.

Physical allocation and virtual mapping are separate operations. Reserved boot regions and the kernel image must remain unavailable to the physical allocator.
