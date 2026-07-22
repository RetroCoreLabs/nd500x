# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ND500X is a Norsk Data ND-500 CPU emulator written in C (C11). It emulates the ND-500 architecture with byte-addressed memory, CPU state, and provides both native and WebAssembly (WASM) build targets.

## Setup

### Initialize Git Submodules

The optional DAP support requires the external libdap library:

```bash
# Initialize submodules (required for `make with-dap` builds)
git submodule update --init external/libdap
```

Without this, `make with-dap` and `make dap-sanitizer` will fail. Regular `make` (auto-detect mode) will build without DAP if the submodule is missing.

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
# Run instruction validation tests (39,598 test cases from C# reference)
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
./build/bin/test_instruction_validation
./build/bin/test_float_arithmetic
./build/bin/test_lget_instruction
./build/bin/test_mmu_translation
./build/bin/test_mmu_separate_id
./build/bin/test_source_mapping
./build/bin/test_ote_instructions
```

## Important Build Notes

- **In-source builds are forbidden**: CMake will error if you try to build in the source directory
- **Build output locations**: Binaries go to `build/bin/`, libraries to `build/lib/`
- **Dispatch table**: Pre-generated `src/cpu/nd500_instructions.{c,h}` files are committed (1,078 instruction mappings)

## Build Directory Reference

| Directory | Purpose |
|-----------|---------|
| `build/` | Primary native build output (default target for `make`); contains `bin/`, `lib/`, test executables |
| `build_wasm/` | WebAssembly build output (created by `make wasm` or `make wasm-serve`) |
| `build/nc_sandbox/` | Sandbox directory for ND-500 compiler (CAT-500/NC) testing with isolated SYSTEM files |
| `build/link_sandbox/` | Sandbox directory for ND linker testing with isolated SYSTEM files and DDBTABLES |
| `build-ubsan/`, `build-sanitizer/` | Alternative build directories (if using different sanitizer variants) |

**Note:** After `make`, test binaries and the JSON test data (`nd500_tests.json`) are copied to `build/bin/` by CMake; the test runner auto-locates them in its executable directory.

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
- `nd500_domain.c`: Domain system (process isolation, cross-domain calls, CED/CAD registers)
- `instructions/<CLASS>/`: 242 instruction implementation files across 13 categories

**src/libmon/** - SINTRAN MON call emulation
- `mon_context.h`: MON call context and parameter access helpers
- `mon_dispatcher.c`: MON call dispatch table (300+ handlers)
- `handlers/mon_*B_*.c`: Individual MON call implementations
- Key MON calls: 1B (INBT), 2B (OUTBT), 3B (EXIT), 41B/42B (OPEN), 43B (CLOSE), 117B/120B (READ/WRITE)

**src/debugger/** - Interactive CLI debugger
- `debugger.c`: REPL with tab completion (requires libreadline)
- `commands.c`: 60+ debugger command implementations
- `dap_adapter.c`: Debug Adapter Protocol support (requires external/libdap)

**src/frontend/** - Entry points (choose based on debugging needs)
- `nd500x/`: Native executable entry point with integrated CLI debugger (`./build/bin/nd500x --debug` opens REPL)
  - Best for: Interactive debugging, rapid iteration, command-line control
  - Run with `make run` or `./build/bin/nd500x --debug`
- `nd500wasm/`: WebAssembly entry with JSON debug exports
  - Best for: Browser-based UI, remote debugging, embedded environments
  - Run with `make wasm-serve` (includes web server and example kernel)

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
Control via debugger: `mmu on|off` to enable/disable, `show mmu [off|errors|trace|all]` to set logging level

### Domain System

The domain system provides process isolation (`src/cpu/nd500_domain.c`):
- **CED** (Current Executing Domain): Active domain for instruction fetch
- **CAD** (Current Alternative Domain): Domain for data access (ALT prefix switches)
- **DIT** (Domain Information Table): Per-domain state (TOS, LL, HL, THA)
- **PCB** (Process Control Block): Per-domain capability tables

Debugger commands: `domain`, `domain <n>`, `domain switch <n>`, `domain symbols <n>`, `unload <n>`

### Debug Adapter Protocol (DAP)

Optional DAP server for IDE integration (`src/debugger/dap_adapter.c`):
- **Port**: 4500 (ND500X; ND100X uses 4711)
- **Protocol**: Debug Adapter Protocol (RFC 8879)
- **Clients**: VS Code, Neovim DAP, and other DAP-compatible IDEs
- **Features**: Breakpoints, data breakpoints (memory/register watchpoints), variable inspection, memory read/write, disassembly, console I/O
- **Build requirement**: `git submodule update --init external/libdap` then `make with-dap`
- **Reference**: See `docs/DAP_INTEGRATION.md` for complete setup and usage

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

**Full command reference:** See `docs/DEBUGGER_COMMAND_REFERENCE.md` for all 60+ commands.

**Essential commands:**
- `load <path>` / `loaddom <path>`: Load a.out or DOM binary
- `d [addr] [len]`: Disassemble instructions
- `m [addr] [len]`: Display memory hex dump (data space)
- `mp [addr] [len]`: Display memory (program space)
- `m! [addr] [len]`: Display physical memory (bypass MMU)
- `step [n]` / `s [n]`: Single-step CPU
- `run` / `stop` / `continue`: Control execution
- `regs`: Display CPU registers
- `bp [addr]`: Set breakpoint (use `bp list`, `bp del <id>`)
- `wp <addr> [len] [type]`: Set watchpoint (type: read, write, change)
- `set <reg> <value>`: Set register (PC, I1-I4, A1-A4, E1-E4, L, B, R, FLAGS, TOS, etc.)
- `show trace [on|off]`: Toggle instruction tracing
- `show memtrace [off|read|write|all]`: Toggle memory access tracing
- `show mmu [off|errors|trace|all]`: Toggle MMU logging
- `show profile/ea [on|off]`: Toggle profiling, effective address display
- `mon log/status/list/info`: MON call debugging
- `files`: List open SINTRAN files
- `file <n>`: Show details for open file
- `user [name]`: Show/set current SINTRAN user
- `symb`: List symbols
- `q`: Quit

## Common Debugging Workflows

### Testing Instruction Implementations

```bash
# Build and run instruction validation tests
make
./build/bin/test_instruction_validation

