TARGET1 = LineOS/EFI/BOOT/BOOTX64.EFI
TARGET2 = LineOS/KERNEL/LINEOS_KERNEL.ELF

CC = clang
LD = ld.lld

MAKEFLAGS += --no-print-directory --silent

ifeq ($(OS),Windows_NT)
	IS_WINDOWS := 1
endif

ifdef IS_WINDOWS
	MKDIR_P = powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(1)' | Out-Null"
	RM_RF = powershell -NoProfile -Command "if (Test-Path '$(1)') { Remove-Item -Recurse -Force '$(1)' }"
	PRINT_YELLOW = powershell -NoProfile -Command "Write-Host '$(1)' -ForegroundColor Yellow"
	PRINT_CYAN = powershell -NoProfile -Command "Write-Host '$(1)' -ForegroundColor Cyan"
	PRINT_GREEN = powershell -NoProfile -Command "Write-Host '$(1)' -ForegroundColor Green"
	LINK_EFI = $(LD) -flavor link
	LINK_KERNEL = $(LD) -flavor gnu
else
	CYAN    := \033[36m
	YELLOW  := \033[33m
	GREEN   := \033[32m
	RESET   := \033[0m

	MKDIR_P = mkdir -p $(1)
	RM_RF = rm -rf $(1)
	PRINT_YELLOW = printf '%b\n' '$(YELLOW)$(1)$(RESET)'
	PRINT_CYAN = printf '%b\n' '$(CYAN)$(1)$(RESET)'
	PRINT_GREEN = printf '%b\n' '$(GREEN)$(1)$(RESET)'
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
			 -Ibootloader/edk2/MdePkg/Include \
			 -Ibootloader/edk2/MdePkg/Include/X64 \
			 -Icommon \
			 -Ibootloader/include \
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
			  -Icommon \
			  -Ikernel/include \
			  -Wunused-variable \
			  -fshort-wchar

KERN_FLOAT_CFLAGS = $(filter-out -mgeneral-regs-only,$(KERN_CFLAGS)) \
					-msse2 \
					-mfpmath=sse

.PHONY: all clean compile_boot compile_kernel link_boot link_kernel

rwildcard = $(foreach d,$(wildcard $1/*),$(call rwildcard,$d,$2)) $(wildcard $1/$2)

BOOT_SRCS = $(call rwildcard,bootloader,*.c)
KERN_SRCS = $(call rwildcard,kernel,*.c)
KERN_ASM_SRCS = $(call rwildcard,kernel,*.S)

BOOT_OBJS = $(BOOT_SRCS:.c=.o)
BOOT_OBJS := $(patsubst %,build/obj/%,$(BOOT_OBJS))
KERN_OBJS = $(KERN_SRCS:.c=.o)
KERN_OBJS := $(patsubst %,build/obj/%,$(KERN_OBJS))
KERN_ASM_OBJS = $(KERN_ASM_SRCS:.S=.o)
KERN_ASM_OBJS := $(patsubst %,build/obj/%,$(KERN_ASM_OBJS))
KERN_OBJS += $(KERN_ASM_OBJS)

all:
	@$(call PRINT_YELLOW,    [*] compile:)
	@$(call PRINT_CYAN,        [*] TARGET1...)
	@$(MAKE) compile_boot

	@$(call PRINT_CYAN,        [*] TARGET2...)
	@$(MAKE) compile_kernel

	@$(call PRINT_YELLOW,    [*] link:)
	@$(call PRINT_CYAN,        [*] TARGET1...)
	@$(MAKE) link_boot

	@$(call PRINT_CYAN,        [*] TARGET2...)
	@$(MAKE) link_kernel

	@$(call PRINT_GREEN,    [*] Done.)

compile_boot: $(BOOT_OBJS)
compile_kernel: $(KERN_OBJS)

link_boot: $(TARGET1)
link_kernel: $(TARGET2)

$(TARGET1): $(BOOT_OBJS)
	@$(call MKDIR_P,LineOS/EFI/BOOT)
	@$(LINK_EFI) -nodefaultlib \
		$(BOOT_OBJS) \
		-dll \
		-entry:EfiMain \
		-subsystem:efi_application \
		-out:$(TARGET1)

$(TARGET2): $(KERN_OBJS)
	@$(call MKDIR_P,LineOS/KERNEL)
	@$(LINK_KERNEL) -static \
		-T link/link.ld \
		-e KMain \
		-o $(TARGET2) \
		$(KERN_OBJS)

build/obj/bootloader/%.o: bootloader/%.c
	@$(call MKDIR_P,$(dir $@))
	@$(CC) $(EFI_CFLAGS) -c $< -o $@

build/obj/kernel/%.o: kernel/%.c
	@$(call MKDIR_P,$(dir $@))
	@$(CC) $(KERN_CFLAGS) -c $< -o $@

build/obj/kernel/%.o: kernel/%.S
	@$(call MKDIR_P,$(dir $@))
	@$(CC) $(KERN_CFLAGS) -c $< -o $@

build/obj/kernel/kernel.o: kernel/kernel.c
	@$(call MKDIR_P,$(dir $@))
	@$(CC) $(KERN_FLOAT_CFLAGS) -c $< -o $@

build/obj/kernel/src/render/truetype.o: kernel/src/render/truetype.c
	@$(call MKDIR_P,$(dir $@))
	@$(CC) $(KERN_FLOAT_CFLAGS) -c $< -o $@

build/obj/kernel/src/render/truetype_engine.o: kernel/src/render/truetype_engine.c
	@$(call MKDIR_P,$(dir $@))
	@$(CC) $(KERN_FLOAT_CFLAGS) -c $< -o $@

build/obj/kernel/src/render/truetype_runtime.o: kernel/src/render/truetype_runtime.c
	@$(call MKDIR_P,$(dir $@))
	@$(CC) $(KERN_FLOAT_CFLAGS) -c $< -o $@

clean:
	@$(call RM_RF,build)
	@$(call RM_RF,$(TARGET1))
	@$(call RM_RF,$(TARGET2))
