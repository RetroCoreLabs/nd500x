# * - Multiply (Operator Syntax)

## Overview

**Mnemonic:** `*`
**Function:** Multiply (operator syntax for register-based multiplication)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `tn * <operand>`

---

## Description

Multiplies the specified register by `<operand>` and stores product in that register. Operator syntax equivalent of MUL2.

**Operation:**
```
Rn = Rn * <operand>
```

**Key Characteristics:**
- Register-based multiplication (implicit destination in Rn)
- Operator syntax (`*`) for natural mathematical notation
- Supports 5 data types (BY, H, W, F, D)
- Works with 4 index registers (I1-I4)
- Integer overflow (V) flag set if product exceeds register size
- Floating-point overflow/underflow traps for F/D types
- More concise than MUL2 instruction for register operations
- Common in scaling, array indexing, and mathematical computations

**Operands:** 1
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20 (5 data types × 4 registers)

| Data Type | Registers | Opcodes |
|-----------|-----------|---------|
| BY | 1-4 | 0x006C-0x006F |
| H | 1-4 | 0x0070-0x0073 |
| W | 1-4 | 0x0074-0x0077 |
| F | 1-4 | 0xFC44-0xFC47 |
| D | 1-4 | 0xFC48-0xFC4B |

---

## Operands

### Operand 1 (Multiplier)

The operand to multiply the specified register by.

**Type:** Byte, Halfword, Word, Float, or Double Float
**Access:** Read

**Supported modes:**
- **LOCAL**, **RECORD**, **CONSTANT**, **REGISTER**, **PRE_INDEXED**, **ABSOLUTE**

---

## Trap Conditions

- **Addressing traps**, **Integer overflow (O)**, **Floating overflow/underflow (FO/FU)**

---

## Data Status Bits

- **Z, S, O** (integers), **FO, FU** (floats)

---

## Examples

### Example 1: Multiply by constant

```assembly
        % Double register value
        W1 * 2
```

### Example 2: Scale by variable

```assembly
        % Multiply by scaling factor
        W2 * B.SCALE
```

### Example 3: Float multiplication

```assembly
        % Multiply by coefficient
        F3 * B.COEFFICIENT
```

---

## Performance Notes

- **Typical:** 4-7 cycles
- **Note:** Same as MUL2

---

## Reference Manual

**Section:** §11.7
**Title:** Multiply two operands

---

## See Also

- [MUL2](mul2.md), [MUL3](mul3.md), [MUL4](mul4.md)
