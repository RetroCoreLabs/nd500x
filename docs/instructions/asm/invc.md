# INVC - Invert with Carry Add

## Overview

**Mnemonic:** `invc`
**Function:** One's complement plus carry bit
**Class:** LOGICAL
**Privilege:** user
**Format:** `Wn INVC`

---

## Description

Performs a bitwise NOT operation on the contents of a specified word register, then adds the carry flag to the result. This instruction is specifically designed for multi-precision arithmetic operations, particularly for negating multi-word values.

The operation is essential for propagating carries when performing two's complement negation on values larger than 32 bits.

**Operation:**
```
Rn = ~Rn + C
result = 0 → Z flag
result.signbit → S flag
carry from addition → C flag
overflow from addition → O flag
```

**Common Use Cases:**
- Multi-precision negation (64-bit, 128-bit, etc.)
- Extended precision arithmetic
- High-word complement in multi-word operations
- Carry propagation in extended arithmetic

**Operands:** 0 (operates on implicit register)
**Variants:** 4 opcodes (word only, registers I1-I4)

---

## Variants

| Variant | Opcode | Type | Register | Assembly |
|---------|--------|------|----------|----------|
| 1 | 0xFF10 | W | I1 | W1 INVC |
| 2 | 0xFF11 | W | I2 | W2 INVC |
| 3 | 0xFF12 | W | I3 | W3 INVC |
| 4 | 0xFF13 | W | I4 | W4 INVC |

**Note:** INVC is only available for word (W) data type, not for BI, BY, or H types.

---

## Operands

**Register Rn** (Destination, Read-Modify-Write):
- **Implicit operand**: Specified by instruction variant (I1-I4)
- **Data type**: Word (32-bit) only
- **Role**: Register to be inverted and incremented by carry

---

## Trap Conditions

- **Integer overflow (O)**: Set when the addition of carry produces overflow

---

## Data Status Bits

- **Z (Zero)**: Set if result = 0, cleared otherwise
- **S (Sign)**: Set to result's sign bit
- **C (Carry)**: Set if addition of carry produces carry-out
- **V (Overflow)**: Set if overflow occurs during carry addition

---

## Examples

### Example 1: Basic INVC operation
```assembly
        % Invert W2 and add carry
        W2 INVC
```

### Example 2: 64-bit negation (two's complement)
```assembly
        % Negate 64-bit value in W1:W2 (W2 = high, W1 = low)
        W1 NEG              % Negate low word, sets carry
        W2 INVC             % Negate high word with carry propagation
```

### Example 3: 96-bit negation
```assembly
        % Negate 96-bit value in W1:W2:W3
        W1 NEG              % Negate lowest word
        W2 INVC             % Propagate to middle word
        W3 INVC             % Propagate to highest word
```

### Example 4: 128-bit negation
```assembly
        % Negate 128-bit value in W1:W2:W3:W4
        W1 NEG              % Negate word 0 (lowest)
        W2 INVC             % Negate word 1 with carry
        W3 INVC             % Negate word 2 with carry
        W4 INVC             % Negate word 3 (highest)
```

### Example 5: Conditional multi-word negate
```assembly
        % Negate if negative flag set
        IF<GO SKIP
        W1 NEG
        W2 INVC
SKIP:
```

### Example 6: Extended precision absolute value
```assembly
        % 64-bit absolute value
        W2 TEST             % Check sign of high word
        IF>=GO POSITIVE
        W1 NEG              % Negate if negative
        W2 INVC
POSITIVE:
```

### Example 7: Multi-precision subtraction helper
```assembly
        % Part of extended precision subtraction
        % After low-word subtraction:
        W3 SUBC B.HIGH1, B.HIGH2  % Subtract with borrow
        % Alternative using INVC for negation:
        W1 NEG
        W2 INVC
        W1 ADD B.VAL_LOW
        W2 ADDC B.VAL_HIGH
```

---

## Performance Notes

- **Execution**: 1 cycle
- **Word only**: Not available for BI, BY, or H data types
- **Carry dependency**: Result depends on carry flag state from previous operation
- **Overflow trap**: May trap if overflow occurs during carry addition

**Multi-precision negation pattern:**
```assembly
% Always start with NEG on lowest word, then INVC on higher words
W1 NEG              % Sets carry appropriately
W2 INVC             % Uses carry from NEG
W3 INVC             % Uses carry from previous INVC
```

**Why INVC exists:**
- For single-word: `INV` + `INC` = two's complement
- For multi-word: `NEG` (low) + `INVC` (high words) = extended two's complement
- The carry from NEG propagates through INVC chain

**Common mistake:**
```assembly
% WRONG - Don't use INV for high words in multi-precision negate
W1 NEG
W2 INV              % ERROR: Doesn't add carry!
W2 INC              % This won't handle carry correctly

% CORRECT - Use INVC
W1 NEG
W2 INVC             % Correctly adds carry from NEG
```

---

## Reference Manual

**Section:** §10.14
**Title:** Invert with carry add

---

## See Also

- [INV](inv.md) - Bitwise invert (one's complement, no carry)
- [NEG](neg.md) - Two's complement negation (single word)
- [SUBC](subc.md) - Subtract with carry (multi-precision subtraction)
- [ADDC](addc.md) - Add with carry (multi-precision addition)
