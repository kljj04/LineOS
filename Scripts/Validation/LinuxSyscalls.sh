#!/usr/bin/env bash
set -euo pipefail
Root=$(cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$Root"
Work=$(mktemp -d)
trap 'rm -rf -- "$Work"' EXIT
python3 - "$Work/VFS.c" <<'PY'
import pathlib
import sys
source = pathlib.Path('Kernel/Source/FS/VFS/VFS.c').read_text()
source = source.replace('ASM("pushfq; popq %0; cli" : "=r"(Flags) : : "memory");', 'Flags = 0;')
source = source.replace('ASM("sti" :: : "memory");', '')
pathlib.Path(sys.argv[1]).write_text(source)
PY
clang -g -O1 -DLINEOS_KERNEL_BUILD -ICommon -IKernel/Include -ffunction-sections -fdata-sections -fsanitize=address,undefined -Wno-pointer-sign -Wl,--gc-sections Scripts/Validation/LinuxSyscalls/Test.c "$Work/VFS.c" Kernel/Source/Syscall/SyscallError.c Kernel/Source/Syscall/File/SyscallFile.c Kernel/Source/Syscall/File/SyscallStat.c Kernel/Source/Syscall/Memory/SyscallMemory.c Kernel/Source/Syscall/Memory/SyscallDeviceMap.c Kernel/Source/Syscall/Process/SyscallThread.c Kernel/Source/Syscall/Device/SyscallIoctl.c -o "$Work/Test"
"$Work/Test"
clang -g -O1 -DLINEOS_KERNEL_BUILD -ICommon -IKernel/Include -ffunction-sections -fdata-sections -fsanitize=address,undefined -Wl,--gc-sections Scripts/Validation/LinuxSyscalls/Generation.c Kernel/Source/FS/EXT2/EXT2Inode.c -o "$Work/Generation"
"$Work/Generation"
