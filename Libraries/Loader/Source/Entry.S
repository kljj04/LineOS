// Libraries/Loader/Source/Entry.S
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

.text
.global _LoaderStart
.hidden LoaderStart
_LoaderStart:
    movq %rsp, %rdi
    andq $-16, %rsp
    call LoaderStart
    ud2

.global LoaderEnter
.hidden LoaderEnter
LoaderEnter:
    movq %rdi, %rax
    movq %rsi, %rsp
    xorq %rbp, %rbp
    jmp *%rax
.section .note.GNU-stack,"",@progbits
