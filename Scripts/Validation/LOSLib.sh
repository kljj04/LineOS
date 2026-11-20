#!/usr/bin/env bash
set -euo pipefail
Root=$(cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$Root"
Work=$(mktemp -d)
trap 'rm -rf -- "$Work"' EXIT
python3 - <<'PY'
import pathlib
import subprocess
library = pathlib.Path('LineOS/Libraries/liblos.so')
symbols = {line.split()[-1] for line in subprocess.check_output(['nm', '-D', '--defined-only', str(library)], text=True).splitlines()}
expected = {'DisplayGetInfo', 'DisplayPresent', 'DisplayPresentRect', 'LOSAssetPublish', 'LOSAssetAcquire', 'LOSAssetRelease', 'LOSIPCCreate', 'LOSIPCLookup', 'LOSIPCBind', 'LOSIPCSend', 'LOSIPCReceive', 'LOSIPCClose', 'LOSIPCAlive', 'LOSSystemMonitor', 'LOSRemove', 'LOSParentChdir', 'LOSSudoCache'}
assert symbols == expected, (symbols - expected, expected - symbols)
for path in [*pathlib.Path('LineOS/Libraries').glob('lib*.so'), *pathlib.Path('LineOS/Programs').rglob('*.lix')]:
    info = subprocess.check_output(['readelf', '-d', str(path)], text=True)
    assert 'liblosc.so' not in info and 'libc.so.6' not in info, path
    assert '[LineOS/' not in info, path
    if path.suffix == '.lix':
        headers = subprocess.check_output(['readelf', '-l', str(path)], text=True)
        assert '[Requesting program interpreter: /lib/libc.so]' in headers, path
print('LOSLib export and musl dependency audit PASS')
PY
cat > "$Work/Test.c" <<'C'
#include <Asset.h>
#include <Display.h>
#include <LOS.h>
#include <errno.h>
int main(void)
{
    LOS_ASSET Asset = {0};
    errno = 0;
    if (LOSAssetRelease(&Asset) != -1 || errno != EINVAL) return 1;
    errno = 0;
    if (DisplayGetInfo(0) != -1 || errno != EINVAL) return 2;
    errno = 0;
    if (LOSRemove("/loslib-validation-missing") != -1 || errno != ENOSYS) return 3;
    return 0;
}
C
clang -target x86_64-elf -fPIC -ffreestanding -fno-builtin -nostdlib -nostdinc -ICommon -ILibraries/LOSLib/Include -isystem Libraries/musl/obj/include -isystem Libraries/musl/arch/x86_64 -isystem Libraries/musl/arch/generic -isystem Libraries/musl/include -c "$Work/Test.c" -o "$Work/Test.o"
ld.lld -pie -e _start --dynamic-linker "$Root/LineOS/Libraries/libc.so" -rpath "$Root/LineOS/Libraries" -o "$Work/Test" Libraries/musl/lib/{Scrt1,crti}.o "$Work/Test.o" -LLineOS/Libraries -l:liblos.so -l:libc.so Libraries/musl/lib/crtn.o
timeout 10 "$Work/Test"
printf 'LOSLib with musl errno PASS\n'
set +e
timeout 15 LineOS/Libraries/libc.so --library-path "$Root/LineOS/Libraries" LineOS/Programs/Shell/Commands/hello/hello.lix > "$Work/HelloGate.log" 2>&1
Status=$?
set -e
HelloAssetCount=$(python3 - <<'PYCONFIG'
import pathlib
import sys
sys.path.insert(0, 'Scripts/Backend')
from ProgramConfig import ProgramConfigs
print(len(next(Entry for Entry in ProgramConfigs(pathlib.Path.cwd()) if Entry['Program'] == 'Shell/Commands/hello')['Assets']))
PYCONFIG
)
if (( HelloAssetCount > 0 )); then
    [[ "$Status" == 127 ]] || { cat "$Work/HelloGate.log"; exit 1; }
    grep -q 'asset dependency unavailable:' "$Work/HelloGate.log"
    printf 'hello guest-asset gate PASS\n'
else
    [[ "$Status" == 0 ]] || { cat "$Work/HelloGate.log"; exit 1; }
    printf 'hello without declared assets PASS\n'
fi
ld.lld -pie -e _start --dynamic-linker "$Root/LineOS/Libraries/libc.so" -rpath "$Root/LineOS/Libraries" -o "$Work/HelloMain" Libraries/musl/lib/{Scrt1,crti}.o Build/Objects/UserPrograms/Shell/Commands/hello/hello.o -LLineOS/Libraries -l:libc.so Libraries/musl/lib/crtn.o
timeout 15 "$Work/HelloMain"
timeout 15 LineOS/Libraries/libc.so --library-path "$Root/LineOS/Libraries" LineOS/Programs/Shell/Commands/pthreadtest/pthreadtest.lix
