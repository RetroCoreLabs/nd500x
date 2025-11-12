# MUL2 - Multiply Two Operands

## Overview

**Mnemonic:** `mul2`
**Function:** Multiply two operands (destructive multiply)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `t MUL2 <a>,<b>`

---

## Description

Multiplies the `<a>` operand by the `<b>` operand and stores the product in the `<a>` operand (destination). This is a destructive operation - the original value of `<a>` is overwritten.

**Operation:**
```
<a> = <a> * <b>
```

**Key Characteristics:**
- Destructive two-operand multiplication (first operand overwritten)
- 5 data types supported (BY, H, W, F, D)
- Integer overflow when upper half ≠ sign extension
- Float types may trap on overflow/underflow
- Essential for in-place scaling and accumulation
- Faster than MUL3 (fewer operand encodings, 4-7 cycles)
- Common in array indexing and coefficient scaling
- Overflow detection via O flag (integer) or trap (float)
- First operand must be writeable (not constant)

For integer types (BY, H, W), integer overflow occurs if the upper half of the double-length result is not equal to the sign extension of the lower half. For floating point types (F, D), overflow and underflow traps may occur.

The operands are assumed to have the same data type. This instruction is commonly used for in-place multiplication operations where the first operand can be modified.

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5 (one per data type)

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/5 | 0xFC5D | BY | Byte |
| 2/5 | 0xFC5E | H | Halfword |
| 3/5 | 0xFC5F | W | Word |
| 4/5 | 0xFC60 | F | Float |
| 5/5 | 0xFC61 | D | Double Float |

---

## Operands

### Operand 1 (Multiplicand/Destination)

The first operand - serves as both source (multiplicand) and destination. Result is stored here.

**Type:** Byte, Halfword, Word, Float, or Double Float
**Access:** Read/Write

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **REGISTER** - Integer register (I1-I4) or float register (A1-A4/D1-D4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

**Note:** CONSTANT mode not allowed for destination operands.

### Operand 2 (Multiplier)

The second operand (multiplier) - value to multiply operand 1 by.

**Type:** Same data type as operand 1
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer or float register
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

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

### Example 1: Multiply with register

```assembly
        % Multiply argument PROD by D4, store result in PROD
        D MUL2 ALT(B.PROD), D4
```

### Example 2: Scale value by constant

```assembly
        % Multiply local variable by 2
        W MUL2 B.COUNT, 2
```

### Example 3: Accumulate product in register

```assembly
        % Multiply register by memory value
        W MUL2 I1, B.FACTOR
```

### Example 4: Float multiplication

```assembly
        % Multiply float variable by coefficient
        F MUL2 B.RESULT, B.COEFFICIENT
```

### Example 5: Array element multiplication

```assembly
        % Multiply array element by value
        W MUL2 B.ARRAY(I2), I3
```

---

## Performance Notes

- **Typical cycles:** 4-7 cycles depending on addressing modes and data type
- **Best case:** 4 cycles (register to register, integer)
- **Worst case:** 7+ cycles (memory to memory with page fault)
- **Float/Double:** Additional cycles for floating point normalization and rounding

**Note:** Integer multiplication is significantly faster than floating point multiplication.

---

## Reference Manual

**Section:** §11.7
**Title:** Multiply two operands

---

## See Also

- [MUL3](mul3.md) - Multiply three operands (non-destructive)
- [MUL4](mul4.md) - Multiply with overflow to register
- [DIV2](div2.md) - Divide two operands
- [UMUL](umul.md) - Unsigned multiply
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
