#!/usr/bin/env python3
import json
import pathlib
import re
import subprocess
import sys


class ConfigError(ValueError):
    pass


def Fail(Path, Line, Message):
    raise ConfigError(f'{Path}:{Line}: {Message}')


def ValidPath(Value, Absolute=True):
    Pattern = r'/[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*' if Absolute else r'[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*'
    return len(Value.encode()) < 4096 and re.fullmatch(Pattern, Value) is not None and all(Part not in ('.', '..') for Part in Value.split('/'))


def LibraryNames(Root):
    Result = subprocess.run(['bash', str(Root / 'Scripts/Backend/Libraries.sh'), str(Root)], capture_output=True, text=True)
    if Result.returncode:
        raise ConfigError(Result.stderr.strip())
    Names = set()
    for Row in Result.stdout.splitlines():
        Fields = Row.split('|')
        if Fields[4] == '-' or Fields[5] != '-':
            Names.add(Fields[0])
    return Names


def ReadConfig(Path, Libraries):
    if Path.is_symlink() or not Path.is_file():
        Fail(Path, 0, 'configuration must be a regular file')
    Result = {'Name': Path.parent.name, 'CopyPath': '/bin', 'LibC': 'musl', 'Enable': True, 'Mode': '0755', 'DynamicLinker': '/lib/libc.so', 'Libraries': [], 'Assets': [], 'Publish': []}
    Section = None
    ListKey = None
    Sections = set()
    Keys = set()
    Items = set()
    Allowed = {'Program': {'Name', 'CopyPath', 'LibC', 'Enable', 'Mode'}, 'Dependencies': {'Libraries', 'Assets'}, 'Publish': {'Assets'}, 'Runtime': {'DynamicLinker'}}
    try:
        Lines = Path.read_text(encoding='utf-8').splitlines()
    except (OSError, UnicodeError) as Error:
        Fail(Path, 0, str(Error))
    for Number, Raw in enumerate(Lines, 1):
        Text = Raw.strip()
        if not Text or Text.startswith('#'):
            continue
        if Raw[0].isspace():
            if ListKey is None:
                Fail(Path, Number, 'indented item outside a list')
            if ListKey == ('Publish', 'Assets'):
                if Text.count('=') != 1:
                    Fail(Path, Number, 'publication must be Name = /disk/path')
                Name, Source = (Part.strip() for Part in Text.split('=', 1))
                if not ValidPath(Name, False) or len(Name.encode()) >= 64 or not ValidPath(Source):
                    Fail(Path, Number, 'invalid published name or source path')
                Value = (Name, Source)
                Identity = (ListKey, Name)
            else:
                if '=' in Text:
                    Fail(Path, Number, 'unexpected assignment in dependency list')
                Value = Text
                Identity = (ListKey, Text)
                if ListKey[1] == 'Libraries':
                    if Text == 'musl' or Text not in Libraries:
                        Fail(Path, Number, f'unknown library {Text!r}; use logical names, libc is automatic')
                elif not ValidPath(Text, False) or len(Text.encode()) >= 64:
                    Fail(Path, Number, 'asset dependencies must be published names')
            if Identity in Items:
                Fail(Path, Number, 'duplicate list item')
            Items.add(Identity)
            Result['Publish' if ListKey[0] == 'Publish' else ListKey[1]].append(Value)
            continue
        ListKey = None
        if Text.startswith('['):
            Match = re.fullmatch(r'\[([A-Za-z]+)\]', Text)
            if not Match or Match[1] not in Allowed:
                Fail(Path, Number, 'unknown section')
            Section = Match[1]
            if Section in Sections:
                Fail(Path, Number, 'duplicate section')
            Sections.add(Section)
            continue
        if Section is None or '=' not in Text:
            Fail(Path, Number, 'expected a section or Key = Value')
        Key, Value = (Part.strip() for Part in Text.split('=', 1))
        if Key not in Allowed[Section]:
            Fail(Path, Number, f'unknown key {Section}/{Key}')
        Identity = (Section, Key)
        if Identity in Keys:
            Fail(Path, Number, 'duplicate key')
        Keys.add(Identity)
        if Section in ('Dependencies', 'Publish'):
            if Value:
                Fail(Path, Number, 'list items must be indented on following lines')
            ListKey = Identity
        elif Key == 'Name':
            if not re.fullmatch(r'[A-Za-z0-9_-]+', Value):
                Fail(Path, Number, 'invalid executable name')
            Result[Key] = Value
        elif Key == 'CopyPath':
            if '//' in Value:
                Fail(Path, Number, 'repeated slash in CopyPath')
            Value = Value.rstrip('/') or '/'
            if Value != '/' and not ValidPath(Value):
                Fail(Path, Number, 'CopyPath must be an absolute guest directory')
            Result[Key] = Value
        elif Key == 'LibC':
            if Value != 'musl':
                Fail(Path, Number, 'only LibC = musl is supported')
        elif Key == 'Enable':
            if Value not in ('true', 'false'):
                Fail(Path, Number, 'Enable must be true or false')
            Result[Key] = Value == 'true'
        elif Key == 'Mode':
            if not re.fullmatch(r'[0-7]{4}', Value):
                Fail(Path, Number, 'Mode must contain four octal digits')
            Result[Key] = Value
        elif Key == 'DynamicLinker':
            if Value != 'auto' and not ValidPath(Value):
                Fail(Path, Number, 'DynamicLinker must be auto or an absolute guest path')
            Result[Key] = '/lib/libc.so' if Value == 'auto' else Value
    if Result['Assets'] or Result['Publish']:
        if 'LOSLib' not in Result['Libraries']:
            Result['Libraries'].append('LOSLib')
    return Result


