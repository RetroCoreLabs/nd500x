# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ND500X is a Norsk Data ND-500 CPU emulator written in C (C11). It emulates the ND-500 architecture with byte-addressed memory, CPU state, and provides both native and WebAssembly (WASM) build targets.

**Key Features:**
- Professional tab completion with context-aware command and subcommand completion
- Command history with arrow key navigation and persistent storage
- Advanced debugging features (conditional breakpoints, instruction tracing, performance profiling, call stack tracking)
- Interactive CLI debugger with memory inspection, disassembly, and step-through execution

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
- **Instruction dispatch table**: Pre-generated `src/cpu/nd500_instructions_gen.{c,h}` files are committed to the repository
  - Contains O(1) opcode-indexed dispatch table with 1,078 instruction mappings
  - Previously auto-generated during build, now maintained as source files

## Tab Completion Implementation

The debugger features professional tab completion using GNU Readline:

**Key Implementation Details:**
- Uses `rl_attempted_completion_function` for context-aware completion
- `command_completion()` dispatches between command and subcommand generators
- `subcommand_generator()` provides completions based on line context
- `rl_variable_bind("show-all-if-ambiguous", "on")` shows all matches with single TAB
- `rl_basic_word_break_characters` configured for proper word boundary detection
- Avoids `strtok()` buffer modification by using `strncmp()` for context detection

**Commands with Tab Completion:**
- Main commands: `help`, `step`, `continue`, `symb`, `bp`, `wp`, `q`, etc.
- Subcommands: `show` (ea, demangle, trace, profile, trap, traps, trap-status)
- Register names: `set` (PC, I1-I4, A1-A4, E1-E4, L, B, R, FLAGS, TOS, LL, HL, THA, ST1, ST2)

**Dependencies:**
- `libreadline-dev` (optional) - provides tab completion and command history
- Build system detects readline via `pkg_check_modules(READLINE QUIET readline)`
- Graceful fallback if readline not available

## WebAssembly Debugger Architecture

The WebAssembly debugger provides a complete web-based debugging interface:

**Backend (src/frontend/nd500wasm/main.c):**
- Exports C functions to JavaScript via Emscripten
- JSON API for all debugger operations (registers, memory, disassembly, breakpoints)
- File upload support for .o/.out files via WASM memory allocation
- Breakpoint management with JSON responses

**Frontend (src/frontend/nd500wasm/web/):**
- `index.html`: Split-pane layout with disassembly, registers, memory, breakpoints
- `style.css`: Professional styling with responsive design
- `debugger.js`: ND500Debugger class managing all UI interactions

**Key WASM Exports:**
```c
// Core operations
nd500_dbg_step_js(), nd500_dbg_run_js(), nd500_dbg_stop_js()
nd500_dbg_load_aout_js()  // File loading

// JSON APIs
nd500_dbg_regs_json()     // CPU registers
nd500_dbg_mem_json()      // Memory hex dump with ASCII
nd500_dbg_disasm_json()   // Disassembly text
nd500_dbg_status_json()   // Execution status

// Breakpoint management
nd500_dbg_bp_add_js(), nd500_dbg_bp_del_js()
nd500_dbg_bp_enable_js(), nd500_dbg_bp_disable_js()
nd500_dbg_bp_list_json()  // Breakpoint list
```

**Build Integration:**
- CMake copies web files to build_wasm/bin/ during WASM build
- Makefile `wasm-serve` target builds and starts local web server
- No external dependencies - pure vanilla JavaScript

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
  - Uses instruction dispatch table from `nd500_instructions_gen.h`
  - Implements ND-500 addressing mode classification (14 modes including CONSTANT_SHORT, LOCAL, RECORD, ABSOLUTE, REGISTER, PREINDEXED, etc.)
  - `nd500_decode_at()`: Full instruction decoder with operand parsing
- `nd500_instructions_gen.{c,h}`: Pre-generated dispatch table (committed to repository)
- `cpu_protos.h`: CPU function prototypes

**src/debugger/** - Interactive CLI debugger
- `debugger.c`: REPL with commands: `m` (memory), `d` (disassemble), `step`, `regs`, `load`, `run`, `stop`, `dap`, `q`
- `dap_adapter.c`: Debug Adapter Protocol support (requires external libdap)

**src/frontend/** - Entry points
- `nd500x/`: Native executable entry point
- `nd500wasm/`: WebAssembly entry with JSON debug exports

### Key Data Flow

1. **Instruction Execution Loop**: Machine → CPU → Fetch (bus_read8) → Decode (cpu_instr.c) → Execute (dispatch table lookup) → Instruction handler
2. **Debug API**: Debugger → Unified API (debug_api.c) → Machine/CPU

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
- Never mention claude code in any document