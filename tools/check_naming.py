#!/usr/bin/env python3
# tools/check_naming.py
# LineOS Project
# Copyright (C) 2026 LineOS Developer kljj04

import re
import sys
from pathlib import Path


ACRONYMS = (
    "ACPI",
    "APIC",
    "CPU",
    "ELF",
    "GDT",
    "GOP",
    "GPU",
    "IDT",
    "MMU",
    "MCFG",
    "OS",
    "PCI",
    "PML4",
    "RSDP",
    "SDF",
    "SVG",
    "SYSV",
    "TT",
    "TTF",
    "UEFI",
    "VGA",
)

KNOWN_WORDS = (
    "Address",
    "Bitmap",
    "Buffer",
    "Codepoint",
    "Color",
    "Command",
    "Debug",
    "Device",
    "Display",
    "Draw",
    "File",
    "Font",
    "Frame",
    "Glyph",
    "Height",
    "Image",
    "Index",
    "Info",
    "Init",
    "Kernel",
    "Load",
    "Memory",
    "Pixel",
    "Queue",
    "Read",
    "Rect",
    "Render",
    "Scanout",
    "Shape",
    "Size",
    "Test",
    "Text",
    "True",
    "Type",
    "Virt",
    "Window",
    "Width",
    "Write",
)

CONTROL_WORDS = {
    "do",
    "else",
    "for",
    "if",
    "sizeof",
    "switch",
    "while",
}

KEYWORD_MACROS = {
    "CONST": "const",
    "NULL": "((VOID *) 0)",
    "TRUE": "((BOOLEAN) 1)",
    "FALSE": "((BOOLEAN) 0)",
    "STATIC": "static",
    "EXTERN": "extern",
    "INLINE": "inline",
    "VOLATILE": "volatile",
    "PACKED": "__attribute__((packed))",
    "MS_ABI": "__attribute__((ms_abi))",
    "SYSV_ABI": "__attribute__((sysv_abi))",
    "ASM": "__asm__ volatile",
}

KEYWORD_REPLACEMENTS = (
    (re.compile(r"\b__asm__\s+volatile\b"), "ASM"),
    (re.compile(r"\b__attribute__\s*\(\s*\(\s*packed\s*\)\s*\)"), "PACKED"),
    (re.compile(r"\b__attribute__\s*\(\s*\(\s*ms_abi\s*\)\s*\)"), "MS_ABI"),
    (re.compile(r"\b__attribute__\s*\(\s*\(\s*sysv_abi\s*\)\s*\)"), "SYSV_ABI"),
    (re.compile(r"\(\s*\(\s*VOID\s*\*\s*\)\s*0\s*\)"), "NULL"),
    (re.compile(r"\(\s*\(\s*BOOLEAN\s*\)\s*1\s*\)"), "TRUE"),
    (re.compile(r"\(\s*\(\s*BOOLEAN\s*\)\s*0\s*\)"), "FALSE"),
    (re.compile(r"\bvoid\b"), "VOID"),
    (re.compile(r"\bconst\b"), "CONST"),
    (re.compile(r"\bstatic\b"), "STATIC"),
    (re.compile(r"\bextern\b"), "EXTERN"),
    (re.compile(r"\binline\b"), "INLINE"),
    (re.compile(r"\bvolatile\b"), "VOLATILE"),
)

ALLOWED_FUNCTIONS = {
    "main",
}

ALLOWED_VARIABLE_PATTERNS = (
    re.compile(r"gEfi[A-Za-z0-9]*Guid$"),
)

DECLARATION_TYPES = (
    "BOOLEAN",
    "CHAR8",
    "CHAR16",
    "CONST",
    "EFI_",
    "FLOAT32",
    "FLOAT64",
    "INT8",
    "INT16",
    "INT32",
    "INT64",
    "STATIC",
    "UINT8",
    "UINT16",
    "UINT32",
    "UINT64",
    "UINTN",
    "VOID",
    "VOLATILE",
)

