.text
.global _Start
_Start:
    movq %rdx, %r12
    call Value@PLT
    cmpl $42, %eax
    jne 1f
    movl $1, %eax
    movl $1, %edi
    leaq Message(%rip), %rsi
    movl $12, %edx
    syscall
    call *%r12
    xorl %edi, %edi
    jmp 2f
1:
    movl $1, %edi
2:
    movl $231, %eax
    syscall
.section .rodata
Message:
    .ascii "loader PASS\n"
.section .note.GNU-stack,"",@progbits
