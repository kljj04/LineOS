
# Makefiles/Utils.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

RWildcard = $(foreach Dir,$(wildcard $1/*),$(call RWildcard,$(Dir),$2)) $(wildcard $1/$2)

MKDIR_P = mkdir -p "$(1)"
RM_RF = rm -rf -- "$(1)"

PRINT_YELLOW = printf '\033[33m%s\033[0m\n' '$(1)'
PRINT_CYAN = printf '\033[36m%s\033[0m\n' '$(1)'
PRINT_PURPLE = printf '\033[35m%s\033[0m\n' '$(1)'
PRINT_GREEN = printf '\033[32m%s\033[0m\n' '$(1)'
PRINT_RED = printf '\033[31m%s\033[0m\n' '$(1)'

BUILD_PROGRESS = printf 'LOS_PROGRESS\t%s\n' '$(1)'
BUILD_DONE = printf 'LOS_DONE\t%s\n' '$(1)'

.PHONY: ProgressFormatList ProgressCleanList

ProgressFormatList:
	@$(foreach File,$(FORMAT_SRCS),printf '%s\n' '$(File)';)

ProgressCleanList:
	@if [ -d Build ]; then find Build \( -type f -o -type l \) -print; fi
	@for File in "$(BOOT_TARGET)" "$(KERN_TARGET)"; do if [ -e "$$File" ] || [ -L "$$File" ]; then printf '%s\n' "$$File"; fi; done

Format:
	@set -e; for File in $(FORMAT_SRCS); do printf 'LOS_PROGRESS\t%s\n' "$$File"; $(CLANG_FORMAT) -i "$$File"; printf 'LOS_DONE\t%s\n' "$$File"; done

Clean:
	@if [ -d Build ]; then find Build \( -type f -o -type l \) -exec sh -c 'for File do printf "LOS_PROGRESS\t%s\n" "$$File"; rm -f -- "$$File" || exit; printf "LOS_DONE\t%s\n" "$$File"; done' sh {} +; fi
	@rm -rf -- Build
	@set -e; for File in "$(BOOT_TARGET)" "$(KERN_TARGET)"; do if [ -e "$$File" ] || [ -L "$$File" ]; then printf 'LOS_PROGRESS\t%s\n' "$$File"; rm -f -- "$$File"; printf 'LOS_DONE\t%s\n' "$$File"; fi; done