RETURN_TYPE_PATTERN = (
    r"(?:EXTERN\s+|STATIC\s+|INLINE\s+|CONST\s+|VOLATILE\s+)*"
    r"(?:VOID|BOOLEAN|CHAR8|CHAR16|FLOAT32|FLOAT64|INT8|INT16|INT32|INT64|UINT8|UINT16|UINT32|UINT64|UINTN|"
    r"EFI_[A-Z0-9_]+|[A-Z][A-Z0-9_]+)"
    r"(?:\s+\*|\s*\*)*"
)


def compact_spaces(value):
    return re.sub(r"\s+", " ", value.strip())


def is_screaming_snake(name):
    return re.fullmatch(r"[A-Z][A-Z0-9_]*", name) is not None


def is_lower_word(name):
    return re.fullmatch(r"[a-z][a-z0-9]*", name) is not None


def is_pascal_case(name):
    return re.fullmatch(r"[A-Z][A-Za-z0-9]*", name) is not None and "_" not in name


def acronym_usage_ok(name):
    for acronym in ACRONYMS:
        wrong = acronym[0] + acronym[1:].lower()
        if wrong in name:
            return False

    return True


def word_boundary_usage_ok(name):
    for word in KNOWN_WORDS:
        wrong = word[0].lower() + word[1:]
        if wrong not in name:
            continue

        for match in re.finditer(re.escape(wrong), name):
            index = match.start()
            if index == 0:
                continue

            previous = name[index - 1]
            if previous.isupper() or previous.isdigit():
                return False

    return True


def is_function_name(name):
    if name in ALLOWED_FUNCTIONS:
        return True

    return is_pascal_case(name) and acronym_usage_ok(name) and word_boundary_usage_ok(name)


def is_variable_name(name):
    if any(pattern.fullmatch(name) for pattern in ALLOWED_VARIABLE_PATTERNS):
        return True

    if "_" in name:
        return False

    if is_lower_word(name):
        return True

    return is_pascal_case(name) and acronym_usage_ok(name)


def is_lower_file(name):
    return re.fullmatch(r"[a-z0-9_]+\.[ch]", name) is not None


def strip_line(line, in_block_comment):
    result = []
    index = 0
    in_string = False
    string_quote = ""

    while index < len(line):
        char = line[index]
        nxt = line[index + 1] if index + 1 < len(line) else ""

        if in_block_comment:
            if char == "*" and nxt == "/":
                in_block_comment = False
                index += 2
            else:
                index += 1
            continue

        if in_string:
            result.append(" ")
            if char == "\\":
                index += 2
                continue
            if char == string_quote:
                in_string = False
            index += 1
            continue

        if char == "/" and nxt == "/":
            break

        if char == "/" and nxt == "*":
            in_block_comment = True
            index += 2
            continue

        if char == '"' or char == "'":
            in_string = True
            string_quote = char
            result.append(" ")
            index += 1
            continue

        result.append(char)
        index += 1

    return "".join(result), in_block_comment


def replace_keywords_in_code(code):
    result = code
    for pattern, replacement in KEYWORD_REPLACEMENTS:
        result = pattern.sub(replacement, result)

    return result


def rewrite_keyword_uses(lines):
    rewritten = []
    replacements = 0
    in_block_comment = False

    for line in lines:
        if re.match(r"\s*#\s*define\s+(" + "|".join(KEYWORD_MACROS) + r")\b", line):
            rewritten.append(line)
            continue
        if re.match(r"\s*typedef\b", line):
            rewritten.append(line)
            continue

        result = []
        code = []
        index = 0
        in_string = False
        string_quote = ""

        while index < len(line):
            char = line[index]
            nxt = line[index + 1] if index + 1 < len(line) else ""

            if in_block_comment:
                result.append(char)
                if char == "*" and nxt == "/":
                    result.append(nxt)
                    in_block_comment = False
                    index += 2
                else:
                    index += 1
                continue

            if in_string:
                result.append(char)
                if char == "\\" and nxt:
                    result.append(nxt)
                    index += 2
                    continue
                if char == string_quote:
                    in_string = False
                index += 1
                continue

            if char == "/" and nxt in {"/", "*"}:
                replaced = replace_keywords_in_code("".join(code))
                replacements += replaced != "".join(code)
                result.append(replaced)
                code.clear()
                result.append(char)
                result.append(nxt)
                if nxt == "*":
                    in_block_comment = True
                    index += 2
                else:
                    result.append(line[index + 2 :])
                    index = len(line)
                continue

            if char in {'"', "'"}:
                replaced = replace_keywords_in_code("".join(code))
                replacements += replaced != "".join(code)
                result.append(replaced)
                code.clear()
                in_string = True
                string_quote = char
                result.append(char)
                index += 1
                continue

            code.append(char)
            index += 1

        replaced = replace_keywords_in_code("".join(code))
        replacements += replaced != "".join(code)
        result.append(replaced)
        rewritten.append("".join(result))

    return rewritten, replacements


