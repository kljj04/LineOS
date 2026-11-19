// LOSLibC/Source/Process/clone.S
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

.text
.global clone
.type clone, @function
clone:
    testq %rdi, %rdi
    jz CloneInvalid
    testq %rsi, %rsi
    jz CloneInvalid
    andq $-16, %rsi
    subq $16, %rsi
    movq %rdi, 0(%rsi)
    movq %rcx, 8(%rsi)
    movslq %edx, %rdi
    movq %r8, %rdx
    movq %r9, %r8
    movq 8(%rsp), %r10
    movl $56, %eax
    syscall
    testq %rax, %rax
    jnz CloneResult
    popq %rax
    popq %rdi
    call *%rax
    movl %eax, %edi
    movl $60, %eax
    syscall
    ud2
CloneInvalid:
    movq $-22, %rax
CloneResult:
    movq %rax, %rdi
    jmp LineOSSyscallResult
.size clone, .-clone

.global clone3
.type clone3, @function
clone3:
    movq (%rsp), %r9
    movq %rsp, %r8
    movl $435, %eax
    syscall
    testq %rax, %rax
    jz Clone3Child
    movq %rax, %rdi
    jmp LineOSSyscallResult
Clone3Child:
    cmpq %r8, %rsp
    jne 1f
    addq $8, %rsp
1:
    jmp *%r9
.size clone3, .-clone3

.global syscall
.type syscall, @function
syscall:
    movq %rdi, %rax
    movq %rsi, %rdi
    movq %rdx, %rsi
    movq %rcx, %rdx
    movq %r8, %r10
    movq %r9, %r8
    movq 8(%rsp), %r9
    syscall
    movq %rax, %rdi
    jmp LineOSSyscallResult
.size syscall, .-syscall
.section .note.GNU-stack,"",@progbits
