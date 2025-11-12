# LIND - Load Index

## Overview

**Mnemonic:** `lind`
**Function:** Load array index with bounds checking
**Class:** SYSTEM
**Privilege:** supervisor
**Format:** `tn LIND <index>, <lower>, <upper>`

---

## Description

Loads an array index value into a specified register while validating it against lower and upper bounds. This privileged instruction provides automatic range checking for array access. If the index is outside the specified bounds, the K flag is set and an Illegal Index trap occurs. Otherwise, the K flag is cleared and the index is loaded into the register.

**Key Characteristics:**
- Supervisor-only bounds-checked index load
- Automatic range validation (lower ≤ index ≤ upper)
- K flag indicates out-of-bounds condition
- Illegal index trap (IX) on range violation
- Essential for safe array access
- 12 variants (BY, H, W types × 4 registers)
- Faster than separate load + compare
- Common in compiler-generated array code

This instruction is commonly used by compilers for safe array indexing and by system routines that require validated index operations. It supports byte, halfword, word, float, and double data types through different variants.

**Operation:**
```
index → Rn
if (index < lower) or (index > upper):
    K = 1
    IllegalIndexTrap
else:
    K = 0
```

**Common Use Cases:**
- Array bounds checking during index load
- Compiler-generated safe array access
- Loop index validation
- Runtime index verification

**Operands:** 3
**Variants:** 12 opcodes

---

## Variants

| Variant | Opcode | Data Type | Assembly |
|---------|--------|-----------|----------|
| 1-4 | 0xFD0C-0xFD0F | BY | BYn LIND |
| 5-8 | 0xFD10-0xFD13 | H | Hn LIND |
| 9-12 | 0x00AC-0x00AF | W | Wn LIND |

---

## Operands

**Operand 1** (Index, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches prefix
- **Role**: Index value to load and validate

**Operand 2** (Lower Bound, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches prefix
- **Role**: Minimum valid index value

**Operand 3** (Upper Bound, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches prefix
- **Role**: Maximum valid index value

**Result**: Index loaded into Rn, K flag set if out of bounds

---

## Trap Conditions
- **Addressing traps**: Invalid address or protection violation
- **Privilege violation**: If executed in user mode
- **Illegal index (IX)**: Index outside [lower, upper] range

---

## Data Status Bits
- **Z**: Set if index = 0
- **S**: Set if index sign bit = 1
- **K**: Set if index out of bounds

---

## Examples

### Example 1: Simple array index load
```assembly
BY2 LIND IX, -10, 10
```

### Example 2: Word array with validation
```assembly
W1 LIND ARRAY_INDEX, 0, 99
IFKGO INDEX_ERROR
% Valid index in W1
```

### Example 3: Loop counter validation
```assembly
LOOP:
        H1 LIND COUNTER, 1, MAX_ITER
        IF-KGO PROCESS
        % Counter out of range
        GO EXIT
PROCESS:
        % Process with valid counter
```

### Example 4: String bounds check
```assembly
% Validate string index before access
BY1 LIND STR_POS, 0, STR_LENGTH
IFKGO STR_ERROR
% Safe to access string[BY1]
```

### Example 5: Multi-dimensional array index
```assembly
% Load first dimension index
W1 LIND ROW, 0, MAX_ROWS
IFKGO ERROR
% Load second dimension index
W2 LIND COL, 0, MAX_COLS
IFKGO ERROR
```

### Example 6: Dynamic bounds
```assembly
% Array with runtime-determined bounds
W1 LIND ELEMENT, MIN_BOUND, MAX_BOUND
IFKGO OUT_OF_RANGE
% Element is valid
```

### Example 7: Table lookup validation
```assembly
LOOKUP:
        BY1 LIND TABLE_IDX, 0, 255
        IF-KGO FETCH
        % Invalid table index
        W1 MOVE -1, I1
        RET
FETCH:
        % Fetch from table
        RET
```

---

## Performance Notes
- Execution: 3-5 cycles (privileged)
- Bounds check adds 1-2 cycles
- Faster than separate load + compare operations
- Requires supervisor mode

---

## Reference Manual
**Section:** §15.8
**Title:** Load index

---

## See Also
- [CIND](cind.md) - Calculate index
- [IXI](ixi.md) - Index extension
- [AXI](axi.md) - Arithmetic index
