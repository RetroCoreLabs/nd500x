# Example 05: Simulated Unix Kernel with PSEG/DSEG Split

This example demonstrates the complete NDIX-C kernel build process for the ND-500 architecture, including splitting executables into separate instruction (PSEG) and data (DSEG) segments.

## Overview

The ND-500 uses a **Harvard-style architecture** with separate instruction and data address spaces. This example shows how to:

1. Compile a C kernel to ND-500 assembly
2. Assemble to object format
3. Link with OMAGIC format (required for segment splitting)
4. Split the executable into PSEG (text) and DSEG (data+bss) segments

This matches the exact build process used for the historical NDIX-C kernel (Release 3, 1988).

## Files

| File | Description |
|------|-------------|
| `kernel.c` | Simulated Unix kernel in C |
| `Makefile` | Complete build pipeline |
| `README.md` | This file |
| `kernel.s` | Generated assembly (build artifact) |
| `kernel.o` | Object file (IMAGIC format) |
| `kernel` | Linked kernel (OMAGIC format with symbols) |
| `kernel.pseg` | Program segment (instruction/text) |
| `kernel.dseg` | Data segment (data + bss) |

## Quick Start

```bash
# Build everything (default)
make

# Or step by step
make compile   # C → assembly
make assemble  # assembly → object
make link      # object → kernel
make split     # kernel → PSEG/DSEG

# Inspect the results
make inspect

# Run full test
make test

# See detailed demo
make demo
```

## Build Process

### Step 1: Compile C to Assembly

```bash
$ bin/cc -S kernel.c -o kernel.s
```

The PCC compiler generates ND-500 assembly code from C source.

**Output**: `kernel.s` (assembly language)

### Step 2: Assemble to Object File

```bash
$ bin/nd500-as kernel.s -o kernel.o
```

The assembler creates an object file in IMAGIC (0411) format.

**Output**: `kernel.o` (relocatable object, IMAGIC format)

### Step 3: Link with OMAGIC Format

```bash
$ bin/nd500-ld -i -e start -x -o kernel kernel.o
```

**Linker flags:**
- `-i` : **Impure format** (OMAGIC 0407) - text segment is not write-protected
- `-e start` : Entry point is the `start` symbol (not `main`)
- `-x` : **Preserve local symbols** for debugging
- `-o kernel` : Output filename

**Why OMAGIC?**

The splitseg tool requires OMAGIC format (magic number 0407). This format:
- Does not write-protect the text segment
- Is required for the PSEG/DSEG split operation
- Matches the format used by NDIX-C kernel builds

**Output**: `kernel` (executable in OMAGIC format with full symbol table)

### Step 4: Split into PSEG and DSEG

```bash
$ bin/nd500-splitseg kernel
```

The splitseg utility extracts:
- **PSEG** (kernel.pseg): Text segment (executable instructions)
- **DSEG** (kernel.dseg): Data segment (initialized data + BSS)

Both segments are padded to 2KB (2048 byte) boundaries to match ND-500 page size.

**Output**:
- `kernel.pseg` (instruction segment)
- `kernel.dseg` (data segment)

## Memory Layout

The simulated kernel demonstrates NDIX-C memory layout:

```
Virtual Address Space:
  0x08000000 - _Textbase  - Kernel text (code) loaded from PSEG
  0x10000000 - _Physbase  - Data/BSS loaded from DSEG
  0x18000000 - _Sysbase   - System data structures
  0x20000000 - _usrpt     - User process page tables
  0x30000000 - _sharebase - Shared memory with ND-100 front-end
  0x38000000 - _rawbuf    - No-cache raw I/O buffers
  0xE8000000 - _u         - Current process U-area (8KB)
  0xF0000000 - _Udata     - User data mapping
  0xF8000000 - _Ustack    - User stack mapping
```

## Kernel Components

The simulated kernel includes:

### Data Structures (in DSEG)
- **Process table** (100 entries)
- **File table** (128 entries)
- **Inode table** (64 entries)
- **Buffer cache** (32 buffers)

### Functions (in PSEG)
- `start()` - Kernel entry point
- `init_kernel()` - Initialize kernel subsystems
- `scheduler()` - Process scheduler
- `sys_read()`, `sys_write()`, `sys_exit()` - System calls
- `trap_handler()` - Trap/exception handler

## Segment Details

### PSEG (Program Segment)

Contains:
- Executable machine code
- Function implementations
- Read-only constants

Properties:
- Loaded to instruction memory
- Base address: 0x08000000
- Padded to 2KB boundaries
- No symbol table (stripped)

### DSEG (Data Segment)

Contains:
- Initialized data (`.data` section)
- Uninitialized data (`.bss` section as zeros)

Properties:
- Loaded to data memory
- Base address: 0x10000000
- BSS created as sparse file (efficient)
- Padded to 2KB boundaries
- No symbol table (stripped)

## a.out Format Details

### IMAGIC (Object Files)

Magic number: **0411** (octal) = 0x109 (hex)

Used for:
- Intermediate object files (`kernel.o`)
- Contains relocations
- Contains symbol table
- Can be linked with other objects

