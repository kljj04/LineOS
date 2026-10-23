# Makefiles/BootLoader.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

CompileBoot: $(BOOT_OBJS)

LinkBoot: $(BOOT_TARGET)

$(BOOT_TARGET): $(BOOT_OBJS)
	@$(call MKDIR_P,$(dir $@))
	@$(call PRINT_YELLOW,[LD] $@)
	@$(LINK_EFI) -nodefaultlib \
		$(BOOT_OBJS) \
		-dll \
		-entry:EfiMain \
		-subsystem:efi_application \
		-out:$@

Build/Objects/BootLoader/%.o: BootLoader/%.c Makefiles/BuildSettings.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call PRINT_CYAN,[CC] $<)
	@$(CC) $(EFI_CFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(BOOT_DEPS)
