# ND500X Advanced Debugging Features - Manual Test Guide

## Overview

This document provides comprehensive test procedures for all advanced debugging features implemented in the nd500x emulator. The features include conditional breakpoints, instruction tracing, performance profiling, call stack tracking, and register watchpoints.

## Test Environment Setup

### Prerequisites
- Built nd500x emulator: `./build/bin/nd500x`
- Test binary: `$PCC_ND500/examples/04-c-math/math.o`
- Terminal with ANSI color support

### Basic Test Commands
```bash
cd .
./build/bin/nd500x --debug
```

## Test Categories

### 1. Enhanced Disassembly Features

#### Test 1.1: EA Breakdown Toggle
**Purpose**: Test effective address breakdown in disassembly
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
show ea
d 0x59 16
show ea on
d 0x59 16
show ea off
d 0x59 16
```
**Expected Results**:
- `show ea` toggles and shows current state
- `show ea on` enables EA breakdown
- `show ea off` disables EA breakdown
- When enabled, memory operands show `[BASE±disp]→0xEA` format
- When disabled, standard operand format

#### Test 1.2: Symbol Demangling
**Purpose**: Test C symbol demangling (strip leading underscore)
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
show demangle
d 0x59 16
show demangle on
d 0x59 16
show demangle off
```
**Expected Results**:
- `show demangle` toggles and shows current state
- When enabled, `_write` becomes `write`
- When disabled, shows original symbol names with underscores

#### Test 1.3: Enhanced Memory Dump
**Purpose**: Test improved memory dump with ASCII and colors
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
m 0x59 32
m 0x100 64
```
**Expected Results**:
- Memory dump shows hex bytes grouped every 8 bytes
- ASCII column on the right
- ANSI colors: cyan for addresses, dim for zero bytes
- Non-printable characters shown as dots

### 2. Conditional Breakpoints

#### Test 2.1: Basic Conditional Breakpoints
**Purpose**: Test conditional breakpoint creation and listing
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
bp cond 0x100 "I1 == 0x42"
bp cond 0x200 "PC > 0x1000"
bp cond 0x300 "L != 0"
bp list
```
**Expected Results**:
- Conditional breakpoints created successfully
- `bp list` shows breakpoints with type "conditional" and conditions
- Conditions displayed in the list

#### Test 2.2: Conditional Breakpoint Management
**Purpose**: Test enable/disable/delete of conditional breakpoints
**Commands**:
```
bp list
bp disable 0
bp enable 0
bp del 1
bp list
```
**Expected Results**:
- `bp disable` disables breakpoint
- `bp enable` enables breakpoint
- `bp del` removes breakpoint
- `bp list` reflects changes

### 3. Instruction Tracing

#### Test 3.1: Basic Instruction Tracing
**Purpose**: Test instruction execution tracing
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
show trace
step 5
show trace on
step 5
show trace off
step 5
```
**Expected Results**:
- `show trace` toggles and shows current state
- When enabled, shows `[TRACE] PC=0x... instruction I1=0x... I2=0x...` for each step
- When disabled, no trace output
- Trace shows PC, instruction mnemonic, and register values

#### Test 3.2: Environment Variable Support
**Purpose**: Test trace mode via environment variable
**Commands**:
```bash
ND500X_TRACE=1 ./build/bin/nd500x --debug
```
**Expected Results**:
- Trace mode automatically enabled on startup
- No need to manually enable with `show trace on`

### 4. Performance Profiling

#### Test 4.1: Basic Profiling
**Purpose**: Test instruction execution profiling
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
show profile
step 10
profile show
show profile on
step 10
profile show
profile reset
profile show
```
**Expected Results**:
- `show profile` toggles and shows current state
- `profile show` displays instruction frequency statistics
- Shows mnemonic, count, and percentage for each instruction
- `profile reset` clears all profiling data

#### Test 4.2: Profiling Environment Variable
**Purpose**: Test profiling via environment variable
**Commands**:
```bash
ND500X_PROFILE=1 ./build/bin/nd500x --debug
```
**Expected Results**:
- Profiling automatically enabled on startup
- Statistics collected without manual enable

### 5. Call Stack Tracking

#### Test 5.1: Basic Backtrace
**Purpose**: Test call stack display
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
backtrace
bt
```
**Expected Results**:
- `backtrace` and `bt` show call stack
- Initially shows "Call stack is empty"
- Displays depth, PC, return address, and symbol names

#### Test 5.2: Call Stack with Execution
**Purpose**: Test call stack during execution
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
step 5
backtrace
step 10
backtrace
```
**Expected Results**:
- Call stack shows entries as functions are called
- Each entry shows depth, PC, return address, and symbol

### 6. Register Watchpoints

