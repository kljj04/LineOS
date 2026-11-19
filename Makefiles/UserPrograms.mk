# Makefiles/UserPrograms.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

USER_PROGRAM_MANIFEST := $(shell bash Scripts/Backend/Programs.sh .)
ifneq ($(.SHELLSTATUS),0)
$(error Invalid program configuration)
endif

Comma := ,
ProgramFields = $(subst |, ,$(1))
ProgramLibraryIncludes = $(foreach Library,$(subst $(Comma), ,$(1)),$(USER_LIBRARY_INCLUDE_$(Library)))
ProgramLibraries = $(foreach Library,$(subst $(Comma), ,$(1)),$(if $(filter -,$(Library)),,$(if $(USER_LIBRARY_$(Library)),$(USER_LIBRARY_$(Library)),$(error Library $(Library) has no exported link target))))
ProgramLinkLibraries = $(foreach Library,$(subst $(Comma), ,$(1)),$(if $(USER_LIBRARY_LINK_$(Library)),$(USER_LIBRARY_LINK_$(Library)),$(USER_LIBRARY_$(Library))))
USER_LIBRARY_LOSLibC := $(LOSLIBC_TARGET)
USER_PROGRAMS := $(foreach Row,$(USER_PROGRAM_MANIFEST),$(word 1,$(call ProgramFields,$(Row))))

USER_CRT0_TARGET := Build/Objects/Libraries/LOSLibC/Runtime/crt0.o
USER_START_TARGET := Build/Objects/Libraries/LOSLibC/Runtime/Start.o
USER_INTERPRETER := /lib/ld-lineos-x86_64.so.1
MUSL_CFLAGS = $(filter-out -ILibraries/LOSLibC/Include,$(USER_PROGRAM_CFLAGS)) -nostdinc -isystem Libraries/musl/obj/include -isystem Libraries/musl/arch/x86_64 -isystem Libraries/musl/arch/generic -isystem Libraries/musl/include
MUSL_START_TARGETS := Libraries/musl/lib/Scrt1.o Libraries/musl/lib/crti.o
MUSL_END_TARGET := Libraries/musl/lib/crtn.o

ifneq ($(LIBRARY_TARGET_musl),)
$(MUSL_START_TARGETS) $(MUSL_END_TARGET): | $(LIBRARY_TARGET_musl)
	@test -f $@
endif

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

Build/Objects/Libraries/LOSLibC/Runtime/%.o: Libraries/LOSLibC/Runtime/%.c Makefiles/BuildSettings.mk Makefiles/UserPrograms.mk
	@$(call MKDIR_P,$(dir $@))
	@$(CC) $(USER_PROGRAM_CFLAGS) $(DEPFLAGS) -c $< -o $@

Build/Objects/Libraries/LOSLibC/Runtime/%.o: Libraries/LOSLibC/Runtime/%.S Makefiles/BuildSettings.mk Makefiles/UserPrograms.mk
	@$(call MKDIR_P,$(dir $@))
	@$(CC) $(USER_PROGRAM_CFLAGS) $(DEPFLAGS) -c $< -o $@

