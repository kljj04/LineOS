#!/usr/bin/env bash
# Scripts/Validation/NetworkGuest.sh
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

set -euo pipefail
Root=$(cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$Root"
Work=Build/Validation/Network
mkdir -p "$Work"
make -f Makefiles/Makefile -j8 LinkKernel LineOS/Libraries/libc.so
clang -target x86_64-elf -D_GNU_SOURCE -fPIC -fno-stack-protector -nostdlib -nostdinc -ICommon -isystem Libraries/musl/obj/include -isystem Libraries/musl/arch/x86_64 -isystem Libraries/musl/arch/generic -isystem Libraries/musl/include -c Scripts/Validation/Network/Guest.c -o "$Work/Guest.o"
ld.lld -pie -e _start --dynamic-linker /lib/libc.so -o "$Work/Guest.lix" Libraries/musl/lib/{Scrt1,crti}.o "$Work/Guest.o" -LLineOS/Libraries -l:libc.so Libraries/musl/lib/crtn.o
printf 'Built disposable socket guest fixture: %s/Guest.lix\n' "$Work"