# Run tests for a specific instruction (e.g., MUL, ADD)
./build/bin/test_instruction_validation --filter mul

# Run all tests without stopping on first failure (comprehensive check)
./build/bin/test_instruction_validation --continue

# Run a subset of tests
./build/bin/test_instruction_validation --start 100 --count 50
```

### Interactive Debugging

```bash
# Launch the CLI debugger
make run

# Or manually:
./build/bin/nd500x --debug

# In the debugger:
load <path>              # Load a.out binary
step 10                  # Single-step 10 instructions
d 0x1000                 # Disassemble at address 0x1000
m 0x2000 32              # Display 32 bytes of memory (data space)
mp 0x1000 32             # Display 32 bytes of program space
regs                     # Show all CPU registers
bp 0x1234                # Set breakpoint at 0x1234
show trace on            # Enable instruction tracing
show memtrace all        # Enable memory access tracing
```

### Debugging with IDE (DAP)

If built with DAP support (`make with-dap` or `make dap-sanitizer`):

```bash
# Build with DAP enabled
make with-dap

# Run the emulator (DAP server listens on port 4500)
./build/bin/nd500x --debug

# In VS Code or another DAP client, attach to localhost:4500
# Full ND-500 register file, breakpoints, memory inspection, and disassembly available
```

See `docs/DAP_INTEGRATION.md` for detailed DAP setup instructions.

### WASM Web Debugger

```bash
# Build WASM debugger and start server
make wasm-serve

# Open http://localhost:8000 in browser
# Load a kernel.zip or debug program through the web UI
```

## Code Style

- C11 standard with standard compliance required
- Use snake_case for functions, UPPER_CASE for macros/constants
- Generated files marked with `AUTO-GENERATED FILE - DO NOT EDIT` header
- Instruction implementations in `src/cpu/instructions/<CLASS>/<FunctionName>.c`
- No Python/JS/TypeScript for core emulator (JavaScript only for WASM frontend)
- **Never use Unicode** in code comments or strings - the ND-500 toolchain is from the late 80s

## Examples

The `examples/` directory contains runnable ND-500 program examples:

| Example | Path | Purpose |
|---------|------|---------|
| Hello World | `examples/01-hello/` | Basic program with simple output |
| Addressing Modes | `examples/03-addressing-modes/` | Demonstrates all 14 ND-500 addressing modes |
| C Math | `examples/04-c-math/` | Floating-point and integer arithmetic examples |
| C Kernel | `examples/05-c-kernel/` | Full C kernel build system; compiles to `kernel.zip` for WASM debugger |

**C Kernel Workflow:**
```bash
# Build the C kernel example (compile → assemble → link → ZIP)
make kernel-example
# Creates: examples/05-c-kernel/kernel.zip

