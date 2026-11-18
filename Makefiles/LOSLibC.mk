# Makefiles/LOSLibC.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

CompileLOSLibC: $(LOSLIBC_OBJS)

LinkLOSLibC: $(LOSLIBC_TARGET)

$(LOSLIBC_TARGET): $(LOSLIBC_OBJS) Makefiles/LOSLibC.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$@)
	@rm -f $@
	@$(AR) rcs $@ $(LOSLIBC_OBJS)
	@$(call BUILD_DONE,$@)

Build/Objects/LOSLibC/%.o: LOSLibC/%.c Makefiles/BuildSettings.mk Makefiles/LOSLibC.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(LOSLIBC_CFLAGS) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

Build/Objects/LOSLibC/%.o: LOSLibC/%.S Makefiles/BuildSettings.mk Makefiles/LOSLibC.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(LOSLIBC_CFLAGS) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

-include $(LOSLIBC_DEPS)

.PHONY: VerifyPthread

VerifyPthread: LinkKernel LinkLOSLibC
	@set -e; for Entry in SyscallClone SyscallClone3 SyscallFutex SyscallExitGroup; do \
		llvm-nm --defined-only "$(KERN_TARGET)" | awk -v Name="$$Entry" '$$NF == Name { Found = 1 } END { exit !Found }' || { echo "Missing kernel symbol: $$Entry"; exit 1; }; \
	done
	@set -e; for Entry in pthread_create pthread_join pthread_detach pthread_exit pthread_mutex_lock pthread_cond_wait pthread_rwlock_rdlock pthread_barrier_wait pthread_once __errno_location; do \
		llvm-nm --defined-only "$(LOSLIBC_TARGET)" | awk -v Name="$$Entry" '$$NF == Name { Found = 1 } END { exit !Found }' || { echo "Missing LOSLibC symbol: $$Entry"; exit 1; }; \
	done
	@echo "pthread kernel and LOSLibC build and symbol checks passed"
