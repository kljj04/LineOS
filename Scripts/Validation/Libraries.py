#!/usr/bin/env python3
# Scripts/Validation/Libraries.py
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

import pathlib
import shutil
import subprocess
import tempfile

Root = pathlib.Path(__file__).resolve().parents[2]
Count = 0


def Check(Condition, Message):
    global Count
    if not Condition:
        raise AssertionError(Message)
    Count += 1


with tempfile.TemporaryDirectory(prefix='lineos-libraries-') as Directory:
    TestRoot = pathlib.Path(Directory)
    for Relative in ('Scripts/Backend', 'Makefiles', 'Libraries/Example/Source', 'Libraries/Example/Include'):
        (TestRoot / Relative).mkdir(parents=True)
    for Relative in ('Scripts/Backend/Libraries.sh', 'Scripts/Backend/LibraryConfig.py', 'Makefiles/Libraries.mk'):
        shutil.copy(Root / Relative, TestRoot / Relative)
    (TestRoot / 'Makefiles/BuildSettings.mk').touch()
    Library = TestRoot / 'Libraries/Example'
    Config = Library / 'Library.cfg'
    Base = '[Library]\nName = Example\nOutput = libexample.so\nLibC = none\nCopyPath = /opt/example\n'
    (Library / 'Source/Example.c').write_text('#include <stddef.h>\n#include <stdint.h>\nint Example(void) { return sizeof(uint32_t); }\n')

    def Manifest(Text):
        Config.write_text(Text)
        return subprocess.run(['bash', str(TestRoot / 'Scripts/Backend/Libraries.sh'), str(TestRoot)], capture_output=True, text=True)

    Result = Manifest(Base.replace('\n', '\r\n'))
    Check(Result.returncode == 0, Result.stderr)
    Check(Result.stdout.split('|')[6:10] == ['libexample.so', '/opt/example', 'none', '-'], 'manifest metadata')
    for Text in ('Enable = true', '[Unknown]\nName=x', Base + '\n[Library]', Base + 'Name = Again', Base.replace('none', 'glibc'), Base.replace('/opt/example', '../lib'), Base.replace('libexample.so', '../bad.so'), Base + '\n[Build]\nCFlags = -O2', Base + '\n[Build]\nCFlags =\n    $(id)', Base + '\n[Dependencies]\nLibraries =\n    Missing', Base + '\n[Dependencies]\nLibraries =\n    Example', Base + '\n[Dependencies]\nLibraries =\n    A\n    A', Base + '\n[Build]\nMakefile = ../bad', Base + '\n[Build]\nArtifact = output.so', Base + '\n[Build]\nMakefile = Missing'):
        Check(Manifest(Text).returncode != 0, 'reject ' + repr(Text))
    Check(Manifest(Base.replace('LibC = none', 'LibC = musl')).returncode != 0, 'missing implicit musl')
    Check(Manifest('[Library]\nEnable = false\n').stdout == '', 'disabled')
    (Library / 'Bad.mk').symlink_to('/tmp/missing')
    Check(Manifest(Base + '\n[Build]\nMakefile = Bad.mk\nArtifact = out.so').returncode != 0, 'reject symlink makefile')
    (Library / 'Source/Bad.c').symlink_to(Library / 'Source/Example.c')
    Check(Manifest(Base).returncode != 0, 'reject symlink source')
    (Library / 'Source/Bad.c').unlink()
    Manifest(Base)
    (TestRoot / 'Harness.mk').write_text('CC = clang\nLD = ld.lld\nKERNEL_TYPE_TARGET = x86_64-elf\nDEPFLAGS = -MMD -MP\nMKDIR_P = mkdir -p $(1)\ninclude Makefiles/Libraries.mk\nall: $(LIBRARY_TARGETS)\n')

    def Make(*Arguments):
        return subprocess.run(['make', '--no-print-directory', '-f', 'Harness.mk', *Arguments], cwd=TestRoot, capture_output=True, text=True)

    Result = Make('all', '-j4')
    Check(Result.returncode == 0, Result.stdout + Result.stderr)
    Check('Libraries/musl' not in Result.stdout and '-l:libc.so' not in Result.stdout, 'none omits musl includes and link')
    Output = TestRoot / 'LineOS/Libraries/libexample.so'
    Dynamic = subprocess.check_output(['readelf', '-d', str(Output)], text=True)
    Check('libexample.so' in Dynamic and '(NEEDED)' not in Dynamic, 'freestanding shared library has no libc dependency')
    (Library / 'Include/Example.h').write_text('int Example(void);\n')
    Consumer = TestRoot / 'Libraries/Consumer'
    (Consumer / 'Source').mkdir(parents=True)
    (Consumer / 'Library.cfg').write_text('[Library]\nLibC = none\nCopyPath = /opt/consumer\n\n[Dependencies]\nLibraries =\n    Example\n')
    (Consumer / 'Source/Consumer.c').write_text('#include <Example.h>\nint Consumer(void) { return Example(); }\n')
    Result = Make('all', '-j4')
    Check(Result.returncode == 0, Result.stdout + Result.stderr)
    Dynamic = subprocess.check_output(['readelf', '-d', str(TestRoot / 'LineOS/Libraries/libconsumer.so')], text=True)
    Check('Shared library: [libexample.so]' in Dynamic and '/opt/example' in Dynamic, 'dependency link and guest RUNPATH')
    Probe = TestRoot / 'Probe.c'
    Probe.write_text('int Consumer(void); int main(void) { return Consumer() != 4; }\n')
    Result = subprocess.run(['clang', str(Probe), '-L' + str(Output.parent), '-lconsumer', '-Wl,-rpath-link,' + str(Output.parent), '-o', str(TestRoot / 'Probe')], capture_output=True, text=True)
    Check(Result.returncode == 0, Result.stderr)
    Result = subprocess.run([str(TestRoot / 'Probe')], env={'LD_LIBRARY_PATH': str(Output.parent)}, capture_output=True)
    Check(Result.returncode == 0, 'execute dependency chain')
    Top = TestRoot / 'Libraries/Top'
    (Top / 'Source').mkdir(parents=True)
    (Top / 'Library.cfg').write_text('[Library]\nLibC = none\n\n[Dependencies]\nLibraries =\n    Consumer\n')
    (Top / 'Source/Top.c').write_text('#include <Example.h>\nint Consumer(void); int Top(void) { return Consumer(); }\n')
    Result = Make('all', '-j4')
    Check(Result.returncode == 0, Result.stdout + Result.stderr)
    Result = Make('-np', 'all')
    Check('USER_LIBRARY_RPATH_Top = $(USER_LIBRARY_PATH_Top) $(call LibraryPaths,$(USER_LIBRARY_CLOSURE_Top))' in Result.stdout, 'transitive program search path export')
    Dynamic = subprocess.check_output(['readelf', '-d', str(TestRoot / 'LineOS/Libraries/libtop.so')], text=True)
    Check('libconsumer.so' in Dynamic and '/opt/consumer' in Dynamic, 'transitive dependency chain builds in parallel')
    (Top / 'Library.cfg').unlink()
    Config.write_text(Base + '\n[Dependencies]\nLibraries =\n    Consumer\n')
    Result = Make('all')
    Check(Result.returncode != 0 and 'cyclic' in Result.stderr, 'cycle rejected before make')
    Config.write_text(Base)
    (Consumer / 'Library.cfg').write_text('[Library]\nName = Other\nOutput = libexample.so\nLibC = none\n')
    Check(Manifest(Base).returncode != 0, 'duplicate output')
    (Consumer / 'Library.cfg').unlink()
    (Library / 'Build.mk').write_text('all:\n\t@pwd > location\n\t@mkdir -p out\n\t@printf fixture > out/original.so\n')
    Custom = Base + '\n[Build]\nMakefile = Build.mk\nArtifact = out/original.so\n'
    Manifest(Custom)
    Result = Make('all')
    Check(Result.returncode == 0, Result.stdout + Result.stderr)
    Check((Library / 'location').read_text().strip() == str(Library), 'custom working directory')
    Check(Output.read_bytes() == b'fixture', 'custom Artifact renamed to Output')
    Output.unlink()
    (Library / 'location').unlink()
    Result = Make('-n', 'all')
    Check(Result.returncode == 0 and not Output.exists() and not (Library / 'location').exists(), 'dry run has no side effects')
    (Library / 'Build.mk').write_text('all:\n\t@exit 7\n')
    Check(Make('all').returncode != 0 and not Output.exists(), 'custom failure propagates')
    (Library / 'out/original.so').unlink()
    (Library / 'out/original.so').symlink_to('/etc/passwd')
    Result = subprocess.run(['bash', str(TestRoot / 'Scripts/Backend/Libraries.sh'), '--copy-so', str(TestRoot), 'Example'], capture_output=True)
    Check(Result.returncode != 0 and not Output.exists(), 'copy rejects symlink artifact')

Result = subprocess.run(['bash', str(Root / 'Scripts/Backend/Libraries.sh'), str(Root)], capture_output=True, text=True)
Check(Result.returncode == 0, Result.stderr)
Check(any(Row.startswith('LOSLib|') for Row in Result.stdout.splitlines()), 'LOSLib uses generic manifest')
print(f'Library validation: {Count} checks passed')
