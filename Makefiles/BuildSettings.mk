# Makefiles/BuildSettings.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

CC = clang
LD = ld.lld
LINK_EFI = lld-link
CLANG_FORMAT = clang-format
PYTHON = python3

EFI_TYPE_TARGET := x86_64-unknown-windows
KERNEL_TYPE_TARGET := x86_64-elf
BOOT_TARGET := LineOS/EFI/BOOT/BOOTX64.EFI
KERN_TARGET := LineOS/KERNEL/LINEOS_KERNEL.ELF
DEPFLAGS := -MMD -MP

EFI_CFLAGS = -target $(EFI_TYPE_TARGET) \
             -Wall -Wextra -nostdlib -fno-builtin \
             -ffreestanding -fshort-wchar -mno-red-zone \
             -IBootLoader/EDK2/MdePkg/Include \
             -IBootLoader/EDK2/MdePkg/Include/X64 \
             -ICommon -IBootLoader/Include \
             -Wunused-variable

KERN_CFLAGS = -target $(KERNEL_TYPE_TARGET) \
              -Wall -Wextra -nostdlib -fno-builtin \
              -ffreestanding -mgeneral-regs-only \
              -mno-red-zone -fno-pie -fno-stack-protector \
              -DLINEOS_KERNEL_BUILD \
              -ICommon -IKernel/Include -IKernel/Assets \
              -Wunused-variable -fshort-wchar

BOOT_SRCS = $(call RWildcard,BootLoader,*.c)
KERN_SRCS = $(call RWildcard,Kernel,*.c)
KERN_ASM_SRCS = $(call RWildcard,Kernel,*.S)

BOOT_OBJS = $(patsubst %.c,Build/Objects/%.o,$(BOOT_SRCS))
KERN_C_OBJS = $(patsubst %.c,Build/Objects/%.o,$(KERN_SRCS))
KERN_ASM_OBJS = $(patsubst %.S,Build/Objects/%.o,$(KERN_ASM_SRCS))
KERN_OBJS = $(KERN_C_OBJS) $(KERN_ASM_OBJS)

BOOT_DEPS = $(BOOT_OBJS:.o=.d)
KERN_DEPS = $(KERN_OBJS:.o=.d)

KERN_FLOAT_CFLAGS = $(filter-out -mgeneral-regs-only,$(KERN_CFLAGS)) -msse2 -mfpmath=sse
KERN_OBJ_CFLAGS = $(KERN_CFLAGS)
KERN_FLOAT_OBJS = Build/Objects/Kernel/Kernel.o \
                  Build/Objects/Kernel/Source/Render/TrueType/TrueType.o \
                  Build/Objects/Kernel/Source/Render/TrueType/TrueTypeEngine.o \
                  Build/Objects/Kernel/Source/Render/TrueType/TrueTypeRuntime.o

FORMAT_EXCLUDE_SRCS := BootLoader/EDK2/%
FORMAT_ALL_SRCS = $(call RWildcard,BootLoader,*.c) $(call RWildcard,BootLoader,*.h) \
                  $(call RWildcard,Common,*.c) $(call RWildcard,Common,*.h) \
                  $(call RWildcard,Kernel,*.c) $(call RWildcard,Kernel,*.h)
FORMAT_SRCS = $(filter-out $(FORMAT_EXCLUDE_SRCS),$(FORMAT_ALL_SRCS))