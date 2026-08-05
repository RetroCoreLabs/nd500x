# ND500X Advanced Debugging - Quick Reference

## 🚀 New Advanced Features

### Tab Completion & Command History
```bash
# Press TAB to complete commands and subcommands
show <TAB>                   # Complete show subcommands
bp <TAB>                     # Complete breakpoint subcommands
wp <TAB>                     # Complete watchpoint subcommands
profile <TAB>                # Complete profile subcommands
set <TAB>                    # Complete register names

# Command History Navigation
UP/DOWN arrows              # Browse command history
!!                          # Repeat last command
!nnn                        # Execute command number nnn
!string                     # Search for command starting with string
history                     # List all command history
```

### Register Manipulation
```bash
set <register> <value>      # Set register value
# Available registers: PC, I1-I4, A1-A4, E1-E4, L, B, R, FLAGS, TOS, LL, HL, THA, ST1, ST2
# Examples:
set PC 0x1000              # Set program counter
set I1 0x42                 # Set integer register 1
set FLAGS 0x01              # Set flags register
```

### Enhanced Disassembly
```bash
show ea [on|off]            # Toggle effective address breakdown
show demangle [on|off]      # Toggle C symbol demangling (strip _)
```

### Instruction Analysis
```bash
show trace [on|off]         # Toggle instruction execution tracing
show profile [on|off]       # Toggle instruction execution profiling
profile [show|reset]        # Show profiling stats or reset data
backtrace (bt)              # Show call stack
```

### Advanced Breakpoints
```bash
bp [addr]                   # Normal breakpoint
bp cond <addr> <condition>  # Conditional breakpoint
bp list                     # List all breakpoints
bp del <id>                 # Delete breakpoint
bp enable <id>              # Enable breakpoint
bp disable <id>             # Disable breakpoint
```

### Watchpoints
```bash
wp <addr> [len] [type]      # Memory watchpoint (read/write/change)
wp reg <register>          # Register watchpoint (PC, I1-I4, L, B, R)
wp list                     # List all watchpoints
wp del <id>                 # Delete watchpoint
wp enable <id>              # Enable watchpoint
wp disable <id>             # Disable watchpoint
```

### Trap System
```bash
show trap [on|off]          # Toggle invalid instruction 0x00 trap
show traps [on|off]         # Show trap system status
show trap-status            # Show current trap status
clear-traps                 # Clear any pending traps
```

### MMU Control (Separate I&D)
```bash
mmu                         # Show Program and Data MMU status
mmu on                      # Enable both Program and Data MMU
mmu off                     # Disable both Program and Data MMU
mmu on program              # Enable Program MMU only (PMON)
mmu off program             # Disable Program MMU only (PMOF)
mmu on data                 # Enable Data MMU only (DMON)
mmu off data                # Disable Data MMU only (DMOF)
showmmu                     # Show detailed MMU status and configuration
mmusetup                    # Setup demo MMU configuration (3 domains)
listpst                     # List all configured PST entries
showpst <psn>               # Show specific PST entry details
listpcb                     # List all configured PCB domains
showpcb <domain> [seg]      # Show PCB capabilities for domain/segment
phyladr <vaddr>             # Translate virtual to physical address
```

## 🎯 Conditional Breakpoint Examples

```bash
bp cond 0x100 "I1 == 0x42"     # Break when I1 equals 0x42
bp cond 0x200 "PC > 0x1000"    # Break when PC > 0x1000
bp cond 0x300 "L != 0"         # Break when L register not zero
```

## 🗺️ MMU Configuration Example

```bash
# Setup MMU with 3 domains
mmusetup                       # Creates kernel + 2 user domains

# Check MMU status
mmu                            # Shows: Program MMU: enabled, Data MMU: enabled
showmmu                        # Detailed status with PST/PCB counts

# Selective MMU control
mmu off program                # Disable program MMU, keep data MMU
mmu                            # Shows: Program MMU: disabled, Data MMU: enabled
mmu on program                 # Re-enable program MMU

# Address translation
phyladr 0x00000000            # Translate virtual address to physical
phyladr 0x08000000 write      # Check data write translation

# Inspect MMU tables
listpst                        # See all 768 configured PST entries
listpcb                        # See all configured domains (0, 1, 2)
showpcb 0 31                   # Show segment 31 (special: ND-100 interface)
showpst 100                    # Show PST entry details
```