def report(path, line_number, rule, message):
    print(f"{path}:{line_number}: {rule}: {message}")


def warn(path, line_number, rule, message):
    print(f"{path}:{line_number}: warning: {rule}: {message}")


def next_code_line(lines, start):
    in_block_comment = False

    for index in range(start, len(lines)):
        line, in_block_comment = strip_line(lines[index], in_block_comment)
        stripped = line.strip()
        if stripped:
            return index + 1, stripped

    return None, ""


def check_allman(path, line_number, stripped):
    if "{" not in stripped:
        return 0

    if re.match(r"^(if|for|while|switch|else|do)\b", stripped):
        report(path, line_number, "allman", "opening brace should be on its own line")
        return 1

    return 0


def check_keyword_macro(path, line_number, name, body):
    if name not in KEYWORD_MACROS:
        return 0

    expected = KEYWORD_MACROS[name]
    if compact_spaces(body) == expected:
        return 0

    report(path, line_number, "keyword-macro", f"'{name}' should expand to '{expected}'")
    return 1


def check_keyword_use(path, line_number, stripped):
    if stripped.startswith("#"):
        return 0

    failures = 0
    if not stripped.startswith("typedef ") and re.search(r"\bvoid\b", stripped):
        report(path, line_number, "keyword-use", "'void' should be written as 'VOID'")
        failures += 1

    for keyword, macro in (
        ("const", "CONST"),
        ("static", "STATIC"),
        ("extern", "EXTERN"),
        ("inline", "INLINE"),
        ("volatile", "VOLATILE"),
    ):
        if re.search(rf"\b{keyword}\b", stripped):
            report(path, line_number, "keyword-use", f"'{keyword}' should be written as '{macro}'")
            failures += 1

    if re.search(r"\b__asm__\s+volatile\b", stripped):
        report(path, line_number, "keyword-use", "'__asm__ volatile' should be written as 'ASM'")
        failures += 1

    return failures


def check_function(path, lines, line_number, stripped):
    if "__asm__" in stripped or stripped.startswith("ASM("):
        return 0
    if re.search(r"\(\s*\*\s*[A-Za-z_][A-Za-z0-9_]*(?:\[[^\]]*\])?\s*\)\s*\(", stripped):
        return 0

    match = re.match(
        rf"^{RETURN_TYPE_PATTERN}\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(",
        stripped,
    )
    if not match:
        return 0

    name = match.group(1)
    if name in CONTROL_WORDS or name in {"ASM", "__asm__"}:
        return 0

    failures = 0
    if not is_function_name(name):
        warn(path, line_number, "function-name", f"function '{name}' should be PascalCase with uppercase acronyms")

    if ")" not in stripped:
        report(path, line_number, "function-params", f"function '{name}' parameters should stay on one line")
        failures += 1
    elif "{" in stripped:
        report(path, line_number, "allman", f"function '{name}' opening brace should be on the next line")
        failures += 1
    elif stripped.endswith(")") or stripped.endswith(") MS_ABI") or stripped.endswith(") SYSV_ABI"):
        next_line_number, next_line = next_code_line(lines, line_number)
        if next_line_number is not None and not next_line.startswith("{") and not stripped.endswith(";"):
            report(path, line_number, "allman", f"function '{name}' opening brace should be on the next line")
            failures += 1

    return failures


