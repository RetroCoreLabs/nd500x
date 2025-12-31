# ND500X Debugger Command Reference

> Comprehensive reference for all nd500x interactive debugger commands

## Introduction

The nd500x debugger provides an interactive command-line interface (REPL) for debugging ND-500 programs. It supports memory inspection, disassembly, CPU register manipulation, breakpoints, watchpoints, MMU control, and MON call debugging.

### Starting the Debugger

```bash
./build/bin/nd500x --debug
```

### Command-Line Options

| Option | Description |
|--------|-------------|
| `--debug` | Start interactive debugger REPL |
| `-i <path>` | Load ND-500 a.out file at startup |
| `-ansi` | Force-enable ANSI color output |
| `-noansi` | Force-disable ANSI color output |
| `--disasm <len>` | Disassemble <len> bytes and exit |
| `--addr <addr>` | Start address for disassembly |
| `--hexdump <len>` | Hex dump <len> bytes and exit |

### Tab Completion and History

The debugger supports:
- **Tab completion**: Press TAB to complete commands and subcommands
- **Command history**: Arrow keys for navigation
- **History file**: Persisted in `~/.nd500x_history`
- **History shortcuts**: `!!` (last command), `!nnn` (by number), `!string` (search)
- **`history`** command to list command history

### Number Input Format

All commands accept numbers in multiple formats:
- **Hex**: `0x1000` or `0X1000`
- **Octal**: `0777`
- **Decimal**: `1234`
- **Decimal override**: `$10` (always decimal, regardless of radix setting)

Use `set radix [decimal|hex|octal]` to change the default parsing mode.

---

## Memory Commands

### `m [addr] [len]` - Memory Dump (Data Space)

Display memory as hex dump using **data space** addressing (uses MMU data translation when enabled).

```
m              # Dump 64 bytes from PC
m 0x1000       # Dump 64 bytes from 0x1000
m 0x1000 256   # Dump 256 bytes from 0x1000
```

**Aliases**: None

### `mp [addr] [len]` - Memory Dump (Program Space)

Display memory using **program space** addressing (uses MMU program translation when enabled).

```
mp 0x08000000 128   # Dump 128 bytes from program segment 1
```

### `m! [addr] [len]` - Physical Memory Dump

Display **physical memory** directly, bypassing MMU translation.

```
m! 0x0 256    # Dump first 256 bytes of physical RAM
```

---

## Disassembly Commands

### `d [addr] [len]` - Disassemble

Disassemble instructions with hex bytes.

```
d              # Disassemble 10 instructions from PC
d 0x08000000   # Disassemble from address
d 0x08000000 50   # Disassemble 50 instructions
```

**Aliases**: `dis`, `disasm`

### `dsym <symbol> [len]` - Disassemble at Symbol

Disassemble instructions at a symbol address.

```
dsym _main 20     # Disassemble 20 instructions at _main
```

### `msym <symbol> [len]` - Memory Dump at Symbol

Display memory at a symbol address.

```
msym _buffer 64   # Dump 64 bytes at _buffer
```

---

## Execution Commands

### `step [n]` / `s [n]` - Single Step

Execute one or more instructions.

```
step          # Execute 1 instruction
step 100      # Execute 100 instructions
s 10          # Execute 10 instructions
```

### `run` - Start Execution

Start background execution. The CPU runs in a separate thread until stopped.

```
run
```

### `stop` - Stop Execution

Stop background execution.

```
stop
```

### `continue` / `c` / `cont` - Continue Execution

Continue execution after a breakpoint or trap.

```
continue
c
```

### `status` - Execution Status

Show current execution status (running/stopped, PC, stop reason).

```
status
```

---

## Register Commands

### `regs` - Display Registers

Display all CPU registers.

```
regs
```

Output includes:
- **PC, FLAGS**: Program counter and flags
- **I1-I4 (W1-W4)**: Index/Word registers
- **A1-A4 (F1-F4)**: Address/Float registers
- **E1-E4**: Extension registers (D1-D4 high part)
- **L, B, R**: Level, Base, Return registers
- **TOS, LL, HL, THA**: Stack registers
- **OTE1/2, CTE1/2, MTE1/2, TEMM1/2**: Table registers
- **ST1, ST2**: Status registers
- **PSTP, DITBASE, PS, CED, CAD**: System registers

