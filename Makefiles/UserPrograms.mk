# Makefiles/UserPrograms.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

USER_PROGRAM_MANIFEST := $(shell bash Scripts/Backend/Programs.sh .)
ifneq ($(.SHELLSTATUS),0)
$(error Invalid program configuration)
endif

Comma := ,
ProgramFields = $(subst |, ,$(1))
ProgramLibraries = $(foreach Library,$(subst $(Comma), ,$(1)),$(USER_LIBRARY_$(Library)))
USER_LIBRARY_LOSLibC := $(LOSLIBC_TARGET)
USER_PROGRAMS := $(foreach Row,$(USER_PROGRAM_MANIFEST),$(word 1,$(call ProgramFields,$(Row))))

USER_CRT0_TARGET := Build/Objects/LOSLibC/crt0.o

ifneq ($(strip $(Program)),)
ifneq ($(filter $(Program),$(USER_PROGRAMS)),$(Program))
$(error Unknown or disabled program: $(Program))
endif
USER_PROGRAMS := $(Program)
endif

.PHONY: CompileUserPrograms LinkUserPrograms UserProgram PrepareUserPrograms

All CompileUserPrograms LinkUserPrograms UserProgram: PrepareUserPrograms

PrepareUserPrograms:
	@$(call RM_RF,$(USER_PROGRAM_OUTPUT_DIR))

$(USER_CRT0_TARGET): LOSLibC/crt0.c Makefiles/BuildSettings.mk Makefiles/UserPrograms.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(USER_PROGRAM_CFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

define USER_PROGRAM_TEMPLATE
USER_PROGRAM_SRCS_$(1) := $(filter-out -,$(subst $(Comma), ,$(4)))
USER_PROGRAM_OBJS_$(1) := $$(addprefix Build/Objects/,$$(addsuffix .o,$$(basename $$(USER_PROGRAM_SRCS_$(1)))))
USER_PROGRAM_OUTPUT_$(1) := $(USER_PROGRAM_OUTPUT_DIR)/$(1)/$(notdir $(1)).lix
USER_PROGRAM_LIBS_$(1) := $(filter-out $(LOSLIBC_TARGET),$(call ProgramLibraries,$(3))) $(LOSLIBC_TARGET)

$$(USER_PROGRAM_OBJS_$(1)): private USER_PROGRAM_INCLUDE := -I$(USER_PROGRAM_ROOT)/$(1)/Include
$$(USER_PROGRAM_OBJS_$(1)): $(USER_PROGRAM_ROOT)/$(1)/Program.cfg
$$(USER_PROGRAM_OUTPUT_$(1)): PrepareUserPrograms $(USER_CRT0_TARGET) $$(USER_PROGRAM_OBJS_$(1)) $$(USER_PROGRAM_LIBS_$(1)) $(USER_PROGRAM_ROOT)/$(1)/Program.cfg $(shell find $(USER_PROGRAM_ROOT)/$(1) -type d) Link/UserProgram.ld Makefiles/UserPrograms.mk Scripts/Backend/Programs.sh
	@$$(call MKDIR_P,$$(dir $$@))
	@$$(call BUILD_PROGRESS,$$@)
	@$$(LD) -static -T Link/UserProgram.ld -e _Start -o $$@ $(USER_CRT0_TARGET) $$(USER_PROGRAM_OBJS_$(1)) --start-group $$(USER_PROGRAM_LIBS_$(1)) --end-group
	@$$(call BUILD_DONE,$$@)
endef

$(foreach Row,$(USER_PROGRAM_MANIFEST),$(if $(filter $(word 1,$(call ProgramFields,$(Row))),$(USER_PROGRAMS)),$(eval $(call USER_PROGRAM_TEMPLATE,$(word 1,$(call ProgramFields,$(Row))),$(word 2,$(call ProgramFields,$(Row))),$(word 3,$(call ProgramFields,$(Row))),$(word 4,$(call ProgramFields,$(Row)))))))

CompileUserPrograms: $(USER_PROGRAM_OBJS) $(USER_CRT0_TARGET)
LinkUserPrograms: $(USER_PROGRAM_OUTPUTS)
UserProgram: $(USER_PROGRAM_OUTPUTS)

Build/Objects/UserPrograms/%.o: UserPrograms/%.c Makefiles/BuildSettings.mk Makefiles/UserPrograms.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(USER_PROGRAM_CFLAGS) $(USER_PROGRAM_INCLUDE) -I$(dir $<) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

Build/Objects/UserPrograms/%.o: UserPrograms/%.S Makefiles/BuildSettings.mk Makefiles/UserPrograms.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(USER_PROGRAM_CFLAGS) $(USER_PROGRAM_INCLUDE) -I$(dir $<) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

-include $(USER_PROGRAM_DEPS)