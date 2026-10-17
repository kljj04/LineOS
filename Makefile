.DEFAULT_GOAL := all

TARGET1 = LineOS/EFI/BOOT/BOOTX64.EFI
TARGET2 = LineOS/KERNEL/LINEOS_KERNEL.ELF

CC = clang
LD = ld.lld
DEPFLAGS = -MMD -MP
CLANG_FORMAT = clang-format
PYTHON = python3

MAKEFLAGS += --no-print-directory --silent

ifeq ($(OS),Windows_NT)
    IS_WINDOWS := 1
endif

ifdef IS_WINDOWS
    MKDIR_P = powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(1)' | Out-Null"
    RM_RF = powershell -NoProfile -Command "if (Test-Path '$(1)') { Remove-Item -Recurse -Force '$(1)' }"
    PRINT_YELLOW = powershell -NoProfile -Command "Write-Host '$(1)' -ForegroundColor Yellow"
    PRINT_CYAN = powershell -NoProfile -Command "Write-Host '$(1)' -ForegroundColor Cyan"
    PRINT_PURPLE = powershell -NoProfile -Command "Write-Host '$(1)' -ForegroundColor Magenta"
    PRINT_GREEN = powershell -NoProfile -Command "Write-Host '$(1)' -ForegroundColor Green"
    LINK_EFI = $(LD) -flavor link
    LINK_KERNEL = $(LD) -flavor gnu
