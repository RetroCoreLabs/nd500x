# GO - Unconditional Jump (Relative)

## Overview

**Mnemonic:** `go`
**Function:** Unconditional relative jump
**Class:** BRANCH
**Privilege:** user

**Format:** `GO <displacement>`

---

## Description

Performs an unconditional jump to a target address computed by adding a signed displacement to the current program counter. This is the fundamental unconditional branch instruction in the ND-500 architecture, used for loops, forward jumps, and control flow that doesn't depend on conditions.

**Jump Behavior:**
```
PC = PC + sign_extend(displacement)
```

**Key Characteristics:**
- Unconditional PC-relative jump (always branches)
- Three displacement sizes: 4-bit (-8 to +7), byte (-128 to +127), halfword (-32768 to +32767)
- Assembler auto-selects smallest displacement (code optimization)
- Sign-extended displacement for backward/forward jumps
- Pipeline flush on jump (3-4 cycles typical)
- Essential for loops, forward jumps, and unconditional control flow
- No flag modification (unlike conditional branches)
- Cannot be predicted (unconditional, always taken)

The displacement is PC-relative, meaning the target is specified as an offset from the current instruction. The assembler typically calculates this displacement from symbolic labels, allowing programmers to write `GO LABEL` instead of computing byte offsets manually.

**Displacement Variants:**
- **4-bit**: -8 to +7 bytes (ultra-short jumps, opcode 0x00C0)
- **Byte**: -128 to +127 bytes (short jumps, opcode 0x00C1)
- **Halfword**: -32768 to +32767 bytes (long jumps, opcode 0x00C2)

The assembler automatically selects the smallest displacement that can reach the target, optimizing code size.

**Common Use Cases:**
- **Unconditional loops**: `GO LOOP_START`
- **Skip over code blocks**: `GO AFTER_ERROR`
- **Multi-way branches**: Combined with computed jumps
- **End of conditionals**: Jump past else clause
- **Exit paths**: Jump to function epilogue

**Operands:** 1 (displacement)
**Variants:** 3 opcodes (4-bit, byte, halfword displacements)

---

## Variants

| Variant | Opcode | Displacement | Range | Assembly Notation |
|---------|--------|--------------|-------|-------------------|
| 1/3 | 0x00C0 | 4-bit | -8 to +7 | GO:S <disp> |
| 2/3 | 0x00C1 | Byte | -128 to +127 | GO:B <disp> |
| 3/3 | 0x00C2 | Halfword | -32768 to +32767 | GO:H <disp> |

**Note**: Assemblers typically auto-select the smallest variant.

---

## Operands

**Operand 1** (Displacement):
- **4-bit variant**: 4-bit signed displacement (-8 to +7)
- **Byte variant**: 8-bit signed displacement (-128 to +127)
- **Halfword variant**: 16-bit signed displacement (-32768 to +32767)
- **Role**: Offset added to PC

**Result**: PC = PC + displacement

---

## Trap Conditions

- **Branch trap (BT)**: If target address protection violation

---

## Data Status Bits

**All flags unaffected**

---

## Examples

### Example 1: Infinite loop

```assembly
% Infinite loop
LOOP:
        % Process something
        CALL DO_WORK
        GO LOOP
```

### Example 2: Skip code block

```assembly
% Skip error handling
        W1 COMP STATUS, 0
        IF>=GO NO_ERROR
        % Handle error
        CALL ERROR_HANDLER
NO_ERROR:
        % Continue normal execution
```

### Example 3: If-then-else structure

```assembly
% If-then-else
        W1 COMP VALUE, THRESHOLD
        IF<GO LESS_THAN
        % Greater than or equal
        CALL HANDLE_GE
        GO ENDIF
LESS_THAN:
        % Less than
        CALL HANDLE_LT
ENDIF:
        % Continue
```

### Example 4: Switch/case with GO

```assembly
% Multi-way branch
        W1 COMP CASE_VAL, 0
        IF=GO CASE_0
        W1 COMP CASE_VAL, 1
        IF=GO CASE_1
        W1 COMP CASE_VAL, 2
        IF=GO CASE_2
        GO DEFAULT

CASE_0:
        CALL HANDLE_0
        GO END_SWITCH
CASE_1:
        CALL HANDLE_1
        GO END_SWITCH
CASE_2:
        CALL HANDLE_2
        GO END_SWITCH
DEFAULT:
        CALL HANDLE_DEFAULT
END_SWITCH:
```

### Example 5: Forward jump to epilogue

```assembly
FUNCTION:
        % Early exit condition
        W1 COMP PARAM, 0
        IF=GO EPILOGUE
        % Main function body
        CALL PROCESS
EPILOGUE:
        % Cleanup and return
        RET
```

### Example 6: Loop with multiple exits

```assembly
SEARCH_LOOP:
        % Check condition 1
        W1 COMP I1, LIMIT
        IF>=GO NOT_FOUND
        % Check condition 2
        W2 COMP ARRAY(I1), TARGET
        IF=GO FOUND
        % Continue loop
        W1 ADD 1, I1
        GO SEARCH_LOOP

FOUND:
        % Found target
        GO DONE
NOT_FOUND:
        % Not found
        W1 CLR
DONE:
```

### Example 7: State machine jump table

```assembly
% Jump to current state handler
        W1 COMP STATE, 0
        IF=GO STATE_IDLE
        W1 COMP STATE, 1
        IF=GO STATE_ACTIVE
        W1 COMP STATE, 2
        IF=GO STATE_DONE
        GO STATE_ERROR
```

---

## Performance Notes

- **Size**: 1-3 bytes depending on displacement size
  - 4-bit: 1 byte (ultra-compact)
  - Byte: 2 bytes (typical)
  - Halfword: 3 bytes (long jumps)
- **Execution**: 1-2 cycles (fastest branch instruction)
- **Range**: Assembler selects optimal size automatically
- **vs JUMPG**: GO is PC-relative, JUMPG is absolute
- **Branch Prediction**: Modern implementations may predict GO as always taken
- **Code Density**: Ultra-short GO variant provides excellent code density

---

## Reference Manual

**Section:** §13.1
**Title:** Unconditional relative jump

---

## See Also

- [JUMPG](jumpg.md) - Unconditional absolute jump
- [CALL](call.md) - Call subroutine
- [IF=GO](if=go.md) - Conditional jump if equal
- [IF<GO](if<go.md) - Conditional jump if less than
- [RET](ret.md) - Return from subroutine
