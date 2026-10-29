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
LIBRARY_TARGETS = $(foreach Library,$(LIBRARIES),$(USER_LIBRARY_$(Library)))
LIBRARY_OBJS = $(foreach Library,$(LIBRARIES),$(LIBRARY_OBJS_$(Library)))

define LIBRARY_TEMPLATE
USER_LIBRARY_$(1) := Build/Libraries/lib$(shell printf '%s' '$(1)' | tr '[:upper:]' '[:lower:]').a
LIBRARY_SRCS_$(1) := $(filter-out -,$(subst $(Comma), ,$(4)))
LIBRARY_OBJS_$(1) := $$(addprefix Build/Objects/,$$(addsuffix .o,$$(basename $$(LIBRARY_SRCS_$(1)))))
$$(LIBRARY_OBJS_$(1)): private LIBRARY_INCLUDE := -ILibraries/$(2)/Include
$$(LIBRARY_OBJS_$(1)): private LIBRARY_CFLAGS := $(subst $(Comma), ,$(filter-out -,$(3)))
$$(LIBRARY_OBJS_$(1)): Libraries/$(2)/Library.cfg
$$(USER_LIBRARY_$(1)): $$(LIBRARY_OBJS_$(1)) Libraries/$(2)/Library.cfg Makefiles/Libraries.mk Scripts/Backend/Libraries.sh $(shell find Libraries/$(2) -type d)
	@$$(call MKDIR_P,$$(dir $$@))
	@$$(call BUILD_PROGRESS,$$@)
	@rm -f $$@
	@$$(AR) rcs $$@ $$(LIBRARY_OBJS_$(1))
	@$$(call BUILD_DONE,$$@)
endef

$(foreach Row,$(LIBRARY_MANIFEST),$(eval $(call LIBRARY_TEMPLATE,$(word 1,$(call LibraryFields,$(Row))),$(word 2,$(call LibraryFields,$(Row))),$(word 3,$(call LibraryFields,$(Row))),$(word 4,$(call LibraryFields,$(Row))))))

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