# Build WASM debugger and serve with kernel example
make wasm-serve
# Opens http://localhost:8000 in browser with ND500X Web Debugger
# Load kernel.zip in the web UI to debug
```

## Reference Documentation

- `docs/DEBUGGER_COMMAND_REFERENCE.md`: Complete reference for all 60+ debugger commands
- `docs/cpu_implementation_changes.md`: Detailed documentation of all CPU bug fixes, test results, and implementation notes (24 sections covering variant-to-datatype mapping, status flags, branch PC calculation, etc.)
- `test/nd500_tests.json`: 39,598 test cases generated from C# test generators

## Test Generation Architecture

Tests are generated by C# generators in the RetroCore project and exported to JSON for the C test runner.

### Pipeline

```
instructions.json ──► Orchestrator ──► 14 Generators ──► 39,598 Scenarios ──► JSON
   (1,078 variants)                    (per class)        (assembled)
```

### C# Generator Location

`/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.Tests.ND500/Validation/`

| File | Purpose |
|------|---------|
| `ComprehensiveTestOrchestrator.cs` | Routes variants to generators |
| `ComprehensiveTestFramework.cs` | Edge case values, flag calculation |
| `ComprehensiveTestExporter.cs` | Assembles and exports to JSON |
| `Generators/Comprehensive*Generator.cs` | 14 class-specific generators |

### Generators by Instruction Class

| Generator | Tests | Instructions |
|-----------|-------|--------------|
| ArithmeticGenerator | ~12,800 | +, -, *, /, MUL4, DIV4, ABS, NEG |
| LogicalGenerator | ~5,800 | AND, OR, XOR, INV |
| CompareGenerator | ~2,100 | CMP, TST, CMPU |
| MoveGenerator | ~3,400 | ASSIGN, LGET, LPUT, EXCH |
| ShiftGenerator | ~1,800 | SHL, SHR, ROL, ROR |
| BranchGenerator | ~700 | BEQ, BNE, BLT, LOOP |
| Others | ~5,000 | CALL, STRING, FLOAT, SYSTEM, etc. |

### Regenerating Tests

```bash
# In RetroCore directory
cd /mnt/e/Dev/Repos/Ronny/RetroCore
dotnet test Emulated.Tests.ND500 --filter "Generate_Master_JSON"

# Copy to nd500x
cp Emulated.Tests.ND500/bin/Debug/net9.0/nd500_tests.json \
   /home/ronny/repos/nd500x/test/nd500_tests.json

# Rebuild (CMake copies to build/bin/)
cd /home/ronny/repos/nd500x && make

