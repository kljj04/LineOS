# Makefiles/BootLoader.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

CompileBoot: $(BOOT_OBJS)

LinkBoot: $(BOOT_TARGET)

$(BOOT_TARGET): $(BOOT_OBJS)
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$@)
	@$(LINK_EFI) -nodefaultlib \
		$(BOOT_OBJS) \
		-dll \
		-entry:EfiMain \
		-subsystem:efi_application \
		-out:$@
	@$(call BUILD_DONE,$@)

Build/Objects/BootLoader/%.o: BootLoader/%.c Makefiles/BuildSettings.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(EFI_CFLAGS) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

-include $(BOOT_DEPS)
