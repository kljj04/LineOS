#!/usr/bin/env python3
import importlib.util
import os
import pathlib
import subprocess
import tempfile

Root = pathlib.Path(__file__).resolve().parents[2]
Spec = importlib.util.spec_from_file_location('ProgramConfig', Root / 'Scripts/Backend/ProgramConfig.py')
Config = importlib.util.module_from_spec(Spec)
Spec.loader.exec_module(Config)
Count = 0


def Check(Condition, Message):
    global Count
    if not Condition:
        raise AssertionError(Message)
    Count += 1


with tempfile.TemporaryDirectory(prefix='lineos-programs-') as Directory:
    Work = pathlib.Path(Directory)
    Path = Work / 'Program.cfg'

    def Parse(Text, Valid=True):
        Path.write_text(Text)
        try:
            Result = Config.ReadConfig(Path, {'LOSLib', 'TrueType'})
        except Config.ConfigError:
            Check(not Valid, 'unexpected rejection: ' + Text)
            return None
        Check(Valid, 'unexpected acceptance: ' + Text)
        return Result

    Result = Parse('')
    Check(Result['LibC'] == 'musl' and Result['CopyPath'] == '/bin' and Result['Enable'], 'defaults')
    Result = Parse('[Program]\r\nName = renamed\r\nCopyPath = /bin/\r\nMode = 4755\r\n[Dependencies]\r\nLibraries =\r\n    TrueType\r\nAssets =\r\n    Fonts/Test\r\n[Runtime]\r\nDynamicLinker = auto\r\n')
    Check(Result['Name'] == 'renamed' and Result['Mode'] == '4755' and Result['CopyPath'] == '/bin', 'name/mode/path')
    Check(Result['Libraries'] == ['TrueType', 'LOSLib'] and Result['Assets'] == ['Fonts/Test'], 'logical asset names and automatic wrapper dependency')
    Result = Parse('[Runtime]\nDynamicLinker = /lib/custom.so\n')
    Check(Result['DynamicLinker'] == '/lib/custom.so', 'explicit interpreter')
    for Text in ['[Program]\nLibC = glibc', '[Program]\nName = a.lix', '[Program]\nCopyPath = /a/../b', '[Program]\nCopyPath = /a//b', '[Program]\nEnable = yes', '[Program]\nMode = 8888', '[Program]\nName = a\nName = b', '[Program]\n[Program]', '[Unknown]', '[Dependencies]\nPrograms =', '[Dependencies]\nLibraries =\n    libc.so', '[Dependencies]\nLibraries =\n    Missing', '[Dependencies]\nAssets =\n    /disk/path', '[Dependencies]\nAssets =\n    Fonts/../Bad', '[Dependencies]\nAssets =\n    Fonts/Test\n    Fonts/Test', '[Dependencies]\nAssets = Fonts/Test', '[Dependencies]\nAssets =\nFonts/Test', '[Publish]\nAssets =\n    Fonts/Test = relative', '[Publish]\nAssets =\n    Fonts/Test = /a\n    Fonts/Test = /b', '[Runtime]\nDynamicLinker = $(touch /tmp/invalid)', '[Program]\nName = x # inline', 'Name = unsectioned']:
        Parse(Text, False)
    Parse('[Program]\nName = allowed\n# comment\n\n[Publish]\nAssets =\n    Fonts/Test = /sys/font.ttf\n')
    for Name in ['Libraries', 'Scripts/Backend', 'UserPrograms/Fixture']:
        (Work / Name).mkdir(parents=True)
    for Name in ['Libraries.sh', 'LibraryConfig.py', 'ProgramRuntime.c.in']:
        (Work / 'Scripts/Backend' / Name).write_bytes((Root / 'Scripts/Backend' / Name).read_bytes())
    ProgramPath = Work / 'UserPrograms/Fixture/Program.cfg'
    Payload = Work / 'Payload'
    Payload.write_bytes(b'payload!')
    ProgramPath.write_text(f'[Program]\nName = renamed\n[Publish]\nAssets =\n    Validation/Bytes = {Payload}\n[Dependencies]\nAssets =\n    Validation/Bytes\n')
    Programs = Config.ProgramConfigs(Work)
    Check(Programs[0]['Name'] == 'renamed', 'manifest name')
    (Work / 'UserPrograms/Other').mkdir()
    Other = Work / 'UserPrograms/Other/Program.cfg'
    Other.write_text('[Program]\nName = renamed\n')
    try:
        Config.ProgramConfigs(Work)
        Check(False, 'installation collision')
    except Config.ConfigError:
        Check(True, 'installation collision')
    Other.write_text(f'[Publish]\nAssets =\n    Validation/Bytes = {Payload}\n')
    try:
        Config.ProgramConfigs(Work)
        Check(False, 'duplicate publisher')
    except Config.ConfigError:
        Check(True, 'duplicate publisher')
    Other.write_text('[Program]\nEnable = false\n')
    Generated = Work / 'Runtime.c'
    Config.Generate(Work, 'Fixture', Generated)
    Test = Work / 'Test'
    subprocess.run(['clang', '-g', '-O1', '-fsanitize=address,undefined', '-I' + str(Root / 'Common'), '-I' + str(Root / 'Libraries/LOSLib/Include'), str(Generated), str(Root / 'Scripts/Validation/ProgramConfig/Runtime.c'), '-o', str(Test)], check=True)
    Result = subprocess.run([str(Test)], capture_output=True, text=True)
    Check(Result.returncode == 0 and 'main reached' in Result.stdout, 'publication before main')
    for Variable in ['DENY_PUBLISH', 'MISSING_ASSET']:
        Result = subprocess.run([str(Test)], env={**os.environ, Variable: '1'}, capture_output=True, text=True)
        Check(Result.returncode == 127 and 'main reached' not in Result.stdout, 'fail before main: ' + Variable)
    Payload.write_bytes(b'')
    Result = subprocess.run([str(Test)], capture_output=True, text=True)
    Check(Result.returncode == 127, 'empty publication rejected')
    Payload.unlink()
    Result = subprocess.run([str(Test)], capture_output=True, text=True)
    Check(Result.returncode == 127, 'missing source rejected')
    Data = Work / 'data'
    Data.mkdir()
    (Data / 'file').write_text('test')
    (Data / 'link').symlink_to(Data / 'file')
    Shell = """source Scripts/Backend/Mount.sh
MountPrivilege() { "$@"; }
findmnt() { printf '%s\\n' "$TestRoot"; }
TestRoot="$1"
MountProgramFileExists "$2" "$TestRoot"
"""
    for Source, Valid in [('/data/file', True), ('/data/missing', False), ('/data/link', False)]:
        Result = subprocess.run(['bash', '-c', Shell, 'validation', str(Work), Source], cwd=Root, capture_output=True)
        Check((Result.returncode == 0) == Valid, 'deployment source: ' + Source)
