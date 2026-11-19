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
LIBRARY_OBJS = $(foreach Library,$(LIBRARIES),$(LIBRARY_OBJS_$(Library)))

define LIBRARY_TEMPLATE
USER_LIBRARY_$(1) := LineOS/Libraries/lib$(shell printf '%s' '$(1)' | tr '[:upper:]' '[:lower:]').so
LIBRARY_TARGET_$(1) = $$(USER_LIBRARY_$(1))
USER_LIBRARY_INCLUDE_$(1) := -ILibraries/$(2)/Include
LIBRARY_SRCS_$(1) := $(filter-out -,$(subst $(Comma), ,$(4)))
LIBRARY_OBJS_$(1) := $$(addprefix Build/Objects/,$$(addsuffix .o,$$(basename $$(LIBRARY_SRCS_$(1)))))
$$(LIBRARY_OBJS_$(1)): private LIBRARY_INCLUDE := $$(USER_LIBRARY_INCLUDE_$(1))
$$(LIBRARY_OBJS_$(1)): private LIBRARY_CFLAGS := $(subst $(Comma), ,$(filter-out -,$(3)))
$$(LIBRARY_OBJS_$(1)): Libraries/$(2)/Library.cfg
$$(USER_LIBRARY_$(1)): $$(LIBRARY_OBJS_$(1)) $(LOSLIBC_TARGET) Libraries/$(2)/Library.cfg Makefiles/Libraries.mk Scripts/Backend/Libraries.sh $(shell find Libraries/$(2)/Source -type d)
	@$$(call MKDIR_P,$$(dir $$@))
	@$$(call BUILD_PROGRESS,$$@)
	@rm -f $$@
	@$$(LD) -shared -z max-page-size=4096 -soname $$(notdir $$@) -z defs -z noexecstack -o $$@ $$(LIBRARY_OBJS_$(1)) $(LOSLIBC_TARGET)
	@$$(call BUILD_DONE,$$@)
endef

define LIBRARY_MAKEFILE_TEMPLATE
LIBRARY_TARGET_$(1) := LibraryMakefile_$(1)
$(if $(filter-out -,$(6)),USER_LIBRARY_$(1) := LineOS/Libraries/$(notdir $(6)))
$(if $(filter-out -,$(6)),USER_LIBRARY_LINK_$(1) := -LLineOS/Libraries -l:$(notdir $(6)))
$(if $(filter-out -,$(6)),LineOS/Libraries/$(notdir $(6)): LibraryMakefile_$(1))
.PHONY: LibraryMakefile_$(1)
LibraryMakefile_$(1): Libraries/$(2)/Library.cfg Libraries/$(2)/$(5)
	@$$(call BUILD_PROGRESS,Libraries/$(2)/$(5))
	+$$(MAKE) -C Libraries/$(2) -f $(5)
	$(if $(filter-out -,$(6)),@bash Scripts/Backend/Libraries.sh --copy-so . $(2) $(6))
	@$$(call BUILD_DONE,Libraries/$(2)/$(5))
endef

$(foreach Row,$(LIBRARY_MANIFEST),$(eval $(call $(if $(filter -,$(word 5,$(call LibraryFields,$(Row)))),LIBRARY_TEMPLATE,LIBRARY_MAKEFILE_TEMPLATE),$(word 1,$(call LibraryFields,$(Row))),$(word 2,$(call LibraryFields,$(Row))),$(word 3,$(call LibraryFields,$(Row))),$(word 4,$(call LibraryFields,$(Row))),$(word 5,$(call LibraryFields,$(Row))),$(word 6,$(call LibraryFields,$(Row))))))

Build/Objects/Libraries/%.o: Libraries/%.c Makefiles/BuildSettings.mk Makefiles/Libraries.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(USER_PROGRAM_CFLAGS) $(LIBRARY_INCLUDE) $(LIBRARY_CFLAGS) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

Build/Objects/Libraries/%.o: Libraries/%.S Makefiles/BuildSettings.mk Makefiles/Libraries.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(USER_PROGRAM_CFLAGS) $(LIBRARY_INCLUDE) $(LIBRARY_CFLAGS) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

-include $(LIBRARY_OBJS:.o=.d)
