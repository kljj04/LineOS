// Libraries/LOSLibC/Source/Process/pthread_clone.S
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

.text
.global PthreadClone
.type PthreadClone, @function
PthreadClone:
    movq %rcx, %r10
    movl $56, %eax
    syscall
    testq %rax, %rax
    jnz 1f
    popq %rdi
    andq $-16, %rsp
    call PthreadRun@PLT
    ud2
1:
    ret
.size PthreadClone, .-PthreadClone
.section .note.GNU-stack,"",@progbits
