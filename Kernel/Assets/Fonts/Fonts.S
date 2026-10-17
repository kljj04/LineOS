// kernel/assets/Fonts/Fonts.S
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

.section .rodata.fonts, "a", @progbits
.balign 16

.global LineOSJetBrainsMonoFontStart
.global LineOSJetBrainsMonoFontEnd
LineOSJetBrainsMonoFontStart:
.incbin "Kernel/Assets/Fonts/JBMNFSB.ttf"
LineOSJetBrainsMonoFontEnd:

.balign 16

.global LineOSPretendardFontStart
.global LineOSPretendardFontEnd
LineOSPretendardFontStart:
.incbin "Kernel/Assets/Fonts/PTDSB.ttf"
LineOSPretendardFontEnd:
