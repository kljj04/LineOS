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
    syscall
    ret

LineOSSyscall1:
    movq %rdi, %rax
    movq %rsi, %rdi
    syscall
    ret

LineOSSyscall2:
    movq %rdi, %rax
    movq %rsi, %rdi
    movq %rdx, %rsi
    syscall
    ret

LineOSSyscall3:
    movq %rdi, %rax
    movq %rsi, %rdi
    movq %rdx, %rsi
    movq %rcx, %rdx
    syscall
    ret

.global LineOSSyscall6
LineOSSyscall6:
    movq %rdi, %rax
    movq %rsi, %rdi
    movq %rdx, %rsi
    movq %rcx, %rdx
    movq %r8, %r10
    movq %r9, %r8
    movq 8(%rsp), %r9
    syscall
    ret

.section .note.GNU-stack,"",@progbits