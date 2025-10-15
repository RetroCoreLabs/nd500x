# Example 04: C Math Example (PCC Compiler)

**Status:** ✅ **WORKING** - Two-step compilation process

---

## Success! PCC Compiler Works!

The PCC C compiler can generate ND-500 assembly code! The workflow is:

1. **C → Assembly** - PCC generates ND-500 assembly (`.s` file)
2. **Assembly → Object** - nd500-as assembles to object file (`.o`)

### Quick Start:
```bash
make         # Compile math.c → math.s → math.o
make demo    # Show generated code and disassembly
make clean   # Clean generated files
```

---

## The Two-Step Process

### Step 1: C to Assembly (PCC)
```bash
../../bin/cc -S math.c -o math.s
```

This generates ND-500 assembly code in `math.s`.

### Step 2: Assembly to Object (nd500-as)
```bash
../../bin/nd500-as math.s -o math.o
```

This assembles to an object file.

---

## Solution 1: Install PCC System-Wide (Recommended)

This creates symlinks so `cc` can find its components:

```bash
make install-pcc
```

This runs (requires sudo):
```bash
sudo ln -sf $(pwd)/../../bin/cc1 /lib/pcc1
sudo ln -sf $(pwd)/../../bin/cc2 /lib/pcc2
```

After installation, you can compile C code:
```bash
../../bin/cc -c math.c -o math.o
```

---

## Solution 2: Write Assembly Directly (Works Now!)

Since the assembler works perfectly, you can write ND-500 assembly directly:

### Create math.s:
```assembly
; Example: Simple math in assembly
; Calculate: (12 * 5 + 7) - (12 / 2) = 67 - 6 = 61

start:
    w1 := 12        ; a = 12
    w2 := 5         ; b = 5
    w3 := w1        ; temp = a
    * w2            ; w3 = a * b (multiply)
    + 7             ; w3 = a * b + 7 (add)
    w4 := w3        ; c = result
    
    w1 := 12        ; reload a
    / 2             ; w1 = a / 2 (divide)
    w2 := w1        ; save quotient
    
    w3 - w2 =: w4   ; d = c - (a/2)
    
    ret
```

### Assemble:
```bash
make asm
```

---

## Solution 3: Alternative - Native GCC Cross-Compile

For testing C code logic (not for ND-500 target):

```bash
gcc -m32 -S math.c -o math-x86.s    # Generate x86 assembly for reference
gcc -m32 math.c -o math-x86         # Compile for x86 to test logic
```

---

## What's in math.c

```c
main()
{
    int a, b, c, d;
    a = 12;
    b = 5;
    c = a * b + 7;    // 67
    d = c - a / 2;    // 67 - 6 = 61
    
    // Print result (simplified)
    write(1, "Result\n", 7);
}
```

---

## Makefile Targets

```bash
make           # Show status and options
make info      # Show PCC configuration details
make install-pcc  # Install PCC to /lib/ (requires sudo)
make try-compile  # Try to compile C file
make asm       # Assemble from .s file (if you write it)
make clean     # Clean generated files
```

---

## Why PCC Isn't Ready

**Root Cause:** The PCC compiler driver has hardcoded paths baked in at compile time.

**Hardcoded paths in cc:**
- `/lib/pcc1` - Compiler pass 1 (parsing, optimization)
- `/lib/pcc2` - Compiler pass 2 (code generation)
- `/lib/cpp` - C preprocessor

**Actual locations:**
- `../../bin/cc1` - Compiler pass 1
- `../../bin/cc2` - Compiler pass 2

**Solution:** Either install system-wide or rebuild PCC with configurable paths.

---

## Recommended Workflow

### For Now (Assembler Works!)
1. Write ND-500 assembly in `.s` files
2. Use the working `nd500-as` assembler
3. Test with working toolchain

### When PCC is Configured
1. Run `make install-pcc` (one time)
2. Compile C files: `../../bin/cc -c file.c`
3. Link: `../../bin/nd500-ld file.o -o file`

---

## Related Documentation

- `../../docs/toolchain/compiler/` - PCC compiler documentation
- `../../README.md` - Main project README
- `../../TOOLCHAIN_STATUS.md` - Toolchain component status (if exists)

---

*Created: 2025-01-11*  
*Status: Assembler works, C compiler needs configuration*

