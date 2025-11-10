# STZ - Store Zero

## Overview

**Mnemonic:** `stz`
**Function:** Store zero to operand
**Class:** MOVE
**Privilege:** user
**Format:** `t STZ <operand>`

---

## Description

Replaces the contents of the destination operand with zero. This instruction provides an efficient way to clear variables, array elements, or registers without requiring a separate zero constant. The zero flag (Z) is always set to 1 after execution.

STZ is commonly used for initialization, clearing accumulators, and resetting state variables. It is more efficient than loading zero into a register and then storing it.

**Operation:**
```
0 → operand
1 → Z flag
```

**Common Use Cases:**
- Variable initialization
- Array element clearing
- Register zeroing
- State reset operations
- Clearing flags or status bytes

**Operands:** 1 (destination, write-only)
**Variants:** 6 opcodes (BI, BY, H, W, F, D)

---

## Variants

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/6 | 0xFC85 | Bit | BI STZ |
| 2/6 | 0x0048 | Byte | BY STZ |
| 3/6 | 0x0049 | Halfword | H STZ |
| 4/6 | 0x004A | Word | W STZ |
| 5/6 | 0x004B | Float | F STZ |
| 6/6 | 0x004C | Double | D STZ |

---

## Operands

**Operand 1** (Destination, Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix (BI/BY/H/W/F/D)
- **Role**: Destination to be cleared to zero
- **Note**: CONSTANT addressing mode is not supported (cannot write to constant)

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation
- **Write protection**: Attempt to write to read-only memory

---

## Data Status Bits

- **Z (Zero)**: Always set to 1
- **S (Sign)**: Unaffected
- **C (Carry)**: Unaffected
- **V (Overflow)**: Unaffected

---

## Examples

### Example 1: Clear byte variable
```assembly
        % Clear the byte FLAGS
        BY STZ FLAGS
```

### Example 2: Clear word counter
```assembly
        % Zero a word variable
        W STZ B.COUNT
```

### Example 3: Clear array element
```assembly
        % Clear array element indexed by W2
        W STZ ARRAY(W2)
```

### Example 4: Reset record field
```assembly
        % Clear a record field
        W STZ R.VALUE
```

### Example 5: Initialize float to zero
```assembly
        % Set float variable to 0.0
        F STZ TEMPERATURE
```

### Example 6: Clear local variable
```assembly
        % Zero a local stack variable at offset 16
        W STZ B.16
```

### Example 7: Clear multiple array elements
```assembly
        % Clear first 10 elements of array
        W1 CLR
LOOP:
        W STZ BUFFER(W1)
        W1 INC
        W1 COMP 10
        IF<GO LOOP
```

---

## Performance Notes

- **Execution**: 2-3 cycles (faster than MOVE 0, dest)
- **Efficiency**: More compact encoding than loading zero constant
- **Memory**: Single instruction vs. multi-byte constant load + store
- **Best for**: Single-element clearing (use BMOVE for bulk zeroing)

**Comparison:**
- `W STZ VAR`: 3 bytes, 2 cycles
- `W MOVE 0, VAR`: 5-7 bytes, 3-4 cycles

---

## Reference Manual

**Section:** §10.17
**Title:** Store zero

---

## See Also

- [CLR](clr.md) - Clear register
- [MOVE](move.md) - General move/store
- [BMOVE](bmove.md) - Block move/fill (bulk zeroing)
- [INIT](init.md) - Initialize operations
