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

## Supported environment

The default launcher targets QEMU Q35 with VirtIO GPU, block, keyboard, tablet and network devices. This does not establish support for arbitrary physical GPUs, network cards or input controllers. See [networking](Network.md) for the network backend and protocol limits.
