# Makefiles/LOSLibC.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

CompileLOSLibC: $(LOSLIBC_OBJS)

LinkLOSLibC: $(LOSLIBC_TARGET)

$(LOSLIBC_TARGET): $(LOSLIBC_OBJS) Makefiles/LOSLibC.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$@)
	@rm -f $@
	@$(LD) -shared -z max-page-size=4096 -soname liblosc.so -z defs -z noexecstack -o $@ $(LOSLIBC_OBJS)
	@$(call BUILD_DONE,$@)

Build/Objects/Libraries/LOSLibC/%.o: Libraries/LOSLibC/%.c Makefiles/BuildSettings.mk Makefiles/LOSLibC.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(LOSLIBC_CFLAGS) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

Build/Objects/Libraries/LOSLibC/%.o: Libraries/LOSLibC/%.S Makefiles/BuildSettings.mk Makefiles/LOSLibC.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(LOSLIBC_CFLAGS) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

-include $(LOSLIBC_DEPS)
