# Library.cfg

[Documentation index](README.md) · [한국어](../ko_KR/LibraryConfig.md) · [English](../en_US/LibraryConfig.md)

Each `Libraries/<Directory>/Library.cfg` declares a shared library. Configurations are discovered automatically; ordinary library additions and dependency changes require no common Makefile edits.

## Example

```ini
[Library]
Name = UserDB
Output = libuserdb.so
CopyPath = /lib
LibC = musl
Enable = true

[Build]
CFlags =
    -O2

[Dependencies]
Libraries =
    LOSLib
    Locale
```

## Settings

| Section | Key | Default | Meaning |
| --- | --- | --- | --- |
| Library | `Name` | Directory basename | Case-sensitive logical dependency name |
| Library | `Output` | `lib<lowercase-name>.so` | Installation filename and generic-build ELF SONAME; numeric version suffixes are accepted |
| Library | `CopyPath` | `/lib` | Absolute guest installation directory |
| Library | `LibC` | `musl` | `musl` or `none` |
| Library | `Enable` | `true` | Exclude from build/deployment when false |
| Build | `CFlags` | Empty list | Additional generic compiler options, one option per indented line |
| Build | `Makefile` | Unset | Custom Makefile relative to the library root |
| Build | `Artifact` | Unset | Custom shared-object output relative to the library root; required with Makefile |
| Dependencies | `Libraries` | Empty list | Enabled logical dependency names |

## Syntax

The Library section is required; other sections and defaulted settings may be omitted. Names and keys are case-sensitive. Blank lines, full-line `#` comments and CRLF are accepted. Lists use an empty `Key =` followed by indented items.

Unknown/duplicate keys or sections, repeated dependencies, missing/disabled dependencies, cycles and name/output collisions are errors. Legacy flat keys, substitutions, inline comments, quoted or space-containing paths, relative installation directories, dot/dot-dot components and symlink traversal are unsupported. CFlags items cannot contain shell characters, spaces or commas. Use single-token options such as `-DNAME=value` or `-Ipath`.

## Generic builds and dependencies

The builder discovers `.c` and `.S` files under Source. Compilation receives the library's Include directory plus transitive public Include directories. Declared dependency builds complete first, including in parallel builds, so generated headers are available. Direct dependencies are linked with `-z defs`; `--as-needed` removes unused dynamic dependencies. Declare a direct dependency for each library whose symbols you call directly.

LibC=musl adds project musl headers and libc. LibC=none uses Common and Clang freestanding builtin headers without automatically adding musl. An explicitly declared hosted dependency can still bring libc transitively. A fully freestanding graph therefore needs freestanding dependencies too; provide required runtime symbols such as memcpy yourself.

## Installation and runtime lookup

Outputs stage at `LineOS/Libraries/<Output>`. Image updates deploy only active manifest entries to `<CopyPath>/<Output>`, ignoring stale staged shared objects and rejecting conflicts with program/font destinations. Old files already installed elsewhere in the image are not automatically deleted.

Programs and generic libraries record guest dependency directories in RUNPATH. Programs include paths for declared libraries and their transitive dependencies, allowing nondefault directories such as `/opt/...`.

## Custom builds

```ini
[Library]
Name = musl
Output = libc.so
CopyPath = /lib
LibC = none

[Build]
Makefile = Makefile
Artifact = lib/libc.so
```

The builder invokes `$(MAKE) -C Libraries/<Directory> -f <Makefile>` and copies Artifact under the Output name after success. Failure and recursive dry-run behavior propagate to the parent. Artifact need not exist before the first build; copying requires a regular file without symlink traversal.

Custom make owns its compiler settings, headers, SONAME and RUNPATH. The parent guarantees dependency ordering and exposes a custom library's Include directory to consumers when present.

## Limits and validation

Musl's logical name requires Output=libc.so, CopyPath=/lib and LibC=none because startup and secure-exec policy use `/lib/libc.so`. The dedicated LineOS loader remains installed at `/lib/ld-lineos-x86_64.so.1`. Static archives, build-only custom targets and asset publication are outside this format.

See [Validation](Validation.md) for parser tests, actual dependency builds and deployment checks.
