// kernel/src/interrupt/isr.S
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

.section .text

.extern IDTInterruptHandler

.global PRRRestoreContext
.type PRRRestoreContext, @function
PRRRestoreContext:
    movq %rdi, %rsp
    jmp ISRRestoreContext

.macro ISR_NO_ERROR_CODE vector
.global ISR\vector
.type ISR\vector, @function
ISR\vector:
    pushq $0
    pushq $\vector
    jmp ISRCommon
.endm

.macro ISR_ERROR_CODE vector
.global ISR\vector
.type ISR\vector, @function
ISR\vector:
    pushq $\vector
    jmp ISRCommon
.endm

ISR_NO_ERROR_CODE 0
ISR_NO_ERROR_CODE 1
ISR_NO_ERROR_CODE 2
ISR_NO_ERROR_CODE 3
ISR_NO_ERROR_CODE 4
ISR_NO_ERROR_CODE 5
ISR_NO_ERROR_CODE 6
ISR_NO_ERROR_CODE 7
ISR_ERROR_CODE    8
ISR_NO_ERROR_CODE 9
ISR_ERROR_CODE    10
ISR_ERROR_CODE    11
ISR_ERROR_CODE    12
ISR_ERROR_CODE    13
ISR_ERROR_CODE    14
ISR_NO_ERROR_CODE 15
ISR_NO_ERROR_CODE 16
ISR_ERROR_CODE    17
ISR_NO_ERROR_CODE 18
ISR_NO_ERROR_CODE 19
ISR_NO_ERROR_CODE 20
ISR_ERROR_CODE    21
ISR_NO_ERROR_CODE 22
ISR_NO_ERROR_CODE 23
ISR_NO_ERROR_CODE 24
ISR_NO_ERROR_CODE 25
ISR_NO_ERROR_CODE 26
ISR_NO_ERROR_CODE 27
ISR_NO_ERROR_CODE 28
ISR_ERROR_CODE    29
ISR_ERROR_CODE    30
ISR_NO_ERROR_CODE 31

ISR_NO_ERROR_CODE 64
ISR_NO_ERROR_CODE 65
ISR_NO_ERROR_CODE 66
ISR_NO_ERROR_CODE 67

ISRCommon:
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

    movq %rsp, %rdi
    movq %rsp, %rbx
    andq $-16, %rsp

    cld
    call IDTInterruptHandler

    cmpq $0, %rax
    jne 1f
    movq %rbx, %rax
1:
    movq %rax, %rsp

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

    iretq

ISRRestoreContext:
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

    iretq

.section .note.GNU-stack,"",@progbits