### OMAGIC (Executable)

Magic number: **0407** (octal) = 0x107 (hex)

Used for:
- Final linked executable (`kernel`)
- Required by `nd500-splitseg`
- Text segment not write-protected
- Suitable for kernel builds

## Inspecting the Kernel

### View Object File

```bash
# Show header
$ bin/nd500-dump -h kernel.o

# Show symbols
$ bin/nd500-dump -s kernel.o

# Show relocations
$ bin/nd500-dump -r kernel.o

# Disassemble
$ bin/nd500-dis kernel.o
```

### View Linked Kernel

```bash
# Show header (should show OMAGIC)
$ bin/nd500-dump -h kernel

# Show symbols
$ bin/nd500-dump -s kernel

# Find specific symbols
$ bin/nd500-dump -s kernel | grep start
$ bin/nd500-dump -s kernel | grep init_kernel

# Disassemble
$ bin/nd500-dis kernel
```

### View Segments

```bash
# Check file sizes
$ ls -lh kernel.pseg kernel.dseg

# Check actual disk usage (sparse files)
$ du -h kernel.pseg kernel.dseg

# View in hex
$ hexdump -C kernel.pseg | head -20
$ hexdump -C kernel.dseg | head -20
```

## Testing

Run the complete test suite:

```bash
$ make test
```

This verifies:
- ✓ All build steps complete successfully
- ✓ All files are created
- ✓ Object file is IMAGIC (0411)
- ✓ Kernel is OMAGIC (0407)
- ✓ PSEG and DSEG files exist

## Advanced: Understanding the Split

The splitseg algorithm:

1. **Read a.out header** - Verify OMAGIC format
2. **Create PSEG**:
   - Seek to text offset in a.out
   - Copy `a_text` bytes
   - Pad to 2KB boundary (sparse file)
3. **Create DSEG**:
   - Seek to data offset in a.out
   - Copy `a_data` bytes (initialized data)
   - Extend file by `a_bss` bytes (sparse file for zeros)
   - Pad to 2KB boundary

Sparse file technique:
- Uses `lseek()` to skip ahead
- Writes one byte at the end
- Creates logical file size without allocating disk blocks
- Minimizes actual disk usage

## Comparison with NDIX-C

This example matches the NDIX-C kernel build:

| NDIX-C Kernel | This Example |
|---------------|--------------|
| `locore.c` + `basic.c` + ... | `kernel.c` |
| `make vmunix` | `make all` |
| `ld -i -e start -x ...` | Same flags |
| `splitseg vmunix` | `nd500-splitseg kernel` |
| `vmunix.pseg` | `kernel.pseg` |
| `vmunix.dseg` | `kernel.dseg` |

## Troubleshooting

### Error: "bad magic number"

**Problem**: splitseg reports bad magic number

**Solution**: Link must use `-i` flag for OMAGIC format:
```bash
$ bin/nd500-ld -i -e start -o kernel kernel.o
```

### Error: "cannot open file"

**Problem**: Input file not found

**Solution**: Verify file exists and path is correct:
```bash
$ ls -l kernel
$ make link  # Rebuild if needed
```

### Entry point not found

**Problem**: Linker can't find `start` symbol

**Solution**: The `start()` function is defined in `kernel.c`. Verify:
```bash
$ bin/nd500-dump -s kernel.o | grep start
```

## References

### NDIX-C Documentation

See `$NDIX/NDIX_BUILD_MEMORY_LAYOUT.md` for:
- Complete NDIX-C build system documentation
- Memory layout architecture
- splitseg algorithm details
- Original NDIX-C Makefile examples

### ND-500 Architecture

- **docs/architecture/ND500_COMPLETE_REFERENCE.md** - ND-500 architecture guide
- **docs/reference/nd500/instructions.md** - Instruction set reference
- **docs/reference/nd500/AOUT_FORMAT.md** - a.out file format

### Tools

- **src/nd500-splitseg/** - PSEG/DSEG splitter source
- **src/nd500-dump/** - Binary inspector
- **src/nd500-dis/** - Disassembler

## Notes

### K&R C Style

The kernel uses old-style (K&R) C function definitions:

```c
int sys_read(fd, buf, count)
int fd;
char *buf;
int count;
{
    ...
}
```

This matches the historical NDIX-C code style (pre-ANSI C, 1988).

### Symbol Preservation

The `-x` flag preserves local symbols in the kernel for debugging:
- Function names
- Global variables
- Static variables

These symbols are in the `kernel` a.out file but **not** in `.pseg` or `.dseg`.

### Page Alignment

The 2KB (2048 byte) padding matches:
- ND-500 page size
- NDIX-C memory management unit
- Efficient memory loading

## What's Next?

- **examples/04-c-math/** - C math compilation
- **examples/03-addressing-modes/** - ND-500 addressing modes
- **tests/1_compiler/** - Compiler test suite

## License

This code is part of the PCC-ND500 toolchain project.

Historical components (NDIX-C) are from Norsk Data systems (1988).

---

**Generated with PCC-ND500 Toolchain**
