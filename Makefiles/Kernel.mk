# Makefiles/Kernel.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

CompileKernel: $(KERN_OBJS)

LinkKernel: $(KERN_TARGET)

$(KERN_TARGET): $(KERN_OBJS) Link/Link.ld
	@$(call MKDIR_P,$(dir $@))
	@$(call PRINT_YELLOW,[LD] $@)
	@$(LD) -static \
		-T Link/Link.ld \
		-e KMain \
		-o $@ \
		$(KERN_OBJS)

Build/Objects/Kernel/%.o: Kernel/%.c Makefiles/BuildSettings.mk Makefiles/Kernel.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call PRINT_CYAN,[CC] $<)
	@$(CC) $(KERN_OBJ_CFLAGS) $(DEPFLAGS) -c $< -o $@

Build/Objects/Kernel/%.o: Kernel/%.S Makefiles/BuildSettings.mk Makefiles/Kernel.mk
	@$(call MKDIR_P,$(dir $@))
	@$(call PRINT_PURPLE,[AS] $<)
	@$(CC) $(KERN_OBJ_CFLAGS) $(DEPFLAGS) -c $< -o $@

$(KERN_FLOAT_OBJS): KERN_OBJ_CFLAGS = $(KERN_FLOAT_CFLAGS)

-include $(KERN_DEPS)
