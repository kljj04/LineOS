#!/usr/bin/env bash
set -euo pipefail
Root=$(cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$Root"
make -f Makefiles/Makefile All -j8
Work=$(mktemp -d)
trap 'rm -rf -- "$Work"' EXIT
Loader="$Root/LineOS/Libraries/ld-lineos-x86_64.so.1"
clang -fPIC -nostdlib -c Scripts/Validation/DynamicLoader/Math.c -o "$Work/Math.o"
ld.lld -shared -soname "$Work/libmathfixture.so" -o "$Work/libmathfixture.so" "$Work/Math.o"
clang -fPIC -nostdlib -c Scripts/Validation/DynamicLoader/TLS.c -o "$Work/TLS.o"
printf 'TEST_1 { global: Value; local: *; };\n' > "$Work/Version.map"
ld.lld -shared --version-script="$Work/Version.map" -soname "$Work/libtest.so" -o "$Work/libtest.so" "$Work/TLS.o" "$Work/libmathfixture.so"
clang -c Scripts/Validation/DynamicLoader/Entry.S -o "$Work/Entry.o"
ld.lld -pie --allow-shlib-undefined -e _Start --dynamic-linker "$Loader" -o "$Work/TLS" "$Work/Entry.o" "$Work/libtest.so"
timeout 10 "$Work/TLS"
Musl="$Root/LineOS/Libraries/libc.so"
clang -target x86_64-elf -fPIC -ffreestanding -fno-builtin -nostdlib -nostdinc -isystem Libraries/musl/obj/include -isystem Libraries/musl/arch/x86_64 -isystem Libraries/musl/arch/generic -isystem Libraries/musl/include -c Scripts/Validation/DynamicLoader/Pthread.c -o "$Work/Pthread.o"
ld.lld -pie -e _start --dynamic-linker "$Musl" -rpath "$Root/LineOS/Libraries" -o "$Work/Pthread" Libraries/musl/lib/{Scrt1,crti}.o "$Work/Pthread.o" -LLineOS/Libraries -l:libc.so Libraries/musl/lib/crtn.o
timeout 10 "$Work/Pthread"
ld.lld -pie -e _start --dynamic-linker "$Musl" -rpath "$Root/LineOS/Libraries" -o "$Work/PthreadTest" Libraries/musl/lib/{Scrt1,crti}.o Build/Objects/UserPrograms/Shell/Commands/pthreadtest/pthreadtest.o -LLineOS/Libraries -l:libc.so Libraries/musl/lib/crtn.o
timeout 10 "$Work/PthreadTest"
printf '.text\n.global _Start\n_Start: movl $231, %%eax; xorl %%edi, %%edi; syscall\n.section .note.GNU-stack,"",@progbits\n' > "$Work/Empty.S"
clang -c "$Work/Empty.S" -o "$Work/Empty.o"
ld.lld -pie -e _Start --dynamic-linker "$Loader" -o "$Work/Empty" "$Work/Empty.o"
timeout 10 "$Work/Empty"
printf 'dependency-free PIE PASS\n'
printf 'bad ELF' > "$Work/libtest.so"
set +e
timeout 10 "$Work/TLS" > "$Work/Bad.log" 2>&1
Status=$?
set -e
[[ "$Status" == 127 ]] || { cat "$Work/Bad.log"; exit 1; }
grep -q 'invalid ELF file size' "$Work/Bad.log"
printf 'malformed ELF rejection PASS\n'
printf 'dynamic loader host tests passed\n'