## 📊 Profiling Output Example

```
=== INSTRUCTION PROFILE ===
Total instructions executed: 100

Instruction frequency:
Mnemonic        Count  Percent
---------       -----  -------
move               45    45.0%
add                20    20.0%
cmp                15    15.0%
jmp                10    10.0%
call                5     5.0%
ret                 5     5.0%
```

## 🔍 Trace Output Example

```
[TRACE] PC=0x00000000 move I1=0x00000000 I2=0x00000000 I3=0x00000000 I4=0x00000000
[TRACE] PC=0x00000006 add I1=0x00000001 I2=0x00000000 I3=0x00000000 I4=0x00000000
[TRACE] PC=0x00000009 cmp I1=0x00000001 I2=0x00000000 I3=0x00000000 I4=0x00000000
```

## 📋 Call Stack Example

```
=== CALL STACK ===
Depth  PC        Return   Symbol
-----  --------  -------- ------
  1 0x00000100 0x00000150 main
  2 0x00000150 0x00000200 add
  3 0x00000200 0x00000250 multiply
```

## 🎨 Environment Variables

```bash
ND500X_SHOW_EA=1      # Enable EA breakdown by default
ND500X_DEMANGLE=1     # Enable symbol demangling by default
ND500X_TRACE=1        # Enable instruction tracing by default
ND500X_PROFILE=1      # Enable profiling by default
```

## 🔧 Complete Debugging Session Example

```bash
# Start with all features enabled
ND500X_SHOW_EA=1 ND500X_DEMANGLE=1 ND500X_TRACE=1 ND500X_PROFILE=1 ./build/bin/nd500x --debug

# Load program
load /path/to/program.o

# Set up debugging
bp cond 0x100 "I1 == 0x42"    # Conditional breakpoint
wp reg I1                      # Watch I1 register changes
wp 0x2000 4 write             # Watch memory writes

# Execute and analyze
step 20                        # Step through execution
profile show                   # Check instruction frequency
backtrace                      # See call stack
bp list                        # Check breakpoints
wp list                        # Check watchpoints

# Continue debugging
continue                       # Resume execution
```

## 🚨 Watchpoint Triggers

```
Register watchpoint 0 hit on I1 (old=0x00000000 new=0x00000042)
Watchpoint 1 hit on write at 0x00002000 (value=0x12345678)
Breakpoint 0 hit at 0x00000100 (hit count: 1)
```

## 📈 Performance Tips

- Use `show trace off` for large programs to avoid output spam
- Use `show profile off` when not needed to reduce overhead
- Enable only necessary watchpoints to minimize performance impact
- Use conditional breakpoints instead of stepping through loops

## 🎯 Common Debugging Patterns

### Finding Memory Corruption
```bash
wp 0x1000 256 change          # Watch for changes in data structure
run                           # Let program run
# When watchpoint hits:
m 0x1000 256                  # Examine memory
d                            # See what instruction caused change
```

### Analyzing Performance
```bash
show profile on               # Enable profiling
run                          # Run program
stop                         # Stop execution
profile show                 # Analyze instruction frequency
```

### Tracing Function Calls
```bash
show trace on                # Enable instruction tracing
step 100                     # Step through code
# Analyze trace output for function call patterns
```

## 🔍 Troubleshooting

- **Colors not showing**: Ensure terminal supports ANSI colors
- **Symbols not found**: Verify binary has debug symbols
- **Watchpoints not triggering**: Check if memory/registers are actually accessed
- **Performance issues**: Disable trace and profiling for large programs

## 📚 Related Commands

```bash
help                         # Show all available commands
regs                         # Show CPU register state
symb                         # List all loaded symbols
m <addr> <len>               # Examine memory
d <addr> <len>               # Disassemble code
```
