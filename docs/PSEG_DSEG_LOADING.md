# PSEG and DSEG Loading with Mode Auto-Detection

## Overview

The nd500x emulator now supports loading PSEG (program segment) and DSEG (data segment) files with automatic kernel/user mode detection and manual overrides.

## Memory Layout

### Kernel Mode
- **PSEG Base**: `0x08000000`
- **DSEG Base**: `0x00000000`

### User Mode
- **PSEG Base**: `0xD0000000`
- **DSEG Base**: `0xF0000000`

## Auto-Detection

The emulator automatically detects kernel vs user mode from the filename:
- If filename contains `"user"` → User mode
- Otherwise → Kernel mode

### Examples
```bash
# Auto-detected as USER mode
nd500x --pseg user_prog.pseg --dseg user_data.dseg --debug

# Auto-detected as KERNEL mode
nd500x --pseg kernel.pseg --dseg kernel.dseg --debug
```

## Manual Mode Override

Use the `--mode` flag to explicitly set the mode:

```bash
# Force kernel mode
nd500x --pseg program.pseg --dseg data.dseg --mode kernel --debug

# Force user mode
nd500x --pseg program.pseg --dseg data.dseg --mode user --debug

# Explicit auto-detection
nd500x --pseg program.pseg --mode auto --debug
```

## Command-Line Usage

### Basic Syntax
```
nd500x [--pseg <path>] [--dseg <path>] [--mode <mode>] [--debug]
```

### Options
- `--pseg <path>` - Load PSEG file
- `--dseg <path>` - Load DSEG file
- `--mode <mode>` - Set mode: `kernel` | `user` | `auto`
- `--pc <addr>` - Override starting PC
- `--debug` - Enter interactive debugger

### Full Example
```bash
# Load kernel PSEG/DSEG and start debugger
nd500x --pseg kernel.pseg --dseg kernel.dseg --mode kernel --pc 0x08000000 --debug
```

## Debugger Commands

Inside the debugger, use the `load` command:

### Load PSEG
```
load pseg <path> [mode] [addr]
```

**Examples:**
```
# Auto-detect mode from filename
load pseg kernel.pseg

# Explicit kernel mode
load pseg program.pseg kernel

# User mode with custom address
load pseg user_prog.pseg user 0xE0000000

# Just address (auto-detect mode)
load pseg kernel.pseg 0x08000000
```

### Load DSEG
```
load dseg <path> [mode] [addr]
```

**Examples:**
```
# Auto-detect mode
load dseg kernel.dseg

# Explicit user mode
load dseg data.dseg user

# Kernel mode with custom address
load dseg kernel.dseg kernel 0x10000000
```

## Mode Detection Logic

1. **Check `--mode` flag (command-line) or mode parameter (debugger)**
   - If `kernel` → Use kernel addresses
   - If `user` → Use user addresses
   - If `auto` → Check filename

2. **If no mode specified**
   - Check if filename contains `"user"`
   - If yes → User mode
   - If no → Kernel mode

3. **Apply base addresses**
   - Kernel: PSEG=0x08000000, DSEG=0x00000000
   - User: PSEG=0xD0000000, DSEG=0xF0000000

4. **Custom address override**
   - If address parameter provided, use it instead of default

## Usage Examples

### Example 1: Load Kernel with Auto-Detection
```bash
$ nd500x --pseg kernel.pseg --dseg kernel.dseg --debug
PSEG loaded at 0x08000000 from kernel.pseg (kernel mode)
DSEG loaded at 0x00000000 from kernel.dseg (kernel mode)
nd500x debug mode. Commands: m, d, step, regs, load, run, stop, ...
```

### Example 2: Load User Program
```bash
$ nd500x --pseg user_app.pseg --mode user --debug
PSEG loaded at 0xD0000000 from user_app.pseg (user mode)
nd500x debug mode. Commands: m, d, step, regs, load, run, stop, ...
```

### Example 3: Load in Debugger
```
[00000000] load pseg kernel.pseg kernel
PSEG loaded at 0x08000000 (kernel mode)

[00000000] load dseg data.dseg user
DSEG loaded at 0xF0000000 (user mode)

[00000000] regs
PC=00000000 FLAGS=00000000
...
```

### Example 4: Custom Addresses
```
[00000000] load pseg program.pseg kernel 0x10000000
PSEG loaded at 0x10000000 (kernel mode)
```

## Help Output

### Command-Line Help
```bash
$ nd500x --help
ND-500 Emulator - nd500x

Usage: nd500x [options]

Options:
  --debug                  Enter interactive debugger REPL
  -i <path>                Load a.out file (legacy)
  --aout <path>            Load a.out file
  --pseg <path>            Load PSEG binary (auto-detects kernel/user mode)
  --dseg <path>            Load DSEG binary (auto-detects kernel/user mode)
  --mode <mode>            Override mode: kernel | user | auto
                           (kernel: PSEG=0x08000000, DSEG=0x00000000)
                           (user:   PSEG=0xD0000000, DSEG=0xF0000000)
                           (auto:   detect from filename - 'user' in name)
  --pc <addr>              Set starting PC address
  --disasm <len>           Disassemble <len> bytes and exit
  --addr <addr>            Start address for disassembly (default: 0)
  --hexdump <len>          Hex dump <len> bytes and exit
  -ansi                    Force enable ANSI colors
  -noansi                  Force disable ANSI colors
  --help                   Show this help message

Examples:
  nd500x --aout kernel --debug
  nd500x --pseg kernel.pseg --dseg kernel.dseg --mode kernel --debug
  nd500x --pseg user_prog.pseg --mode user --debug
  nd500x --aout program --disasm 100 --addr 0x1000
```

### Debugger Help
In the debugger, type `help` to see:
```
  load <path>                 Load ND-500 a.out into memory
  load pseg <path> [mode] [addr]  Load PSEG binary (auto-detect mode from filename)
  load dseg <path> [mode] [addr]  Load DSEG binary (auto-detect mode from filename)
                              mode: kernel (0x08000000) | user (0xD0000000)
```

## Implementation Files

- `repos/nd500x/src/frontend/nd500x/nd500x.c` - Command-line parsing and loading
- `repos/nd500x/src/debugger/debugger.c` - Debugger `load` commands
- `repos/nd500x/src/machine/machine_loader.c` - Low-level PSEG/DSEG loading functions

## See Also

- `repos/nd500x/docs/AOUT_STRUCT_FIX.md` - a.out format fixes
- `repos/ragge/pcc-nd500/docs/toolchain/OBJECT_VS_EXECUTABLE_DETECTION.md` - Detection methodology
