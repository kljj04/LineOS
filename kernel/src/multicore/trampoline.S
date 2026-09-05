// kernel/src/multicore/trampoline.S
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#define SMP_TRAMPOLINE_ADDRESS       0x8000
#define SMP_TRAMPOLINE_CR3_ADDRESS   0x8F00
#define SMP_TRAMPOLINE_STACK_ADDRESS 0x8F08
#define SMP_TRAMPOLINE_ENTRY_ADDRESS 0x8F10
#define SMP_TRAMPOLINE_CPUID_ADDRESS 0x8F18

#define IA32_EFER_MSR 0xC0000080
#define IA32_EFER_LME (1 << 8)

#define CR4_PAE (1 << 5)
#define CR0_PE  (1 << 0)
#define CR0_PG  (1 << 31)

.section .text.ap_trampoline, "ax"
.code16

.global APTrampolineStart
.global APTrampolineEnd

APTrampolineStart:
    cli
    cld

    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    lgdt SMP_TRAMPOLINE_ADDRESS + (APTrampolineGDTR - APTrampolineStart)

    movl %cr0, %eax
    orl $CR0_PE, %eax
    movl %eax, %cr0

    ljmp $0x08, $(SMP_TRAMPOLINE_ADDRESS + APTrampolineProtectedMode - APTrampolineStart)

.code32

APTrampolineProtectedMode:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    movl %cr4, %eax
    orl $CR4_PAE, %eax
    movl %eax, %cr4

    movl SMP_TRAMPOLINE_CR3_ADDRESS, %eax
    movl %eax, %cr3

    movl $IA32_EFER_MSR, %ecx
    rdmsr
    orl $IA32_EFER_LME, %eax
    wrmsr

    movl %cr0, %eax
    orl $CR0_PG, %eax
    movl %eax, %cr0

    ljmp $0x18, $(SMP_TRAMPOLINE_ADDRESS + APTrampolineLongMode - APTrampolineStart)

.code64

APTrampolineLongMode:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    movabs $SMP_TRAMPOLINE_STACK_ADDRESS, %rax
    movq (%rax), %rsp

    andq $-16, %rsp

    xorq %rbp, %rbp

    movabs $SMP_TRAMPOLINE_CPUID_ADDRESS, %rax
    movl (%rax), %edi

    movabs $SMP_TRAMPOLINE_ENTRY_ADDRESS, %rax
    movq (%rax), %rax

    call *%rax

APTrampolineHalt:
    cli
    hlt
    jmp APTrampolineHalt

.align 8

APTrampolineGDT:
    .quad 0x0000000000000000
    .quad 0x00CF9A000000FFFF
    .quad 0x00CF92000000FFFF
    .quad 0x00AF9A000000FFFF

APTrampolineGDTEnd:

APTrampolineGDTR:
    .word APTrampolineGDTEnd - APTrampolineGDT - 1
    .long SMP_TRAMPOLINE_ADDRESS + (APTrampolineGDT - APTrampolineStart)

APTrampolineEnd: