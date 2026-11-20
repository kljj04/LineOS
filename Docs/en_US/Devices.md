# Devices and rendering

[Documentation index](README.md) · [한국어](../ko_KR/Devices.md) · [English](../en_US/Devices.md)

## PCI and VirtIO

PCI configuration scanning supports Q35 and supplies common VirtIO PCI transport and virtqueue code.

| Device | Implemented operations | Source |
| --- | --- | --- |
| VirtIO GPU | Framebuffer, scanout, transfer, flush and cursor | Kernel/Source/Render/GPU |
| VirtIO input | Keyboard and tablet events | Kernel/Source/Input |
| VirtIO block | Synchronous/asynchronous read, write and flush | Kernel/Source/Disk |
| VirtIO network | RX/TX queues and network device access | Kernel/Source/Network |

See [Storage](Storage.md) and [Networking](Network.md) for layers above the devices.

## Graphics and text

CPU rendering includes pixel access, shapes, alpha blending, framebuffer copies and dirty rectangles. TrueType fonts are parsed/rasterized at runtime; kernel fonts are embedded with assembler incbin. Pretendard SemiBold and JetBrains Mono Nerd Font SemiBold provide ASCII, Hangul and Nerd Font private-use glyphs.

## Fonts and licenses

The kernel retains embedded fonts and TrueType rendering for panic output. Userspace has a separate shared TrueType library and read-only font assets published by Init. Terminal renders UTF-8, Hangul, Nerd Font glyphs and ANSI controls; lsh handles interactive input and lwm composites IPC frames. See [user programs](UserLibraries.md) and [third-party notices](../THIRD_PARTY_NOTICES.md).

## Input queues and blocking

Keyboard IRQ vector 65 and tablet vector 66 use separate VirtIO queues, 64-entry event rings and IRQ-saving spinlocks. Each ring has an effective capacity of 63; the existing full-ring policy drops the new event. PushKeyEvent/PushPointerEvent publish the event and wake waiters under the corresponding event lock.

`InputMouseHandler()` drains pointer events and calls `InputWaitMouseEvents()`. `InputKeyboardHandler()` drains key events and calls `InputWaitKeyboardEvent()`. Their VirtIO wait functions check ring indices under the same condition lock and call WaitQueueSleep only when empty. A wake cannot be lost between checking and blocking. See [scheduling](Scheduler.md) for IRQ flags, lock ordering and stack ownership.

KMain registers keyboard and mouse consumers. TTY/graphics ownership routes input to Terminal or lwm; device event rings and shell-forwarded input queues are separate.

## Supported environment

The default launcher targets QEMU Q35 with VirtIO GPU, block, keyboard, tablet and network devices. This does not establish support for arbitrary physical GPUs, network cards or input controllers. See [networking](Network.md) for the network backend and protocol limits.
