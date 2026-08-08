# DIV3 - Divide Three Operands

## Overview

**Mnemonic:** `div3`
**Function:** Divide two operands, store quotient in third operand (non-destructive)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `t DIV3 <a>,<b>,<c>`

---

## Description

Divides the `<a>` operand by the `<b>` operand and stores the quotient in the `<c>` operand (destination). This is a non-destructive three-operand divide - neither source operand is modified.

**Operation:**
```
<c> = <a> / <b>
```

**Key Characteristics:**
- Non-destructive three-operand division (sources preserved)
- Supports 5 data types (BY, H, W, F, D)
- Truncates toward zero (integer division)
- Remainder has same sign as dividend
- Divide-by-zero trap (DZ) when divisor = 0
- Overflow only when MIN_INT / -1
- Slowest arithmetic operation (13-21 cycles)
- Essential for rate and ratio calculations

For integer types (BY, H, W), the remainder (unless it is zero) has the same sign as the `<a>` operand - the quotient is truncated towards zero. Integer overflow occurs if and only if the largest possible negative integer is divided by -1.

Division by zero triggers a divide-by-zero (DZ) trap. For floating point types (F, D), overflow and underflow traps may also occur.

The operands are assumed to have the same data type. This instruction is commonly used when you need to preserve both the dividend and divisor while computing their quotient.

**Operands:** 3
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5 (one per data type)

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/5 | 0xFC76 | BY | Byte |
| 2/5 | 0xFC77 | H | Halfword |
| 3/5 | 0xFC78 | W | Word |
| 4/5 | 0xFC79 | F | Float |
| 5/5 | 0xFC7A | D | Double Float |

---

## Operands

### Operand 1 (Dividend)

The first source operand (dividend). This operand is not modified.

**Type:** Byte, Halfword, Word, Float, or Double Float
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer register (I1-I4) or float register (A1-A4/D1-D4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 2 (Divisor)

The second source operand (divisor). This operand is not modified.

**Type:** Same data type as operand 1
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer or float register
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 3 (Quotient/Destination)

The destination operand where the quotient is stored.

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

### Example 1: Divide with pointer indirection

```assembly
        % Divide float value at PTR by F1, store quotient in R.Q
        F DIV3 IND(PTR), F1, R.Q
```

### Example 2: Divide local variables

```assembly
        % Divide two local variables, store in third
        W DIV3 B.DIVIDEND, B.DIVISOR, B.QUOTIENT
```

### Example 3: Divide with constant

```assembly
        % Divide variable by constant factor
        W DIV3 B.VALUE, 10, B.SCALED_VALUE
```

### Example 4: Float division preserving sources

```assembly
        % Divide two float values, preserve sources
        F DIV3 B.NUMERATOR, B.DENOMINATOR, B.RESULT
```

### Example 5: Register-based calculation

```assembly
        % Compute quotient of two registers into third
        W DIV3 I1, I2, I3
```

---

## Performance Notes

- **Typical cycles:** 13-21 cycles depending on addressing modes and data type
- **Best case:** 13 cycles (register to register, integer)
- **Worst case:** 21+ cycles (memory to memory with page fault)
- **Float/Double:** Additional cycles for floating point normalization

**Note:** Division is significantly slower than multiplication. Integer division is faster than floating point division. DIV3 is slightly slower than DIV2 due to the extra operand fetch/store.

---

## Reference Manual

**Section:** §11.12
**Title:** Divide three operands

---

## See Also

- [DIV2](div2.md) - Divide two operands (destructive)
- [DIV4](div4.md) - Divide with remainder to register
- [MUL3](mul3.md) - Multiply three operands
- [UDIV](udiv.md) - Unsigned divide
- [REM](rem.md) - Remainder
- [Trap System](../../ND-500-TRAPS.md)
