# - - Subtract (Operator Syntax)

## Overview

**Mnemonic:** `-`
**Function:** Subtract (operator syntax for register-based subtraction)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `tn - <operand>`

---

## Description

Subtracts the `<operand>` from the specified register and stores the result in that register. Operator syntax equivalent of SUB2.

**Operation:**
```
Rn = Rn - <operand>
```

**Key Characteristics:**
- Register-based subtraction (implicit destination in Rn)
- Operator syntax (`-`) for natural mathematical notation
- Supports 5 data types (BY, H, W, F, D)
- Works with 4 index registers (I1-I4)
- Sets borrow/carry (C) and overflow (V) flags for integers
- Floating-point overflow/underflow traps for F/D types
- More concise than SUB2 instruction for register operations
- Common in loop decrements and difference calculations

**Operands:** 1
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20 (5 data types × 4 registers)

| Data Type | Registers | Opcodes |
|-----------|-----------|---------|
| BY | 1-4 | 0x0060-0x0063 |
| H | 1-4 | 0x0064-0x0067 |
| W | 1-4 | 0x0068-0x006B |
| F | 1-4 | 0xFC3C-0xFC3F |
| D | 1-4 | 0xFC40-0xFC43 |

---

## Operands

### Operand 1 (Subtrahend)

The operand to subtract from the specified register.

**Type:** Byte, Halfword, Word, Float, or Double Float
**Access:** Read

**Supported modes:**
- **LOCAL**, **RECORD**, **CONSTANT**, **REGISTER**, **PRE_INDEXED**, **ABSOLUTE**

---

## Trap Conditions

- **Addressing traps**, **Integer overflow (O)**, **Floating overflow/underflow (FO/FU)**

---

## Data Status Bits

- **Z, S, O, C** (integers), **FO, FU** (floats)

---

## Examples

### Example 1: Subtract constant

```assembly
        % Decrement register by 1
        W1 - 1
```

### Example 2: Subtract variable

```assembly
        % Subtract offset from register
        W2 - B.OFFSET
```

### Example 3: Float subtraction

```assembly
        % Subtract delta from float register
        F3 - B.DELTA
```

---

## Performance Notes

- **Typical:** 3-6 cycles
- **Note:** Same as SUB2

---

## Reference Manual

**Section:** §11.6
**Title:** Subtract two operands

---

## See Also

- [SUB2](sub2.md), [SUB3](sub3.md), [+](add.md)
