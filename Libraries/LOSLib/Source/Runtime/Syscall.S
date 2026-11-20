// Libraries/LOSLib/Source/Runtime/Syscall.S
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

.text
.hidden LOSSyscall1
.hidden LOSSyscall2
.hidden LOSSyscall3
.global LOSSyscall1
.global LOSSyscall2
.global LOSSyscall3
LOSSyscall1:
    movq %rdi, %rax
    movq %rsi, %rdi
    syscall
    ret
LOSSyscall2:
    movq %rdi, %rax
    movq %rsi, %rdi
    movq %rdx, %rsi
    syscall
    ret
LOSSyscall3:
    movq %rdi, %rax
    movq %rsi, %rdi
    movq %rdx, %rsi
    movq %rcx, %rdx
    syscall
    ret
.section .note.GNU-stack,"",@progbits