### `set <register> <value>` - Set Register

Set a register value.

```
set PC 0x08000000
set I1 0x12345678
set B 0x0801D000
set radix hex         # Change number parsing radix
```

**Available registers**: PC, I1-I4, A1-A4, E1-E4, L, B, R, FLAGS, TOS, LL, HL, THA, ST1, ST2, PSTP, DITBASE, CED, CAD, PS

**Alias**: `reg`

---

## Loading Commands

### `load <path>` - Load a.out Binary

Load an ND-500 a.out binary with symbols. Automatically loads associated `.map` and `.s` files if present.

```
load program.o
load /path/to/myprogram.out
```

### `loaddom <path>` - Load DOM/SEG File

Load a SINTRAN domain (DOM) or segment (SEG) file. Configures MMU and sets up virtual memory.

```
loaddom /path/to/program.dom
```

### `loadmap <path>` - Load Map File

Load additional symbol map file.

```
loadmap program.map
```

### `loadsrc <path>` - Load Source File

Load additional source file for source-level debugging.

```
loadsrc program.s
```

### `load-pseg <path>` - Load Program Segment

Load a raw program segment binary.

### `load-dseg <path>` - Load Data Segment

Load a raw data segment binary.

---

## Symbol Commands

### `symb` / `symbols` - List Symbols

List all loaded symbols with type, value, and description.

```
symb
```

### `goto <symbol>` - Go to Symbol

Set PC to a symbol address.

```
goto _main
```

---

## Breakpoint Commands

### `bp [addr]` - Set Breakpoint

Set a breakpoint at address (defaults to current PC).

```
bp             # Set breakpoint at current PC
bp 0x08000100  # Set breakpoint at address
```

**Aliases**: `break`, `breakpoint`

### `bp list` - List Breakpoints

List all breakpoints with ID, address, and status.

```
bp list
```

### `bp del <id>` - Delete Breakpoint

Delete a breakpoint by ID.

```
bp del 0
```

### `bp enable <id>` - Enable Breakpoint

Enable a disabled breakpoint.

```
bp enable 0
```

### `bp disable <id>` - Disable Breakpoint

Disable a breakpoint without deleting it.

```
bp disable 0
```

### `bp cond <addr> <condition>` - Conditional Breakpoint

Set a conditional breakpoint.

```
bp cond 0x08000100 "I1 == 0x1234"
```

### `bp source <file> <line>` - Source Breakpoint

Set breakpoint at source file and line number.

```
bp source main.c 42
```

---

## Watchpoint Commands

### `wp <addr> [len] [type]` - Set Watchpoint

Set a memory watchpoint. Type can be: `read` (r), `write` (w), or `change` (c).

```
wp 0x5000           # Watch 4 bytes for writes (default)
wp 0x5000 8 write   # Watch 8 bytes for writes
wp 0x5000 4 read    # Watch 4 bytes for reads
wp 0x5000 4 change  # Watch for value changes
```

**Aliases**: `watch`, `watchpoint`

### `wp list` - List Watchpoints

List all watchpoints.

```
wp list
```

### `wp del <id>` - Delete Watchpoint

Delete a watchpoint by ID.

```
wp del 0
```

### `wp enable <id>` / `wp disable <id>` - Enable/Disable

Enable or disable a watchpoint.

```
wp enable 0
wp disable 1
```

### `wp reg <register>` - Register Watchpoint

Watch a CPU register for changes.

```
wp reg PC
wp reg I1
```

---

## Display Options (show)

The `show` command controls debugger display options. Without arguments, most toggle the current state.

### `show trace [on|off]` - Instruction Trace

Toggle instruction-level execution trace. Shows PC, bytes, mnemonic, and register state.

```
show trace on
show trace off
show trace       # Toggle
```

### `show memtrace [off|read|write|all]` - Memory Access Trace

Control memory access tracing separately from instruction trace.

```
show memtrace off      # Disable memory tracing
show memtrace read     # Trace memory reads only
show memtrace write    # Trace memory writes only
show memtrace all      # Trace all memory access
show memtrace          # Toggle off/all
```

### `show ea [on|off]` - Effective Address

Show effective address breakdown in disassembly.

