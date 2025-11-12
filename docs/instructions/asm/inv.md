# INV - Bitwise Invert (One's Complement)

## Overview

**Mnemonic:** `inv`
**Function:** One's complement (bitwise NOT) of register
**Class:** LOGICAL
**Privilege:** user
**Format:** `tn INV`

---

## Description

Performs a bitwise NOT operation on the contents of a specified register, storing the result back in the register. This is the one's complement operation where each bit is inverted (0 becomes 1, 1 becomes 0).

**Key Characteristics:**
- One's complement (bitwise NOT) operation
- Single-cycle execution (fastest logic operation)
- 16 variants (BI, BY, H, W × 4 registers)
- Upper bits cleared for sub-word types
- Essential for bit masks and logical NOT
- Two INV operations restore original value
- Paired with INC for two's complement negation
- Common in bit manipulation and pattern generation

For sub-word data types (BI, BY, H), only the lower part of the register is complemented and the upper bits are cleared to zero.

**Operation:**
```
Rn = ~Rn
result = 0 → Z flag
result.signbit → S flag
```

**Common Use Cases:**
- Creating inverse bit masks
- Implementing logical NOT operations
- Bit pattern manipulation
- Preparing values for two's complement negation (INV followed by INC)
- Generating all-1s patterns from all-0s

**Operands:** 0 (operates on implicit register)
**Variants:** 16 opcodes (4 data types × 4 registers)

---

## Variants

| Variant | Opcode | Type | Register | Assembly |
|---------|--------|------|----------|----------|
| 1-4 | 0xFE10-0xFE13 | BI | I1-I4 | BIn INV |
| 5-8 | 0xFE14-0xFE17 | BY | I1-I4 | BYn INV |
| 9-12 | 0xFE18-0xFE1B | H | I1-I4 | Hn INV |
| 13-16 | 0x0098-0x009B | W | I1-I4 | Wn INV |

---

## Operands

**Register Rn** (Destination, Read-Modify-Write):
- **Implicit operand**: Specified by instruction variant (I1-I4)
- **Role**: Register to be inverted
- **Note**: For BI, BY, H types, upper register bits are cleared to zero

---

## Trap Conditions

None

---

## Data Status Bits

- **Z (Zero)**: Set if result = 0, cleared otherwise
- **S (Sign)**: Set to result's sign bit
- **C (Carry)**: Unaffected
- **V (Overflow)**: Unaffected

---

## Examples

### Example 1: Invert all bits in word register
```assembly
        % Invert all 32 bits
        W1 INV              % 0xFFFFFFFF becomes 0x00000000
```

### Example 2: Create inverse mask
```assembly
        % Create inverse of a bit mask
        W2 := 0x00FF
        W2 INV              % Now contains 0xFFFFFF00
```

### Example 3: Invert single bit (BI type)
```assembly
        % Invert lowermost bit and clear upper 31 bits
        BI4 INV
```

### Example 4: Invert byte value
```assembly
        % Complement byte value
        BY1 := 0x0F
        BY1 INV             % Result: 0xF0 (upper 24 bits cleared)
```

### Example 5: Two's complement negation
```assembly
        % Negate using INV + INC (two's complement)
        W3 INV              % One's complement
        W3 INC              % Add 1 = two's complement
        % Equivalent to NEG instruction
```

### Example 6: Toggle all bits in halfword
```assembly
        % Invert halfword register
        H2 := 0x1234
        H2 INV              % Result: 0xEDCB (upper 16 bits cleared)
```

### Example 7: Create all-1s pattern
```assembly
        % Generate 0xFFFFFFFF
        W1 CLR              % W1 = 0
        W1 INV              % W1 = 0xFFFFFFFF
```

### Example 8: Logical NOT for conditional
```assembly
        % Invert boolean value
        W4 MOVE B.FLAG      % Load flag (0 or non-zero)
        W4 INV              % Invert all bits
        IF=GO ZERO_CASE     % Branch if original was all 1s
```

---

## Performance Notes

- **Execution**: 1 cycle
- **No operands**: Fastest logical operation (no memory access)
- **Upper bits**: For BI, BY, H types, upper register bits are cleared to zero

**Common patterns:**
- Generate all-1s: `Wn CLR` followed by `Wn INV`
- Two's complement: `Wn INV` followed by `Wn INC`
- Invert mask: Useful for creating complementary bit patterns
- No-op on zero: `Wn CLR; Wn INV; Wn INV` returns to zero

**Relationship to other instructions:**
- `Wn INV` is equivalent to `Wn XOR 0xFFFFFFFF`
- Two `INV` operations cancel out: `Wn INV; Wn INV` restores original value
- `INV` followed by `INC` performs two's complement negation (same as `NEG`)

---

## Reference Manual

**Section:** §10.13
**Title:** Invert

---

## See Also

- [INVC](invc.md) - Invert and add carry (for multi-word negation)
- [NEG](neg.md) - Two's complement negation
- [XOR](xor.md) - Bitwise XOR (can achieve same result with 0xFFFFFFFF)
- [NOT](not.md) - Logical NOT (boolean negation)
