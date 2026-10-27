# Makefiles/UserPrograms.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

CompileUserPrograms: $(USER_PROGRAM_OBJS)

LinkUserPrograms: $(USER_PROGRAM_OUTPUTS)

Build/Objects/UserPrograms/%.o: UserPrograms/%.c Makefiles/BuildSettings.mk Makefiles/UserPrograms.mk
	@$(call MKDIR_P,$(dir $@))
	@$(CC) $(USER_PROGRAM_CFLAGS) \
		-IUserPrograms/$(firstword $(subst /, ,$*))/Include \
		-I$(dir $<) \
		$(DEPFLAGS) \
		-c $< \
		-o $@

Build/Objects/UserPrograms/%.o: UserPrograms/%.S Makefiles/BuildSettings.mk Makefiles/UserPrograms.mk
	@$(call MKDIR_P,$(dir $@))
	@$(CC) $(USER_PROGRAM_CFLAGS) \
		-IUserPrograms/$(firstword $(subst /, ,$*))/Include \
		-I$(dir $<) \
		$(DEPFLAGS) \
		-c $< \
		-o $@

USER_PROGRAM_EXTRA_AddUser := $(USER_DB_TARGET)
USER_PROGRAM_EXTRA_DelUser := $(USER_DB_TARGET)
USER_PROGRAM_EXTRA_UserRescue := $(USER_DB_TARGET)
USER_PROGRAM_EXTRA_Login := $(USER_DB_TARGET)

define USER_PROGRAM_TEMPLATE

USER_PROGRAM_SRCS_$(1) := $(call RWildcard,$(USER_PROGRAM_ROOT)/$(1),*.c) $(call RWildcard,$(USER_PROGRAM_ROOT)/$(1),*.S)
USER_PROGRAM_OBJS_$(1) := $$(patsubst $(USER_PROGRAM_ROOT)/$(1)/%.c,Build/Objects/UserPrograms/$(1)/%.o,$$(filter %.c,$$(USER_PROGRAM_SRCS_$(1)))) $$(patsubst $(USER_PROGRAM_ROOT)/$(1)/%.S,Build/Objects/UserPrograms/$(1)/%.o,$$(filter %.S,$$(USER_PROGRAM_SRCS_$(1))))
USER_PROGRAM_OUTPUT_$(1) := $(USER_PROGRAM_OUTPUT_DIR)/$(1).lix

$$(USER_PROGRAM_OUTPUT_$(1)): $$(USER_PROGRAM_OBJS_$(1)) $(TRUE_TYPE_TARGET) $(LOSLIBC_TARGET) $$(USER_PROGRAM_EXTRA_$(1)) Link/UserProgram.ld Makefiles/UserPrograms.mk
	@$$(call MKDIR_P,$$(dir $$@))
	@$$(LD) -static \
		-T Link/UserProgram.ld \
		-e _Start \
		-o $$@ \
		$$(USER_PROGRAM_OBJS_$(1)) \
		$(TRUE_TYPE_TARGET) \
		$$(USER_PROGRAM_EXTRA_$(1)) \
		$(LOSLIBC_TARGET)

endef

$(foreach Program,$(USER_PROGRAMS),$(eval $(call USER_PROGRAM_TEMPLATE,$(Program))))

-include $(USER_PROGRAM_DEPS)