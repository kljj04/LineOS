# Makefiles/Mesa.mk
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

MESA_MESON ?= meson
MESA_NINJA ?= ninja
MESA_CXX ?= clang++
MESA_BUILD := Build/Mesa
MESA_PYTHON_PATH ?=
ifneq ($(strip $(MESA_PYTHON_PATH)),)
MESA_ENV := env PYTHONPATH="$(MESA_PYTHON_PATH)$(if $(PYTHONPATH),:$(PYTHONPATH))"
endif
MESA_CROSS := Build/MesaConfig/lineos.cross
MESA_CONFIG := $(MESA_BUILD)/build.ninja
MESA_SOURCE := Libraries/Mesa/Source
MESA_FLAGS := '-ffreestanding', '-nostdlibinc', '-D__lineos__', '-D_XOPEN_SOURCE=700', '-Dalloca=__builtin_alloca', '-fno-pie', '-mcmodel=large', '-mno-red-zone', '-I$(CURDIR)/LOSLibC/Include', '-I$(CURDIR)/Common', '-I$(CURDIR)/Libraries/LibM/Include'
MESA_OPTIONS := --wrap-mode=nofallback -Dgallium-drivers=virgl -Dvulkan-drivers= -Dplatforms= -Dglx=disabled -Degl=disabled -Dgbm=disabled -Dllvm=disabled -Dvideo-codecs= -Dexpat=disabled -Dzlib=disabled -Dzstd=disabled -Dshader-cache=disabled

MESA_USER_LIBS := \
    Build/Mesa/src/mapi/glapi/libglapi_bridge.a \
    Build/Mesa/src/mesa/libmesa.a \
    Build/Mesa/src/compiler/glsl/libglsl.a \
    Build/Mesa/src/compiler/nir/libnir.a \
    Build/Mesa/src/compiler/libcompiler.a \
    Build/Mesa/src/gallium/drivers/virgl/libvirgl.a \
    Build/Mesa/src/gallium/winsys/virgl/lineos/libvirgllineos.a \
    Build/Mesa/src/mapi/shared-glapi/libglapi.a \
    Build/Mesa/src/gallium/auxiliary/libgallium.a \
    Build/Mesa/src/util/libmesa_util.a \
    Build/Mesa/src/c11/impl/libmesa_util_c11.a

$(MESA_USER_LIBS): CompileMesa

.PHONY: ConfigureMesa CompileMesa

ConfigureMesa: $(MESA_CONFIG)

CompileMesa: $(MESA_CONFIG) $(USER_LIBRARY_LibM) $(LOSLIBC_TARGET)
	@$(MESA_ENV) $(MESA_NINJA) -C "$(MESA_BUILD)" src/gallium/winsys/virgl/lineos/libvirgllineos.a src/gallium/drivers/virgl/libvirgl.a src/mapi/shared-glapi/libglapi.a src/compiler/glsl/libglsl.a src/mesa/libmesa.a src/mapi/glapi/libglapi_bridge.a src/c11/impl/libmesa_util_c11.a src/util/libmesa_util.a src/compiler/libcompiler.a src/compiler/nir/libnir.a src/gallium/auxiliary/libgallium.a

$(MESA_CROSS): Makefiles/Mesa.mk Makefiles/BuildSettings.mk
	@mkdir -p "$(dir $@)"
	@printf '%s\n' '[binaries]' "c = ['$(CC)', '--target=$(KERNEL_TYPE_TARGET)']" "cpp = ['$(MESA_CXX)', '--target=$(KERNEL_TYPE_TARGET)']" "ar = '$(AR)'" "strip = 'llvm-strip'" '' '[host_machine]' "system = 'lineos'" "cpu_family = 'x86_64'" "cpu = 'x86_64'" "endian = 'little'" '' '[properties]' 'needs_exe_wrapper = true' '' '[built-in options]' "c_args = [$(MESA_FLAGS)]" "cpp_args = [$(MESA_FLAGS), '-fno-exceptions']" "c_link_args = ['-nostdlib', '-fuse-ld=lld']" "cpp_link_args = ['-nostdlib', '-fuse-ld=lld']" > "$@"

$(MESA_CONFIG): $(MESA_CROSS) $(MESA_SOURCE)/meson.build $(MESA_SOURCE)/meson.options
	@$(MESA_ENV) $(MESA_MESON) setup $(if $(wildcard $(MESA_CONFIG)),--wipe) --cross-file "$(abspath $(MESA_CROSS))" "$(MESA_BUILD)" "$(MESA_SOURCE)" $(MESA_OPTIONS)
