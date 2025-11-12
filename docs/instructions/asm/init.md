# INIT - Initialize Stack

## Overview

**Mnemonic:** `init`
**Function:** Initialize stack frame
**Class:** CONTROL
**Privilege:** user

**Format:** `INIT <bottom_of_stack>, <main_demand>, <total_demand>`

---

## Description

Initializes the system stack by setting up the base register (B), stack pointer (SP), and top-of-stack register (TOS) according to the three operands. This instruction is typically executed once at program startup to establish the stack environment for the main program.

**Key Characteristics:**
- Single-instruction stack initialization (replaces ~6 operations)
- Sets B, TOS, L registers and stack frame fields
- Automatic overflow detection (trap if main_demand >= total_demand)
- Essential for program startup and context creation
- Initializes both main and system stack spaces
- Clears linkage fields (PREVB, RETA) for clean startup
- Prevents stack corruption via demand validation
- Typically first instruction in main program

The instruction performs the following operations:
1. Loads B register with bottom-of-stack address
2. Sets TOS = bottom_of_stack + total_system_demand
3. Sets B.SP = bottom_of_stack + main_program_demand
4. Clears B.PREVB (previous base) = 0
5. Clears B.RETA (return address) = 0
6. Clears L register = 0

**Stack Layout After INIT:**
```
[Bottom of stack] ← B register
[Main program stack space] (size = main_demand)
[Main program SP] ← B.SP
[Additional stack space] (size = total_demand - main_demand)
[Top of stack] ← TOS register
```

**Trap Prevention:** If main_demand >= total_demand, a stack overflow trap (STO) occurs immediately, preventing stack corruption.

**Common Use Cases:**
- Program initialization (first instruction in main)
- Creating new execution contexts
- Stack frame setup for isolated tasks
- Testing/debugging with controlled stack environments

**Operands:** 3
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0x00DC | INIT |

---

## Operands

**Operand 1** (Bottom of Stack, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word (W)
- **Role**: 4-byte absolute address of stack bottom

**Operand 2** (Main Program Demand, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word (W)
- **Role**: Stack space required by main program (bytes)

**Operand 3** (Total System Demand, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word (W)
- **Role**: Total stack space (main + system, bytes)

**Result**: Registers B, TOS, L initialized; B.SP, B.PREVB, B.RETA set

---

## Trap Conditions

- **Addressing traps**: Invalid address or protection violation
- **Stack overflow (STO)**: main_demand >= total_demand

---

## Data Status Bits

**All flags unaffected**

---

## Examples

### Example 1: Standard program initialization

```assembly
% Initialize stack at program start
MAIN:
        INIT STACK_BASE, 4096, 16384
        % Stack: 4KB for main, 16KB total
        % Continue with main program
```

### Example 2: Using symbolic constants

```assembly
% Define stack parameters
STACK_BASE     EQU 0x100000    % 1MB base address
MAIN_STACK     EQU 0x1000      % 4KB for main
TOTAL_STACK    EQU 0x4000      % 16KB total

PROGRAM_START:
        INIT STACK_BASE, MAIN_STACK, TOTAL_STACK
```

### Example 3: Dynamic stack allocation

```assembly
% Allocate stack at runtime
        W1 MOVE HEAP_PTR, I1     % Get heap address
        INIT I1, 8192, 32768     % 8KB main, 32KB total
```

### Example 4: Minimal stack for simple program

```assembly
% Small stack for simple task
        INIT SMALL_STACK, 256, 1024
        % 256 bytes for main, 1KB total
```

### Example 5: Validating stack demand

```assembly
% Check before initializing
        W1 COMP MAIN_DEMAND, TOTAL_DEMAND
        IF>=GO STACK_ERROR      % main >= total would trap
        INIT STACK_ADDR, MAIN_DEMAND, TOTAL_DEMAND
```

### Example 6: Multi-context system

```assembly
% Initialize separate stacks for different tasks
TASK1_INIT:
        INIT TASK1_STACK, 2048, 8192
        % Task 1 stack initialized

TASK2_INIT:
        INIT TASK2_STACK, 2048, 8192
        % Task 2 stack initialized
```

### Example 7: Embedded system boot

```assembly
% Boot sequence with fixed memory layout
BOOT:
        INIT 0x10000, 0x800, 0x2000
        % Stack at 64KB, 2KB main, 8KB total
        CALL SYSTEM_INIT
```

---

## Performance Notes

- **Execution**: Single instruction replaces multiple register loads
- **Safety**: Automatic overflow detection prevents stack corruption
- **Typical Use**: Called once per program/context, not performance-critical
- **Stack Size**: Choose total_demand carefully based on call depth and local variables
- **Alignment**: Stack base should be word-aligned for optimal performance

---

## Reference Manual

**Section:** §13.9
**Title:** Initialize stack

---

## See Also

- [ENTB](entb.md) - Enter block (pushes new stack frame)
- [RETB](retb.md) - Return from block (pops stack frame)
- [CALL](call.md) - Call subroutine
- [RET](ret.md) - Return from subroutine
