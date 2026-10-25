# Devices and rendering

[Documentation index](README.md) · [한국어](../ko_KR/Devices.md) · [English](../en_US/Devices.md)

## PCI and VirtIO

- PCI configuration space scanning
- Q35 machine support
- Generic VirtIO PCI transport infrastructure
- Generic VirtQueue infrastructure
- VirtIO network device
- VirtIO GPU
- VirtIO keyboard input
- VirtIO tablet input
- VirtIO block device with synchronous and asynchronous reads, writes, and flushes

## Graphics

- VirtIO GPU initialization
- VirtIO GPU framebuffer creation
- Scanout setup
- Framebuffer transfer and flushing
- Dirty rectangle tracking
- Hardware cursor support
- Pixel read/write
- Rectangle drawing
- Line drawing
- Circle drawing
- Alpha blending
- Framebuffer copy operations

## Text Rendering

- Runtime TrueType parsing and rasterization
- Embedded fonts through assembler `.incbin`
- Pretendard SemiBold
- JetBrains Mono Nerd Font SemiBold
- ASCII rendering
- Korean Hangul rendering
- Nerd Font private-use glyph rendering
- Runtime glyph blending into the VirtIO GPU framebuffer

## Fonts and licenses

The kernel retains embedded fonts and TrueType rendering for panic output. Userspace has a separate static TrueType library and reads deployed font files from `/sys/fonts/`. The current Shell renders ASCII output to a userspace framebuffer and presents it through display syscalls; it is not yet an interactive terminal. See [user programs](UserLibraries.md) and [third-party notices](../THIRD_PARTY_NOTICES.md).

## Input queues and blocking

Keyboard IRQ vector 65 and tablet vector 66 use separate VirtIO queues, 64-entry event rings and IRQ-saving spinlocks. Each ring has an effective capacity of 63; the existing full-ring policy drops the new event. PushKeyEvent/PushPointerEvent publish the event and wake waiters under the corresponding event lock.

`InputMouseHandler()` drains pointer events and calls `InputWaitMouseEvents()`. `InputKeyboardHandler()` drains key events and calls `InputWaitKeyboardEvent()`. Their VirtIO wait functions check ring indices under the same condition lock and call WaitQueueSleep only when empty. A wake cannot be lost between checking and blocking. See [scheduling](Scheduler.md) for IRQ flags, lock ordering and stack ownership.

The current KMain registers neither keyboard nor mouse consumers. Driver IRQ handling alone does not provide keyboard input to Shell or drain the rings into higher-level handlers.

## Supported environment

The default launcher targets QEMU Q35 with VirtIO GPU, block, keyboard, tablet and network devices. This does not establish support for arbitrary physical GPUs, network cards or input controllers. See [networking](Network.md) for the network backend and protocol limits.
