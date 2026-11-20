#!/usr/bin/env python3
import importlib.util
import pathlib
import subprocess

Root = pathlib.Path(__file__).resolve().parents[2]
Spec = importlib.util.spec_from_file_location('ProgramConfig', Root / 'Scripts/Backend/ProgramConfig.py')
Config = importlib.util.module_from_spec(Spec)
Spec.loader.exec_module(Config)
Work = Root / 'Build/Validation/ProgramConfig'
Fixture = Work / 'Fixture'
for Name in ['Libraries', 'Scripts/Backend']:
    (Fixture / Name).mkdir(parents=True, exist_ok=True)
for Name in ['Libraries.sh', 'ProgramRuntime.c.in']:
    (Fixture / 'Scripts/Backend' / Name).write_bytes((Root / 'Scripts/Backend' / Name).read_bytes())
Text = {
    'Publisher': '[Program]\nName = ConfigInit\n[Publish]\nAssets =\n    Validation/Config = /cfg-payload\n[Dependencies]\nAssets =\n    Validation/Config\n',
    'Consumer': '[Dependencies]\nAssets =\n    Validation/Config\n',
    'Missing': '[Dependencies]\nAssets =\n    Validation/Missing\n',
    'Denied': '[Publish]\nAssets =\n    Validation/Denied = /cfg-payload\n',
}
for Name, Contents in Text.items():
    Directory = Fixture / 'UserPrograms' / Name
    Directory.mkdir(parents=True, exist_ok=True)
    (Directory / 'Program.cfg').write_text(Contents)
(Work / 'Empty.c').write_text('int main(void) { return 0; }\n')
(Work / 'Payload').write_bytes(b'payload!')
Flags = ['clang', '-target', 'x86_64-elf', '-fPIC', '-ffreestanding', '-fno-builtin', '-nostdlib', '-nostdinc', '-ICommon', '-ILibraries/LOSLib/Include', '-isystem', 'Libraries/musl/obj/include', '-isystem', 'Libraries/musl/arch/x86_64', '-isystem', 'Libraries/musl/arch/generic', '-isystem', 'Libraries/musl/include']
for Entry in Config.ProgramConfigs(Fixture):
    Name = Entry['Program']
    Runtime = Work / (Name + 'Runtime.c')
    Config.Generate(Fixture, Name, Runtime)
    Main = Root / 'Scripts/Validation/ProgramConfig/Guest.c' if Name == 'Publisher' else Work / 'Empty.c'
    for Source, Output in [(Runtime, Work / (Name + 'Runtime.o')), (Main, Work / (Name + '.o'))]:
        subprocess.run([*Flags, '-c', str(Source), '-o', str(Output)], cwd=Root, check=True)
    subprocess.run(['ld.lld', '-pie', '-e', '_start', '--dynamic-linker', Entry['DynamicLinker'], '-o', str(Work / (Entry['Name'] + '.lix')), 'Libraries/musl/lib/Scrt1.o', 'Libraries/musl/lib/crti.o', str(Work / (Name + 'Runtime.o')), str(Work / (Name + '.o')), '-LLineOS/Libraries', '-l:liblos.so', '-l:libc.so', 'Libraries/musl/lib/crtn.o'], cwd=Root, check=True)
print('Built Program.cfg guest fixtures:', Work)