define USER_PROGRAM_TEMPLATE
USER_PROGRAM_SRCS_$(1) := $(filter-out -,$(subst $(Comma), ,$(4)))
USER_PROGRAM_OBJS_$(1) := $$(addprefix Build/Objects/,$$(addsuffix .o,$$(basename $$(USER_PROGRAM_SRCS_$(1)))))
USER_PROGRAM_OUTPUT_$(1) := $(USER_PROGRAM_OUTPUT_DIR)/$(1)/$(notdir $(1)).lix
$(if $(filter musl,$(subst $(Comma), ,$(3))),$(if $(filter-out musl,$(subst $(Comma), ,$(3))),$(error musl programs currently require musl as their only dependency)))
USER_PROGRAM_LIBS_$(1) := $(call ProgramLibraries,$(3)) $(if $(filter musl,$(subst $(Comma), ,$(3))),,$(LOSLIBC_TARGET))
USER_PROGRAM_START_$(1) := $(if $(filter musl,$(subst $(Comma), ,$(3))),$(MUSL_START_TARGETS),$(USER_CRT0_TARGET) $(USER_START_TARGET))
USER_PROGRAM_END_$(1) := $(if $(filter musl,$(subst $(Comma), ,$(3))),$(MUSL_END_TARGET))
USER_PROGRAM_INTERPRETER_$(1) := $(if $(filter musl,$(subst $(Comma), ,$(3))),/lib/$(notdir $(USER_LIBRARY_musl)),$(USER_INTERPRETER))
USER_PROGRAM_ENTRY_$(1) := $(if $(filter musl,$(subst $(Comma), ,$(3))),_start,_Start)
USER_PROGRAM_LINK_$(1) := $(call ProgramLinkLibraries,$(3)) $(if $(filter musl,$(subst $(Comma), ,$(3))),,$(LOSLIBC_TARGET))
$$(USER_PROGRAM_OBJS_$(1)): private USER_PROGRAM_COMPILE_FLAGS := $(if $(filter musl,$(subst $(Comma), ,$(3))),$(MUSL_CFLAGS),$(USER_PROGRAM_CFLAGS))
$(if $(filter musl,$(subst $(Comma), ,$(3))),$$(USER_PROGRAM_OBJS_$(1)): | $(LIBRARY_TARGET_musl))

$$(USER_PROGRAM_OBJS_$(1)): private USER_PROGRAM_INCLUDE := -I$(USER_PROGRAM_ROOT)/$(1)/Include $(call ProgramLibraryIncludes,$(3))
$$(USER_PROGRAM_OBJS_$(1)): $(USER_PROGRAM_ROOT)/$(1)/Program.cfg
$$(USER_PROGRAM_OUTPUT_$(1)): PrepareUserPrograms $$(USER_PROGRAM_START_$(1)) $$(USER_PROGRAM_END_$(1)) $$(USER_PROGRAM_OBJS_$(1)) $$(USER_PROGRAM_LIBS_$(1)) $(USER_PROGRAM_ROOT)/$(1)/Program.cfg $(shell find $(USER_PROGRAM_ROOT)/$(1) -type d) Link/UserProgram.ld Makefiles/UserPrograms.mk Scripts/Backend/Programs.sh
	@$$(call MKDIR_P,$$(dir $$@))
	@$$(call BUILD_PROGRESS,$$@)
	@$$(LD) -pie -z max-page-size=4096 --dynamic-linker $$(USER_PROGRAM_INTERPRETER_$(1)) -z noexecstack -e $$(USER_PROGRAM_ENTRY_$(1)) -o $$@ $$(USER_PROGRAM_START_$(1)) $$(USER_PROGRAM_OBJS_$(1)) --start-group $$(USER_PROGRAM_LINK_$(1)) --end-group $$(USER_PROGRAM_END_$(1))
	@$$(call BUILD_DONE,$$@)
endef

$(foreach Row,$(USER_PROGRAM_MANIFEST),$(if $(filter $(word 1,$(call ProgramFields,$(Row))),$(USER_PROGRAMS)),$(eval $(call USER_PROGRAM_TEMPLATE,$(word 1,$(call ProgramFields,$(Row))),$(word 2,$(call ProgramFields,$(Row))),$(word 3,$(call ProgramFields,$(Row))),$(word 4,$(call ProgramFields,$(Row)))))))

CompileUserPrograms: $(USER_PROGRAM_OBJS) $(USER_CRT0_TARGET) $(USER_START_TARGET)
LinkUserPrograms: $(USER_PROGRAM_OUTPUTS)
UserProgram: $(USER_PROGRAM_OUTPUTS)

Build/Objects/UserPrograms/%.o: UserPrograms/%.c Makefiles/BuildSettings.mk Makefiles/UserPrograms.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(USER_PROGRAM_COMPILE_FLAGS) $(USER_PROGRAM_INCLUDE) -I$(dir $<) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

Build/Objects/UserPrograms/%.o: UserPrograms/%.S Makefiles/BuildSettings.mk Makefiles/UserPrograms.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(USER_PROGRAM_COMPILE_FLAGS) $(USER_PROGRAM_INCLUDE) -I$(dir $<) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

-include $(USER_PROGRAM_DEPS) $(USER_CRT0_TARGET:.o=.d) $(USER_START_TARGET:.o=.d)