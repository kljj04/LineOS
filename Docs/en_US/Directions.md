# Coding conventions

[Documentation index](README.md) · [한국어](../ko_KR/Directions.md) · [English](../en_US/Directions.md)

## Naming

Functions, variables, parameters and structure members use PascalCase, preserving established acronym spellings such as CPU, GPU, PCI and VirtIO. Structure/enum/typedef names, enum members and macros use SCREAMING_SNAKE_CASE. These naming rules have no exceptions. Reuse the actual declared names when calling existing interfaces; do not invent APIs.

## Layout and comments

Use Allman braces, four spaces and no tabs. Attach parentheses to function names and put a space after control-flow keywords. No-argument declarations and definitions specify `VOID`. Keep argument and parameter lists on one line even when long. Preserve existing three-line file banners. Do not add implementation-body comments; header interface comments are allowed.

The root `.clang-format` is the formatting source of truth:

The root .clang-format also sets AlignArrayOfStructures=Left, AlignEscapedNewlines=DontAlign and enables assignment alignment with PadOperators. The actual root configuration takes precedence over this overview; bundled upstream source is excluded from project-wide formatting.

| Setting | Value |
| ------------------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| Base style | LLVM |
| IndentWidth / TabWidth / UseTab | 4 / 4 / Never |
| ColumnLimit | 12000 |
| PointerAlignment / DerivePointerAlignment | Right / false |
| BreakBeforeBraces | Allman |
| SpaceBeforeParens / SpaceAfterCStyleCast | ControlStatements / true |
| BinPackParameters / BinPackArguments | true / true |
| AllowShortFunctionsOnASingleLine | Empty |
| AlwaysBreakAfterReturnType | None |
| AlignConsecutiveAssignments | Enabled, PadOperators; no cross-empty/comment alignment |
| AlignConsecutiveDeclarations | Enabled with AlignFunctionPointers and PadOperators; AlignCompound disabled; not across empty lines/comments |
| AlignConsecutiveMacros | Enabled; not across empty lines/comments; AlignCompound, AlignFunctionPointers and PadOperators disabled |
| IndentCaseLabels | true |
| SortIncludes | false |
| AttributeMacros | `__attribute__` |
| StatementMacros | `CLI`, `HLT` |

## Kernel definitions

Use the project's `CONST`, `NULL`, `TRUE`, `FALSE`, `STATIC`, `EXTERN`, `INLINE`, `PACKED`, `MS_ABI` and `SYSV_ABI` definitions, and `ASM` for inline assembly. Verify types and interfaces in the live headers before editing callers.

## Scope and documentation

Preserve user changes and existing subsystem ownership. Distinguish compile/link, image deployment and observed runtime results. Keep README as an overview and matching subsystem pages under `Docs/en_US/` and `Docs/ko_KR/`. Korean documentation uses formal, polite declarative prose. Implementation alone does not establish that the default KMain starts a service.

## Documentation maintenance

Keep matching filenames under Docs/en_US and Docs/ko_KR and register each topic in both indexes. Translate the full reference rather than appending an English summary to Korean prose. Shared license/notice originals remain at the Docs root.

ProgramConfig/LibraryConfig own configuration syntax, DeviceFS owns detailed ABI behavior, Syscalls owns the number inventory and Validation owns check procedures. Other pages provide overviews and links instead of maintaining duplicate rules. Examples must match the parser/API; links are relative to their document and source paths/commands are relative to the repository root.

Separate implementation descriptions from observations of a particular run. Do not append progress updates or conversational change history to feature references. Keep tested versions, environments and limits in validation records, and do not promise unmeasured performance.
