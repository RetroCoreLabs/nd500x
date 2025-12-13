# ND500X Emulator

> A Norsk Data ND-500 CPU emulator written in C11

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C Standard](https://img.shields.io/badge/C-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20WebAssembly-lightgrey.svg)](#platform-support)

## Overview

ND500X is an emulator for the Norsk Data ND-500 architecture, featuring:

- **Accurate CPU emulation** with full register set (PC, FLAGS, I/A/E registers, etc.)
- **O(1) instruction dispatch** with opcode-indexed function pointer table for million-per-second execution
- **Modular instruction architecture** with 241 functions organized in 13 class-based directories
- **Byte-addressed memory** with bus interface
- **14 addressing modes** including short forms, extended modes, and special modifiers
- **Interactive CLI debugger** with memory inspection, disassembly, and step-through execution
- **Professional tab completion** with context-aware command and subcommand completion
- **Command history** with arrow key navigation and persistent storage
- **Breakpoints and Watchpoints** for advanced debugging (PC-based, memory read/write/change)
- **Advanced debugging features** including conditional breakpoints, instruction tracing, performance profiling, and call stack tracking
- **WebAssembly support** for browser-based emulation
- **Instruction code generation** from JSON specification (1078 instruction variants, 241 unique functions)
- **Debug Adapter Protocol** (DAP) support for IDE integration (optional)

## Table of Contents

- [Quick Start](#quick-start)
- [Building](#building)
  - [Native Build](#native-build)
  - [WebAssembly Build](#webassembly-build)
- [Usage](#usage)
- [Architecture](#architecture)
- [Dependencies](#dependencies)
- [Platform Support](#platform-support)
- [Project Structure](#project-structure)
- [Contributing](#contributing)
- [License](#license)

## Quick Start

```bash
# Clone the repository
git clone https://github.com/HackerCorpLabs/nd500x.git
cd nd500x

# Build and run (using Makefile wrapper)
make
make run

# Or build with CMake directly
mkdir -p build && cd build
cmake ..
make
./bin/nd500x --debug
```

## Building

### Prerequisites

**Native Build:**
- CMake 3.16+
- C11-compatible compiler (GCC, Clang, MSVC)
- libcjson (via pkg-config) - optional, for JSON output
- pthread (Linux/Unix)

**WebAssembly Build:**
- Emscripten SDK
- CMake 3.16+

### Native Build

Using the Makefile wrapper:
```bash
make            # Build the project
make run        # Build and run with --debug flag
make clean      # Clean build artifacts
```

Using CMake directly:
```bash
mkdir -p build
cd build
cmake ..
make
```

The native build will:
1. Auto-generate instruction tables from `instructions.json`
2. Compile all libraries and the main executable
3. Output binary to `build/bin/nd500x`

**Note:** In-source builds are not allowed. Always use a separate build directory.

### WebAssembly Build

Using the Makefile wrapper:
```bash
make wasm           # Build WASM target
make wasm-clean     # Clean WASM build
```

Using CMake directly:
```bash
mkdir -p build_wasm
cd build_wasm
emcmake cmake -DBUILD_WASM=ON ..
make
```

The WASM build automatically:
- Fetches cJSON via CMake FetchContent
- Exports JavaScript-callable functions for debugging
- Configures Emscripten with proper memory and export settings
- Copies web interface files to build output

### WebAssembly Debugger

The WASM build includes a complete web-based debugger interface:

**Build and Run:**
```bash
make wasm-serve    # Build WASM and start web server
```

This will:
1. Build the WebAssembly version with debugger support
2. Start a local web server on http://localhost:8000
3. Open your browser to use the debugger

**Web Debugger Features:**
- **File Upload**: Load .o/.out files via file picker or drag-and-drop
- **Modern Three-Panel Layout**: Disassembly (left), registers/TRAP/memory (right)
- **JSON-Based Disassembly**: Structured output with address, bytes, mnemonic, operands
- **Interactive Registers**: Click any register to edit value (hex or decimal input)
- **PC-Synchronized Disassembly**: Changing PC automatically updates disassembly view
- **TRAP Information Panel**: Real-time trap display with clear traps button
- **Clickable Memory Addresses**: Click addresses to navigate memory view
- **Enhanced Breakpoints**: Click gutter to toggle, visual dots without content shifting
- **Color-Coded Disassembly**: ANSI-inspired colors (blue mnemonics, magenta branches)
- **Step/Run Controls**: Single-step execution or continuous running
- **Memory Viewer**: Hex dump with ASCII representation
- **Real-time Updates**: Live register, trap, and disassembly updates during execution

**Browser Compatibility:**
- Modern browsers with WebAssembly support (Chrome 57+, Firefox 52+, Safari 11+)
- No external dependencies - pure vanilla JavaScript
- Responsive design works on desktop and mobile

**Usage:**
1. Run `make wasm-serve`
2. Open http://localhost:8000 in your browser
3. Load a .o or .out file using the "Load File" button or drag-and-drop
4. Use Step/Run controls to execute code
5. **Click registers** to edit values (PC changes update disassembly)
6. **Click breakpoint gutter** (left of address) to toggle breakpoints
7. **Click memory addresses** to navigate to new memory locations
8. View traps in real-time and clear them with the clear button
9. Enjoy color-coded disassembly with proper spacing and alignment

## Usage

### Running the Emulator

```bash
./build/bin/nd500x --debug
```

### Command-Line Options

| Option | Description |
|--------|-------------|
| `--debug` | Start interactive debugger REPL |
| `-i <path>` | Load ND-500 a.out file at startup |
| `-ansi` | Force-enable ANSI color output (even when piped) |
| `-noansi` | Force-disable ANSI color output |
| `--disasm <len>` | Disassemble <len> bytes and exit |
| `--addr <addr>` | Start address for disassembly |
| `--hexdump <len>` | Hex dump <len> bytes and exit |

**Color Output:**
- By default, color output is automatically detected based on TTY and terminal type
- When output is redirected to a file or pipe, colors are automatically disabled
- Use `-ansi` to force colors on, or `-noansi` to force them off

**Color Scheme:**
- **Gray** - Memory addresses
- **Yellow** - Hex byte codes
- **Green** - Instructions (mov, add, sub, etc.)
- **Red** - Branch/jump instructions (go, if>=go, etc.)
- **White** - Operands and registers
- **Cyan** - Labels and symbols
- **Blue** - Comments

### Debugger Commands

The interactive debugger supports 60+ commands for memory inspection, disassembly, execution control, breakpoints, watchpoints, MMU control, MON call debugging, and more.

**For complete command reference, see: [`docs/DEBUGGER_COMMAND_REFERENCE.md`](docs/DEBUGGER_COMMAND_REFERENCE.md)**

#### Quick Reference

| Category | Commands |
|----------|----------|
| **Memory** | `m`, `mp`, `m!`, `msym` |
| **Disassembly** | `d`, `dis`, `dsym` |
| **Execution** | `step`/`s`, `run`, `stop`, `continue`/`c`, `status` |
| **Registers** | `regs`, `set` |
| **Loading** | `load`, `loaddom`, `loadmap`, `loadsrc` |
| **Symbols** | `symb`, `goto`, `segments` |
| **Breakpoints** | `bp` (set/list/del/enable/disable/cond/source) |
| **Watchpoints** | `wp` (set/list/del/enable/disable/reg) |
| **Display Options** | `show trace/memtrace/ea/hex/demangle/source/profile/trap/mmu` |
| **Profiling** | `profile show/reset` |
| **Call Stack** | `backtrace`/`bt`, `stackframe`/`sf` |
| **MMU Control** | `mmu`, `showmmu`, `phyladr`, `showcap`, `showpages`, `memmap`, `listpst`, `listpcb`, `dumppt` |
| **Domains** | `domains`, `domain`, `domverify` |
| **MON Calls** | `mon log/status/list/info/break` |
| **Utility** | `help`, `history`, `clear-traps`, `q`/`quit` |

#### Tab Completion and History

- **Tab completion**: Context-aware command and subcommand completion
- **Command history**: Arrow keys, persistent storage in `~/.nd500x_history`
- **History shortcuts**: `!!` (last), `!nnn` (by number), `!string` (search)

#### Example Session

```
$ ./build/bin/nd500x --debug
[00000000] loaddom /path/to/program.dom
PC set to start address: 0x08000004

[08000004] bp 0x08001000
Breakpoint 0 set at 0x08001000

[08000004] run
running...
Stopped: Breakpoint at 0x08001000

[08001000] regs
PC=08001000 FLAGS=00000000
I1/W1=00000000 ...

[08001000] show trace on
show trace: on

[08001000] step 5
0x08001000 C3 08 02 D4 47 00  call  $134403143,$0
...
Stepped 5 instructions

[08001006] q
```

### ND-500 A.out File Format Support

The emulator supports loading ND-500 a.out format files with full symbol table parsing:

**File Type Detection:**
- Automatically distinguishes between object files (.o) and executables
- Object files: Detected by presence of relocations or placeholder entry point (0x4)
- Executables: No relocations and valid entry point
- Object files are loaded but entry point is set to 0 (cannot execute unlinked code)

**Symbol Table:**
- Parses 24-byte symbol entries with proper padding
- Displays symbol types: TEXT|EXT, DATA|EXT, BSS|EXT, UNDF|EXT
- Shows symbol values, descriptors, and string table
- Use `symb` command to list all loaded symbols

**Disassembler Features:**
- Hex byte display for each instruction (e.g., `C2 5B 00 00 00  go $91`)
- Proper operand size detection (handles all size prefixes: bi, by, h, w, f, d)
- Correct decoding of complex instructions (branches, calls, extended addressing)
- Matches nd500-dis reference output

## Architecture

### Core Components

```
┌─────────────────────────────────────────────────┐
│                  Frontend                        │
│  (native: nd500x.c, WASM: main.c)               │
└────────────────┬────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────┐
│               Debugger                           │
│  (REPL, DAP adapter)                            │
└────────────────┬────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────┐
│            Machine (Debug API)                   │
│  (memory, bus, unified debug interface)         │
└──────┬──────────────────────────┬───────────────┘
       │                          │
       ▼                          ▼
┌─────────────┐           ┌─────────────────────┐
│    CPU      │           │      I/O & Loader   │
│  (state,    │           │    (machine_loader, │
│   decode,   │◄──────────┤     debug_api)      │
│   execute)  │           └─────────────────────┘
└─────────────┘
       │
       ▼
┌──────────────────────────────────────────────┐
│       Instruction Tables (Generated)          │
│  (nd500_instructions_gen.h/.c)               │
│  Built from instructions.json                │
└──────────────────────────────────────────────┘
```

### Key Features

**Instruction Dispatch Architecture:**
1. **O(1) Dispatch Table**: 65,536-entry sparse array indexed by opcode (1,078 populated entries)
2. **Modular Implementation**: 241 unique instruction functions organized by class:
   - ARITHMETIC (55), MOVE (48), SYSTEM (26), CALL (17), STRING (15), BRANCH (15)
   - FLOAT_MATH (25), COMPARE (4), BITFIELD (6), LOGICAL (6), SHIFT (5), CONTROL (7), IO (1)
3. **Auto-Generated Stubs**: Build-time code generator creates stub files (never overwritten)
4. **Fast Execution**: Function pointer dispatch enables million-per-second instruction execution
5. **Safe Fallback**: NULL dispatch entries trigger illegal instruction trap instead of crashes

**Instruction Dispatch Table:**
1. `instructions.json` (424KB+) defines the ND-500 instruction set
2. Pre-generated dispatch table (`src/cpu/nd500_instructions.{c,h}`) committed to repository
3. 241 instruction stubs organized in `src/cpu/instructions/<CLASS>/<FunctionName>.c`
4. Each stub includes documentation, operand helpers, and implementation notes
5. CPU uses O(1) opcode-indexed lookup for instant instruction execution

**ND-500 Addressing Modes (14 modes):**
- Short forms: CONSTANT_SHORT, LOCAL_SHORT, RECORD_SHORT
- Extended forms: ABSOLUTE, REGISTER, LOCAL, RECORD, DESCRIPTOR, PREINDEXED
- Special: ALT prefix (0xC8), post-increment modes, indirect addressing

**CPU Register Set:**
- PC (Program Counter), FLAGS
- I[4] (Index registers), A[4] (Address registers), E[4] (Extension registers)
- L (Level), B (Base), R (Return)
- Stack: TOS, LL, HL, THA
- Table registers: OTE1/2, CTE1/2, MTE1/2, TEMM1/2

## Dependencies

### Native Build

| Dependency | Required | Purpose | Installation |
|------------|----------|---------|--------------|
| **libcjson** | Optional | JSON output support | `apt install libcjson-dev` (Debian/Ubuntu)<br>`pkg-config --modversion cjson` |
| **pthread** | Yes (Unix) | Background execution thread | Built into libc on Linux/Unix |
| **external/libdap** | Optional | Debug Adapter Protocol | Clone into `external/libdap/` |
| **external/libsymbols** | Optional | Symbol table support | Clone into `external/libsymbols/` |

If libcjson is not found, the native build proceeds without JSON support.
If libreadline is not found, the debugger will work without tab completion and command history.

### WebAssembly Build

| Dependency | Required | Purpose | Installation |
|------------|----------|---------|--------------|
| **Emscripten SDK** | Yes | WASM compilation | [Get Emscripten](https://emscripten.org/docs/getting_started/downloads.html) |
| **cJSON** | Yes | JSON support | Auto-fetched via CMake FetchContent |

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| **Linux** | ✅ Full support | Tested on Ubuntu 20.04+ |
| **Windows** | ⚠️ Experimental | Native build via MSVC or MinGW |
| **WebAssembly** | ✅ Full support | Emscripten-based, web debugger interface |
| **macOS** | ⚠️ Untested | Should work (Unix-like) |

## Project Structure

```
nd500x/
├── src/
│   ├── ndlib/              # Logging and utilities
│   │   ├── ndlib.c         # Core logging
│   │   ├── ndlib_aout.c    # a.out binary loader
│   │   └── ndlib_symbols.c # Symbol table support
│   ├── disasm/             # Disassembly module
│   │   ├── nd500_disasm.c  # JSON-based disassembly formatter
│   │   └── nd500_disasm.h  # Disassembly API
│   ├── machine/            # Machine state and debug API
│   │   ├── machine.c       # Memory, bus, run loop
│   │   ├── machine_loader.c
│   │   ├── debug_api.c     # Unified debug interface
│   │   ├── breakpoints.c   # Breakpoint management
│   │   └── io.c
│   ├── cpu/                # CPU core
│   │   ├── cpu.c           # Register state, reset, step, traps
│   │   ├── cpu_instr.c     # Instruction decoder, O(1) dispatch, addressing modes
│   │   ├── cpu_protos.h    # CPU API and operand access helpers
│   │   ├── nd500_instructions.c  # Pre-generated dispatch table (1,078 entries)
│   │   ├── nd500_instructions.h  # Dispatch table declarations
│   │   └── instructions/   # Instruction implementations (241 files in 13 categories)
│   │       ├── ARITHMETIC/ # 37 arithmetic operations (add, sub, mul, div, etc.)
│   │       ├── MOVE/       # 56 data transfer instructions
│   │       ├── SYSTEM/     # 35 system control instructions
│   │       ├── CALL/       # Call/return instructions
│   │       ├── STRING/     # String manipulation operations
│   │       ├── BRANCH/     # Conditional/unconditional branches
│   │       ├── FLOAT_MATH/ # Floating-point math operations
│   │       ├── COMPARE/    # Comparison instructions
│   │       ├── BITFIELD/   # Bit manipulation operations
│   │       ├── LOGICAL/    # Logical operations (and, or, xor, etc.)
│   │       ├── SHIFT/      # Shift and rotate instructions
│   │       ├── CONTROL/    # Control flow operations
│   │       └── IO/         # I/O instructions
│   ├── debugger/           # Interactive debugger
│   │   ├── debugger.c      # REPL implementation
│   │   └── dap_adapter.c   # Debug Adapter Protocol
│   └── frontend/
│       ├── nd500x/         # Native entry point
│       │   └── nd500x.c    # Main entry point for native builds
│       └── nd500wasm/      # WebAssembly entry point
│           ├── main.c      # WASM C interface (21 exported functions)
│           ├── tests/      # Unit tests for WASM
│           └── web/        # Web debugger interface
│               ├── index.html    # Main UI
│               ├── style.css     # Styling
│               ├── debugger.js   # Frontend logic
│               └── demo/
│                   └── kernel    # Demo ND-500 kernel binary
├── external/               # External dependencies (git submodules)
│   ├── libdap/             # Debug Adapter Protocol library (optional)
│   └── libsymbols/         # Symbol table support library (optional)
├── test/                   # Test suite
├── build/                  # Build output (created by CMake, not in git)
│   ├── bin/                # Executables (nd500x, nd500wasm.js/wasm)
│   └── lib/                # Compiled libraries
├── CMakeLists.txt          # Main CMake configuration
├── Makefile                # Convenience wrapper for build commands
├── README.md               # This file
└── CLAUDE.md               # AI assistant guidance for development
```

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_WASM` | OFF | Build WebAssembly target with Emscripten |
| `DEBUGGER_ENABLED` | ON | Enable debugger support (forced OFF for WASM) |

**Example:**
```bash
cmake -DBUILD_WASM=ON ..
cmake -DDEBUGGER_ENABLED=OFF ..
```

## Contributing

Contributions are welcome! Please follow these guidelines:

1. **Code Style**: Follow existing C11 conventions, use snake_case for functions
2. **Build**: Ensure both native and WASM builds succeed
3. **Testing**: Test debugger commands and instruction decoding
4. **Documentation**: Update README.md and CLAUDE.md for architectural changes

### Development Workflow

```bash
# Make changes
vim src/cpu/cpu.c

# Build and test
make clean
make
make run

# Test WASM build
make wasm-clean
make wasm
```

## License

[Specify your license here - e.g., MIT, GPL, Apache 2.0]

## Acknowledgments

- Norsk Data ND-500 architecture documentation
- Emscripten project for WebAssembly tooling
- cJSON library by DaveGamble

## Support

- **Issues**: [GitHub Issues](https://github.com/HackerCorpLabs/nd500x/issues)
- **Discussions**: [GitHub Discussions](https://github.com/HackerCorpLabs/nd500x/discussions)

---

**Status**: Active Development | **Version**: 0.1.0 (Early Scaffold)
