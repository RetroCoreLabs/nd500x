# ND500X Emulator

> A Norsk Data ND-500 CPU emulator written in C11

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C Standard](https://img.shields.io/badge/C-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20WebAssembly-lightgrey.svg)](#platform-support)

## Overview

ND500X is an emulator for the Norsk Data ND-500 architecture, featuring:

- **Accurate CPU emulation** with full register set (PC, FLAGS, I/A/E registers, etc.)
- **Byte-addressed memory** with bus interface
- **14 addressing modes** including short forms, extended modes, and special modifiers
- **Interactive CLI debugger** with memory inspection, disassembly, and step-through execution
- **WebAssembly support** for browser-based emulation
- **Instruction code generation** from JSON specification (424KB+ instruction set)
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

## Usage

### Running the Emulator

```bash
./build/bin/nd500x --debug
```

### Debugger Commands

The interactive debugger REPL supports the following commands:

| Command | Description | Example |
|---------|-------------|---------|
| `m [addr] [len]` | Display memory as hex dump | `m 0x1000 256` |
| `d [addr] [len]` | Disassemble instructions with hex bytes | `d 0x1000 100` |
| `step [n]` / `s [n]` | Single-step CPU execution | `step 10` |
| `regs` | Display all CPU registers | `regs` |
| `load <path>` | Load ND-500 a.out binary with symbols | `load program.o` |
| `symb` | List all loaded symbols from file | `symb` |
| `run` | Start background execution | `run` |
| `stop` | Stop background execution | `stop` |
| `help` | Show available commands | `help` |
| `dap <port>` | Start DAP server (if libdap available) | `dap 47285` |
| `q` / `quit` / `exit` | Exit the debugger | `q` |

**Example Session:**
```
nd500x debug mode. Commands: m [addr [len]], d [addr [len]], step [n], regs, load <path>, run, stop, symb, dap <port>, help, q
[00000000] load math.o
File Type:      OBJECT FILE (needs linking)
Relocations:    text=24 data=0 bytes (not yet resolved)
Note:           Setting entry point to 0 (object files cannot execute)
loaded, entry=0x00000000
symbols loaded
[00000000] symb
=== SYMBOL TABLE ===
Total symbols: 4
String table size: 31 bytes

Idx  Name                 Type         Value      Desc
---  ----                 ----         -----      ----
0    _main                TEXT|EXT     0x00000026 0
1    _add                 TEXT|EXT     0x00000000 0
2    _write               UNDF|EXT     0x00000000 0
3    _sub                 TEXT|EXT     0x00000013 0
[00000000] d 0 50
00000000: B8 CF 1C 00 00 00       ents         $28
00000006: 1A 08 44                w move       $8,b.16
00000009: 0C 45                   w1 :=        b.20
0000000B: 54 46                   w1 +         b.24
[...]
[00000000] q
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

**Instruction Generation Pipeline:**
1. `instructions.json` (424KB+) defines the ND-500 instruction set
2. `gen_instructions` tool parses JSON at build time
3. Generates C code with instruction lookup tables
4. CPU uses generated tables for decoding and disassembly

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
| **WebAssembly** | ✅ Full support | Emscripten-based, debugger disabled |
| **macOS** | ⚠️ Untested | Should work (Unix-like) |

## Project Structure

```
nd500x/
├── src/
│   ├── ndlib/              # Logging and utilities
│   │   ├── ndlib.c         # Core logging
│   │   ├── ndlib_aout.c    # a.out binary loader
│   │   └── ndlib_symbols.c # Symbol table support
│   ├── machine/            # Machine state and debug API
│   │   ├── machine.c       # Memory, bus, run loop
│   │   ├── machine_loader.c
│   │   ├── debug_api.c     # Unified debug interface
│   │   └── io.c
│   ├── cpu/                # CPU core
│   │   ├── cpu.c           # Register state, reset, step
│   │   └── cpu_instr.c     # Instruction decoder, addressing modes
│   ├── debugger/           # Interactive debugger
│   │   ├── debugger.c      # REPL implementation
│   │   └── dap_adapter.c   # Debug Adapter Protocol
│   └── frontend/
│       ├── nd500x/         # Native entry point
│       └── nd500wasm/      # WebAssembly entry point
├── tools/
│   └── gen_instructions/   # Build-time code generator
│       └── gen_instructions.c
├── build/                  # Build output (created by CMake)
│   ├── bin/                # Executables
│   ├── lib/                # Libraries
│   ├── include/            # Generated headers
│   └── src/cpu/instructions.json  # Instruction set definition
├── CMakeLists.txt          # Main CMake configuration
├── Makefile                # Convenience wrapper
├── README.md
└── CLAUDE.md               # AI assistant guidance
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
