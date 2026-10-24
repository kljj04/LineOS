# Makefile
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

.DEFAULT_GOAL := All

MAKEFLAGS += --no-print-directory --silent

include Makefiles/BuildSettings.mk
include Makefiles/Utils.mk
include Makefiles/BootLoader.mk
include Makefiles/Kernel.mk
include Makefiles/UserPrograms.mk

.PHONY: All Clean Format CompileBoot LinkBoot CompileKernel LinkKernel CompileUserPrograms LinkUserPrograms

All: $(BOOT_TARGET) $(KERN_TARGET) $(USER_PROGRAM_OUTPUTS)