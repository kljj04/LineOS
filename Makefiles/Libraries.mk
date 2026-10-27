# Makefiles/Libraries.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

$(TRUE_TYPE_TARGET): $(TRUE_TYPE_OBJS) Makefiles/Libraries.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$@)
	@rm -f $@
	@$(AR) rcs $@ $(TRUE_TYPE_OBJS)
	@$(call BUILD_DONE,$@)

Build/Objects/Libraries/%.o: Libraries/%.c Makefiles/BuildSettings.mk Makefiles/Libraries.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(USER_PROGRAM_CFLAGS) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

-include $(TRUE_TYPE_OBJS:.o=.d)

$(USER_DB_TARGET): $(USER_DB_OBJS) Makefiles/Libraries.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$@)
	@rm -f $@
	@$(AR) rcs $@ $(USER_DB_OBJS)
	@$(call BUILD_DONE,$@)

Build/Objects/Libraries/Source/UserDB/%.o: Libraries/Source/UserDB/%.c Makefiles/BuildSettings.mk Makefiles/Libraries.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(USER_PROGRAM_CFLAGS) -O2 $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

-include $(USER_DB_OBJS:.o=.d)
