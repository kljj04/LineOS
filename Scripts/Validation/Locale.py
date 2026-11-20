#!/usr/bin/env python3
# Scripts/Validation/Locale.py
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

import os
import pathlib
import subprocess
import tempfile

Root = pathlib.Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='lineos-locale-') as Directory:
    Work = pathlib.Path(Directory)
    Flags = ['-target', 'x86_64-elf', '-D_GNU_SOURCE', '-fPIC', '-ffreestanding', '-fno-builtin', '-fno-stack-protector', '-nostdlib', '-nostdinc', '-ICommon', '-ILibraries/LOSLib/Include', '-ILibraries/Locale/Include', '-isystem', 'Libraries/musl/obj/include', '-isystem', 'Libraries/musl/arch/x86_64', '-isystem', 'Libraries/musl/arch/generic', '-isystem', 'Libraries/musl/include']
    for Name, Source, Extra in [('Configure', 'Libraries/Locale/Source/Configure.c', ['-Dopen=LocaleTestOpen']), ('Command', 'UserPrograms/Shell/Commands/locale/locale.c', ['-Dmain=LocaleCommandMain', '-Dopen=LocaleTestOpen', '-Drename=LocaleTestRename', '-Dgeteuid=LocaleTestGetEUID']), ('Gettext', 'Libraries/Locale/Source/Gettext.c', []), ('Test', 'Scripts/Validation/Locale/Locale.c', [])]:
        subprocess.run(['clang', *Flags, *Extra, '-c', Source, '-o', str(Work / (Name + '.o'))], cwd=Root, check=True)
    subprocess.run(['ld.lld', '-pie', '-e', '_start', '--dynamic-linker', str(Root / 'LineOS/Libraries/libc.so'), '-o', str(Work / 'Test'), str(Root / 'Libraries/musl/lib/Scrt1.o'), str(Root / 'Libraries/musl/lib/crti.o'), *(str(Work / (Name + '.o')) for Name in ('Configure', 'Command', 'Gettext', 'Test')), '-L' + str(Root / 'LineOS/Libraries'), '-l:libc.so', str(Root / 'Libraries/musl/lib/crtn.o')], cwd=Root, check=True)
    Environment = {Key: Value for Key, Value in os.environ.items() if Key != 'LANG' and not Key.startswith('LC_')}
    Environment.update({'LOS_TEST_DIRECTORY': str(Work), 'TZ': 'UTC'})
    subprocess.run([str(Work / 'Test')], env=Environment, check=True, timeout=20)
