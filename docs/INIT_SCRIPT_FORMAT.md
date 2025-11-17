# ND500X Initialization Script Format

## Overview

Initialization scripts are **debugger command files** that configure the emulator
state before executing a program. They use the existing debugger command language.

## File Format

**Filename**: `init.dbg` or `<program>.init` (e.g., `kernel.init`)

**Location**:
- Same directory as a.out file (e.g., `kernel.init` alongside `kernel`)
- For WASM: Upload alongside a.out file, or extract from zip on JavaScript side
- Executed automatically before a.out loading

**Syntax**: Standard debugger commands, one per line, `#` for comments

## How It Works

### Native CLI:
1. User runs: `nd500x --aout kernel --debug`
2. Emulator looks for `kernel.init` in same directory
3. If found, executes init script commands
4. Then loads `kernel` a.out file
5. Program begins execution with configured state

### WASM Interface:
1. JavaScript uploads files to MEMFS (kernel + kernel.init)
2. JavaScript calls `nd500_dbg_load_aout_path_js("/memfs/kernel")`
3. WASM looks for `/memfs/kernel.init`
4. If found, executes init script commands
5. Then loads kernel a.out file
6. Program begins execution with configured state

**Note**: For zip file support, JavaScript must extract files to MEMFS before loading

## Commands

### MMU Configuration

```
# Enable MMU address translation
mmu enable

# Disable MMU address translation
mmu disable

# Map virtual address range to physical address range
# Format: mmu map <virt_start> <virt_end> <phys_start> [flags]
# Flags: r (read), w (write), x (execute)
mmu map 0x00000000 0x00FFFFFF 0x00000000 rwx

# Identity map (virtual == physical)
mmu identity 0x00000000 0x00FFFFFF rwx

# Map high virtual addresses to low physical addresses
mmu map 0xE8000000 0xE8FFFFFF 0x00000000 rw
```

### Register Initialization

```
# Set CPU register values
reg PC 0x00000004
reg B 0xE8001000
reg TOS 0xE8005000

# Set status register bits
reg ST1 0x00000002   # PIA bit set (privileged mode)
```

### Memory Initialization

```
# Write to memory address
mem write32 0xE8001000 0x00000000
mem write16 0xE8001004 0x1234
mem write8 0xE8001006 0x42
```

### System Configuration

```
# Set system parameters
sys memory 16M          # Physical memory size
sys verbose on          # Enable verbose logging
sys trace off           # Disable instruction tracing
```

## Example: Kernel Initialization

```init
# kernel.init - ND-500 Kernel Bootstrap Configuration
# This script configures MMU and system state for kernel boot

# ===== MMU Setup =====
# Enable MMU address translation
mmu enable

# Identity map low memory (0-16MB) for kernel code/data
# Virtual 0x00000000-0x00FFFFFF → Physical 0x00000000-0x00FFFFFF
mmu identity 0x00000000 0x00FFFFFF rwx

# Map U-area/kernel stack (high virtual) to physical memory
# Virtual 0xE8000000-0xE8FFFFFF → Physical 0x00000000-0x00FFFFFF (wraparound)
mmu map 0xE8000000 0xE8FFFFFF 0x00000000 rw

# ===== CPU Initialization =====
# CPU starts in privileged mode (PIA bit set)
reg ST1 0x00000002

# ===== Ready to Execute =====
# Kernel will now boot with MMU enabled
# Entry point: 0x00000004 (from a.out header)
```

## Implementation Notes

### Script Execution Order

1. Load a.out file (text, data, symbols)
2. Check for `.init` file (in zip or standalone)
3. If found, execute init script
4. Set PC to entry point
5. Begin execution

### Error Handling

- Syntax errors: Display line number and error message, abort load
- Invalid addresses: Warn and skip command
- Unknown commands: Warn and skip

### Script Location Priority

1. `<basename>.init` in zip file (e.g., `kernel.zip/kernel.init`)
2. `<basename>.init` in same directory as executable
3. `init.script` in zip file (generic name)
4. No script (proceed with defaults)

## Script Command Reference

### MMU Commands

| Command | Arguments | Description |
|---------|-----------|-------------|
| `mmu enable` | - | Enable MMU translation |
| `mmu disable` | - | Disable MMU translation |
| `mmu identity` | `<start> <end> <flags>` | Identity map range |
| `mmu map` | `<vstart> <vend> <pstart> <flags>` | Map virtual→physical |
| `mmu clear` | - | Clear all MMU mappings |

### Register Commands

| Command | Arguments | Description |
|---------|-----------|-------------|
| `reg <name>` | `<value>` | Set register to value |

Valid register names: `PC`, `B`, `TOS`, `L`, `R`, `ST1`, `ST2`, `I1`-`I4`, `A1`-`A4`, `E1`-`E4`

### Memory Commands

| Command | Arguments | Description |
|---------|-----------|-------------|
| `mem write8` | `<addr> <value>` | Write byte |
| `mem write16` | `<addr> <value>` | Write halfword |
| `mem write32` | `<addr> <value>` | Write word |
| `mem fill` | `<start> <end> <value>` | Fill range with value |

### System Commands

| Command | Arguments | Description |
|---------|-----------|-------------|
| `sys memory` | `<size>` | Set physical memory size |
| `sys verbose` | `on\|off` | Toggle verbose mode |
| `sys trace` | `on\|off` | Toggle instruction trace |

## File Format Version

Version: 1.0
Syntax: Line-based, case-insensitive keywords, hex/decimal numbers

Numbers:
- Hex: `0x` prefix (e.g., `0xE8001000`)
- Decimal: no prefix (e.g., `16777216`)
- Suffixes: `K`=1024, `M`=1048576, `G`=1073741824 (e.g., `16M`)

Comments: `#` to end of line

Whitespace: Spaces/tabs separate tokens, blank lines ignored
