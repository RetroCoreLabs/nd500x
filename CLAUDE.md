# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ND500X is a Norsk Data ND-500 CPU emulator written in C (C11). It emulates the ND-500 architecture with byte-addressed memory, CPU state, and provides both native and WebAssembly (WASM) build targets.

## Build Commands

### Native Build
```bash
# Using CMake directly
mkdir -p build && cd build && cmake .. && make

# Using Makefile wrapper
make

# Run the emulator in debug mode
./build/bin/nd500x --debug

# Or via Makefile
make run
```

### WebAssembly Build
```bash
# Using CMake directly
mkdir -p build_wasm && cd build_wasm && emcmake cmake -DBUILD_WASM=ON .. && make

# Using Makefile wrapper
make wasm
```

### Clean
```bash
make clean          # Clean native build
make wasm-clean     # Clean WASM build
```

## Important Build Notes

- **In-source builds are forbidden**: CMake will error if you try to build in the source directory
- **Build output locations**: Binaries go to `build/bin/`, libraries to `build/lib/`
- **Instruction code generation**: The build system auto-generates instruction tables from `build/src/cpu/instructions.json` via the `gen_instructions` tool (native builds only)
  - Generates: `build/include/nd500_instructions_gen.h` and `build/nd500_instructions_gen.c`
  - This step runs automatically during CMake build via custom commands

## Architecture & Code Organization

### Core Components

**src/ndlib/** - Logging and utilities
- `ndlib.c/h`: Logging infrastructure
- `ndlib_aout.c`: a.out format loader
- `ndlib_symbols.c`: Symbol table support

**src/machine/** - Machine state and unified debugger API
- `machine.c/h`: Byte-addressed memory, bus interface, machine state
- `machine_loader.c`: Binary loading
- `debug_api.c`: Unified debug API exposing memory, disassembly, registers
- `io.c`: I/O operations
- `machine.c`: Background run loop for native builds (pthread-based)

**src/cpu/** - CPU core
- `cpu.c`: CPU state management (registers: PC, FLAGS, I[4], A[4], E[4], L, B, R, TOS, etc.)
- `cpu_instr.c`: Instruction decoding and operand parsing
  - Uses generated instruction tables from `build/include/nd500_instructions_gen.h`
  - Implements ND-500 addressing mode classification (14 modes including CONSTANT_SHORT, LOCAL, RECORD, ABSOLUTE, REGISTER, PREINDEXED, etc.)
  - `nd500_decode_at()`: Full instruction decoder with operand parsing
- `cpu_protos.h`: CPU function prototypes

**src/debugger/** - Interactive CLI debugger
- `debugger.c`: REPL with commands: `m` (memory), `d` (disassemble), `step`, `regs`, `load`, `run`, `stop`, `dap`, `q`
- `dap_adapter.c`: Debug Adapter Protocol support (requires external libdap)

**src/frontend/** - Entry points
- `nd500x/`: Native executable entry point
- `nd500wasm/`: WebAssembly entry with JSON debug exports

**tools/gen_instructions/** - Build-time code generator
- `gen_instructions.c`: Parses `instructions.json` to generate C code for instruction lookup tables
- Extracts: opcode, mnemonic, operandCount from JSON

### Key Data Flow

1. **Instruction Execution Loop**: Machine → CPU → Fetch (bus_read8) → Decode (cpu_instr.c) → Execute (step)
2. **Debug API**: Debugger → Unified API (debug_api.c) → Machine/CPU
3. **Code Generation**: instructions.json → gen_instructions → .h/.c files → compiled into nd500_cpu

### ND-500 Addressing Modes

The CPU implements 14 addressing modes identified by address codes (see `cpu_instr.c:66-84`):
- Short forms (2-bit prefix): CONSTANT_SHORT, LOCAL_SHORT, RECORD_SHORT
- Extended forms (0xC0-0xFF range): ABSOLUTE, REGISTER, LOCAL, RECORD, DESCRIPTOR, PREINDEXED, etc.
- Special: ALT prefix (0xC8), post-increment modes, indirect modes

Operands include address code byte + optional data part (1-8 bytes depending on mode).

## Dependencies

### Native Build
- **libcjson** (via pkg-config): Required for JSON output. If missing, build proceeds without JSON support.
- **Optional externals**:
  - `external/libdap/`: Debug Adapter Protocol library (enables `dap` debugger command)
  - `external/libsymbols/`: Symbol table support (enables symbol loading)

### WASM Build
- **cJSON**: Auto-fetched via CMake FetchContent from https://github.com/DaveGamble/cJSON.git

## CMake Options

- `BUILD_WASM=ON/OFF`: Build WebAssembly target (default: OFF)
- `DEBUGGER_ENABLED=ON/OFF`: Enable debugger support (default: ON, forced OFF for WASM)

## Platform Detection

- `PLATFORM_LINUX`, `PLATFORM_WINDOWS`, `PLATFORM_WASM` are set based on `CMAKE_SYSTEM_NAME`
- Native builds use pthread for background execution (`run` command)
- WASM builds export specific JS functions via Emscripten flags

## Debugger Commands

When running `./build/bin/nd500x --debug`, the REPL supports:

**Basic Commands:**
- `m [addr] [len]`: Display memory (hex dump)
- `d [addr] [len]`: Disassemble instructions
- `step [n]`: Single-step CPU (default n=1)
- `regs`: Display all CPU registers
- `load <path>`: Load a.out binary and symbols
- `run`: Start background execution thread
- `stop`: Stop background execution
- `continue`/`c`: Continue execution after breakpoint
- `symb`: List all symbols
- `help`: Show all commands
- `dap <port>`: Start DAP server (requires libdap, default port 47285)
- `q`/`quit`/`exit`: Quit debugger

**Breakpoint Commands:**
- `bp [addr]`: Set breakpoint at address (default: PC)
- `bp list`: List all breakpoints
- `bp del <id>`: Delete breakpoint
- `bp enable <id>`: Enable breakpoint
- `bp disable <id>`: Disable breakpoint

**Watchpoint Commands:**
- `wp <addr> [len] [type]`: Set watchpoint (type: read, write, change)
- `wp list`: List all watchpoints
- `wp del <id>`: Delete watchpoint
- `wp enable <id>`: Enable watchpoint
- `wp disable <id>`: Disable watchpoint

## Code Style Notes

- C11 standard with standard compliance required
- No Python/JS/TypeScript - pure C project
- Generated files marked with `AUTO-GENERATED FILE - DO NOT EDIT` header
- Instruction JSON is ~424KB and contains ND-500 architecture instruction set definitions
