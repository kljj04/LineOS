# Makefiles/BuildSettings.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

CC = clang
LD = ld.lld
AR = llvm-ar
LINK_EFI = lld-link
CLANG_FORMAT = clang-format
PYTHON = python3

EFI_TYPE_TARGET := x86_64-unknown-windows
KERNEL_TYPE_TARGET := x86_64-elf

BOOT_TARGET := LineOS/EFI/BOOT/BOOTX64.EFI
KERN_TARGET := LineOS/KERNEL/LINEOS_KERNEL.ELF

DEPFLAGS := -MMD -MP

BOOT_SRCS = $(call RWildcard,BootLoader,*.c)
KERN_SRCS = $(call RWildcard,Kernel,*.c)
KERN_ASM_SRCS = $(call RWildcard,Kernel,*.S)

BOOT_OBJS = $(patsubst %.c,Build/Objects/%.o,$(BOOT_SRCS))
KERN_C_OBJS = $(patsubst %.c,Build/Objects/%.o,$(KERN_SRCS))
KERN_ASM_OBJS = $(patsubst %.S,Build/Objects/%.o,$(KERN_ASM_SRCS))
KERN_OBJS = $(KERN_C_OBJS) $(KERN_ASM_OBJS)

BOOT_DEPS = $(BOOT_OBJS:.o=.d)
KERN_DEPS = $(KERN_OBJS:.o=.d)

KERN_FLOAT_OBJS = Build/Objects/Kernel/Kernel.o \
                  Build/Objects/Kernel/Source/Render/TrueType/TrueType.o \
                  Build/Objects/Kernel/Source/Render/TrueType/TrueTypeEngine.o \
                  Build/Objects/Kernel/Source/Render/TrueType/TrueTypeRuntime.o

USER_PROGRAM_ROOT := UserPrograms
USER_PROGRAM_OUTPUT_DIR := LineOS/Programs

USER_PROGRAM_DIRS := $(wildcard $(USER_PROGRAM_ROOT)/*/)
USER_PROGRAMS := $(patsubst $(USER_PROGRAM_ROOT)/%/,%,$(USER_PROGRAM_DIRS))

USER_PROGRAM_OBJS = $(foreach Program,$(USER_PROGRAMS),$(USER_PROGRAM_OBJS_$(Program)))
USER_PROGRAM_OUTPUTS = $(foreach Program,$(USER_PROGRAMS),$(USER_PROGRAM_OUTPUT_$(Program)))
USER_PROGRAM_DEPS = $(USER_PROGRAM_OBJS:.o=.d)

LOSLIBC_TARGET := Build/Libraries/liblosc.a

LOSLIBC_C_SRCS = $(call RWildcard,LOSLibC,*.c)
LOSLIBC_ASM_SRCS = $(call RWildcard,LOSLibC,*.S)

LOSLIBC_C_OBJS = $(patsubst %.c,Build/Objects/%.o,$(LOSLIBC_C_SRCS))
LOSLIBC_ASM_OBJS = $(patsubst %.S,Build/Objects/%.o,$(LOSLIBC_ASM_SRCS))

LOSLIBC_OBJS = $(LOSLIBC_C_OBJS) $(LOSLIBC_ASM_OBJS)
LOSLIBC_DEPS = $(LOSLIBC_OBJS:.o=.d)

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

USER_PROGRAM_CFLAGS = -target $(KERNEL_TYPE_TARGET) \
                      -Wall -Wextra -nostdlib -fno-builtin \
                      -ffreestanding -mno-red-zone \
                      -fno-pie -fno-stack-protector -mcmodel=large \
                      -fno-asynchronous-unwind-tables \
                      -fno-unwind-tables \
                      -ICommon -ILOSLibC/Include -ILibraries/Include

KERN_FLOAT_CFLAGS = $(filter-out -mgeneral-regs-only,$(KERN_CFLAGS)) -msse2 -mfpmath=sse
KERN_OBJ_CFLAGS = $(KERN_CFLAGS)
LOSLIBC_CFLAGS = $(USER_PROGRAM_CFLAGS)

FORMAT_EXCLUDE_SRCS := BootLoader/EDK2/%

FORMAT_ALL_SRCS = $(call RWildcard,Libraries,*.c) $(call RWildcard,Libraries,*.h) \
                  $(call RWildcard,BootLoader,*.c) $(call RWildcard,BootLoader,*.h) \
                  $(call RWildcard,Common,*.c) $(call RWildcard,Common,*.h) \
                  $(call RWildcard,Kernel,*.c) $(call RWildcard,Kernel,*.h) \
                  $(call RWildcard,LOSLibC,*.c) $(call RWildcard,LOSLibC,*.h) \
                  $(call RWildcard,UserPrograms,*.c) $(call RWildcard,UserPrograms,*.h)

FORMAT_SRCS = $(filter-out $(FORMAT_EXCLUDE_SRCS),$(FORMAT_ALL_SRCS))
TRUE_TYPE_TARGET := Build/Libraries/libtruetype.a
TRUE_TYPE_SRCS = $(call RWildcard,Libraries/Source/TrueType,*.c)
TRUE_TYPE_OBJS = $(patsubst %.c,Build/Objects/%.o,$(TRUE_TYPE_SRCS))