else
    CYAN   := \033[36m
    PURPLE := \033[35m
    YELLOW := \033[33m
    GREEN  := \033[32m
    RESET  := \033[0m

    MKDIR_P = mkdir -p $(1)
    RM_RF = rm -rf $(1)

    PRINT_YELLOW = printf '%b\n' '$(YELLOW)$(1)$(RESET)'
    PRINT_CYAN = printf '%b\n' '$(CYAN)$(1)$(RESET)'
    PRINT_PURPLE = printf '%b\n' '$(PURPLE)$(1)$(RESET)'
    PRINT_GREEN = printf '%b\n' '$(GREEN)$(1)$(RESET)'

    LINK_EFI = lld-link
    LINK_EFI = lld-link
    LINK_KERNEL = $(LD)
endif

EFI_CFLAGS = -target x86_64-unknown-windows \
             -Wall -Wextra \
             -nostdlib \
             -fno-builtin \
             -ffreestanding \
             -fshort-wchar \
             -mno-red-zone \
             -IBootLoader/EDK2/MdePkg/Include \
             -IBootLoader/EDK2/MdePkg/Include/X64 \
             -ICommon \
             -IBootLoader/Include \
             -Wunused-variable

KERN_CFLAGS = -target x86_64-elf \
              -Wall -Wextra \
              -nostdlib \
              -fno-builtin \
              -ffreestanding \
              -mgeneral-regs-only \
              -mno-red-zone \
              -fno-pie \
              -fno-stack-protector \
              -DLINEOS_KERNEL_BUILD \
              -ICommon \
              -IKernel/Include \
              -IKernel/Assets \
              -Wunused-variable \
              -fshort-wchar

KERN_FLOAT_CFLAGS = $(filter-out -mgeneral-regs-only,$(KERN_CFLAGS)) \
                    -msse2 \
                    -mfpmath=sse

KERN_OBJ_CFLAGS = $(KERN_CFLAGS)

.PHONY: all clean \
        compile_boot compile_kernel \
        link_boot link_kernel \
        format naming

rwildcard = $(foreach d,$(wildcard $1/*),$(call rwildcard,$d,$2)) $(wildcard $1/$2)

BOOT_SRCS = $(call rwildcard,BootLoader,*.c)
KERN_SRCS = $(call rwildcard,Kernel,*.c)
KERN_ASM_SRCS = $(call rwildcard,Kernel,*.S)

FORMAT_EXCLUDE_SRCS = BootLoader/edk2/%

FORMAT_ALL_SRCS = $(call rwildcard,BootLoader,*.c) \
                  $(call rwildcard,BootLoader,*.h) \
                  $(call rwildcard,Common,*.c) \
                  $(call rwildcard,Common,*.h) \
                  $(call rwildcard,Kernel,*.c) \
                  $(call rwildcard,Kernel,*.h)

FORMAT_SRCS = $(filter-out $(FORMAT_EXCLUDE_SRCS),$(FORMAT_ALL_SRCS))
FORMAT_TARGETS = $(addprefix format/,$(FORMAT_SRCS))

NAMING_EXCLUDE_SRCS = $(FORMAT_EXCLUDE_SRCS)
NAMING_SRCS = $(filter-out $(NAMING_EXCLUDE_SRCS),$(FORMAT_ALL_SRCS))

BOOT_OBJS = $(patsubst %.c,Build/Objects/%.o,$(BOOT_SRCS))
KERN_OBJS = $(patsubst %.c,Build/Objects/%.o,$(KERN_SRCS))
KERN_ASM_OBJS = $(patsubst %.S,Build/Objects/%.o,$(KERN_ASM_SRCS))

KERN_OBJS += $(KERN_ASM_OBJS)

KERN_FLOAT_OBJS = Build/Objects/Kernel/Kernel.o \
                  Build/Objects/Kernel/Source/Render/TrueType/TrueType.o \
                  Build/Objects/Kernel/Source/Render/TrueType/TrueTypeEngine.o \
                  Build/Objects/Kernel/Source/Render/TrueType/TrueTypeRuntime.o

FONT_ASSETS = Kernel/Assets/Fonts/JBMNFSB.ttf \
              Kernel/Assets/Fonts/PTDSB.ttf

DEPS = $(BOOT_OBJS:.o=.d) $(KERN_OBJS:.o=.d)

all: $(TARGET1) $(TARGET2)
	@$(call PRINT_GREEN,[*] Done.)

compile_boot: $(BOOT_OBJS)
compile_kernel: $(KERN_OBJS)

link_boot: $(TARGET1)
link_kernel: $(TARGET2)

$(TARGET1): $(BOOT_OBJS)
	@$(call MKDIR_P,$(dir $@))
	@$(call PRINT_YELLOW,[LD] $@)
	@$(LINK_EFI) -nodefaultlib \
		$(BOOT_OBJS) \
		-dll \
		-entry:EfiMain \
		-subsystem:efi_application \
		-out:$@

$(TARGET2): $(KERN_OBJS) Link
	@$(call MKDIR_P,$(dir $@))
	@$(call PRINT_YELLOW,[LD] $@)
	@$(LINK_KERNEL) -static \
		-T Link/Link.ld \
		-e KMain \
		-o $@ \
		$(KERN_OBJS)

Build/Objects/BootLoader/%.o: BootLoader/%.c
	@$(call MKDIR_P,$(dir $@))
	@$(call PRINT_CYAN,[CC] $<)
	@$(CC) $(EFI_CFLAGS) $(DEPFLAGS) -c $< -o $@

Build/Objects/Kernel/%.o: Kernel/%.c
	@$(call MKDIR_P,$(dir $@))
	@$(call PRINT_CYAN,[CC] $<)
	@$(CC) $(KERN_OBJ_CFLAGS) $(DEPFLAGS) -c $< -o $@

Build/Objects/Kernel/%.o: Kernel/%.S
	@$(call MKDIR_P,$(dir $@))
	@$(call PRINT_PURPLE,[AS] $<)
	@$(CC) $(KERN_OBJ_CFLAGS) $(DEPFLAGS) -c $< -o $@

$(KERN_FLOAT_OBJS): KERN_OBJ_CFLAGS = $(KERN_FLOAT_CFLAGS)


clean:
	@$(call RM_RF,Build)
	@$(call RM_RF,$(TARGET1))
	@$(call RM_RF,$(TARGET2))

format:
	@$(call PRINT_YELLOW,[*] Format:)
	@$(MAKE) $(FORMAT_TARGETS)
	@$(call PRINT_GREEN,[*] Done.)

$(FORMAT_TARGETS):
	@$(CLANG_FORMAT) -i $(patsubst format/%,%,$@)

-include $(DEPS)