```
show ea on
```

### `show hex [on|off]` - Hex Bytes

Show hex bytes in disassembly output.

```
show hex on
```

### `show demangle [on|off]` - Symbol Demangling

Toggle C symbol demangling (strips leading underscore).

```
show demangle on
```

### `show source [off|asm|c|both]` - Source Display

Control source code display during debugging.

```
show source off    # No source
show source asm    # Assembly only
show source c      # C source only
show source both   # Both C and assembly
show source        # Cycle through modes
```

### `show profile [on|off]` - Instruction Profiling

Enable instruction execution profiling.

```
show profile on
```

### `show trap [on|off]` - Invalid Instruction Trap

Toggle trap on invalid instruction (0x00).

```
show trap on
```

### `show trap-status` - Trap Status

Display current trap state.

```
show trap-status
```

### `show traps [on|off]` - Trap System Info

Display trap system status information.

```
show traps on
```

### `show mmu [off|errors|trace|all]` - MMU Logging

Control MMU translation logging level.

```
show mmu off       # No MMU logging
show mmu errors    # Only error messages (default)
show mmu trace     # Translation trace + errors
show mmu all       # All MMU output
show mmu           # Show current level and usage
```

---

## Profiling Commands

### `profile show` - Show Profile Statistics

Display instruction execution statistics.

```
profile show
```

### `profile reset` - Reset Profiling

Reset profiling counters.

```
profile reset
```

---

## Call Stack

### `backtrace` / `bt` - Show Call Stack

Display the call stack (backtrace).

```
backtrace
bt
```

---

## Trap Management

### `clear-traps` - Clear Pending Traps

Clear any pending traps to allow continued execution.

```
clear-traps
```

---

## MMU Commands

### `mmu` - MMU Status

Show current MMU status (program and data MMU enabled/disabled).

```
mmu
```

### `mmu on [program|data]` - Enable MMU

Enable Program MMU (PMON), Data MMU (DMON), or both.

```
mmu on           # Enable both
mmu on program   # Enable program MMU only
mmu on data      # Enable data MMU only
```

### `mmu off [program|data]` - Disable MMU

Disable Program MMU (PMOF), Data MMU (DMOF), or both.

```
mmu off          # Disable both
mmu off program  # Disable program MMU only
mmu off data     # Disable data MMU only
```

### `mmu identity <start> <end> <flags>` - Identity Mapping

Create identity mapping (virtual = physical) for an address range.

```
mmu identity 0x00000000 0x00FFFFFF rwx
```

Flags: `r` (read), `w` (write), `x` (execute)

### `mmu map <vstart> <vend> <pstart> <flags>` - Custom Mapping

Create custom virtual-to-physical mapping.

```
mmu map 0xE8000000 0xE8FFFFFF 0x00000000 rw
```

### `showmmu` - Detailed MMU Status

Show detailed MMU configuration.

```
showmmu
```

### `phyladr <vaddr> [rw] [id]` - Address Translation

Translate virtual address to physical, showing complete walkthrough.

```
phyladr 0x08000000        # Translate (read, data access)
phyladr 0x08000000 1 1    # Translate (write, instruction fetch)
```

### `showpst <psn>` - Show PST Entry

Display Physical Segment Table entry.

```
showpst 100
```

### `showpcb [domain]` - Show PCB Capabilities

Display Process Control Block capabilities.

```
showpcb       # Current domain
showpcb 0     # Domain 0
```

### `listpst` - List PST Entries

List all configured PST entries.

```
listpst
```

### `listpcb` - List PCB Domains

List all configured PCB domains.

```
listpcb
```

### `dumppt <psn> [L2 <l1_index>] [start] [count]` - Dump Page Table

Dump page table entries for a PSN.

```
dumppt 100              # Dump L1/L2 table for PSN 100
dumppt 100 L2 0         # Dump L2 table for L1[0]
dumppt 100 0 32         # Dump entries 0-31
```

### `showcap [domain]` - Show Capabilities

Display capability tables (program and data) for a domain.

```
showcap       # Current domain
showcap 1     # Domain 1
```

### `showpages [domain]` - Show Segment Mappings

Display virtual-to-physical segment mappings.

```
showpages     # Current domain
showpages 0   # Domain 0
```