def check_variable_declaration(path, line_number, stripped):
    if "(" in stripped or stripped.startswith("#") or stripped.startswith("typedef "):
        return 0

    if not stripped.endswith(";") and "=" not in stripped:
        return 0

    if not any(stripped.startswith(prefix) for prefix in DECLARATION_TYPES):
        return 0

    candidates = stripped.rstrip(";").split("=", 1)[0].split(",")
    failures = 0

    for candidate in candidates:
        name_match = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*$", candidate.strip())
        if not name_match:
            continue

        name = name_match.group(1)
        if is_screaming_snake(name):
            continue

        if not is_variable_name(name):
            report(path, line_number, "variable-name", f"variable '{name}' should be PascalCase, or lowercase if single-word")
            failures += 1

    return failures


def check_tag_name(path, line_number, stripped):
    match = re.match(r"^(?:typedef\s+)?(?:struct|enum|union)\s+([A-Za-z_][A-Za-z0-9_]*)\b", stripped)
    if not match:
        return 0

    name = match.group(1)
    if is_screaming_snake(name):
        return 0

    report(path, line_number, "tag-name", f"tag '{name}' should be uppercase with underscores")
    return 1


def check_enum_member(path, line_number, stripped, in_enum_block):
    if not in_enum_block:
        return 0
    if stripped.startswith("enum") or stripped.startswith("typedef enum"):
        return 0

    match = re.match(r"([A-Za-z_][A-Za-z0-9_]*)\b(?:\s*=|,|$)", stripped.rstrip(","))
    if not match:
        return 0

    name = match.group(1)
    if is_screaming_snake(name):
        return 0

    report(path, line_number, "enum-name", f"enum member '{name}' should be uppercase with underscores")
    return 1


def check_file(path):
    failures = 0

    if not is_lower_file(path.name):
        report(path, 1, "file-name", "file names should be lowercase")
        failures += 1

    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError as error:
        report(path, 1, "read", str(error))
        return 1

    fixed_lines, replacements = rewrite_keyword_uses(lines)
    if replacements:
        try:
            path.write_text("\n".join(fixed_lines) + "\n", encoding="utf-8")
        except OSError as error:
            report(path, 1, "write", str(error))
            return 1
        lines = fixed_lines
        print(f"{path}: fixed {replacements} keyword line(s)")

    in_block_comment = False
    in_typedef_block = False
    in_enum_block = False

    for line_number, raw_line in enumerate(lines, 1):
        line, in_block_comment = strip_line(raw_line, in_block_comment)
        stripped = line.strip()

        if not stripped:
            continue

        failures += check_allman(path, line_number, stripped)
        failures += check_keyword_use(path, line_number, stripped)
        failures += check_tag_name(path, line_number, stripped)

        define_match = re.match(r"#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s*(.*)", stripped)
        if define_match:
            name = define_match.group(1)
            body = define_match.group(2)
            if not is_screaming_snake(name):
                report(path, line_number, "macro-name", f"macro '{name}' should be uppercase with underscores")
                failures += 1
            failures += check_keyword_macro(path, line_number, name, body)

        if re.search(r"\btypedef\s+(struct|enum|union)\b", stripped):
            in_typedef_block = True
        if re.match(r"^(?:typedef\s+)?enum\b", stripped):
            in_enum_block = True

        typedef_end = re.search(r"}\s*([A-Za-z_][A-Za-z0-9_]*)\s*;", stripped)
        if in_typedef_block and typedef_end:
            name = typedef_end.group(1)
            if not is_screaming_snake(name):
                report(path, line_number, "typedef-name", f"typedef '{name}' should be uppercase with underscores")
                failures += 1
            in_typedef_block = False

        if stripped.startswith("}"):
            in_enum_block = False

        failures += check_enum_member(path, line_number, stripped, in_enum_block)
        failures += check_function(path, lines, line_number, stripped)
        if not in_typedef_block:
            failures += check_variable_declaration(path, line_number, stripped)

    return failures


def main():
    if len(sys.argv) < 2:
        print("usage: check_naming.py FILE...", file=sys.stderr)
        return 2

    failures = 0
    for arg in sys.argv[1:]:
        failures += check_file(Path(arg))

    if failures != 0:
        print(f"naming check failed: {failures} issue(s)", file=sys.stderr)
        return 1

    print("naming check ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
