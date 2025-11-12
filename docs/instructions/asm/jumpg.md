# JUMPG - Jump General (Absolute)

## Overview

**Mnemonic:** `jumpg`
**Function:** Unconditional absolute jump
**Class:** BRANCH
**Privilege:** user

**Format:** `JUMPG <address>`

---

## Description

Performs an unconditional jump to an absolute address computed from a general operand. Unlike relative jumps (GO), JUMPG takes the target address from a memory location, register, or computed expression, enabling dynamic control flow such as jump tables, function pointers, and computed gotos.

The instruction loads the program counter (PC) with the effective address of the operand, transferring control to that location. This is fundamentally different from GO which uses PC-relative displacements.

**Jump Behavior:**
```
PC = effective_address(operand)
```

**Key Characteristics:**
- Absolute (computed) address jump (not PC-relative)
- Dynamic target determination at runtime
- Essential for jump tables and switch statements
- Enables function pointers and virtual dispatch
- Descriptor range trap causes fall-through (safety feature)
- Slightly slower than GO (address calculation overhead)
- More flexible than GO for multi-way branches
- Common in interpreters and dynamic dispatchers

**Common Use Cases:**
- **Jump Tables**: Switch/case statement implementation using indexed tables
- **Function Pointers**: Indirect function calls via address tables
- **Dynamic Dispatch**: Runtime polymorphism and virtual method tables
- **Computed Gotos**: State machines with address-based dispatch
- **Return from Trampoline**: Non-standard return mechanisms

If a descriptor range trap occurs during address calculation, execution "falls through" to the next instruction rather than jumping, providing a safety mechanism for out-of-bounds table accesses.

**Operands:** 1
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0x00B4 | JUMPG |

---

## Operands

**Operand 1** (Target Address, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word (W) - 32-bit address
- **Role**: Absolute address to jump to

**Result**: PC = target address

---

## Trap Conditions

- **Addressing traps**: Invalid address calculation
- **Branch trap (BT)**: Target address protection violation
- **Illegal operand specifier (IOS)**: Invalid operand format

**Special**: Descriptor range trap causes fall-through (no jump)

---

## Data Status Bits

**All flags unaffected**

---

## Examples

### Example 1: Jump table (switch/case)

```assembly
% Compute jump based on case value (0-3)
        W1 COMP CASE_VALUE, 3
        IF>GO DEFAULT_CASE
        JUMPG JUMP_TABLE(CASE_VALUE)

JUMP_TABLE:
        .WORD CASE_0
        .WORD CASE_1
        .WORD CASE_2
        .WORD CASE_3
```

### Example 2: Function pointer call

```assembly
% Call function via pointer
        JUMPG FUNCTION_PTR
        % Execution continues at function
```

### Example 3: Indexed dispatch

```assembly
% Jump to handler based on R1
        JUMPG HANDLERS(R1)

HANDLERS:
        .WORD HANDLER_0
        .WORD HANDLER_1
        .WORD HANDLER_2
```

### Example 4: Computed goto

```assembly
% State machine dispatch
        JUMPG STATE_TABLE(STATE_INDEX)

STATE_TABLE:
        .WORD IDLE_STATE
        .WORD ACTIVE_STATE
        .WORD DONE_STATE
```

### Example 5: Return trampoline

```assembly
% Non-standard return mechanism
        W1 MOVE SAVED_PC, I1
        JUMPG I1
```

### Example 6: Indirect jump via memory

```assembly
% Jump to address stored in variable
        JUMPG B.NEXT_ADDRESS
```

### Example 7: Descriptor-based jump with safety

```assembly
% Jump with range checking
        JUMPG DESC(TABLE)(INDEX)
        % Falls through if INDEX out of range
        GO ERROR_HANDLER
```

---

## Performance Notes

- **Size**: 2+ bytes (opcode + operand encoding)
- **Execution**: 2-3 cycles (address calculation + jump)
- **vs GO**: JUMPG is slower but more flexible (computed targets)
- **Jump Tables**: Efficient for multi-way branches (>3 cases)
- **Prediction**: Harder to predict than conditional branches
- **Range Safety**: Descriptor range trap provides bounds checking

---

## Reference Manual

**Section:** §13.2
**Title:** Unconditional absolute jump

---

## See Also

- [GO](go.md) - Relative unconditional jump
- [JUMPS](jumps.md) - Jump subroutine
- [CALLG](callg.md) - Call general
- [RET](ret.md) - Return from subroutine
