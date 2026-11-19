# LineOS dynamic interpreter

The build emits LineOS/Libraries/ld-lineos-x86_64.so.1. The image updater installs it in /lib alongside the shared libraries. The interpreter uses raw Linux x86_64 syscalls and has no libc dependency or bootstrap relocations.

The kernel maps the main executable and interpreter and supplies the Linux initial stack. Dependency discovery, symbol resolution, relocation, TLS, initialization and finalization run in user space.

Dependencies are discovered breadth first. Symbol resolution searches the executable followed by its dependencies in discovery order. Local and protected definitions bind locally. Undefined weak references resolve to zero. Symbol versions are matched through version definitions and requirements. SysV and GNU hash metadata determine validated symbol table bounds. Lookup currently scans the bounded symbol table.

The interpreter searches the requesting object's RUNPATH, falling back to its RPATH, then /lib and /usr/lib. Absolute $ORIGIN expansion is supported. Relative directories and inherited ancestor RPATH semantics remain unsupported. Device/inode identity, pathname and SONAME prevent duplicate loading. Dependency cycles do not recursively reinitialize objects.

PLT binding is eager. Supported relocation types are NONE, RELATIVE, 64, GLOB_DAT, JUMP_SLOT, COPY, DTPMOD64, DTPOFF64, TPOFF64 and IRELATIVE. GNU IFUNC symbol resolvers run in a second relocation pass after ordinary relocations and TLS initialization. Text relocations and unsupported dynamic flags fail explicitly. Segment permissions are applied before resolvers execute. GNU RELRO is applied after relocation.

TLS uses x86_64 variant II with the self pointer at FS:0, DTV at FS:8 and module storage at negative offsets. Each thread receives its own initial image and zero-filled tail. LOSLibC errno uses compiler TLS. Loader hooks allocate and free thread TLS. Loading additional TLS modules after startup with dlopen is unimplemented. The TCB is not a full musl internal pthread structure.

Limits are 64 images, 64 program headers per image, 64 MiB per image and segment/TLS alignment up to 4096. Overlapping PT_LOAD pages and secure execution are rejected. Existing kernel restrictions on changing an address space active on other CPUs still apply.

Run bash Scripts/Validation/DynamicLoader.sh from the repository root. It rebuilds the kernel and userspace and runs the actual PT_INTERP interpreter on Linux. Tests cover dependency-free PIE, transitive dependencies, symbol versions, weak symbols, GNU IFUNC, TLS initialization, constructors/finalizers, dynamically linked LOSLibC pthread creation/join, errno isolation, pthreadtest, and truncated ELF rejection.

A two-vCPU LineOS KVM run using a copied disk passed static ET_EXEC startup followed by execve of a dynamic PIE, interpreter entry, liblosc.so loading, pthread creation/join and errno isolation. The result was DYNAMIC LOADER TEST PASS error=0 errno=33. A temporary kernel stdout hook captured the init fixture output. The hook was removed from the source tree.

Musl porting and musl execution tests are outside this change. Cyclic dependency fixtures, COPY/IRELATIVE edge cases, malformed hash/version chains and finalizer ordering need more runtime coverage.
