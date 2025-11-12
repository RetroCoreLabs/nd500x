# CIND - Calculate Index

## Overview

**Mnemonic:** `cind`
**Function:** Calculate multi-dimensional array index
**Class:** SYSTEM
**Privilege:** supervisor
**Format:** `tn CIND <index>, <lower>, <upper>`

---

## Description

Calculates the address of an element in a multi-dimensional array with bounds checking. The instruction multiplies the specified register by the dimension range (upper - lower + 1), adds the index value, and stores the result back in the register. If the index is outside the specified bounds, the K flag is set and an Illegal Index trap occurs.

**Operation:**
```
Rn = Rn * (upper - lower + 1) + index
if (index < lower) or (index > upper):
    K = 1
    IllegalIndexTrap
else:
    K = 0
```

**Key Characteristics:**
- Supervisor-only multi-dimensional array indexing
- Hardware bounds checking (automatic range validation)
- Combined multiply-add with bounds test
- Illegal Index trap when out of range
- K flag set/cleared based on bounds check
- 12 variants (BY1-BY4, H1-H4, W1-W4)
- Essential for compiler-generated array access
- 4-6 cycles (faster than separate checks)
- Supports nested array calculations

This privileged instruction is used for safe array indexing in system code and compilers. It combines index calculation with automatic bounds validation, eliminating the need for separate range checks. The instruction supports byte, halfword, word, float, and double data types.

**Common Use Cases:**
- Multi-dimensional array address calculation
- Compiler-generated array bounds checking
- Safe array indexing in system routines
- Runtime index validation

**Operands:** 3
**Variants:** 12 opcodes

---

## Variants

| Variant | Opcode | Data Type | Assembly |
|---------|--------|-----------|----------|
| 1-4 | 0xFD14-0xFD17 | BY | BYn CIND |
| 5-8 | 0xFD18-0xFD1B | H | Hn CIND |
| 9-12 | 0x00B0-0x00B3 | W | Wn CIND |

---

## Operands

**Operand 1** (Index, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches prefix
- **Role**: Current index value to validate and add

**Operand 2** (Lower Bound, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches prefix
- **Role**: Minimum valid index value

**Operand 3** (Upper Bound, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches prefix
- **Role**: Maximum valid index value

**Result**: Calculated array offset in Rn, K flag set if out of bounds

---

## Trap Conditions
- **Addressing traps**: Invalid address or protection violation
- **Privilege violation**: If executed in user mode
- **Integer overflow (O)**: Result too large for register
- **Illegal index (IX)**: Index outside [lower, upper] range

---

## Data Status Bits
- **Z**: Set if result = 0
- **S**: Set if result sign bit = 1
- **O**: Set if overflow occurred
- **K**: Set if index out of bounds

---

## Examples

### Example 1: Single dimension array
```assembly
% Calculate index for ARRAY(I) where I in 1..10
H1 CIND I_VAR, 1, 10
```

### Example 2: Multi-dimensional array ARR(1..3, 5..10, 2..9)
```assembly
% Load W1 with address of ARR(IX1, IX2, IX3)
H1 CIND IX1, 1, 3
H1 CIND IX2, 5, 10
H1 CIND IX3, 2, 9
```

### Example 3: With bounds check handling
```assembly
W1 CIND INDEX, 0, 99
IFKGO OUT_OF_BOUNDS
% Valid index, continue
GO PROCESS
OUT_OF_BOUNDS:
% Handle error
W1 MOVE -1, I1
```

### Example 4: Byte array indexing
```assembly
BY1 CIND BYTE_IDX, 0, 255
IF-KGO VALID
% Invalid index
VALID:
% Process byte array element
```

### Example 5: Zero-based array
```assembly
% Array with range [0..19]
W1 CLR              % Start with 0 offset
W1 CIND ELEMENT, 0, 19
```

### Example 6: Nested array calculation
```assembly
% Matrix[ROW][COL], ROW: 1-5, COL: 1-10
W1 CLR
W1 CIND ROW, 1, 5
W1 CIND COL, 1, 10
% W1 now contains linear offset
```

### Example 7: Dynamic bounds from variables
```assembly
% Array with runtime-determined bounds
W1 CLR
W1 CIND INDEX, MIN_VAL, MAX_VAL
IFKGO INDEX_ERROR
% Continue with valid index
```

---

## Performance Notes
- Execution: 4-6 cycles (privileged)
- Bounds check adds 1-2 cycles
- Hardware multiply for dimension calculation
- Requires supervisor mode

---

## Reference Manual
**Section:** §15.9
**Title:** Calculate Index

---

## See Also
- [IXI](ixi.md) - Index extension
- [AXI](axi.md) - Arithmetic index
- [LIND](lind.md) - Load index
