# SUB3 - Subtract Three Operands

## Overview

**Mnemonic:** `sub3`
**Function:** Subtract two operands, store difference in third operand (non-destructive)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `t SUB3 <a>,<b>,<c>`

---

## Description

Subtracts the `<b>` operand from the `<a>` operand and stores the difference in the `<c>` operand (destination). This is a non-destructive three-operand subtract - neither source operand is modified.

**Operation:**
```
<c> = <a> - <b>
```

**Key Characteristics:**
- Non-destructive three-operand subtraction (sources preserved)
- Supports 5 data types (BY, H, W, F, D)
- Essential for distance/delta calculations
- More flexible than SUB2 (explicit destination)
- Enables efficient difference computations
- Sets Z, S, C, V flags for conditionals
- Slightly slower than SUB2 (extra operand encoding)
- Common in mathematical and scientific code

For integer types (BY, H, W), carry and overflow flags are set appropriately. For floating point types (F, D), overflow and underflow traps may occur.

The operands are assumed to have the same data type. This instruction is commonly used when you need to preserve both minuend and subtrahend while computing their difference.

**Operands:** 3
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5 (one per data type)

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/5 | 0xFC6C | BY | Byte |
| 2/5 | 0xFC6D | H | Halfword |
| 3/5 | 0xFC6E | W | Word |
| 4/5 | 0xFC6F | F | Float |
| 5/5 | 0xFC70 | D | Double Float |

---

## Operands

### Operand 1 (Minuend)

The first source operand (minuend - value to subtract from). This operand is not modified.

**Type:** Byte, Halfword, Word, Float, or Double Float
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer register (I1-I4) or float register (A1-A4/D1-D4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 2 (Subtrahend)

The second source operand (subtrahend - value to subtract). This operand is not modified.

**Type:** Same data type as operand 1
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer or float register
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 3 (Difference/Destination)

The destination operand where the difference is stored.

**Type:** Same data type as operands 1 and 2
**Access:** Write

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **REGISTER** - Integer or float register
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

**Note:** CONSTANT mode not allowed for destination operands.

---

## Trap Conditions

- **Addressing traps:** Invalid address, descriptor range violation, page fault, protection violation
- **Integer overflow (O):** Signed integer subtraction overflow (for BY, H, W types)
- **Floating overflow (FO):** Result too large to represent (for F, D types)
- **Floating underflow (FU):** Result too small to represent (for F, D types)

---

## Data Status Bits

- **Z (Zero):** Set if difference = 0, cleared otherwise
- **S (Sign):** Set to sign bit of difference
- **O (Overflow):** Set if integer overflow (BY, H, W types)
- **C (Carry):** Set if carry from most significant bit (integer types only)
- **FO (Floating Overflow):** Set if floating overflow (F, D types)
- **FU (Floating Underflow):** Set if floating underflow (F, D types)

---

## Examples

### Example 1: Subtract byte arguments

```assembly
        % Subtract X2 from X1, store difference in DIFF
        B SUB3 IND(B.X1), IND(B.X2), B.DIFF
```

### Example 2: Subtract local variables

```assembly
        % Subtract two local variables, store in third
        W SUB3 B.MINUEND, B.SUBTRAHEND, B.DIFFERENCE
```

### Example 3: Subtract constant

```assembly
        % Subtract constant from variable
        W SUB3 B.VALUE, 5, B.RESULT
```

### Example 4: Float subtraction preserving sources

```assembly
        % Subtract two float values, preserve sources
        F SUB3 B.A, B.B, B.DIFFERENCE
```

### Example 5: Register-based calculation

```assembly
        % Compute difference of two registers into third
        W SUB3 I1, I2, I3
```

---

## Performance Notes

- **Typical cycles:** 5-8 cycles depending on addressing modes and data type
- **Best case:** 5 cycles (register to register, integer)
- **Worst case:** 8+ cycles (memory to memory with page fault)
- **Float/Double:** Additional cycles for floating point normalization

**Note:** Integer subtraction is significantly faster than floating point subtraction. SUB3 is slightly slower than SUB2 due to the extra operand fetch/store.

---

## Reference Manual

**Section:** §11.10
**Title:** Subtract three operands

---

## See Also

- [SUB2](sub2.md) - Subtract two operands (destructive)
- [SUBC](subc.md) - Subtract with carry (multi-precision)
- [ADD3](add3.md) - Add three operands
- [NEG](neg.md) - Negate
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
