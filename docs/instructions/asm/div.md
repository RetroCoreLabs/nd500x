# / - Divide (Operator Syntax)

## Overview

**Mnemonic:** `/`
**Function:** Divide (operator syntax for register-based division)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `tn / <operand>`

---

## Description

Divides the specified register by `<operand>` and stores quotient in that register. Operator syntax equivalent of DIV2.

Operation: `Rn = Rn / <operand>`

**Operands:** 1
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20 (5 data types × 4 registers)

| Data Type | Registers | Opcodes |
|-----------|-----------|---------|
| BY | 1-4 | 0x0078-0x007B |
| H | 1-4 | 0x007C-0x007F |
| W | 1-4 | 0x00E8-0x00EB |
| F | 1-4 | 0xFC4C-0xFC4F |
| D | 1-4 | 0xFC50-0xFC53 |

---

## Operands

### Operand 1 (Divisor)

The operand to divide the specified register by.

**Type:** Byte, Halfword, Word, Float, or Double Float
**Access:** Read

**Supported modes:**
- **LOCAL**, **RECORD**, **CONSTANT**, **REGISTER**, **PRE_INDEXED**, **ABSOLUTE**

---

## Trap Conditions

- **Addressing traps**, **Integer overflow (O)**, **Floating overflow/underflow (FO/FU)**, **Divide by zero (DZ)**

---

## Data Status Bits

- **Z, S, O, DZ** (integers), **FO, FU, DZ** (floats)

---

## Examples

### Example 1: Divide by constant

```assembly
        % Halve register value
        W1 / 2
```

### Example 2: Divide by variable

```assembly
        % Divide by divisor
        W2 / B.DIVISOR
```

### Example 3: Float division

```assembly
        % Normalize by total
        F3 / B.TOTAL
```

---

## Performance Notes

- **Typical:** 12-20 cycles
- **Note:** Same as DIV2

---

## Reference Manual

**Section:** §11.8
**Title:** Divide two operands

---

## See Also

- [DIV2](div2.md), [DIV3](div3.md), [DIV4](div4.md)
