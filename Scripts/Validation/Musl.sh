#!/usr/bin/env bash
set -euo pipefail
Root=$(cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$Root"
Work=Build/Validation/Musl
mkdir -p "$Work"
make -f Makefiles/Makefile All -j8
clang -target x86_64-elf -nostdinc -isystem Libraries/musl/obj/include -isystem Libraries/musl/arch/x86_64 -isystem Libraries/musl/arch/generic -isystem Libraries/musl/include -fPIE -fno-stack-protector -c Scripts/Validation/Musl/Test.c -o "$Work/Test.o"
ld.lld -pie --dynamic-linker /lib/libc.so -o "$Work/Test.lix" Libraries/musl/lib/Scrt1.o Libraries/musl/lib/crti.o "$Work/Test.o" Build/Objects/Programs/Init/Runtime.o -LLineOS/Libraries -l:liblos.so -l:libc.so Libraries/musl/lib/crtn.o
readelf -l "$Work/Test.lix" | rg 'Requesting program interpreter: /lib/libc.so'
readelf -d "$Work/Test.lix" | rg 'Shared library: \[libc.so\]'
readelf -l LineOS/Programs/Shell/Commands/hello/hello.lix | rg 'Requesting program interpreter: /lib/libc.so'
readelf -d LineOS/Programs/Shell/Commands/hello/hello.lix | rg 'Shared library: \[libc.so\]'
printf 'Built musl runtime fixture: %s/Test.lix\n' "$Work"
