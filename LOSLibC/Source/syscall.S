// syscall.S
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

.text

.global LineOSSyscall0
.global LineOSSyscall1
.global LineOSSyscall2
.global LineOSSyscall3

LineOSSyscall0:
    movq %rdi, %rax
    int $0x80
    ret

LineOSSyscall1:
    movq %rdi, %rax
    movq %rsi, %rdi
    int $0x80
    ret

LineOSSyscall2:
    movq %rdi, %rax
    movq %rsi, %rdi
    movq %rdx, %rsi
    int $0x80
    ret

LineOSSyscall3:
    movq %rdi, %rax
    movq %rsi, %rdi
    movq %rdx, %rsi
    movq %rcx, %rdx
    int $0x80
    ret

.section .note.GNU-stack,"",@progbits