# Run tests
./build/bin/test_instruction_validation --continue
```

### Test JSON Format

```json
{
  "name": "Add_AddZero_0",
  "assembly": "w1 + $0",
  "bytes": [108, 0],
  "initial": { "regs": { "pc": 4096, "i1": 0, "st": 0 }, "ram": [] },
  "final": { "regs": { "pc": 4098, "i1": 0, "st": 32 }, "ram": [] }
}
```

### File Locations

| Purpose | Path |
|---------|------|
| Source test data | `test/nd500_tests.json` |
| Build copy | `build/bin/nd500_tests.json` (CMake copies at build) |
| C test runner | `test/test_instruction_validation.c` |

The test runner automatically finds `nd500_tests.json` in its executable directory.

## Instruction Porting Guidelines

When implementing ND-500 instructions:
- Reference implementations should match the C# emulator code as closely as possible
- C# reference location: `/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/`
- Run `./build/bin/test_instruction_validation --filter <instruction>` to validate against reference
- See `docs/cpu_implementation_changes.md` for known issues and fixes
- If missing helper functions or decoding logic, create them rather than duplicating code
- Ask if unsure about implementation approach
- Each instruction file includes documentation, operand helpers, and implementation notes
- 0x27 ' is end of string in sintran
- From the ND-60.113.02 Assembler Reference Manual (lines 730-741):

  Data type specifiers:
  | Specifier | Meaning                        |
  |-----------|--------------------------------|
  | BI        | Bit                            |
  | BY        | Byte (8 bits)                  |
  | H         | Half-word (16 bits)            |
  | W         | Word (32-bit integer)          |
  | F         | Single precision real (32-bit) |
  | D         | Double precision real (64-bit) |

- MON Call INTEGER parameter sizes:
  | Architecture | INTEGER Size | C Type    | Function                    |
  |--------------|--------------|-----------|----------------------------|
  | ND-100       | 16-bit       | uint16_t  | mon_read/write_param_halfword |
  | ND-500       | 32-bit       | uint32_t  | mon_read/write_param_word     |

  **Important**: On ND-500, all INTEGER parameters in MON calls use 32-bit words (W type).
  Assembly code declares them as `W BLOCK` not `H BLOCK`.

- Register aliases on ND-500:
  | Full Name | Aliases (context-based)     | Size    |
  |-----------|----------------------------|---------|
  | I1        | W1, H1, BY1, BI1           | 32-bit  |
  | I2        | W2, H2, BY2, BI2           | 32-bit  |
  | I3        | W3, H3, BY3, BI3           | 32-bit  |
  | I4        | W4, H4, BY4, BI4           | 32-bit  |
  | A1        | F1, D1 (low 32-bit)        | 32-bit  |
  | A2        | F2, D2 (low 32-bit)        | 32-bit  |
  | A3        | F3, D3 (low 32-bit)        | 32-bit  |
  | A4        | F4, D4 (low 32-bit)        | 32-bit  |
  | E1        | D1 (high 32-bit)           | 32-bit  |
  | E2        | D2 (high 32-bit)           | 32-bit  |
  | E3        | D3 (high 32-bit)           | 32-bit  |
  | E4        | D4 (high 32-bit)           | 32-bit  |
- All CPU instructions are defined with examples in the folder ~/repos/nd500x/docs/instructions/asm
- Never duplicate code
- The C# code is not a reference implementation. It is just another emulator with bugs. The ND-500 CPU, linker, and
  assembler reference manuals are the TRUTH. Make sure to never assume anything - verify against documentation.
- NEVER mention Claude or AI assistants in git commit messages

## MON Call Development

The MON subsystem (`src/libmon/`) emulates SINTRAN operating system calls:

- **Architecture**: Dispatcher routes MON numbers to per-handler files (`handlers/mon_*B_*.c`)
- **Parameter access**: Use helper functions in `mon_context.h` to read/write parameters from machine state
- **Testing**: MON call handlers must match SINTRAN semantics exactly
  - Test against real SINTRAN output when possible
  - Use debugger `mon` commands to inspect: `mon log`, `mon status`, `mon list`, `mon info`
- **Reference**: See `docs/HANDOFF_CSHARP_FILE_TABLE_SINTRAN_SEMANTICS.md` for file table semantics
- **Completeness audit**: `docs/MON_COMPLETENESS_AUDIT.md` lists which MON calls are implemented and their status
- **Key MON calls** needing careful implementation:
  - 1B (INBT): Blocking input on device (suspends on empty input)
  - 41B/42B (OPEN): File open with SINTRAN path semantics (own-dir fallback to SYSTEM)
  - 43B (CLOSE): File close and descriptor cleanup
  - 117B/120B (READ/WRITE): Buffered file I/O
  - 3B (EXIT): Process termination

## Notes on Entry Points and Frontends

**nd500x --debug** (Native CLI debugger):
- Interactive REPL with tab completion (requires libreadline)
- Persistent command history
- Full breakpoint and watchpoint support
- Best for rapid development and real-time experimentation

**nd500wasm** (WebAssembly + Web UI):
- JSON-based communication with browser frontend
- No external dependencies for frontend
- Portable across platforms
- Best for demonstrations and documentation

**DAP Server** (IDE integration):
- Listens on port 4500 when built with DAP support
- Integrates with VS Code, Neovim, and other DAP clients
- Best for development in preferred IDE with familiar debugging workflows