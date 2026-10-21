# Coding conventions

[Documentation index](README.md) · [한국어](../ko_KR/Directions.md) · [English](../en_US/Directions.md)
You must strictly follow these formatting and naming conventions for all C/C++ code generation:

1. Keyword macros:
   - Use the LineOS keyword macros consistently:
     - CONST    -> const
     - NULL     -> ((VOID *) 0)
     - TRUE     -> ((BOOLEAN) 1)
     - FALSE    -> ((BOOLEAN) 0)
     - STATIC   -> static
     - EXTERN   -> extern
     - INLINE   -> inline
     - PACKED   -> __attribute__((packed))
     - MS_ABI   -> __attribute__((ms_abi))
     - SYSV_ABI -> __attribute__((sysv_abi))
     - ASM      -> __asm__ volatile

2. Formatting:
   - Use Allman braces.
   - Function definitions and declarations must keep all parameters on one line.

3. Functions:
   - Function names must use PascalCase.
   - Acronyms inside function names must remain uppercase.
   - If a name may collide with compiler/runtime/library symbols, add a K prefix, such as KMemSet.

4. Variables:
   - variables must use PascalCase.

5. Enum, struct, and constant names:
   - Enum type names, struct type names, typedef names, enum members, macros, and constants must use all uppercase with underscores between words.

* Context: 64-bit UEFI kernel (Long Mode). No 16-bit BIOS code. Custom OS development framework.

## Additional source rules

No-argument functions must specify `VOID`. Attach parentheses to function names and separate them from control keywords. Use PascalCase for parameters and structure members as well as variables. Preserve the existing three-line file banner; do not add internal source comments. Use `ASM` for inline assembly. Preserve user changes and existing file structure. The root `.clang-format` is the formatting reference.