### `memmap` - Physical Memory Map

Show physical memory allocation and usage.

```
memmap
```

### `mmusetup` - Demo MMU Setup

Set up a demo MMU configuration with 3 domains.

```
mmusetup
```

---

## Domain Commands

### `domain` - Domain Management

Unified domain management command with subcommands.

```
domain              # Show current context + list loaded domains
domain <n>          # Show details for domain n
domain switch <n>   # Switch execution to domain n (sets CED, CAD, PC)
domain symbols <n>  # Set symbol lookup domain to n
```

**Output (no arguments):**
```
============================================================
  Domain Status
============================================================

  Current Context
  ---------------
  CED (executing): 1    CAD (alternative): 1    Symbols: 0

  Loaded Domains
  --------------
    1  nc-a06            Entry: 0x08000004  Segs: 1 [executing]

  Commands: domain <n>, domain switch <n>, domain symbols <n>
============================================================
```

### `unload <domain>` - Unload Domain

Unload a domain and free all resources.

```
unload 1          # Unload domain 1
```

**Note:** Cannot unload domain 0 (kernel) or the currently executing domain (CED).

### `domverify` - Verify DOM

Verify that DOM file data in memory matches the original disk file.

```
domverify
```

---

## Segment Commands

### `segments` / `seg` - Segment Layout

Show segment layout (TEXT, DATA, BSS) for current domain.

```
segments
seg
```

---

## Stack and Heap Commands

### `stackframe` / `sf` - Stack Frame

Dump current stack frame at B register.

```
stackframe
sf
```

### `heap` - Heap Variables

Dump heap variables at TOS.

```
heap
```

---

## Source Listing

### `list` / `l` - List Source

List source code around current PC or specified address.

```
list           # List around PC
list 10        # Show 10 context lines
list 0x08001000  # List at address
list c         # Force C source
list asm       # Force assembly
```

---

## MON Call Commands

The `mon` command controls SINTRAN MON call emulation and debugging.

### `mon` - Show Help

Show MON command help.

```
mon
```

### `mon log [level]` - Logging Level

Set or show MON call logging level.

```
mon log              # Show current level
mon log off          # Disable logging
mon log error        # Log errors only
mon log warn         # Log warnings and errors
mon log info         # Log info, warnings, errors
mon log debug        # Debug level logging
mon log trace        # Full trace logging
```

### `mon status` - Implementation Status

Show MON call implementation status summary.

```
mon status
```

### `mon list [filter]` - List MON Calls

List MON calls by implementation status.

```
mon list              # List validated and in-progress
mon list all          # List all
mon list validated    # List validated only
mon list inprogress   # List in-progress only
mon list notimpl      # List not implemented
```

### `mon info <number|name>` - MON Details

Show detailed information about a MON call.

```
mon info 11B          # By octal number
mon info 9            # By decimal number
mon info InByte       # By name
```

### `mon break [behavior]` - Break Behavior

Set debugger break behavior on unimplemented MON calls.

```
mon break             # Show current behavior
mon break off         # Never break (continue)
mon break continue    # Never break
mon break unimpl      # Break on unimplemented
mon break inprog      # Break on unimplemented and in-progress
mon break halt        # Halt on unimplemented and in-progress
```

---

## SINTRAN File System Commands

Commands for managing the SINTRAN III file system emulation.

### `files` - List Open Files

List all open SINTRAN files (file numbers 64-127).

```
files
```

**Output columns:**
- **FileNo**: File number (64-127, octal 100-177)
- **Mode**: Access mode (SeqRead, RandRdWr, etc.)
- **Scratch**: Yes if temporary file (deleted on close)
- **Position**: Current read/write position
- **Size**: File size in bytes
- **Path**: Host filesystem path

**Example output:**
```
Open Files (SINTRAN III):
  FileNo  Mode        Scratch  Position      Size          Path
  ------  ----------  -------  ------------  ------------  ----
  64      RandRdWr    Yes      0             0             ./SCRATCH/SCRATCH64.DATA
  65      SeqRead     No       1024          8192          ./GUEST/INPUT.TXT
```

### `file <n>` - File Details

Show detailed information for a specific open file.

```
file 64           # Show details for file 64
file 100          # Show details for file 100
```

