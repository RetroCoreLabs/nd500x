# ND-500 Assembly Examples

**Hands-on examples for learning the ND-500 toolchain**

Welcome! This directory contains practical examples to help you understand ND-500 assembly programming and the PCC-ND500 toolchain.

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Prerequisites](#prerequisites)
3. [Example 1: Hello](#example-1-hello)
4. [Example 2: Arithmetic](#example-2-arithmetic)
5. [Example 3: Addressing Modes](#example-3-addressing-modes)
6. [Tool Reference](#tool-reference)
7. [Common Workflows](#common-workflows)
8. [Troubleshooting](#troubleshooting)

---

## Quick Start

**Build and test all examples in 30 seconds:**

```bash
cd examples
make all        # Assemble all examples
make test       # Test all examples
make demo       # Interactive demonstration
```

**Try a single example:**

```bash
# Assemble
../bin/nd500-as 01-hello/hello.s -o 01-hello/hello.o

# Inspect object file
../bin/nd500-dump -a 01-hello/hello.o

# Disassemble
../bin/nd500-dis 01-hello/hello.o
```

---

## Prerequisites

### 1. Build the Toolchain

From the project root:

```bash
make all
```

This builds:
- `bin/nd500-as` - Assembler
- `bin/nd500-ld` - Linker
- `bin/nd500-dump` - Object file inspector
- `bin/nd500-dis` - Disassembler

### 2. Verify Tools

```bash
ls -lh bin/nd500-*
```

You should see all four tools (nd500-as, nd500-ld, nd500-dump, nd500-dis).

---

## Example 1: Hello

**Location:** `01-hello/hello.s`

**Description:** The simplest possible ND-500 program. Just a single `ret` instruction.

### Source Code

```asm
; hello.s - Simplest ND-500 program
.text
.global _start

_start:
    ret
```

### Complete Workflow

**Step 1: Assemble**

```bash
../bin/nd500-as 01-hello/hello.s -o 01-hello/hello.o
```

This creates `hello.o` (object file in a.out format).

**Step 2: Inspect Object File**

```bash
../bin/nd500-dump -a 01-hello/hello.o
```

**Expected Output:**
```
=== A.OUT HEADER ===
Magic number:   0x0109  IMAGIC (0411 - ND-500 specific)
Text size:      2 bytes (0x2)
Entry point:    0x4
```

**What this tells you:**
- Magic number 0x0109 = IMAGIC (object file format)
- Text size 2 bytes (the `ret` instruction is 1 byte, but alignment)
- Entry point at 0x4

**Step 3: Disassemble**

```bash
../bin/nd500-dis 01-hello/hello.o
```

**Expected Output:**
```
; ND-500 Disassembly
; File: 01-hello/hello.o
; Text size: 2 bytes
;
ret
```

You should see the `ret` instruction decoded back to assembly!

**Step 4: View with Addresses**

```bash
../bin/nd500-dis -a 01-hello/hello.o
```

**Expected Output:**
```
0000:  80                               ret
```

This shows the hex bytes (0x80 = ret opcode).

**Step 5: Link to Executable**

```bash
../bin/nd500-ld 01-hello/hello.o -o 01-hello/hello
```

**Note:** This may produce warnings about missing libraries. That's expected since we don't have a full runtime yet.

**Step 6: Inspect Executable**

```bash
../bin/nd500-dump -h 01-hello/hello
```

**Expected:**
```
Magic number:   0x010b  ZMAGIC (0413 - demand paged)
Text size:      2048 bytes (includes padding)
```

Notice the magic changed from IMAGIC (0x0109) to ZMAGIC (0x010b) - this indicates an executable instead of an object file.

---

## Example 2: Arithmetic

**Location:** `02-arithmetic/`

**Files:**
- `add.s` - Addition operation
- `multiply.s` - Multiplication operation

### Example 2A: Addition

**Source Code:**

```asm
; add.s
.text
.global _add

_add:
    w1 := b.20          ; Load from base+20 into W1
    w1 + b.24           ; Add base+24 to W1
    ret
```

**What this demonstrates:**
- Register variants (w1)
- Local addressing (b.20, b.24)
- Arithmetic operations (+)
- Implicit result storage (W1)

**Try it:**

```bash
# Assemble
../bin/nd500-as 02-arithmetic/add.s -o 02-arithmetic/add.o

# Inspect
../bin/nd500-dump -s 02-arithmetic/add.o

# Disassemble with hex bytes
../bin/nd500-dis -a 02-arithmetic/add.o
```

### Example 2B: Multiplication

**Source Code:**

```asm
; multiply.s
.text
.global _multiply

_multiply:
    w1 := b.16
    w2 := b.20
    w1 * r2             ; Multiply W1 by R2
    ret
```

**What this demonstrates:**
- Multiple registers (w1, w2)
- Register addressing mode (r2)
- Multiplication operation

---

## Example 3: Addressing Modes

**Location:** `03-addressing-modes/`

**Files:**
- `modes.s` - Comprehensive addressing mode demo
- `constants.s` - Constant format variations

### Example 3A: Addressing Modes

**Source:** `modes.s`

This file demonstrates all 13 addressing modes supported by ND-500. Each mode is commented for clarity.

**Try it:**

```bash
# Assemble
../bin/nd500-as 03-addressing-modes/modes.s -o 03-addressing-modes/modes.o

# Disassemble with addresses to see encoding
../bin/nd500-dis -a 03-addressing-modes/modes.o
```

**What to observe:**
- Different hex byte patterns for each mode
- Short vs full encodings
- Data part sizes (1, 2, or 4 bytes)

### Example 3B: Constants

**Source:** `constants.s`

Shows how the assembler encodes different constant values.

**Try it:**

```bash
# Assemble
../bin/nd500-as 03-addressing-modes/constants.s -o 03-addressing-modes/constants.o

# Disassemble
../bin/nd500-dis -a 03-addressing-modes/constants.o
```

**What to look for:**
- Values ≤ 63: Use constant short (1 byte total)
- Values > 63: Use full constant mode (5+ bytes)
- Efficient encoding by assembler

---

## Tool Reference

### nd500-as (Assembler)

**Convert assembly to object file:**

```bash
nd500-as source.s -o output.o
```

**Options:**
- Input: `.s` file (assembly source)
- Output: `.o` file (a.out object format)

**Warnings:** Sanitizer warnings about array bounds are normal and non-fatal.

### nd500-dump (Object Inspector)

**View metadata of object files or executables:**

```bash
nd500-dump [options] file.o
```

**Options:**
- `-h` - Header information
- `-s` - Symbol table
- `-r` - Relocation information
- `-t` - String table
- `-x` - Hex dump of text and data sections
- `-a` - All metadata (equivalent to `-hsrt`)

**Examples:**
```bash
nd500-dump -a program.o          # Show all metadata
nd500-dump -h program.o          # Just header
nd500-dump -s program.o          # Just symbols
nd500-dump -x program.o          # Hex dump of code/data
nd500-dump -hx program.o         # Header + hex dump
```

**Hex Dump Format:**
The `-x` option displays machine code in hexadecimal:
```
00000000  1a 08 c1 10 fc 0a c1 18  1a 0c c1 14 1a 10 c1 1c  |................|
00000010  30 00 f0 8b 18 00                                 |0.....|
```
- Address column shows byte offset
- 16 bytes per line with ASCII representation
- Useful for verifying generated machine code

### nd500-dis (Disassembler)

**Convert object file back to assembly:**

```bash
nd500-dis [options] file.o
```

**Options:**
- `-a` - Show addresses and hex bytes
- `-s offset` - Start at hex offset
- `-n count` - Disassemble N instructions

**Examples:**
```bash
nd500-dis program.o              # Basic disassembly
nd500-dis -a program.o           # With hex bytes
nd500-dis -s 10 -n 5 program.o   # 5 instructions from offset 0x10
```

### nd500-ld (Linker)

**Link object files to executable:**

```bash
nd500-ld input.o -o output
```

**Options:**
- Input: One or more `.o` files
- Output: Executable (a.out format)

**Note:** May produce warnings about missing libraries (expected without full runtime).

---

## Common Workflows

### Workflow 1: Assemble and Inspect

```bash
# 1. Write assembly (use any editor)
vim myprogram.s

# 2. Assemble
../bin/nd500-as myprogram.s -o myprogram.o

# 3. Check if it worked
../bin/nd500-dump -h myprogram.o

# 4. View symbols
../bin/nd500-dump -s myprogram.o

# 5. Disassemble to verify
../bin/nd500-dis myprogram.o
```

### Workflow 2: Complete Build

```bash
# 1. Assemble
../bin/nd500-as source.s -o program.o

# 2. Inspect object
../bin/nd500-dump -a program.o

# 3. Link
../bin/nd500-ld program.o -o program

# 4. Inspect executable
../bin/nd500-dump -a program

# 5. Disassemble executable
../bin/nd500-dis program
```

### Workflow 3: Debug Encoding

```bash
# Assemble with known instruction
echo -e ".text\nret" > test.s
../bin/nd500-as test.s -o test.o

# See hex bytes
../bin/nd500-dis -a test.o

# Expected: 
# 0000:  80                               ret
# (0x80 is the opcode for ret)
```

### Workflow 4: Compare Assembly Output

```bash
# Your source
../bin/nd500-as mysource.s -o mine.o
../bin/nd500-dis mine.o > mine_disasm.s

# Compare with original
diff mysource.s mine_disasm.s
```

---

## Understanding the Output

### Object File vs Executable

**Object File (.o):**
- Magic: 0x0109 (IMAGIC)
- Relocatable
- Contains symbols and relocations
- Not directly executable

**Executable:**
- Magic: 0x010b (ZMAGIC)
- Demand-paged
- Symbols may be stripped
- Relocations resolved

### Symbol Types

When you run `nd500-dump -s`, you'll see:

| Type | Description | Example |
|------|-------------|---------|
| TEXT | Code symbol | Function name |
| DATA | Data symbol | Variable |
| BSS | Uninitialized | Global var |
| EXT | External | Imported symbol |

### Addressing Modes in Output

When disassembling, you'll see these formats:

| Syntax | Mode | Example |
|--------|------|---------|
| `$8` | Constant short | Immediate value 0-63 |
| `$100` | Constant full | Immediate value > 63 |
| `b.20` | Local | Base + displacement |
| `r1` | Register | Direct register |
| `@b.20` | Local indirect | Pointer dereference |
| `b.20+` | Local post-indexed | Auto-increment |

---

## Troubleshooting

### Problem: "runtime error: index -1 out of bounds"

**What it means:** Sanitizer warning from the assembler (non-fatal)

**Solution:** Ignore these warnings. They're from the yacc-generated parser and don't affect output.

**To hide warnings:**
```bash
../bin/nd500-as source.s -o output.o 2>/dev/null
```

### Problem: Disassembler shows "???"

**What it means:** Unknown opcode

**Possible causes:**
1. Corrupted object file
2. Reading wrong offset
3. Not actually an a.out file

**Solution:** Verify with `nd500-dump -h` first.

### Problem: Linker warnings about libraries

**What it means:** Missing C runtime library

**Solution:** This is expected. We don't have a full C library yet. The object files are still valid.

### Problem: "Bad magic number"

**What it means:** File is not a valid a.out format

**Solution:** 
1. Check file was assembled correctly
2. Verify with `hexdump -C file.o | head -1`
3. First bytes should be: `09 01` (IMAGIC in little-endian)

---

## Learning Resources

### ND-500 Architecture Documentation

Located in `docs/architecture/`:

1. **ND500_COMPLETE_REFERENCE.md** - Architecture overview (608 lines)
   - All 241 instructions
   - Register descriptions
   - Addressing modes
   - Instruction categories

2. **ND500_INSTRUCTION_DECODE.md** - Decoding guide (1,409 lines)
   - How instructions are encoded
   - Step-by-step decoding examples
   - All addressing modes explained
   - Implementation reference

3. **ND500_OPCODE_REFERENCE.md** - Complete opcode table (6,349 lines)
   - All 2,551 opcode variants
   - Hex and octal values
   - Metadata for each instruction

### Tool Documentation

- **docs/toolchain/tools/ND500-DUMP.md** - nd500-dump guide
- **bin/README.md** - Binary reference
- **tools/README.md** - Python helper scripts

---

## Example Walkthroughs

### Walkthrough: 01-hello/hello.s

**Step 1: View the source**

```bash
cat 01-hello/hello.s
```

```asm
.text
.global _start

_start:
    ret
```

**Step 2: Assemble it**

```bash
../bin/nd500-as 01-hello/hello.s -o 01-hello/hello.o
```

**Step 3: What did we create?**

```bash
../bin/nd500-dump -a 01-hello/hello.o
```

Output shows:
- **Header:** File type, sizes, entry point
- **Symbols:** `_start` symbol defined
- **Layout:** Where data is in the file

**Step 4: Reverse it**

```bash
../bin/nd500-dis 01-hello/hello.o
```

You should see `ret` - we got back our original instruction!

**Step 5: See the bytes**

```bash
../bin/nd500-dis -a 01-hello/hello.o
```

Output: `0000:  80                               ret`

- Address: 0000
- Hex byte: 80
- Instruction: ret

**What we learned:**
- `ret` instruction = opcode 0x80
- Instructions are 1-2 bytes
- Object files have headers and metadata

---

### Walkthrough: 02-arithmetic/add.s

**The source:**

```asm
.text
.global _add

_add:
    w1 := b.20          ; Load from base+20 into W1
    w1 + b.24           ; Add base+24 to W1
    ret
```

**Assemble and disassemble:**

```bash
../bin/nd500-as 02-arithmetic/add.s -o 02-arithmetic/add.o
../bin/nd500-dis -a 02-arithmetic/add.o
```

**What to observe:**

1. **w1 := b.20** - Register variant instruction
   - Opcode for w1 variant
   - Address code for local mode
   - Displacement value (20)

2. **w1 + b.24** - Arithmetic operation
   - Addition opcode with w1 variant
   - Local addressing mode
   - Displacement (24)

3. **Different instruction lengths** - Some are 2 bytes, some 3 bytes, depending on addressing mode

**Experiment:**

Try changing values and reassembling:
```bash
# Edit add.s to use different displacements
# Reassemble and see how the hex bytes change
```

---

### Walkthrough: 03-addressing-modes/modes.s

**This is the most educational example!**

It demonstrates all major addressing modes:

```bash
../bin/nd500-as 03-addressing-modes/modes.s -o 03-addressing-modes/modes.o
../bin/nd500-dis -a 03-addressing-modes/modes.o
```

**What you'll see:**

```
0000:  1a 08 c1 10                      w move       $8,b.16
0004:  0c c1 14                         w1 :=        b.20
0007:  0c d1                            w1 :=        r2
...
```

**Observations:**

1. **Constant short ($8)** - Only 1 byte (0x08)
2. **Local byte (b.16)** - 2 bytes (0xC1 0x10)
3. **Register (r2)** - 1 byte (0xD1)
4. **Different encodings** - Short vs full modes

**Key insight:** The assembler chooses the most compact encoding automatically!

---

## Tool Reference

### Quick Command Reference

```bash
# Assemble
nd500-as source.s -o program.o

# Inspect header
nd500-dump -h program.o

# Inspect symbols
nd500-dump -s program.o

# Inspect everything
nd500-dump -a program.o

# Disassemble
nd500-dis program.o

# Disassemble with hex
nd500-dis -a program.o

# Link
nd500-ld program.o -o program

# Inspect executable
nd500-dump -a program
nd500-dis program
```

### Understanding Disassembly Output

**Format:**
```
[address]: [hex bytes]  [mnemonic] [operands]
```

**Example:**
```
0000:  1a 08 c1 10     w move       $8,b.16
```

- **0000** - Offset in text segment
- **1a 08 c1 10** - Instruction bytes (little-endian)
- **w move** - Mnemonic with data type prefix
- **$8,b.16** - Operands

### Understanding nd500-dump Output

**Header section:**
- Magic number (file type)
- Segment sizes (text, data, bss)
- Entry point

**Symbol section:**
- Symbol name
- Type (TEXT/DATA/BSS/EXT)
- Address
- Flags

**Relocation section:**
- Address to relocate
- Symbol reference
- Relocation type

---

## Common Workflows

### Create and Test New Example

```bash
# 1. Create new .s file
cat > mytest.s << 'EOF'
.text
.global _test
_test:
    w1 := $42
    ret
EOF

# 2. Assemble
../bin/nd500-as mytest.s -o mytest.o

# 3. Verify
../bin/nd500-dump -a mytest.o
../bin/nd500-dis -a mytest.o

# 4. Check hex encoding
hexdump -C mytest.o | head -5
```

### Compare Different Encodings

```bash
# Test constant short vs full
echo -e ".text\nw1 := \$5\nret" > short.s
echo -e ".text\nw1 := \$100\nret" > full.s

../bin/nd500-as short.s -o short.o
../bin/nd500-as full.s -o full.o

# Compare sizes
ls -l short.o full.o

# Compare disassembly
../bin/nd500-dis -a short.o
../bin/nd500-dis -a full.o
```

### Understand Instruction Encoding

```bash
# Pick an instruction
echo -e ".text\nret" > test.s
../bin/nd500-as test.s -o test.o

# See the exact bytes
../bin/nd500-dis -a test.o

# Look up in documentation
# docs/architecture/ND500_OPCODE_REFERENCE.md
```

---

## Advanced Usage

### Generate Custom Instruction Table

If you modify the assembler instruction set:

```bash
cd ../tools

# Regenerate lookup table
python3 extract_instructions.py \
    ../src/nd500-as/instrtab.h \
    ../src/nd500-as/codetab.h \
    ../src/nd500-dis/nd500_instructions.h

# Rebuild disassembler
make -C ../src/nd500-dis clean all

# Test
cd ../examples
../bin/nd500-dis -a 01-hello/hello.o
```

---

## Practice Exercises

### Exercise 1: Modify Hello

Modify `01-hello/hello.s` to use a different instruction. Try:
```asm
.text
_start:
    nop         ; No operation
    ret
```

Assemble, disassemble, and verify both instructions appear.

### Exercise 2: Create Subtraction Example

Create `02-arithmetic/subtract.s`:
```asm
.text
.global _subtract

_subtract:
    w1 := b.20
    w1 - b.24       ; Subtract instead of add
    ret
```

Questions:
1. What opcode is used for subtraction?
2. How does it differ from addition?
3. What are the hex bytes?

### Exercise 3: Explore Addressing Modes

Modify `modes.s` to try:
- Post-indexed mode: `b.20+`
- Pre-indexed mode: `r1.(20)`
- Different register variants: `w2`, `w3`, `w4`

Use `nd500-dis -a` to see how each encodes.

---

## Next Steps

After working through these examples:

1. **Read the architecture docs:**
   - `docs/architecture/ND500_COMPLETE_REFERENCE.md`
   - Start with the instruction list and addressing modes

2. **Study the opcode reference:**
   - `docs/architecture/ND500_OPCODE_REFERENCE.md`
   - Look up instructions you use

3. **Understand decoding:**
   - `docs/architecture/ND500_INSTRUCTION_DECODE.md`
   - Learn how bytes map to instructions

4. **Explore test files:**
   - `tests/2_assembler/` has more complex examples
   - Real-world usage patterns

5. **Try writing programs:**
   - Loops, conditionals, function calls
   - See what works and what doesn't

---

## Getting Help

### Documentation

- **Main README:** `../README.md`
- **Architecture:** `../docs/architecture/`
- **Toolchain Status:** `../TOOLCHAIN_STATUS.md`

### Quick Tips

**Tip 1:** Always use `nd500-dump -a` to inspect files before disassembling.

**Tip 2:** The `-a` flag for nd500-dis is invaluable for understanding encoding.

**Tip 3:** Sanitizer warnings from nd500-as are normal (don't worry about them).

**Tip 4:** Short modes (constant short, local short) save bytes - use them!

**Tip 5:** Check the docs for all 241 available instructions.

---

## Summary

This examples directory provides:

- ✅ **5 tutorial examples** - Progressive learning
- ✅ **Hands-on commands** - Copy-paste ready
- ✅ **Complete workflows** - Assemble → Inspect → Disassemble → Link
- ✅ **Tool reference** - Quick command lookup
- ✅ **Exercises** - Practice what you learned

**Ready to start?** Run `make demo` and follow along!

---

## See Also

- **Main Project:** `../README.md`
- **ND-500 Reference:** `../docs/architecture/ND500_COMPLETE_REFERENCE.md`
- **Disassembler:** `../docs/architecture/ND500_INSTRUCTION_DECODE.md`
- **Build System:** `../Makefile`

---

Happy coding with ND-500! 🚀

