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

The font files are embedded with assembler `.incbin` and rasterized by the kernel at runtime. See [third-party notices](../THIRD_PARTY_NOTICES.md) and the bundled texts in `Docs/LICENSES/`.

## Input queues and blocking

Keyboard IRQ vector 65 and tablet vector 66 use separate VirtIO queues, 64-entry event rings and IRQ-saving spinlocks. Each ring has an effective capacity of 63; the existing full-ring policy drops the new event. PushKeyEvent/PushPointerEvent publish the event and wake waiters under the corresponding event lock.

`InputMouseHandler()` drains pointer events and calls `InputWaitMouseEvents()`. `InputKeyboardHandler()` drains key events and calls `InputWaitKeyboardEvent()`. Their VirtIO wait functions check ring indices under the same condition lock and call WaitQueueSleep only when empty. A wake cannot be lost between checking and blocking. See [scheduling](Scheduler.md) for IRQ flags, lock ordering and stack ownership.

The current KMain registers only the mouse consumer. The keyboard consumer must be explicitly scheduled by a workload that needs it. Driver IRQ handling alone does not drain the key-event ring into the higher-level keyboard status queue.

## Supported environment

The default launcher targets QEMU Q35 with VirtIO GPU, block, keyboard, tablet and network devices. This does not establish support for arbitrary physical GPUs, network cards or input controllers. See [networking](Network.md) for the network backend and protocol limits.
