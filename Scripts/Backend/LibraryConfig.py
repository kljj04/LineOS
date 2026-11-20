#!/usr/bin/env python3
# Scripts/Backend/LibraryConfig.py
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

import pathlib
import re
import shutil
import sys
import tempfile


class CONFIG_ERROR(Exception):
    pass


def Fail(Path, Message):
    raise CONFIG_ERROR(f'{Path}: {Message}')


def ValidPath(Value, Absolute=False):
    Pattern = r'/[A-Za-z0-9_-][A-Za-z0-9_.-]*(?:/[A-Za-z0-9_-][A-Za-z0-9_.-]*)*' if Absolute else r'[A-Za-z0-9_][A-Za-z0-9_.-]*(?:/[A-Za-z0-9_][A-Za-z0-9_.-]*)*'
    return len(Value.encode()) < 4096 and re.fullmatch(Pattern, Value) is not None and all(Part not in ('.', '..') for Part in Value.split('/'))


def RegularPath(Root, Relative, Required=True):
    Path = Root
    for Part in pathlib.PurePosixPath(Relative).parts:
        Path = Path / Part
        if Path.is_symlink():
            Fail(Path, 'symlink is not allowed')
    if Required and not Path.is_file():
        Fail(Path, 'missing regular file')
    return Path


def ReadConfig(Path):
    RegularPath(Path.parent.parent.parent, str(Path.relative_to(Path.parent.parent.parent)))
    Result = {'Name': Path.parent.name, 'Output': None, 'CopyPath': '/lib', 'LibC': 'musl', 'Enable': 'true', 'Makefile': '-', 'Artifact': '-', 'CFlags': [], 'Libraries': [], 'Directory': Path.parent.name, 'Path': Path}
    Allowed = {'Library': {'Name', 'Output', 'CopyPath', 'LibC', 'Enable'}, 'Build': {'Makefile', 'Artifact', 'CFlags'}, 'Dependencies': {'Libraries'}}
    Section = None
    ListKey = None
    Sections = set()
    Seen = set()
    for Number, Raw in enumerate(Path.read_text().splitlines(), 1):
        Line = Raw.strip()
        if not Line or Line.startswith('#'):
            continue
        if Raw[:1].isspace():
            if ListKey is None:
                Fail(Path, f'line {Number}: unexpected indentation')
            Result[ListKey].append(Line)
            continue
        ListKey = None
        if Line.startswith('[') and Line.endswith(']'):
            Section = Line[1:-1]
            if Section not in Allowed or Section in Sections:
                Fail(Path, f'line {Number}: unknown or duplicate section')
            Sections.add(Section)
            continue
        if Section is None or '=' not in Line:
            Fail(Path, f'line {Number}: expected section or key = value')
        Key, Value = (Part.strip() for Part in Line.split('=', 1))
        if Key not in Allowed[Section] or Key in Seen:
            Fail(Path, f'line {Number}: unknown or duplicate key {Key}')
        Seen.add(Key)
        if Key in ('CFlags', 'Libraries'):
            if Value:
                Fail(Path, f'{Key} requires an indented list')
            ListKey = Key
        else:
            Result[Key] = Value
    if 'Library' not in Sections:
        Fail(Path, 'missing [Library]')
    if Result['Enable'] not in ('true', 'false'):
        Fail(Path, 'Enable must be true or false')
    if Result['Enable'] == 'false':
        return Result
    if re.fullmatch(r'[A-Za-z][A-Za-z0-9_-]*', Result['Name']) is None:
        Fail(Path, 'invalid Name')
    if Result['Name'].lower() == 'loader':
        Fail(Path, 'Loader is reserved')
    if Result['Output'] is None:
        Result['Output'] = f"lib{Result['Name'].lower()}.so"
    if re.fullmatch(r'[A-Za-z0-9_][A-Za-z0-9_.-]*\.so(?:\.[0-9]+)*', Result['Output']) is None or Result['Output'] == 'ld-lineos-x86_64.so.1':
        Fail(Path, 'invalid or reserved Output')
    if not ValidPath(Result['CopyPath'], True):
        Fail(Path, 'invalid CopyPath')
    if Result['Name'] == 'musl' and (Result['Output'] != 'libc.so' or Result['CopyPath'] != '/lib' or Result['LibC'] != 'none'):
        Fail(Path, 'musl requires Output = libc.so, CopyPath = /lib and LibC = none')
    if Result['LibC'] not in ('musl', 'none'):
        Fail(Path, 'LibC must be musl or none')
    for Key in ('Makefile', 'Artifact'):
        if Result[Key] != '-':
            if not ValidPath(Result[Key]):
                Fail(Path, f'invalid {Key}')
            RegularPath(Path.parent, Result[Key], Key == 'Makefile')
    if Result['Makefile'] == '-' and Result['Artifact'] != '-':
        Fail(Path, 'Artifact requires Makefile')
    if Result['Makefile'] != '-' and Result['Artifact'] == '-':
        Fail(Path, 'Makefile requires Artifact')
    for Flag in Result['CFlags']:
        if re.fullmatch(r'-[A-Za-z0-9_=+./:-]+', Flag) is None:
            Fail(Path, f'invalid CFlags item {Flag}')
    if len(set(Result['Libraries'])) != len(Result['Libraries']):
        Fail(Path, 'duplicate library dependency')
    for Name in Result['Libraries']:
        if re.fullmatch(r'[A-Za-z][A-Za-z0-9_-]*', Name) is None:
            Fail(Path, f'invalid dependency {Name}')
    return Result


