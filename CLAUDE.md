# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ND500X is a Norsk Data ND-500 CPU emulator written in C (C11). It emulates the ND-500 architecture with byte-addressed memory, CPU state, and provides both native and WebAssembly (WASM) build targets.

## Build Commands

### Native Build
```bash
make                    # Build native (auto-detects DAP if available)
make run                # Build and run in debug mode (./build/bin/nd500x --debug)
make with-sanitizer     # Build with address sanitizer for debugging
make clean              # Clean all build directories
```

### WebAssembly Build
```bash
make wasm               # Build WebAssembly target
make wasm-serve         # Build WASM + kernel example, start web server at localhost:8000
make wasm-clean         # Clean WASM build only
```

### Build Variants
```bash
make with-dap           # Build with DAP support (requires external/libdap)
make without-dap        # Build without DAP support
make dap-sanitizer      # Build with both DAP and sanitizer
make kernel-example     # Build C kernel example in examples/05-c-kernel/
make help               # Display all available build targets
```

## Testing

```bash
# Run instruction validation tests (20,902 test cases from C# reference)
./build/bin/test_instruction_validation
./build/bin/test_instruction_validation --continue      # Run all, don't stop on failure
./build/bin/test_instruction_validation --filter mul    # Filter by instruction name
./build/bin/test_instruction_validation --start 100 --count 50  # Run subset

# Run all tests via ctest
cd build && ctest

# Run all tests with verbose output
cd build && ctest -V

# Run a specific test by name
cd build && ctest -R disasm

# Run tests directly
./build/bin/disasm_tests
./build/bin/test_mmu_translation
./build/bin/test_mmu_separate_id
./build/bin/test_source_mapping
```

## Important Build Notes

- **In-source builds are forbidden**: CMake will error if you try to build in the source directory
- **Build output locations**: Binaries go to `build/bin/`, libraries to `build/lib/`
- **Dispatch table**: Pre-generated `src/cpu/nd500_instructions.{c,h}` files are committed (1,078 instruction mappings)

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

**src/cpu/** - CPU core
- `cpu.c`: CPU state management (registers: PC, FLAGS, I[4], A[4], E[4], L, B, R, TOS, etc.)
- `cpu_instr.c`: Instruction decoding and operand parsing with 14 addressing modes
- `nd500_instructions.{c,h}`: Pre-generated O(1) dispatch table (committed to repository)
- `nd500_mmu.c`: Three-level MMU translation (Virtual → Capability → PST → Physical)
- `instructions/<CLASS>/`: 242 instruction implementation files across 13 categories

**src/debugger/** - Interactive CLI debugger
- `debugger.c`: REPL with tab completion (requires libreadline)
- `dap_adapter.c`: Debug Adapter Protocol support (requires external/libdap)

**src/frontend/** - Entry points
- `nd500x/`: Native executable entry point
- `nd500wasm/`: WebAssembly entry with JSON debug exports

### Key Data Flow

1. **Instruction Execution**: Machine → CPU → Fetch (bus_read8) → Decode (cpu_instr.c) → Dispatch table lookup → Instruction handler
2. **Debug API**: Debugger → Unified API (debug_api.c) → Machine/CPU

### ND-500 Addressing Modes

The CPU implements 14 addressing modes (see `cpu_instr.c:66-84`):
- Short forms (2-bit prefix): CONSTANT_SHORT, LOCAL_SHORT, RECORD_SHORT
- Extended forms (0xC0-0xFF): ABSOLUTE, REGISTER, LOCAL, RECORD, DESCRIPTOR, PREINDEXED, etc.
- Special: ALT prefix (0xC8), post-increment modes, indirect modes

### MMU Architecture

Three-level translation (`src/cpu/nd500_mmu.c`):
1. Virtual Address → Capability (via Process Control Block)
2. Capability → PST Entry (via Physical Segment Number)
3. PST Entry → Physical Page

Virtual Address Format: `[Segment(5) | Page(16) | Offset(11)]` (32-bit)
Toggle via debugger: `show mmu [on|off]`

## Dependencies

### Native Build
- **libcjson** (optional): JSON output. Build proceeds without if missing.
- **libreadline-dev** (optional): Tab completion and history. Graceful fallback if missing.
- **pthread**: Required for background execution (`run` command).
- **external/libdap/** (optional): Debug Adapter Protocol library.

### WASM Build
- **Emscripten SDK**: Required for WebAssembly compilation.
- **cJSON**: Auto-fetched via CMake FetchContent.

## CMake Options

- `BUILD_WASM=ON/OFF`: Build WebAssembly target (default: OFF)
- `DEBUGGER_ENABLED=ON/OFF`: Enable debugger support (default: ON, forced OFF for WASM)
- `SKIP_LIBDAP=ON/OFF`: Skip DAP library even if present (default: OFF)

## Debugger Quick Reference

Run with: `./build/bin/nd500x --debug`

**Essential commands:**
- `load <path>`: Load a.out binary and symbols
- `d [addr] [len]`: Disassemble instructions
- `m [addr] [len]`: Display memory hex dump
- `step [n]` / `s [n]`: Single-step CPU
- `run` / `stop` / `continue`: Control execution
- `regs`: Display CPU registers
- `bp [addr]`: Set breakpoint (use `bp list`, `bp del <id>`)
- `wp <addr> [len] [type]`: Set watchpoint (type: read, write, change)
- `set <reg> <value>`: Set register (PC, I1-I4, A1-A4, E1-E4, L, B, R, FLAGS, TOS, etc.)
- `show trace/profile/ea/mmu [on|off]`: Toggle debug features
- `symb`: List symbols
- `q`: Quit

## Code Style

- C11 standard with standard compliance required
- Use snake_case for functions, UPPER_CASE for macros/constants
- Generated files marked with `AUTO-GENERATED FILE - DO NOT EDIT` header
- Instruction implementations in `src/cpu/instructions/<CLASS>/<FunctionName>.c`
- No Python/JS/TypeScript for core emulator (JavaScript only for WASM frontend)
- **Never use Unicode** in code comments or strings - the ND-500 toolchain is from the late 80s

## Reference Documentation

- `docs/cpu_implementation_changes.md`: Detailed documentation of all CPU bug fixes, test results, and implementation notes (24 sections covering variant-to-datatype mapping, status flags, branch PC calculation, etc.)
- `test/nd500_tests.json`: 20,902 test cases generated from C# reference implementation

## Instruction Porting Guidelines

When implementing ND-500 instructions:
- Reference implementations should match the C# emulator code as closely as possible
- C# reference location: `/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/`
- Run `./build/bin/test_instruction_validation --filter <instruction>` to validate against reference
- See `docs/cpu_implementation_changes.md` for known issues and fixes
- If missing helper functions or decoding logic, create them rather than duplicating code
- Ask if unsure about implementation approach
- Each instruction file includes documentation, operand helpers, and implementation notes
