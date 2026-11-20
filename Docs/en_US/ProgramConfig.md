# Program.cfg

[Documentation index](README.md) · [한국어](../ko_KR/ProgramConfig.md) · [English](../en_US/ProgramConfig.md)

Program source directories declare builds, installation and startup requirements in `Program.cfg`. Settings are read during the build; generated startup code is linked into the executable.

## Example

```ini
[Program]
Name = Example
CopyPath = /bin/lsh
LibC = musl
Enable = true
Mode = 0755

[Dependencies]
Libraries =
    TrueType
Assets =
    Fonts/JetBrainsMono

[Runtime]
DynamicLinker = auto
```

## Syntax

Section and key names are case-sensitive and must not be indented. Whitespace around values, blank lines, full-line `#` comments and CRLF are accepted. Lists start with an empty `Key =` followed by indented items; empty lists are valid. Unknown or duplicate sections/keys/items are errors.

Inline comments, quoting, variable substitution, command execution and legacy flat keys are unsupported. Paths use ASCII letters, digits, underscores, hyphens and dots; dot/dot-dot components and repeated separators are rejected. Dependencies do not support a `Programs` key.

## Program

| Key | Default | Meaning |
| --- | --- | --- |
| `Name` | Source directory basename | Executable basename without `.lix` |
| `CopyPath` | `/bin` | Guest installation directory |
| `LibC` | `musl` | Currently only `musl`; programs do not support `none` |
| `Enable` | `true` | Include in build and deployment |
| `Mode` | `0755` | Four octal permission digits |

Name accepts letters, digits, underscores and hyphens. For `UserPrograms/Example` with Name=renamed, the output is `LineOS/Programs/Example/renamed.lix`; CopyPath=/bin installs `/bin/renamed.lix`. Conflicting destinations are errors. Disabled programs are excluded from builds, deployment and publisher collision checks, but their configuration syntax is still checked. Existing installed files are not automatically removed when names or paths change.

## Dependencies

`Libraries` lists enabled project logical names such as `LOSLib`, `TrueType`, `Window`, `Locale` and `UserDB`, not output filenames such as liblos.so. LibC selects musl automatically. The [library graph](LibraryConfig.md) supplies ordering, transitive headers and guest search paths; direct library declarations supply link inputs. Includes and function calls do not infer dependencies. Programs also currently include LOSLib as a default link input. `--as-needed` drops unused inputs.

`Assets` lists published names such as `Fonts/JetBrainsMono`, not disk paths. Names have no leading slash and fit within 63 ASCII bytes. Startup acquires/releases each required asset before main and exits 127 if one is unavailable or cannot be checked. It neither waits for a service nor keeps the mapping alive; applications acquire/release assets when using them.

Asset use or publication adds LOSLib to the dependency list for generated startup code. Assets may be published externally, so a missing publisher in configuration alone does not reject the build.

## Publish

A publisher maps names to absolute guest disk paths:

```ini
[Program]
Name = Init
CopyPath = /sbin

[Publish]
Assets =
    Fonts/JetBrainsMono = /sys/fonts/JBMNFSB.ttf
    Fonts/Pretendard = /sys/fonts/PTDSB.ttf
```

Duplicate published names across enabled programs are build errors. Image updates check that source paths resolve to regular files within EXT2, without symlink or nested-mount traversal. Publication is not an arbitrary host-file copy rule; source files must already be deployed by their normal installation rules.

Before main, generated code opens each file, reads at most 16 MiB and calls LOSAssetPublish. It frees its temporary buffer after the kernel copies the bytes, then checks asset dependencies. Empty files, I/O failure, duplicate publication or insufficient authority exit 127.

Configuration grants no publication authority; only authorized processes such as boot Init can publish. If a later publication fails, earlier successful publications remain. There is no unpublish API.

## Runtime

DynamicLinker=auto selects `/lib/libc.so`. An explicit absolute guest path becomes ELF PT_INTERP; deployment must provide a compatible interpreter there. Runtime does not read Program.cfg, so configuration changes require rebuilding.

## Validation

Run `python3 Scripts/Validation/Programs.py` for configuration, generated-startup and deployment checks. Disposable guest fixtures and result-file meanings are documented in [Validation](Validation.md).