**Displays:**
- Host path and access mode
- Current position and file size
- Scratch file flag
- ObjectEntry metadata (name, type, header flags)
- Size in bytes and pages
- Open counts
- Access bits and file type flags
- Dates in SINTRAN format (valid range: 1950-2013) with hex values

**Example output:**
```
File 64 Details:
  Host Path:     ./SCRATCH/SCRATCH64.DATA
  Access Mode:   RandRdWr (4)
  Position:      0 / 0 bytes
  Scratch:       Yes (delete on close)

  ObjectEntry:
    Name:        SCRATCH64
    Type:        DATA
    Header:      0xC000
                 (Used WriteOpen)
    Size:        0 bytes (0 pages)
    Open Count:  1 (total: 1)
    Access Bits: 0x1F1F
    File Type:   0x0008
    Device:      0
    Object Idx:  64

  Dates (SINTRAN format, valid range: 1950-2013):
    Created:     2005-12-31 14:30:00 [0x5C7F7780]
    Last Read:   2005-12-31 14:30:00 [0x5C7F7780]
    Last Write:  2005-12-31 14:30:00 [0x5C7F7780]
```

### `user [username]` - SINTRAN User

Show or set the current SINTRAN user for path translation.

```
user              # Show current user
user SYSTEM       # Set user to SYSTEM
user GUEST        # Reset to default user
```

**Path Translation:**

When a SINTRAN filename doesn't include an explicit `(USER)` prefix, the current user is used:
- `FILE:DATA` becomes `{sintran_root}/{current_user}/FILE.DATA`

**Example:**
```
Current SINTRAN user: GUEST

  Paths without (USER) prefix use this user:
    FILE:DATA -> ./GUEST/FILE.DATA
```

---

## Utility Commands

### `help` / `?` - Help

Show command help.

```
help
?
```

### `history` - Command History

List command history.

```
history
```

### `q` / `quit` / `exit` - Quit

Exit the debugger.

```
q
quit
exit
```

---

## Command Aliases Summary

| Primary | Aliases |
|---------|---------|
| `d` | `dis`, `disasm` |
| `step` | `s` |
| `continue` | `c`, `cont` |
| `regs` | - |
| `set` | `reg` |
| `symb` | `symbols` |
| `segments` | `seg` |
| `list` | `l` |
| `backtrace` | `bt` |
| `bp` | `break`, `breakpoint` |
| `wp` | `watch`, `watchpoint` |
| `stackframe` | `sf` |
| `help` | `?` |
| `q` | `quit`, `exit` |

---

## Example Session

```
$ ./build/bin/nd500x --debug
nd500x debug mode. Commands: m, d, step, regs, load, run, stop, symb, show, bp, wp, continue, help, q
Tab completion and command history enabled - press TAB to complete, UP/DOWN for history

[00000000] loaddom /path/to/myprogram.dom
Loaded DOM: /path/to/myprogram.dom
  Start addr: 0x08000004
  ...
PC set to start address: 0x08000004

[08000004] show trace on
show trace: on

[08000004] bp 0x08001000
Breakpoint 0 set at 0x08001000

[08000004] run
running...

[08001000] Stopped: Breakpoint at 0x08001000

[08001000] regs
PC=08001000 FLAGS=00000000
I1/W1=00000000 I2/W2=00000000 I3/W3=00000000 I4/W4=00000000
...

[08001000] d
0x08001000: C3 08 02 D4 47 00  call  $134403143,$0
...

[08001000] q
quitting...
```

---

## Color Scheme

When colors are enabled:
- **Gray** - Memory addresses
- **Yellow** - Hex byte codes
- **Green** - Instructions (mov, add, sub, etc.)
- **Red** - Branch/jump instructions (go, if>=go, etc.)
- **White** - Operands and registers
- **Cyan** - Labels and symbols
- **Blue** - Comments

---

## Environment Variables

| Variable | Description |
|----------|-------------|
| `ND500X_TRACE` | Set to `1` to enable trace on startup |
| `TERM` | Terminal type for color detection |

---

## See Also

- `CLAUDE.md` - Development guidelines and architecture
- `docs/cpu_implementation_changes.md` - CPU implementation details
- `docs/INIT_SCRIPT_FORMAT.md` - Init script format for automation
