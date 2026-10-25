// Kernel/Source/Syscall/SyscallEntry.S
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

.section .text
.global SyscallEntry
.type SyscallEntry, @function

SyscallEntry:
    cld

    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax

    movq %rsp, %rdi
    call SyscallDispatch

    movq %rax, (%rsp)

    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    iretq

.size SyscallEntry, .-SyscallEntry