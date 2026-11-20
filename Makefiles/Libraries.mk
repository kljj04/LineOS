# Makefiles/Libraries.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

LIBRARY_MANIFEST := $(shell bash Scripts/Backend/Libraries.sh .)
ifneq ($(.SHELLSTATUS),0)
$(error Invalid library configuration)
endif

Comma := ,
LibraryFields = $(subst |, ,$(1))
LIBRARIES := $(foreach Row,$(LIBRARY_MANIFEST),$(word 1,$(call LibraryFields,$(Row))))
LIBRARY_TARGETS = $(foreach Library,$(LIBRARIES),$(LIBRARY_TARGET_$(Library)))
LIBRARY_CONFIGS := $(wildcard Libraries/*/Library.cfg)
LIBRARY_OBJS = $(foreach Library,$(LIBRARIES),$(LIBRARY_OBJS_$(Library)))
LIBRARY_BASE_CFLAGS = -target $(KERNEL_TYPE_TARGET) -Wall -Wextra -nostdlib -fno-builtin -ffreestanding -mno-red-zone -fPIC -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -ICommon -nostdinc
LIBRARY_NONE_CFLAGS = $(LIBRARY_BASE_CFLAGS) -isystem $(shell $(CC) -print-resource-dir)/include
LIBRARY_MUSL_CFLAGS = $(LIBRARY_BASE_CFLAGS) -D_GNU_SOURCE -isystem Libraries/musl/obj/include -isystem Libraries/musl/arch/x86_64 -isystem Libraries/musl/arch/generic -isystem Libraries/musl/include
LibraryIncludes = $(foreach Library,$(filter-out -,$(subst $(Comma), ,$(1))),$(USER_LIBRARY_OWN_INCLUDE_$(Library)))
LibraryTargets = $(foreach Library,$(filter-out -,$(subst $(Comma), ,$(1))),$(USER_LIBRARY_$(Library)))
LibraryLinks = $(foreach Library,$(filter-out -,$(subst $(Comma), ,$(1))),$(USER_LIBRARY_LINK_$(Library)))
LibraryPaths = $(sort $(foreach Library,$(filter-out -,$(subst $(Comma), ,$(1))),$(USER_LIBRARY_PATH_$(Library))))
LibraryRPath = $(if $(strip $(1)),-rpath $(subst $(Space),:,$(strip $(1))))
Empty :=
Space := $(Empty) $(Empty)

define LIBRARY_SETTINGS
USER_LIBRARY_$(1) := LineOS/Libraries/$(7)
USER_LIBRARY_LINK_$(1) := -LLineOS/Libraries -l:$(7)
USER_LIBRARY_OWN_INCLUDE_$(1) := -ILibraries/$(2)/Include
USER_LIBRARY_PATH_$(1) := $(8)
LIBRARY_TARGET_$(1) := $(if $(filter -,$(5)),LineOS/Libraries/$(7),LibraryMakefile_$(1))
USER_LIBRARY_CLOSURE_$(1) := $(11)
USER_LIBRARY_INCLUDE_$(1) = $$(USER_LIBRARY_OWN_INCLUDE_$(1)) $$(call LibraryIncludes,$$(USER_LIBRARY_CLOSURE_$(1)))
USER_LIBRARY_RPATH_$(1) = $$(USER_LIBRARY_PATH_$(1)) $$(call LibraryPaths,$$(USER_LIBRARY_CLOSURE_$(1)))
LIBRARY_SRCS_$(1) := $(filter-out -,$(subst $(Comma), ,$(4)))
LIBRARY_OBJS_$(1) := $(if $(filter -,$(5)),$(addprefix Build/Objects/,$(addsuffix .o,$(basename $(subst $(Comma), ,$(filter-out -,$(4)))))))
endef

$(foreach Row,$(LIBRARY_MANIFEST),$(eval $(call LIBRARY_SETTINGS,$(word 1,$(call LibraryFields,$(Row))),$(word 2,$(call LibraryFields,$(Row))),$(word 3,$(call LibraryFields,$(Row))),$(word 4,$(call LibraryFields,$(Row))),$(word 5,$(call LibraryFields,$(Row))),$(word 6,$(call LibraryFields,$(Row))),$(word 7,$(call LibraryFields,$(Row))),$(word 8,$(call LibraryFields,$(Row))),$(word 9,$(call LibraryFields,$(Row))),$(word 10,$(call LibraryFields,$(Row))),$(word 11,$(call LibraryFields,$(Row))))))

define LIBRARY_TEMPLATE
$$(LIBRARY_OBJS_$(1)): private LIBRARY_COMPILE_FLAGS := $(if $(filter none,$(9)),$(LIBRARY_NONE_CFLAGS),$(LIBRARY_MUSL_CFLAGS)) $$(USER_LIBRARY_INCLUDE_$(1)) $(subst $(Comma), ,$(filter-out -,$(3)))
$$(LIBRARY_OBJS_$(1)): $(LIBRARY_CONFIGS) Makefiles/Libraries.mk Scripts/Backend/LibraryConfig.py
$$(LIBRARY_OBJS_$(1)): | $(call LibraryTargets,$(11))
$$(USER_LIBRARY_$(1)): $$(LIBRARY_OBJS_$(1)) $(call LibraryTargets,$(11)) Libraries/$(2)/Library.cfg Makefiles/Libraries.mk Scripts/Backend/LibraryConfig.py $(shell find Libraries/$(2)/Source -type d)
	@$$(call MKDIR_P,$$(dir $$@))
	@$$(call BUILD_PROGRESS,$$@)
	@$$(LD) -shared -z max-page-size=4096 -soname $(7) -z defs -z noexecstack --as-needed $(call LibraryRPath,$(call LibraryPaths,$(10))) -o $$@ $$(LIBRARY_OBJS_$(1)) $(call LibraryLinks,$(10))
	@$$(call BUILD_DONE,$$@)
endef

define LIBRARY_MAKEFILE_TEMPLATE
$$(USER_LIBRARY_$(1)): LibraryMakefile_$(1)
.PHONY: LibraryMakefile_$(1)
LibraryMakefile_$(1): Libraries/$(2)/Library.cfg Libraries/$(2)/$(5) Scripts/Backend/LibraryConfig.py $(call LibraryTargets,$(11))
	@$$(call BUILD_PROGRESS,Libraries/$(2)/$(5))
	+$$(MAKE) -C Libraries/$(2) -f $(5)
	@bash Scripts/Backend/Libraries.sh --copy-so . $(1)
	@$$(call BUILD_DONE,Libraries/$(2)/$(5))
endef

$(foreach Row,$(LIBRARY_MANIFEST),$(eval $(call $(if $(filter -,$(word 5,$(call LibraryFields,$(Row)))),LIBRARY_TEMPLATE,LIBRARY_MAKEFILE_TEMPLATE),$(word 1,$(call LibraryFields,$(Row))),$(word 2,$(call LibraryFields,$(Row))),$(word 3,$(call LibraryFields,$(Row))),$(word 4,$(call LibraryFields,$(Row))),$(word 5,$(call LibraryFields,$(Row))),$(word 6,$(call LibraryFields,$(Row))),$(word 7,$(call LibraryFields,$(Row))),$(word 8,$(call LibraryFields,$(Row))),$(word 9,$(call LibraryFields,$(Row))),$(word 10,$(call LibraryFields,$(Row))),$(word 11,$(call LibraryFields,$(Row))))))

Build/Objects/Libraries/%.o: Libraries/%.c Makefiles/BuildSettings.mk Makefiles/Libraries.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(LIBRARY_COMPILE_FLAGS) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

Build/Objects/Libraries/%.o: Libraries/%.S Makefiles/BuildSettings.mk Makefiles/Libraries.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(LIBRARY_COMPILE_FLAGS) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

LOSLIB_TARGET := $(USER_LIBRARY_LOSLib)

-include $(LIBRARY_OBJS:.o=.d)
