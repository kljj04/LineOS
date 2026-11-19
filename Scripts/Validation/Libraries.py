#!/usr/bin/env python3
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
    Library = TestRoot / 'Libraries' / 'Example'
    Library.mkdir(parents=True)
    Config = Library / 'Library.cfg'
    Makefile = Library / 'Build.mk'
    Makefile.write_text('all:\n\t@pwd > location\n')

    def Manifest(Text):
        Config.write_text(Text)
        return subprocess.run(['bash', str(Root / 'Scripts/Backend/Libraries.sh'), str(TestRoot)], capture_output=True, text=True)

    Result = Manifest('Enable = true\r\nIncludedMakefile = Build.mk\r\n')
    Check(Result.returncode == 0 and Result.stdout == 'Example|Example|-|-|Build.mk|-\n', 'custom CRLF manifest')
    Check(Manifest('Enable = false\nIncludedMakefile = missing\n').stdout == '', 'disabled library')
    for Value in ['', '-', '/tmp/Makefile', '../Build.mk', './Build.mk', 'a/../Build.mk', 'a//Build.mk', 'Build.mk/', 'a b', 'a|b', '$(id)', '-Build.mk', 'missing']:
        Check(Manifest('IncludedMakefile = ' + Value + '\n').returncode != 0, 'invalid path: ' + Value)
    for Text in ['Enable = yes', 'Enable=true\nEnable=false', '= value', 'Invalid=true', 'IncludedMakefile=Build.mk\nIncludedMakefile=Build.mk', 'Source' + 'Dir=src', 'Enable true', 'CFlags=-O2\v-bad']:
        Check(Manifest(Text).returncode != 0, 'invalid configuration: ' + repr(Text))
    (Library / 'Link.mk').symlink_to(Makefile)
    Check(Manifest('IncludedMakefile=Link.mk').returncode != 0, 'symlink file')
    (Library / 'alias').symlink_to(Library, target_is_directory=True)
    Check(Manifest('IncludedMakefile=alias/Build.mk').returncode != 0, 'symlink component')
    (Library / 'nested').mkdir()
    (Library / 'nested/Build.mk').write_text('all:\n\t@pwd > location\n')
    Check(Manifest('IncludedMakefile=nested/Build.mk').returncode == 0, 'nested makefile')
    (Library / 'Source').mkdir()
    (Library / 'Source/Test.c').write_text('int Test(void) { return 0; }\n')
    Result = Manifest('CFlags = -O2\t-DTEST=1\n')
    Check(Result.returncode == 0 and Result.stdout == 'Example|Example|-O2,-DTEST=1|Libraries/Example/Source/Test.c|-|-\n', 'ordinary manifest')
    Check(Manifest('IncludedMakefile=Build.mk').stdout == 'Example|Example|-|-|Build.mk|-\n', 'custom excludes existing source')
    (TestRoot / 'Scripts/Backend').mkdir(parents=True)
    shutil.copy(Root / 'Scripts/Backend/Libraries.sh', TestRoot / 'Scripts/Backend/Libraries.sh')
    (TestRoot / 'Makefiles').mkdir()
    shutil.copy(Root / 'Makefiles/Libraries.mk', TestRoot / 'Makefiles/Libraries.mk')
    Harness = TestRoot / 'Harness.mk'
    Harness.write_text('include Makefiles/Libraries.mk\nall: $(LIBRARY_TARGETS)\n')

    def Make(*Arguments):
        return subprocess.run(['make', '-f', 'Harness.mk', *Arguments], cwd=TestRoot, capture_output=True, text=True)

    Check(Make('all').returncode == 0 and (Library / 'location').read_text().strip() == str(Library), 'custom working directory')
    (Library / 'location').unlink()
    Check(Make('-n', 'all').returncode == 0 and not (Library / 'location').exists(), 'recursive dry run')
    Database = Make('-np', 'all')
    Check('LIBRARY_OBJS_Example' not in Database.stdout and 'USER_LIBRARY_Example' not in Database.stdout, 'no generic custom graph')
    Manifest('IncludedMakefile=nested/Build.mk')
    Check(Make('all').returncode == 0 and (Library / 'location').read_text().strip() == str(Library), 'nested makefile working directory')
    Makefile.write_text('all:\n\t@exit 17\n')
    Manifest('IncludedMakefile=Build.mk')
    Check(Make('all').returncode != 0, 'custom failure propagation')
    for Value in ['', '../libc.so', '/libc.so', 'lib//libc.so', 'lib/../libc.so', '`lib/libc.so`', 'lib/libc.a']:
        Check(Manifest('IncludedMakefile=Build.mk\nso=' + Value).returncode != 0, 'invalid so: ' + Value)
    Check(Manifest('so=lib/libc.so').returncode != 0, 'so requires custom make')
    Check(Manifest('IncludedMakefile=Build.mk\nso=lib/liblosc.so').returncode != 0, 'reserved output collision')
    Makefile.write_text('all:\n\t@mkdir -p lib\n\t@printf output > lib/libexample.so\n')
    Manifest('IncludedMakefile=Build.mk\nso=lib/libexample.so')
    Check(Make('-n', 'all').returncode == 0 and not (TestRoot / 'LineOS').exists(), 'copy dry run')
    Check(Make('all').returncode == 0 and (TestRoot / 'LineOS/Libraries/libexample.so').read_text() == 'output', 'copy generated output')
    Check('USER_LIBRARY_Example := LineOS/Libraries/libexample.so' in Make('-np', 'all').stdout, 'custom output exports link target')
    Manifest('IncludedMakefile=Build.mk\nso=lib/missing.so')
    Check(Make('all').returncode != 0, 'missing output fails after make')
    (Library / 'lib/link.so').symlink_to(Library / 'lib/libexample.so')
    Manifest('IncludedMakefile=Build.mk\nso=lib/link.so')
    Check(Make('all').returncode != 0, 'output symlink rejected')
    Manifest('IncludedMakefile=Build.mk\nso=alias/lib/libexample.so')
    Check(Make('all').returncode != 0, 'output symlink component rejected')
    Manifest('IncludedMakefile=Build.mk\nso=lib/libexample.so')
    Makefile.write_text('all:\n\t@exit 17\n')
    (TestRoot / 'LineOS/Libraries/libexample.so').write_text('previous')
    Check(Make('all').returncode != 0 and (TestRoot / 'LineOS/Libraries/libexample.so').read_text() == 'previous', 'failed make does not copy')
    Config.unlink()
    Config.symlink_to(Library / 'nested/Build.mk')
    Check(subprocess.run(['bash', str(Root / 'Scripts/Backend/Libraries.sh'), str(TestRoot)], capture_output=True).returncode != 0, 'symlink configuration')
print(f'PASS: {Count} library configuration/build checks')
