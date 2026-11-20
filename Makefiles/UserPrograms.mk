# Makefiles/UserPrograms.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

ifndef USER_PROGRAM_MANIFEST
USER_PROGRAM_MANIFEST := $(shell bash Scripts/Backend/Programs.sh .)
ifneq ($(.SHELLSTATUS),0)
$(error Invalid program configuration)
endif
endif

Comma := ,
ProgramFields = $(subst |, ,$(1))
ProgramLibraryIncludes = $(foreach Library,$(subst $(Comma), ,$(1)),$(USER_LIBRARY_INCLUDE_$(Library)))
ProgramLibraries = $(foreach Library,$(subst $(Comma), ,$(1)),$(if $(filter -,$(Library)),,$(if $(USER_LIBRARY_$(Library)),$(USER_LIBRARY_$(Library)),$(error Library $(Library) has no exported link target))))
ProgramLinkLibraries = $(foreach Library,$(subst $(Comma), ,$(1)),$(if $(USER_LIBRARY_LINK_$(Library)),$(USER_LIBRARY_LINK_$(Library)),$(USER_LIBRARY_$(Library))))
ProgramLibraryPaths = $(sort $(foreach Library,$(filter-out -,$(subst $(Comma), ,$(1))),$(USER_LIBRARY_RPATH_$(Library))))
USER_PROGRAMS := $(foreach Row,$(USER_PROGRAM_MANIFEST),$(word 1,$(call ProgramFields,$(Row))))

USER_INTERPRETER := /lib/libc.so
MUSL_CFLAGS = $(USER_PROGRAM_CFLAGS)
MUSL_START_TARGETS := Libraries/musl/lib/Scrt1.o Libraries/musl/lib/crti.o
MUSL_END_TARGET := Libraries/musl/lib/crtn.o

$(MUSL_START_TARGETS) $(MUSL_END_TARGET): | LibraryMakefile_musl
	@test -f $@

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

define USER_PROGRAM_TEMPLATE
USER_PROGRAM_SRCS_$(1) := $(filter-out -,$(subst $(Comma), ,$(4)))
USER_PROGRAM_OBJS_$(1) := $$(addprefix Build/Objects/,$$(addsuffix .o,$$(basename $$(USER_PROGRAM_SRCS_$(1)))))
USER_PROGRAM_OUTPUT_$(1) := $(USER_PROGRAM_OUTPUT_DIR)/$(1)/$(6).lix
USER_PROGRAM_RUNTIME_$(1) := Build/Generated/Programs/$(1)/Runtime.c
USER_PROGRAM_RUNTIME_OBJ_$(1) := Build/Objects/Programs/$(1)/Runtime.o
$$(USER_PROGRAM_RUNTIME_$(1)): $(USER_PROGRAM_ROOT)/$(1)/Program.cfg Scripts/Backend/ProgramConfig.py Scripts/Backend/ProgramRuntime.c.in
	@python3 Scripts/Backend/ProgramConfig.py generate . $(1) $$@
$$(USER_PROGRAM_RUNTIME_OBJ_$(1)): $$(USER_PROGRAM_RUNTIME_$(1)) Makefiles/BuildSettings.mk Makefiles/UserPrograms.mk | LibraryMakefile_musl
	@$$(call MKDIR_P,$$(dir $$@))
	@$$(CC) $(USER_PROGRAM_CFLAGS) $(DEPFLAGS) -c $$< -o $$@

USER_PROGRAM_LIBS_$(1) := $(call ProgramLibraries,$(3)) $(LOSLIB_TARGET) LineOS/Libraries/libc.so
USER_PROGRAM_START_$(1) := $(MUSL_START_TARGETS)
USER_PROGRAM_END_$(1) := $(MUSL_END_TARGET)
USER_PROGRAM_INTERPRETER_$(1) := $(8)
USER_PROGRAM_ENTRY_$(1) := _start
USER_PROGRAM_LINK_$(1) := $(call LibraryRPath,$(call ProgramLibraryPaths,$(3)$(Comma)LOSLib$(Comma)musl)) $(call ProgramLinkLibraries,$(3)) $(LOSLIB_TARGET) -LLineOS/Libraries -l:libc.so
$$(USER_PROGRAM_OBJS_$(1)): private USER_PROGRAM_COMPILE_FLAGS := $(USER_PROGRAM_CFLAGS)
$$(USER_PROGRAM_OBJS_$(1)): | LibraryMakefile_musl

$$(USER_PROGRAM_OBJS_$(1)): private USER_PROGRAM_INCLUDE := -I$(USER_PROGRAM_ROOT)/$(1)/Include $(call ProgramLibraryIncludes,$(3))
$$(USER_PROGRAM_OBJS_$(1)): $(USER_PROGRAM_ROOT)/$(1)/Program.cfg $(LIBRARY_CONFIGS)
$$(USER_PROGRAM_OUTPUT_$(1)): PrepareUserPrograms $$(USER_PROGRAM_START_$(1)) $$(USER_PROGRAM_END_$(1)) $$(USER_PROGRAM_RUNTIME_OBJ_$(1)) $$(USER_PROGRAM_OBJS_$(1)) $$(USER_PROGRAM_LIBS_$(1)) $(USER_PROGRAM_ROOT)/$(1)/Program.cfg $(shell find $(USER_PROGRAM_ROOT)/$(1) -type d) $(LIBRARY_CONFIGS) Link/UserProgram.ld Makefiles/UserPrograms.mk Scripts/Backend/Programs.sh Scripts/Backend/ProgramConfig.py
	@$$(call MKDIR_P,$$(dir $$@))
	@$$(call BUILD_PROGRESS,$$@)
	@$$(LD) -pie -z max-page-size=4096 --dynamic-linker $$(USER_PROGRAM_INTERPRETER_$(1)) -z noexecstack --as-needed -e $$(USER_PROGRAM_ENTRY_$(1)) -o $$@ $$(USER_PROGRAM_START_$(1)) $$(USER_PROGRAM_RUNTIME_OBJ_$(1)) $$(USER_PROGRAM_OBJS_$(1)) --start-group $$(USER_PROGRAM_LINK_$(1)) --end-group $$(USER_PROGRAM_END_$(1))
	@$$(call BUILD_DONE,$$@)
endef

$(foreach Row,$(USER_PROGRAM_MANIFEST),$(if $(filter $(word 1,$(call ProgramFields,$(Row))),$(USER_PROGRAMS)),$(eval $(call USER_PROGRAM_TEMPLATE,$(word 1,$(call ProgramFields,$(Row))),$(word 2,$(call ProgramFields,$(Row))),$(word 3,$(call ProgramFields,$(Row))),$(word 4,$(call ProgramFields,$(Row))),$(word 5,$(call ProgramFields,$(Row))),$(word 6,$(call ProgramFields,$(Row))),$(word 7,$(call ProgramFields,$(Row))),$(word 8,$(call ProgramFields,$(Row)))))))

CompileUserPrograms: $(USER_PROGRAM_OBJS) $(foreach Program,$(USER_PROGRAMS),$(USER_PROGRAM_RUNTIME_OBJ_$(Program)))
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

-include $(USER_PROGRAM_DEPS)
-include $(foreach Program,$(USER_PROGRAMS),$(USER_PROGRAM_RUNTIME_OBJ_$(Program):.o=.d))
