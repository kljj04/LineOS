#!/usr/bin/env bash
set -euo pipefail
Root=$(cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$Root"
Work=Build/Validation/MuslCompat
mkdir -p "$Work"
make -f Makefiles/Makefile All -j8
Flags=(-target x86_64-elf -fPIC -ffreestanding -fno-builtin -nostdlib -nostdinc -D_GNU_SOURCE -ICommon -ILibraries/LOSLib/Include -ILibraries/TrueType/Include -ILibraries/Window/Include -IUserPrograms/System/lwm/Include -isystem Libraries/musl/obj/include -isystem Libraries/musl/arch/x86_64 -isystem Libraries/musl/arch/generic -isystem Libraries/musl/include)
clang "${Flags[@]}" -c Scripts/Validation/MuslCompat/Guest.c -o "$Work/Guest.o"
clang "${Flags[@]}" -c Scripts/Validation/MuslCompat/Child.c -o "$Work/Child.o"
Objects=()
for Object in Build/Objects/UserPrograms/System/lwm/Source/*.o; do
    [[ "$Object" == */Lwm.o ]] || Objects+=("$Object")
done
ld.lld -pie -e _start --dynamic-linker /lib/libc.so -o "$Work/Guest.lix" Libraries/musl/lib/{Scrt1,crti}.o "$Work/Guest.o" Build/Objects/Programs/Init/Runtime.o "${Objects[@]}" -LLineOS/Libraries --start-group -l:libtruetype.so -l:libwindow.so -l:liblocale.so -l:liblos.so -l:libc.so --end-group Libraries/musl/lib/crtn.o
ld.lld -pie -e _start --dynamic-linker /lib/libc.so -o "$Work/Child.lix" Libraries/musl/lib/{Scrt1,crti}.o "$Work/Child.o" -LLineOS/Libraries -l:libc.so Libraries/musl/lib/crtn.o
printf 'Built musl signal-mask/fcntl/LwmOpen guest fixtures\n'
