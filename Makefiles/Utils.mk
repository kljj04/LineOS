# Makefiles/Utils.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

RWildcard = $(foreach Dir,$(wildcard $1/*),$(call RWildcard,$(Dir),$2)) $(wildcard $1/$2)

MKDIR_P = mkdir -p "$(1)"
RM_RF = rm -rf -- "$(1)"

PRINT_YELLOW = node Scripts/JS/Print.js '$(1)' 
PRINT_CYAN = node Scripts/JS/Print.js '$(1)' 
PRINT_PURPLE = node Scripts/JS/Print.js '$(1)' 
PRINT_GREEN = node Scripts/JS/Print.js '$(1)' 
PRINT_RED = node Scripts/JS/Print.js '$(1)' 

Format:
	@node Scripts/JS/Files.js Format "$(CLANG_FORMAT)" $(FORMAT_SRCS)

Clean:
	@node Scripts/JS/Files.js Clean Build "$(BOOT_TARGET)" "$(KERN_TARGET)"
