# CIND - Calculate Index

## Overview

**Mnemonic:** `cind`
**Function:** Cind
**Class:** SYSTEM
**Privilege:** user

**Format:** `{prefix}{register} CIND <index>,<lower>,<upper>`

---

## Description

Calculate and validate an array index with bounds checking. The CIND instruction computes an offset into an array while simultaneously verifying that the index falls within the specified lower and upper bounds.

**Operation:**
```
if (index < lower) OR (index > upper) then
    TRAP (INDEX_OUT_OF_BOUNDS)
else
    result ← (index - lower) × element_size
```

This instruction is essential for safe array indexing in high-level language implementations, providing hardware-level bounds checking.

**Operands:** 3
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/20 | 0xFD14 | BY | 1 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 2/20 | 0xFD15 | BY | 2 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 3/20 | 0xFD16 | BY | 3 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 4/20 | 0xFD17 | BY | 4 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 5/20 | 0xFD18 | H | 1 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 6/20 | 0xFD19 | H | 2 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 7/20 | 0xFD1A | H | 3 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 8/20 | 0xFD1B | H | 4 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 9/20 | 0x00B0 | W | 1 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 10/20 | 0x00B1 | W | 2 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 11/20 | 0x00B2 | W | 3 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 12/20 | 0x00B3 | W | 4 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 13/20 | 0xFFD0 | F | 1 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 14/20 | 0xFFD1 | F | 2 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 15/20 | 0xFFD2 | F | 3 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 16/20 | 0xFFD3 | F | 4 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 17/20 | 0xFFD4 | D | 1 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 18/20 | 0xFFD5 | D | 2 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 19/20 | 0xFFD6 | D | 3 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 20/20 | 0xFFD7 | D | 4 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |

---

## Operands

### Operand 1: Index Value
The array index to be validated and converted to an offset.

**Supported modes:**
- **LOCAL:** `B.displacement` - Index stored in local variable
- **RECORD:** `R.displacement` - Index stored in record field
- **CONSTANT:** `value` - Immediate index value
- **REGISTER:** `Rn` - Index in register
- **PRE_INDEXED:** `B.array(Rn)` - Indexed access to index
- **ABSOLUTE:** `address` - Index at absolute address

### Operand 2: Lower Bound
The minimum valid index value (inclusive).

**Supported modes:** Same as Operand 1

### Operand 3: Upper Bound
The maximum valid index value (inclusive).

**Supported modes:** Same as Operand 1

---

## Trap Conditions

- **INDEX_OUT_OF_BOUNDS (Bit 8):** Raised when `index < lower` or `index > upper`
- **OPERAND_ERROR (Bit 5):** Invalid addressing mode combination

---

## Data Status Bits

- **Z (Zero):** Set if result offset is zero (index == lower)
- **S (Sign):** Set if result is negative (shouldn't occur with valid bounds)
- **O (Overflow):** Set if bounds calculation overflows
- **K (Flag):** Preserved

---

## Examples

### Example 1: Basic Array Indexing

```assembly
        W1 := 5                 ; Index value
        W1 CIND 0, 9            ; Check bounds 0-9, calculate offset
        W2 := B.ARRAY(W1)       ; Access array element
```

**Explanation:** Validates that index 5 is within bounds [0,9], calculates offset (5-0)×element_size, stores in W1, then accesses the array element.

### Example 2: Loop with CIND

```assembly
        W1 := 0                 ; Initialize loop counter
loop:
        W1 CIND 0, 99           ; Validate 0 <= index <= 99
        W2 := B.DATA(W1)        ; Access array element
        ; Process W2...
        W1 ADD 1                ; Increment counter
        IF < GO loop            ; Continue if not done
```

**Explanation:** Iterates through array DATA[0..99] with automatic bounds checking on each iteration.

### Example 3: Floating-Point Index Calculation

```assembly
        F1 := B.FLOAT_INDEX     ; Load floating-point index
        F1 CIND 0.0, 99.9       ; Validate float index bounds
        W2 := B.ARRAY(F1)       ; Use validated index
```

**Explanation:** Demonstrates CIND with floating-point index and bounds (using variant 13/20 with F prefix).

---

## Performance Notes

- **Typical cycles:** 8-12 cycles (depends on addressing modes)
- **Best case:** 8 cycles (register operands, bounds valid)
- **Worst case:** 12+ cycles (complex addressing + trap handling if bounds violated)
- **Trap overhead:** +20-50 cycles if INDEX_OUT_OF_BOUNDS trap occurs

**Optimization tips:**
- Use CONSTANT addressing for bounds when possible (faster than memory access)
- Place frequently-accessed bounds in registers
- Consider LIND for unchecked indexing when bounds are guaranteed

---

## Reference Manual

**Section:** §15.9
**Title:** Calculate Index
**Page:** 276

---

## See Also

- [LIND instruction](lind.md) - Load index (no bounds checking)
- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
