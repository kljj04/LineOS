# Shell variables

[Documentation index](README.md) · [한국어](../ko_KR/ShellVariables.md) · [English](../en_US/ShellVariables.md)

Lsh supports standalone assignments such as `A=B`, `A="hello world"` and `A=$HOME`. Quotes are removed and variables expanded without splitting an expanded value on spaces. `A=` stores an empty value. The right-hand side uses the old variable values; replacement happens only after successful expansion.

## Lookup and limits

Assignments remain local to the current shell and are not automatically exported to child processes. `$A` and `${A}` search shell variables before inherited environment values. Up to 128 names and 8191 bytes per value are stored, also bounded by the parser's aggregate storage capacity.

Multiple assignments in one command, temporary command-prefix assignments, export and unset are unsupported. See [userspace overview](UserLibraries.md) for the shell/runtime context.

## Highlighting

| Element | Color |
| --- | --- |
| `$` and variable braces | `#B76FB7` |
| Special keys `?`, `$` and shell-variable names | `#FB530D` |
| Existing environment names | `#F204CF` |
| Unknown names | `#777778` |
| Assignment name | `#00AD00` |
| Assignment `=` | `#0086FF` |
| Ordinary value text | White |

Single-quoted and escaped dollar signs are not highlighted as expansions. Variables within assignment values use the same expansion colors.

## Input layout

Input and history buffers are 4096 bytes; input reserves space for NUL termination and Hangul composition. Lines wrap to screen width. Lsh clears the previous input area and redraws within synchronized screen updates, keeping a cursor-adjacent segment visible when input exceeds screen height. UTF-8 characters are not split.

Wrapping measures JetBrainsMono/Pretendard advances and glyph right edges in pixels, matching Terminal. If assets or pixel size are unavailable, non-ASCII characters fall back to two-column width. Common/Terminal/Text.h shares font size and horizontal margins. Lsh caches measurements rather than rasterizing glyph images.

See [Validation](Validation.md) for input/redraw checks; this is not a claim of full POSIX shell compatibility.
