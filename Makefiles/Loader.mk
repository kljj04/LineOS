# Makefiles/Loader.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

LOADER_TARGET := LineOS/Libraries/ld-lineos-x86_64.so.1
LOADER_OBJS := Build/Objects/Libraries/Loader/Source/Loader.o Build/Objects/Libraries/Loader/Source/Entry.o
LOADER_CFLAGS := -target x86_64-elf -O2 -Wall -Wextra -ffreestanding -fPIE -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -mno-red-zone -nostdlib -DLINEOS_KERNEL_BUILD -ICommon

$(LOADER_TARGET): $(LOADER_OBJS) Makefiles/Loader.mk
	@$(call MKDIR_P,$(dir $@))
	@$(LD) -shared -Bsymbolic -e _LoaderStart --no-undefined -z noexecstack -z max-page-size=4096 -o $@ $(LOADER_OBJS)
	@$(call BUILD_DONE,$@)

Build/Objects/Libraries/Loader/Source/%.o: Libraries/Loader/Source/%.c Makefiles/Loader.mk
	@$(call MKDIR_P,$(dir $@))
	@$(CC) $(LOADER_CFLAGS) $(DEPFLAGS) -c $< -o $@

Build/Objects/Libraries/Loader/Source/%.o: Libraries/Loader/Source/%.S Makefiles/Loader.mk
	@$(call MKDIR_P,$(dir $@))
	@$(CC) $(LOADER_CFLAGS) $(DEPFLAGS) -c $< -o $@

All: $(LOADER_TARGET)

-include $(LOADER_OBJS:.o=.d)
