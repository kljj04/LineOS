// Libraries/LOSLibC/Runtime/crt0.S
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

.text
.global _Start
.type _Start, @function
_Start:
    movq %rdx, %rcx
    xorq %rbp, %rbp
    movq (%rsp), %rdi
    leaq 8(%rsp), %rsi
    leaq 16(%rsp,%rdi,8), %rdx
    andq $-16, %rsp
    call LibCStart
    ud2
.size _Start, .-_Start
.section .note.GNU-stack,"",@progbits