with tempfile.TemporaryDirectory(prefix='lineos-mount-preflight-') as Directory:
    Work = pathlib.Path(Directory)
    for Name in ['LineOS/EFI/BOOT/BOOTX64.EFI', 'LineOS/KERNEL/LINEOS_KERNEL.ELF', 'LineOS/Programs/Example/Renamed.lix', 'LineOS/Libraries/libtest.so', 'LineOS/Libraries/ld-lineos-x86_64.so.1', 'Assets/Fonts/Test.ttf', 'Disk.img']:
        Path = Work / Name
        Path.parent.mkdir(parents=True, exist_ok=True)
        Path.write_bytes(b'fixture')
    Shell = """source Scripts/Backend/Mount.sh
ProgramDestinationCheck /bin/a.lix /bin/b.lix || exit 2
if ProgramDestinationCheck /bin/a.lix /bin/a.lix; then exit 3; fi
if ProgramDestinationCheck /bin/a.lix/child /bin/a.lix; then exit 4; fi
if ProgramDestinationCheck /bin/a.lix /bin/a.lix/child; then exit 5; fi
MountRoot="$1"
DiskImage=Disk.img
ProgramManifest() { printf '%s\\n' 'Example|/bin|-|-|0755|Renamed|musl|/lib/libc.so|-|-'; }
LibraryManifest() { printf '%s\\n' 'Test|Test|-|-|-|-|libtest.so|/opt/test|none|-|-'; }
MountReleaseManaged() { printf 'preflight reached mount boundary\\n'; return 1; }
MountUpdate
"""
    Result = subprocess.run(['bash', '-c', Shell, 'validation', str(Work)], cwd=Root, capture_output=True, text=True)
    Check(Result.returncode == 1 and 'preflight reached mount boundary' in Result.stdout, 'real MountUpdate preflight reaches privilege boundary')
    Check('command not found' not in Result.stderr, 'deployment helper availability')

    DeployShell = Shell[:Shell.index('MountReleaseManaged()')] + """MountReleaseManaged() { return 0; }
sudo() { return 0; }
MountPrivilege()
{
    if [[ "$1" == losetup && "$2" == --find ]]; then
        printf '/dev/fixture\\n'
    fi
    return 0
}
MountProgramCopy() { printf 'copy:%s:%s\\n' "${1##*/}" "$2"; }
MountUpdate
"""
    Result = subprocess.run(['bash', '-c', DeployShell, 'validation', str(Work)], cwd=Root, capture_output=True, text=True)
    Check(Result.returncode == 0 and 'copy:libtest.so:/opt/test' in Result.stdout, 'CopyPath reaches actual MountUpdate copy loop without mounting disks')
    Check('copy:ld-lineos-x86_64.so.1:/lib' in Result.stdout, 'dedicated loader deployment preserved')
    (Work / 'LineOS/Libraries/libstale.so').write_bytes(b'stale')
    Result = subprocess.run(['bash', '-c', DeployShell, 'validation', str(Work)], cwd=Root, capture_output=True, text=True)
    Check(Result.returncode == 0 and 'libstale.so' not in Result.stdout, 'stale staged library omitted')
    CollisionShell = Shell.replace('libtest.so|/opt/test', 'libtest.so|/bin/Renamed.lix')
    Result = subprocess.run(['bash', '-c', CollisionShell, 'validation', str(Work)], cwd=Root, capture_output=True, text=True)
    Check(Result.returncode != 0 and 'preflight reached mount boundary' not in Result.stdout, 'library path collision rejected before mounting')

Row = 'Shell/Commands/hello|/bin|LOSLib|UserPrograms/Shell/Commands/hello/hello.c|0755|Renamed|musl|/lib/custom.so|Fonts/JetBrainsMono|-'
Result = subprocess.run(['make', '-f', 'Makefiles/Makefile', '-n', 'UserProgram', 'Program=Shell/Commands/hello', 'USER_PROGRAM_MANIFEST=' + Row], cwd=Root, capture_output=True, text=True)
Check(Result.returncode == 0, 'make template dry run')
Check('--dynamic-linker /lib/custom.so' in Result.stdout and 'Shell/Commands/hello/Renamed.lix' in Result.stdout, 'name/interpreter reach actual link command')
print(f'PASS: {Count} program configuration/runtime checks')
