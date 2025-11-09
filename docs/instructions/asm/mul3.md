# MUL3 - Multiply Three Operands

## Overview

**Mnemonic:** `mul3`
**Function:** Multiply two operands, store product in third operand (non-destructive)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `t MUL3 <a>,<b>,<c>`

---

## Description

Multiplies the `<a>` operand by the `<b>` operand and stores the product in the `<c>` operand (destination). This is a non-destructive three-operand multiply - neither source operand is modified.

For integer types (BY, H, W), integer overflow occurs if the upper half of the double-length result is not equal to the sign extension of the lower half. For floating point types (F, D), overflow and underflow traps may occur.

The operands are assumed to have the same data type. This instruction is commonly used when you need to preserve both multiplicands while computing their product.

**Operands:** 3
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5 (one per data type)

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/5 | 0xFC71 | BY | Byte |
| 2/5 | 0xFC72 | H | Halfword |
| 3/5 | 0xFC73 | W | Word |
| 4/5 | 0xFC74 | F | Float |
| 5/5 | 0xFC75 | D | Double Float |

---

## Operands

### Operand 1 (Multiplicand)

The first source operand (multiplicand). This operand is not modified.

**Type:** Byte, Halfword, Word, Float, or Double Float
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer register (I1-I4) or float register (A1-A4/D1-D4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 2 (Multiplier)

The second source operand (multiplier). This operand is not modified.

**Type:** Same data type as operand 1
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer or float register
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 3 (Product/Destination)

The destination operand where the product is stored.

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
- **Integer overflow (O):** Upper half of double-length result ≠ sign extension of lower half (for BY, H, W types)
- **Floating overflow (FO):** Result too large to represent (for F, D types)
- **Floating underflow (FU):** Result too small to represent (for F, D types)

---

## Data Status Bits

- **Z (Zero):** Set if product = 0, cleared otherwise
- **S (Sign):** Set to sign bit of product
- **O (Overflow):** Set if integer overflow (BY, H, W types)
- **FO (Floating Overflow):** Set if floating overflow (F, D types)
- **FU (Floating Underflow):** Set if floating underflow (F, D types)

---

## Examples

### Example 1: Multiply array elements

```assembly
        % Multiply second and third elements of word array,
        % store product in first element (array base in R2)
        W MUL3 R2.2, R2.3, R2.1
```

### Example 2: Multiply local variables

```assembly
        % Multiply two local variables, store in third
        W MUL3 B.WIDTH, B.HEIGHT, B.AREA
```

### Example 3: Multiply with constant

```assembly
        % Multiply variable by constant factor
        W MUL3 B.VALUE, 10, B.SCALED_VALUE
```

### Example 4: Float multiplication preserving sources

```assembly
        % Multiply two float values, preserve sources
        F MUL3 B.COEFFICIENT, B.INPUT, B.OUTPUT
```

### Example 5: Register-based calculation

```assembly
        % Compute product of two registers into third
        W MUL3 I1, I2, I3
```

---

## Performance Notes

- **Typical cycles:** 5-8 cycles depending on addressing modes and data type
- **Best case:** 5 cycles (register to register, integer)
- **Worst case:** 8+ cycles (memory to memory with page fault)
- **Float/Double:** Additional cycles for floating point normalization and rounding

**Note:** Integer multiplication is significantly faster than floating point multiplication. MUL3 is slightly slower than MUL2 due to the extra operand fetch/store.

---

## Reference Manual

**Section:** §11.11
**Title:** Multiply three operands

---

## See Also

- [MUL2](mul2.md) - Multiply two operands (destructive)
- [MUL4](mul4.md) - Multiply with overflow to register
- [DIV3](div3.md) - Divide three operands
- [UMUL](umul.md) - Unsigned multiply
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