def ProgramConfigs(Root):
    Programs = []
    Destinations = {}
    Publishers = {}
    Libraries = LibraryNames(Root)
    for Path in sorted((Root / 'UserPrograms').rglob('Program.cfg')):
        Relative = Path.parent.relative_to(Root / 'UserPrograms')
        if not re.fullmatch(r'[A-Za-z0-9_-]+(?:/[A-Za-z0-9_-]+)*', Relative.as_posix()):
            Fail(Path, 0, 'invalid source directory')
        if any(Parent.is_symlink() for Parent in [Path, *Path.parents] if Parent != Root.parent):
            Fail(Path, 0, 'symlink configuration path')
        Config = ReadConfig(Path, Libraries)
        if not Config['Enable']:
            continue
        Destination = Config['CopyPath'].rstrip('/') + '/' + Config['Name'] + '.lix'
        for Existing in Destinations:
            if Destination == Existing or Destination.startswith(Existing + '/') or Existing.startswith(Destination + '/'):
                Fail(Path, 0, f'installation collision with {Destinations[Existing]}: {Destination}')
        Destinations[Destination] = Relative.as_posix()
        for Name, Source in Config['Publish']:
            if Name in Publishers:
                Fail(Path, 0, f'asset {Name!r} already published by {Publishers[Name]}')
            Publishers[Name] = Relative.as_posix()
        Sources = []
        for Source in sorted(Path.parent.rglob('*')):
            if Source.suffix not in ('.c', '.S') or not Source.is_file():
                continue
            Owner = Source.parent
            while Owner != Path.parent and not (Owner / 'Program.cfg').exists():
                Owner = Owner.parent
            if Owner != Path.parent:
                continue
            if Source.is_symlink() or any(Parent.is_symlink() for Parent in Source.parents):
                Fail(Path, 0, 'symlink source path')
            Name = Source.relative_to(Root).as_posix()
            if not re.fullmatch(r'[A-Za-z0-9_./-]+', Name):
                Fail(Path, 0, 'unsupported source path')
            Sources.append(Name)
        Config['Program'] = Relative.as_posix()
        Config['Sources'] = Sources
        Programs.append(Config)
    return Programs


def Manifest(Programs):
    for Config in Programs:
        Fields = [Config['Program'], Config['CopyPath'], ','.join(Config['Libraries']) or '-', ','.join(Config['Sources']) or '-', Config['Mode'], Config['Name'], Config['LibC'], Config['DynamicLinker'], ','.join(Config['Assets']) or '-', ','.join(Name + ':' + Path for Name, Path in Config['Publish']) or '-']
        print('|'.join(Fields))


def Generate(Root, Program, Output):
    Config = next((Entry for Entry in ProgramConfigs(Root) if Entry['Program'] == Program), None)
    if Config is None:
        raise ConfigError(f'unknown or disabled program: {Program}')
    if not Config['Assets'] and not Config['Publish']:
        Text = '#include <Userspace/TypeInfo.h>\n'
    else:
        Text = (Root / 'Scripts/Backend/ProgramRuntime.c.in').read_text()
        Text = Text.replace('@ASSETS@', ',\n'.join('    ' + json.dumps(Name) for Name in Config['Assets']) or '    NULL')
        Text = Text.replace('@PUBLICATIONS@', ',\n'.join('    {' + json.dumps(Name) + ', ' + json.dumps(Path) + '}' for Name, Path in Config['Publish']) or '    {NULL, NULL}')
        Text = Text.replace('@ASSET_COUNT@', str(len(Config['Assets']))).replace('@PUBLISH_COUNT@', str(len(Config['Publish'])))
    Output.parent.mkdir(parents=True, exist_ok=True)
    if not Output.exists() or Output.read_text() != Text:
        Output.write_text(Text)
    else:
        Output.touch()


def Main():
    try:
        Root = pathlib.Path(sys.argv[2]).resolve()
        if sys.argv[1] == 'manifest':
            Manifest(ProgramConfigs(Root))
        elif sys.argv[1] == 'generate':
            Generate(Root, sys.argv[3], pathlib.Path(sys.argv[4]))
        else:
            raise ConfigError('unknown command')
    except (ConfigError, OSError) as Error:
        print(Error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(Main())
