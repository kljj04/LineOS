// Kernel/Source/Syscall/SyscallEntry.S
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

.section .text
.global SyscallEntry
.type SyscallEntry, @function

SyscallEntry:
    swapgs
    lfence
    movq %rsp, %gs:8
    movq %gs:0, %rsp
    pushq $0x1B
    pushq %gs:8
    pushq %r11
    pushq $0x23
    pushq %rcx
    swapgs
    pushq $0
    pushq $0
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    cld
    movq %rsp, %rbx
    movq %rsp, %rdi
    andq $-16, %rsp
    call SyscallDispatch
    movq %rbx, %rsp
    movq %rax, 112(%rsp)

    movabsq $0x800000000000, %rdx
    cmpq %rdx, 136(%rsp)
    jae SyscallInvalidReturn
    cmpq %rdx, 160(%rsp)
    jae SyscallInvalidReturn
    andq $0x240DD5, 152(%rsp)
    orq $0x202, 152(%rsp)

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp
    movq 0(%rsp), %rcx
    movq 16(%rsp), %r11
    movq 24(%rsp), %rsp
    sysretq

SyscallInvalidReturn:
    movq $-1, %rdi
    andq $-16, %rsp
    call SyscallExit
    cli
1:
    hlt
    jmp 1b

.size SyscallEntry, .-SyscallEntry

.section .note.GNU-stack,"",@progbits
