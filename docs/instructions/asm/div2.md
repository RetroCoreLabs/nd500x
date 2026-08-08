# DIV2 - Divide Two Operands

## Overview

**Mnemonic:** `div2`
**Function:** Divide two operands (destructive divide)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `t DIV2 <a>,<b>`

---

## Description

Divides the `<a>` operand by the `<b>` operand and stores the quotient in the `<a>` operand (destination). This is a destructive operation - the original value of `<a>` is overwritten.

**Operation:**
```
<a> = <a> / <b>
```

**Key Characteristics:**
- Destructive two-operand division (first operand overwritten)
- 5 data types supported (BY, H, W, F, D)
- Integer division truncates toward zero
- Remainder has same sign as dividend
- Divide-by-zero trap (DZ) always occurs when divisor = 0
- Overflow only when MIN_INT / -1
- Slowest arithmetic operation (12-20 cycles)
- Essential for rate and ratio calculations
- First operand must be writeable (not constant)

For integer types (BY, H, W), the remainder (unless it is zero) has the same sign as the `<a>` operand - the quotient is truncated towards zero. Integer overflow occurs if and only if the largest possible negative integer is divided by -1.

Division by zero triggers a divide-by-zero (DZ) trap. For floating point types (F, D), overflow and underflow traps may also occur.

The operands are assumed to have the same data type. This instruction is commonly used for in-place division operations where the dividend can be modified.

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5 (one per data type)

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/5 | 0xFC62 | BY | Byte |
| 2/5 | 0xFC63 | H | Halfword |
| 3/5 | 0xFC64 | W | Word |
| 4/5 | 0xFC65 | F | Float |
| 5/5 | 0xFC66 | D | Double Float |

---

## Operands

### Operand 1 (Dividend/Destination)

The first operand - serves as both source (dividend) and destination. Result (quotient) is stored here.

**Type:** Byte, Halfword, Word, Float, or Double Float
**Access:** Read/Write

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **REGISTER** - Integer register (I1-I4) or float register (A1-A4/D1-D4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

**Note:** CONSTANT mode not allowed for destination operands.

### Operand 2 (Divisor)

The second operand (divisor) - value to divide operand 1 by.

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
- **Integer overflow (O):** Largest negative integer divided by -1 (for BY, H, W types)
- **Floating overflow (FO):** Result too large to represent (for F, D types)
- **Floating underflow (FU):** Result too small to represent (for F, D types)
- **Divide by zero (DZ):** Operand 2 = 0

---

## Data Status Bits

- **Z (Zero):** Set if quotient = 0, cleared otherwise
- **S (Sign):** Set to sign bit of quotient
- **O (Overflow):** Set if integer overflow (BY, H, W types)
- **FO (Floating Overflow):** Set if floating overflow (F, D types)
- **FU (Floating Underflow):** Set if floating underflow (F, D types)
- **DZ (Divide by Zero):** Set if operand 2 = 0

---

## Examples

### Example 1: Divide with descriptor addressing

```assembly
        % Divide local float KVOT by R1st element of array LIST
        F DIV2 B.KVOT, ALT(DESC(B.LIST)(R1))
```

### Example 2: Divide by constant

```assembly
        % Divide local variable by 2
        W DIV2 B.COUNT, 2
```

### Example 3: Register division

```assembly
        % Divide register by memory value
        W DIV2 I1, B.DIVISOR
```

### Example 4: Float division

```assembly
        % Divide float variable by coefficient
        F DIV2 B.RESULT, B.COEFFICIENT
```

### Example 5: Array element division

```assembly
        % Divide array element by value
        W DIV2 B.ARRAY(I2), I3
```

---

## Performance Notes

- **Typical cycles:** 12-20 cycles depending on addressing modes and data type
- **Best case:** 12 cycles (register to register, integer)
- **Worst case:** 20+ cycles (memory to memory with page fault)
- **Float/Double:** Additional cycles for floating point normalization

**Note:** Division is significantly slower than multiplication. Integer division is faster than floating point division.

---

## Reference Manual

**Section:** §11.8
**Title:** Divide two operands

---

## See Also

- [DIV3](div3.md) - Divide three operands (non-destructive)
- [DIV4](div4.md) - Divide with remainder to register
- [MUL2](mul2.md) - Multiply two operands
- [UDIV](udiv.md) - Unsigned divide
- [REM](rem.md) - Remainder
- [Trap System](../../ND-500-TRAPS.md)