def Manifest(Root):
    if (Root / 'Libraries').is_symlink():
        Fail(Root, 'Libraries is a symlink')
    Libraries = {}
    Outputs = set()
    Names = set()
    for Path in sorted((Root / 'Libraries').glob('*/Library.cfg')):
        if re.fullmatch(r'[A-Za-z0-9_-]+', Path.parent.name) is None:
            Fail(Path, 'invalid directory')
        Item = ReadConfig(Path)
        if Item['Enable'] == 'false':
            continue
        Name = Item['Name']
        if Name.lower() in Names or Item['Output'] in Outputs:
            Fail(Path, 'duplicate name or output')
        Names.add(Name.lower())
        Outputs.add(Item['Output'])
        Libraries[Name] = Item
    State = {}
    Ordered = []

    def Visit(Name):
        if Name not in Libraries:
            Fail(Root, f'unknown or disabled dependency {Name}')
        if State.get(Name) == 1:
            Fail(Libraries[Name]['Path'], 'cyclic library dependency')
        if State.get(Name) == 2:
            return
        State[Name] = 1
        Item = Libraries[Name]
        Dependencies = list(Item['Libraries'])
        if Item['LibC'] == 'musl' and 'musl' not in Dependencies:
            Dependencies.append('musl')
        Item['Dependencies'] = Dependencies
        Closure = []
        for Dependency in Dependencies:
            Visit(Dependency)
            for Entry in [Dependency] + Libraries[Dependency]['Closure']:
                if Entry not in Closure:
                    Closure.append(Entry)
        Item['Closure'] = Closure
        State[Name] = 2
        Ordered.append(Item)

    for Name in Libraries:
        Visit(Name)
    for Item in Ordered:
        Sources = []
        if Item['Makefile'] == '-':
            Directory = Root / 'Libraries' / Item['Directory'] / 'Source'
            if not Directory.is_dir() or Directory.is_symlink():
                Fail(Directory, 'missing source directory')
            for Path in sorted(Directory.rglob('*')):
                if Path.is_symlink():
                    Fail(Path, 'symlink in source tree')
                if Path.is_file() and Path.suffix in ('.c', '.S'):
                    Relative = str(Path.relative_to(Root))
                    if not ValidPath(Relative):
                        Fail(Path, 'invalid source path')
                    Sources.append(Relative)
            if not Sources:
                Fail(Directory, 'no sources')
        Item['Sources'] = Sources
    return Ordered


def Encode(Values):
    return ','.join(Values) or '-'


def Main():
    if sys.argv[1] == '--copy':
        Root = pathlib.Path(sys.argv[2]).absolute()
        Items = Manifest(Root)
        Item = next((Item for Item in Items if Item['Name'] == sys.argv[3]), None)
        if Item is None or Item['Artifact'] == '-':
            Fail(Root, 'unknown custom library')
        Source = RegularPath(Root / 'Libraries' / Item['Directory'], Item['Artifact'])
        RegularPath(Root, 'LineOS/Libraries/' + Item['Output'], False)
        Directory = Root / 'LineOS/Libraries'
        Directory.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=Directory, prefix='.library.', delete=False) as File:
            Temporary = pathlib.Path(File.name)
        try:
            shutil.copyfile(Source, Temporary)
            Temporary.chmod(0o755)
            Temporary.replace(Directory / Item['Output'])
        finally:
            Temporary.unlink(missing_ok=True)
        return
    Root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else '.').absolute()
    for Item in Manifest(Root):
        print('|'.join([Item['Name'], Item['Directory'], Encode(Item['CFlags']), Encode(Item['Sources']), Item['Makefile'], Item['Artifact'], Item['Output'], Item['CopyPath'], Item['LibC'], Encode(Item['Dependencies']), Encode(Item['Closure'])]))


if __name__ == '__main__':
    try:
        Main()
    except (CONFIG_ERROR, OSError, UnicodeError) as Error:
        print(Error, file=sys.stderr)
        sys.exit(1)
