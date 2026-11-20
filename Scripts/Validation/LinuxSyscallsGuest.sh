#!/usr/bin/env bash
set -euo pipefail
Root=$(cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$Root"
Work=Build/Validation/LinuxSyscalls
mkdir -p "$Work"
clang -DLINEOS_KERNEL_BUILD -ICommon -target x86_64-elf -ffreestanding -fPIE -fno-stack-protector -fno-builtin -mno-red-zone -O1 -c Scripts/Validation/LinuxSyscalls/Guest.c -o "$Work/Guest.o"
ld.lld -static --image-base=0x400000000000 -e _Start -o "$Work/Guest.lix" "$Work/Guest.o"
clang -DLINEOS_KERNEL_BUILD -DTEST_LIBC -ICommon -target x86_64-elf -ffreestanding -fPIE -fno-stack-protector -fno-builtin -mno-red-zone -O1 -c Scripts/Validation/LinuxSyscalls/Guest.c -o "$Work/GuestMusl.o"
ld.lld -pie -e _Start --dynamic-linker /lib/libc.so -o "$Work/GuestMusl.lix" "$Work/GuestMusl.o" -LLineOS/Libraries -l:libc.so
python3 - "$Work/Payload" <<'PY'
import pathlib
import sys
pathlib.Path(sys.argv[1]).write_bytes(bytes(index % 256 for index in range(6000)))
PY
printf 'Built static and project musl guest fixtures in %s\n' "$Work"
