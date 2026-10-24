# Makefiles/Kernel.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

CompileKernel: $(KERN_OBJS)

LinkKernel: $(KERN_TARGET)

$(KERN_TARGET): $(KERN_OBJS) Link/Link.ld
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$@)
	@$(LD) -static \
		-T Link/Link.ld \
		-e KMain \
		-o $@ \
		$(KERN_OBJS)
	@$(call BUILD_DONE,$@)

Build/Objects/Kernel/%.o: Kernel/%.c Makefiles/BuildSettings.mk Makefiles/Kernel.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(KERN_OBJ_CFLAGS) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

Build/Objects/Kernel/%.o: Kernel/%.S Makefiles/BuildSettings.mk Makefiles/Kernel.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call BUILD_PROGRESS,$<)
	@$(CC) $(KERN_OBJ_CFLAGS) $(DEPFLAGS) -c $< -o $@
	@$(call BUILD_DONE,$<)

$(KERN_FLOAT_OBJS): KERN_OBJ_CFLAGS = $(KERN_FLOAT_CFLAGS)

-include $(KERN_DEPS)
