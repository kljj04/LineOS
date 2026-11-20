#!/usr/bin/env bash
set -euo pipefail
Root=$(cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$Root"
Work=Build/Validation/SecureExec
mkdir -p "$Work"
make -f Makefiles/Makefile All -j8
Flags=(-target x86_64-elf -D_GNU_SOURCE -fPIC -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -nostdinc -ICommon -ILibraries/LOSLib/Include -ILibraries/UserDB/Include -isystem Libraries/musl/obj/include -isystem Libraries/musl/arch/x86_64 -isystem Libraries/musl/arch/generic -isystem Libraries/musl/include)
for Name in Guest Probe Target Attack Hash; do
    clang "${Flags[@]}" -c "Scripts/Validation/SecureExec/$Name.c" -o "$Work/$Name.o"
done
clang "${Flags[@]}" -DNORMAL -c Scripts/Validation/SecureExec/Probe.c -o "$Work/Normal.o"
clang "${Flags[@]}" -DLIBRARY -c Scripts/Validation/SecureExec/Unsafe.c -o "$Work/UnsafeLibrary.o"
clang "${Flags[@]}" -c Scripts/Validation/SecureExec/Unsafe.c -o "$Work/Unsafe.o"
ld.lld -shared -soname attack.so -o "$Work/attack.so" "$Work/Attack.o" -LLineOS/Libraries -l:libc.so
ld.lld -shared -soname libunsafe.so -o "$Work/libunsafe.so" "$Work/UnsafeLibrary.o"
for Name in Guest Probe Normal Target; do
    ld.lld -pie -e _start --dynamic-linker /lib/libc.so -o "$Work/$Name.lix" Libraries/musl/lib/{Scrt1,crti}.o "$Work/$Name.o" -LLineOS/Libraries --as-needed -l:liblos.so -l:libc.so Libraries/musl/lib/crtn.o
done
ld.lld -pie -e _start --dynamic-linker /home/test/ld.so -o "$Work/BadLoader.lix" Libraries/musl/lib/{Scrt1,crti}.o "$Work/Probe.o" -LLineOS/Libraries -l:liblos.so -l:libc.so Libraries/musl/lib/crtn.o
ld.lld -pie -e _start --dynamic-linker /lib/libc.so -o "$Work/Unsafe.lix" Libraries/musl/lib/{Scrt1,crti}.o "$Work/Unsafe.o" -L"$Work" -l:libunsafe.so -LLineOS/Libraries -l:libc.so Libraries/musl/lib/crtn.o
ld.lld -pie -e _start --dynamic-linker "$Root/LineOS/Libraries/libc.so" -rpath "$Root/LineOS/Libraries" -o "$Work/Hash" Libraries/musl/lib/{Scrt1,crti}.o "$Work/Hash.o" -LLineOS/Libraries -l:libuserdb.so -l:liblocale.so -l:liblos.so -l:libc.so Libraries/musl/lib/crtn.o
timeout 30 "$Work/Hash" > "$Work/userdb"
printf 'tester\n' > "$Work/sudoers"
printf 'FixturePassword42\n' > "$Work/password"
printf 'incorrect\n' > "$Work/wrong-password"
printf 'Built secure exec and real sudo authentication fixtures\n'
