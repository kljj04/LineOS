#!/usr/bin/env python3
import os
import pathlib
import subprocess
import tempfile

Root = pathlib.Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='lineos-interactive-') as Directory:
    Work = pathlib.Path(Directory)
    Prompt = Work / 'Prompt'
    subprocess.run(['clang', '-D_GNU_SOURCE', '-g', '-O1', '-fsanitize=address,undefined', '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', '-ICommon', '-ILibraries/LOSLib/Include', '-ILibraries/UserDB/Include', '-IUserPrograms/Shell/Login/Include', 'Scripts/Validation/MuslCompat/Prompt.c', '-o', str(Prompt)], cwd=Root, check=True)
    subprocess.run([str(Prompt)], cwd=Work, check=True)
    Shell = Work / 'lsh'
    Objects = sorted((Root / 'Build/Objects/UserPrograms/Shell/lsh/Source').glob('*.o'))
    subprocess.run(['ld.lld', '-pie', '-e', '_start', '--dynamic-linker', str(Root / 'LineOS/Libraries/libc.so'), '-rpath', str(Root / 'LineOS/Libraries'), '-o', str(Shell), str(Root / 'Libraries/musl/lib/Scrt1.o'), str(Root / 'Libraries/musl/lib/crti.o'), *(str(Path) for Path in Objects), '-L' + str(Root / 'LineOS/Libraries'), '--start-group', '-l:libtruetype.so', '-l:liblocale.so', '-l:liblos.so', '-l:libc.so', '--end-group', str(Root / 'Libraries/musl/lib/crtn.o')], cwd=Root, check=True)
    Result = subprocess.run([str(Shell)], input=b'echo ok\nexit\n', capture_output=True, cwd=Work, env={**os.environ, 'HOME': str(Work), 'USER': 'validation', 'LANG': 'C'}, timeout=10)
    assert Result.returncode == 0, (Result.returncode, Result.stderr)
    Batches = Result.stdout.split(b'\x1b[?2026h')[1:]
    assert Batches, Result.stdout
    for Batch in Batches:
        Frame = Batch.split(b'\x1b[?2026l', 1)[0]
        assert b'G' in Frame and b'\x1b[' in Frame, Frame
        assert Frame.endswith(b'G'), Frame
    print('Login prompts flush before input, password stays hidden, shell cursor redraw order PASS')
