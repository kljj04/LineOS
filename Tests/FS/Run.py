from pathlib import Path
import subprocess
import argparse

Parser = argparse.ArgumentParser()
Parser.add_argument('--work', type=Path, required=True)
Args = Parser.parse_args()
Work = Args.work.resolve()
Work.mkdir(parents=True, exist_ok=True)
Root = Path(__file__).resolve().parents[2]
Sources = sorted((Root / 'Kernel/Source/FS/EXT2').glob('*.c')) + sorted((Root / 'Kernel/Source/FS/VFS').glob('*.c'))
Log = (Work / 'Results.log').open('w')
def Run(Command):
    Result = subprocess.run(list(map(str, Command)), text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    Log.write(' '.join(map(str, Command)) + '\n' + Result.stdout + '\n')
    Log.flush()
    print(Result.stdout, end='')
    if Result.returncode:
        raise RuntimeError(Result.returncode)
Run(['clang', '-g', '-O1', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-DLINEOS_KERNEL_BUILD', '-I'+str(Root/'Common'), '-I'+str(Root/'Kernel/Include'), Root/'Tests/FS/Host.c', *Sources, Root/'Kernel/Source/Disk/GPT.c', '-o', Work/'Host'])
Run(['clang', '-g', '-O1', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-DLINEOS_KERNEL_BUILD', '-I'+str(Root/'Common'), '-I'+str(Root/'Kernel/Include'), Root/'Tests/FS/ClockHost.c', Root/'Kernel/Source/Timer/Clock.c', '-o', Work/'ClockHost'])
Run([Work/'ClockHost'])
Run(['clang', '-g', '-O1', '-Wall', '-Wextra', '-Werror', '-fshort-wchar', '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', '-fsanitize=address,undefined', '-DLINEOS_KERNEL_BUILD', '-I'+str(Root/'Common'), '-I'+str(Root/'Kernel/Include'), '-I'+str(Root/'Kernel/Assets'), Root/'Tests/FS/ShellHost.c', *Sources, Root/'Kernel/Source/Disk/GPT.c', '-o', Work/'ShellHost'])
for Block in (1024, 2048, 4096):
    Parts = []
    for Index in range(2):
        Part = Work / f'Part{Block}-{Index}.img'
        with Part.open('wb') as File:
            File.truncate(32*1024*1024)
        Run(['mke2fs', '-q', '-t', 'ext2', '-F', '-b', Block, Part])
        Parts.append(Part)
    Disk = Work / f'Disk{Block}.img'
    with Disk.open('wb') as File:
        for Part in Parts:
            File.write(Part.read_bytes())
    Run([Work/'Host', Disk, 32*1024*1024//512])
    with Disk.open('rb') as File:
        for Part in Parts:
            Part.write_bytes(File.read(32*1024*1024))
            Run(['e2fsck', '-fn', Part])
Log.close()

ShellImage = Work/'Shell.img'
with ShellImage.open('wb') as File:
    File.truncate(32*1024*1024)
Result = subprocess.run(['mke2fs','-q','-F','-t','ext2',str(ShellImage)],check=True)
Result = subprocess.run([str(Work/'ShellHost'),str(ShellImage)],check=True)
subprocess.run(['e2fsck','-fn',str(ShellImage)],check=True)
