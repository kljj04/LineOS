TARGET1 = LineOS/EFI/BOOT/BOOTX64.EFI
TARGET2 = LineOS/KERNEL/LINEOS_KERNEL.ELF

CC = clang
LD = ld.lld

MAKEFLAGS += --no-print-directory --silent

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
			  -Wunused-variable

.PHONY: all clean compile_boot compile_kernel link_boot link_kernel

rwildcard = $(foreach d,$(wildcard $1/*),$(call rwildcard,$d,$2)) $(wildcard $1/$2)

BOOT_SRCS = $(call rwildcard,bootloader,*.c)
KERN_SRCS = $(call rwildcard,kernel,*.c)

BOOT_OBJS = $(BOOT_SRCS:.c=.o)
BOOT_OBJS := $(patsubst %,build/obj/%,$(BOOT_OBJS))
KERN_OBJS = $(KERN_SRCS:.c=.o)
KERN_OBJS := $(patsubst %,build/obj/%,$(KERN_OBJS))

all:
	@powershell -Command "Write-Host '    [*] compile:' -ForegroundColor Yellow"
	@powershell -Command "Write-Host '        [*] TARGET1...' -ForegroundColor Cyan"
	@$(MAKE) compile_boot

	@powershell -Command "Write-Host '        [*] TARGET2...' -ForegroundColor Cyan"
	@$(MAKE) compile_kernel

	@powershell -Command "Write-Host '    [*] link:' -ForegroundColor Yellow"
	@powershell -Command "Write-Host '        [*] TARGET1...' -ForegroundColor Cyan"
	@$(MAKE) link_boot

	@powershell -Command "Write-Host '        [*] TARGET2...' -ForegroundColor Cyan"
	@$(MAKE) link_kernel

	@powershell -Command "Write-Host '    [*] Done.' -ForegroundColor Green"


compile_boot: $(BOOT_OBJS)
compile_kernel: $(KERN_OBJS)

link_boot: $(TARGET1)
link_kernel: $(TARGET2)

$(TARGET1): $(BOOT_OBJS)
	@if not exist LineOS\EFI\BOOT mkdir LineOS\EFI\BOOT > nul
	@$(LD) -flavor link \
		-nodefaultlib \
		$(BOOT_OBJS) \
		-dll \
		-entry:EfiMain \
		-subsystem:efi_application \
		-out:$(TARGET1)

$(TARGET2): $(KERN_OBJS)
	@if not exist LineOS\KERNEL mkdir LineOS\KERNEL > nul
	@$(LD) -flavor gnu \
		-static \
		-T link/link.ld \
		-e KMain \
		-o $(TARGET2) \
		$(KERN_OBJS)

build/obj/bootloader/%.o: bootloader/%.c
	@powershell -Command "$$d = Split-Path -Parent '$@'; if (!(Test-Path $$d)) { New-Item -ItemType Directory -Force -Path $$d | Out-Null }"
	@$(CC) $(EFI_CFLAGS) -c $< -o $@

build/obj/kernel/%.o: kernel/%.c
	@powershell -Command "$$d = Split-Path -Parent '$@'; if (!(Test-Path $$d)) { New-Item -ItemType Directory -Force -Path $$d | Out-Null }"
	@$(CC) $(KERN_CFLAGS) -c $< -o $@

clean:
	@if exist build rmdir /s /q build > nul
	@if exist LineOS\EFI\BOOT\BOOTX64.EFI del /Q LineOS\EFI\BOOT\BOOTX64.EFI > nul
	@if exist LineOS\KERNEL\LINEOS_KERNEL.ELF del /Q LineOS\KERNEL\LINEOS_KERNEL.ELF > nul
