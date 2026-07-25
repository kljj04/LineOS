[LineOS C Coding Style Rules]
You must strictly follow these formatting and naming conventions for all C/C++ code generation:

1. lowercase (lc):
   - File names (e.g., kprint.c, font.c, glyph.h)
   - Single-word variables (e.g., x, y, color, msg, ptr)

2. PascalCase (PC) & Acronyms:
   - Multi-word variables (e.g., TargetWidth, PixelData, KernelBaseAddr)
   - Functions (e.g., KPrint(), DrawPixel(), InitCPU())
   - Struct internal members (e.g., struct FontGlyph, GlyphWidth)
   - Enum members/constants (e.g., FontTypeAscii, FontTypeHangeul)
   - Acronyms MUST be UPPERCASE (e.g., CPU, GPU, VGA, RSDP, GOP, MMU, IDT, GDT, PML4, UEFI, APIC)
     - Combined examples: CPUId, VGABuffer, GPUPixelData, RSDPPointer, InitCPU(), SetupGOP()

3. SCREAMING_SNAKE_CASE / UPPER:
   - Macros and #defines, Structs (e.g., FONT_MAX, BASE_ADDRESS)
   - Standard Integer types and typedefs (e.g., UINT32, INT8, UINTN, PHYS_ADDR)
   - Enum type names themselves (e.g., typedef enum { ... } FONT_TYPE;)

4. Strict Function Parameter Rule (ONE LINE):
   - All function definitions and declarations MUST keep their parameters on ONE SINGLE LINE. 
   - Never break parameters into multiple lines (e.g., void DrawPixel(UINT32 X, UINT32 Y, UINT32 Color);).

* Context: 64-bit UEFI kernel (Long Mode). No 16-bit BIOS code. Custom OS development framework.