#### Test 6.1: Basic Register Watchpoints
**Purpose**: Test register value change watchpoints
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
wp reg I1
wp reg PC
wp reg L
wp list
```
**Expected Results**:
- Register watchpoints created successfully
- `wp list` shows watchpoints with type "change"
- Displays register name and index

#### Test 6.2: Register Watchpoint Management
**Purpose**: Test enable/disable/delete of register watchpoints
**Commands**:
```
wp list
wp disable 0
wp enable 0
wp del 1
wp list
```
**Expected Results**:
- `wp disable` disables watchpoint
- `wp enable` enables watchpoint
- `wp del` removes watchpoint
- `wp list` reflects changes

#### Test 6.3: Register Watchpoint Triggers
**Purpose**: Test register watchpoint triggering
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
wp reg I1
step 10
```
**Expected Results**:
- Watchpoint triggers when I1 register value changes
- Shows "Register watchpoint X hit on I1 (old=0x... new=0x...)"
- Execution stops when watchpoint hits

### 7. Memory Watchpoints

#### Test 7.1: Memory Watchpoint Types
**Purpose**: Test different memory watchpoint types
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
wp 0x1000 4 read
wp 0x2000 8 write
wp 0x3000 4 change
wp list
```
**Expected Results**:
- Different watchpoint types created successfully
- `wp list` shows watchpoints with correct types
- Displays address, length, and type

#### Test 7.2: Memory Watchpoint Triggers
**Purpose**: Test memory watchpoint triggering
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
wp 0x1000 4 write
step 10
```
**Expected Results**:
- Watchpoint triggers on memory write to address 0x1000
- Shows "Watchpoint X hit on write at 0x1000 (value=0x...)"
- Execution stops when watchpoint hits

### 8. Combined Features Test

#### Test 8.1: Multi-Feature Debugging Session
**Purpose**: Test multiple features working together
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
show ea on
show demangle on
show trace on
show profile on
bp 0x100
wp reg I1
wp 0x2000 4 write
step 20
profile show
backtrace
bp list
wp list
```
**Expected Results**:
- All features work together without conflicts
- EA breakdown shows in disassembly
- Symbols are demangled
- Trace output shows instruction execution
- Profiling collects statistics
- Breakpoints and watchpoints are set
- All commands execute successfully

### 9. Error Handling Tests

#### Test 9.1: Invalid Commands
**Purpose**: Test error handling for invalid commands
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
bp cond 0x100 "invalid condition"
wp reg INVALID
show invalid_option
profile invalid_command
```
**Expected Results**:
- Invalid commands show appropriate error messages
- No crashes or undefined behavior
- Helpful usage messages displayed

#### Test 9.2: Edge Cases
**Purpose**: Test edge cases and limits
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
# Test maximum breakpoints
for i in {0..63}; do bp $((0x1000 + i)); done
bp list
# Test maximum watchpoints
for i in {0..31}; do wp $((0x2000 + i)) 4 write; done
wp list
```
**Expected Results**:
- System handles maximum limits gracefully
- No crashes when limits are reached
- Appropriate warnings for overflow

## Environment Variables

### Available Environment Variables
- `ND500X_SHOW_EA=1` - Enable EA breakdown by default
- `ND500X_DEMANGLE=1` - Enable symbol demangling by default
- `ND500X_TRACE=1` - Enable instruction tracing by default
- `ND500X_PROFILE=1` - Enable profiling by default

### Test Environment Variables
```bash
# Test with all features enabled
ND500X_SHOW_EA=1 ND500X_DEMANGLE=1 ND500X_TRACE=1 ND500X_PROFILE=1 ./build/bin/nd500x --debug

# Test with specific features
ND500X_TRACE=1 ./build/bin/nd500x --debug
```

## Performance Considerations

### Test Performance Impact
**Purpose**: Verify features don't significantly impact performance
**Commands**:
```
load $PCC_ND500/examples/04-c-math/math.o
time step 100
show trace on
time step 100
show profile on
time step 100
```
**Expected Results**:
- Performance impact is minimal
- Trace and profiling don't cause significant slowdown
- System remains responsive

## Troubleshooting

### Common Issues
1. **Colors not displaying**: Ensure terminal supports ANSI colors
2. **Symbols not found**: Verify binary has debug symbols
3. **Watchpoints not triggering**: Check if memory/registers are actually accessed
4. **Performance issues**: Disable trace and profiling for large programs

### Debug Commands
```
help                    # Show all available commands
regs                    # Show CPU register state
symb                    # List all loaded symbols
m <addr> <len>          # Examine memory
d <addr> <len>          # Disassemble code
```

## Success Criteria

### Feature Completeness
- [ ] All commands execute without errors
- [ ] All toggles work correctly
- [ ] All displays show proper information
- [ ] All environment variables work
- [ ] Error handling is appropriate
- [ ] Performance is acceptable

### Integration Testing
- [ ] Multiple features work together
- [ ] No feature conflicts
- [ ] Consistent behavior across sessions
- [ ] Proper cleanup on exit

## Conclusion

This test guide covers all advanced debugging features implemented in nd500x. Each feature should be tested individually and in combination to ensure proper functionality. The debugger now provides professional-grade debugging capabilities comparable to GDB and LLDB.

For any issues or questions, refer to the source code documentation or create an issue in the project repository.
