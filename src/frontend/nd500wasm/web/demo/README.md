# ND-500 Web Debugger Demo Files

This directory contains demo files for the ND-500 web debugger.

## kernel.zip - Complete Debug Package

The `kernel.zip` file contains a fully debuggable simulated Unix kernel with source-level debugging support.

**Contents:**
- `kernel` - Binary executable (ND-500 a.out format)
- `kernel.c` - C source code (8.4 KB)
- `kernel.s` - Generated assembly source (10.5 KB)
- `kernel.map` - Map file with line-to-address mappings for both C and assembly

**Usage:**

1. Open the ND-500 web debugger in your browser
2. Click the "📝 Source Files" button in the header
3. In the "ZIP Bundle" section, select `kernel.zip`
4. Click "Upload"

The debugger will automatically:
- Load the kernel executable into memory
- Load both C and assembly source files
- Load the map file for source-to-address mapping
- Enable source-level debugging features

**Source-Level Debugging Features:**

Once loaded, you can:
- **View source code** - Switch to the "Assembly" or "C" tabs to see source code
- **Set breakpoints** - Click any line in the source view to set a breakpoint
- **Step through code** - The current line highlights automatically as you step
- **See both C and assembly** - The map file maps both C source lines and assembly lines to memory addresses

**CLI Commands:**

You can also set breakpoints from source locations using:
```
bp source kernel.c 42    # Set breakpoint at C source line 42
bp source kernel.s 100   # Set breakpoint at assembly line 100
```

**Building from Source:**

The kernel.zip is built from `/examples/05-c-kernel/`:
```bash
cd examples/05-c-kernel
make clean
make all
```

This creates:
- `kernel.c` → `kernel.s` (C compilation with debug info)
- `kernel.s` → `kernel.o` + `kernel.map` (assembly with map generation)
- `kernel.o` → `kernel` + updated `kernel.map` (linking with relocation)
- All files packaged into `kernel.zip`

## kernel - Standalone Binary

The standalone `kernel` file is provided for quick testing without source debugging.
Use the "Load a.out" option to load this file directly.
