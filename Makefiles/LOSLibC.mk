# Makefiles/LOSLibC.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

CompileLOSLibC: $(LOSLIBC_OBJS)

LinkLOSLibC: $(LOSLIBC_TARGET)

$(LOSLIBC_TARGET): $(LOSLIBC_OBJS) Makefiles/LOSLibC.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$@)
	@rm -f $@
	@$(AR) rcs $@ $(LOSLIBC_OBJS)
	@$(call BUILD_DONE,$@)

Build/Objects/LOSLibC/%.o: LOSLibC/%.c Makefiles/BuildSettings.mk Makefiles/LOSLibC.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(LOSLIBC_CFLAGS) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

Build/Objects/LOSLibC/%.o: LOSLibC/%.S Makefiles/BuildSettings.mk Makefiles/LOSLibC.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(LOSLIBC_CFLAGS) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

-include $(LOSLIBC_DEPS)