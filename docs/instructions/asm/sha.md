# SHA - Shift Arithmetic

## Overview

**Mnemonic:** `sha`
**Function:** Arithmetic shift (sign-preserving)
**Class:** SHIFT
**Privilege:** user
**Format:** `t SHA <operand>, <count>`

---

## Description

Performs an arithmetic shift operation on a byte, halfword, or word operand. The shift count is interpreted as a signed byte value:
- **Positive count**: Shifts left (same as logical shift)
- **Negative count**: Shifts right preserving sign bit
- **Zero count**: No operation (operand unchanged)

The critical difference from logical shift (SHL) is that arithmetic right shifts preserve the sign bit by copying it to fill positions vacated on the left. This maintains the numeric sign for signed integer division by powers of 2.

**Operation:**
```
For positive count (left shift):
  operand << count → operand
  Zeros fill from right (same as SHL)

For negative count (arithmetic right shift):
  operand >> |count| → operand
  Sign bit copies fill from left (preserves sign)

result = 0 → Z flag
result.signbit → S flag
```

**Key Characteristics:**
- Sign-preserving arithmetic shift (maintains signedness)
- Bidirectional: positive count = left, negative count = right
- Sign extension on right shifts (sign bit replicated)
- Equivalent to signed division/multiplication by powers of 2
- Supports byte, halfword, and word types (no bit/float/double)
- Sets Z and S flags based on result
- Variable shift count (runtime-determined via signed byte)
- Essential for fixed-point and DSP arithmetic

**Common Use Cases:**
- Signed integer division by powers of 2
- Signed number scaling
- Sign-preserving bit manipulation
- Fixed-point arithmetic
- DSP operations
- Signed value normalization

**Operands:** 2 (operand to shift, shift count)
**Variants:** 3 opcodes (BY, H, W - no bit or float types)

---

## Variants

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/3 | 0xFCAB | Byte | BY SHA |
| 2/3 | 0xFCAC | Halfword | H SHA |
| 3/3 | 0xFCAD | Word | W SHA |

**Note:** SHA is not available for BI (bit), F (float), or D (double) data types.

---

## Operands

**Operand 1** (Destination, Read-Modify-Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix (BY/H/W)
- **Role**: Value to be shifted
- **Access**: Read original, write shifted result

**Operand 2** (Shift Count, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Signed byte value
- **Role**: Number of positions to shift
- **Range**: -(size-1) to +(size-1)
  - BY: -7 to +7
  - H: -15 to +15
  - W: -31 to +31
- **Trap**: Count >= operand size causes illegal operand value trap

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation
- **Illegal operand value (IOV)**: Shift count >= operand bit size

---

## Data Status Bits

- **Z (Zero)**: Set if result = 0, cleared otherwise
- **S (Sign)**: Set to result's sign bit
- **C (Carry)**: Last bit shifted out
- **V (Overflow)**: Unaffected

---

## Examples

### Example 1: Signed divide by 2
```assembly
        % Divide signed integer by 2
        W1 SHA -1           % Arithmetic right shift by 1
```

### Example 2: Shift byte register right
```assembly
        % Shift byte register R4 two places to the right
        BY SHA R4, -2
```

### Example 3: Divide by 4 (signed)
```assembly
        % Signed divide by 4
        W2 SHA -2           % Equivalent to W2 / 4 for signed
```

### Example 4: Multiply by 8 (left shift)
```assembly
        % Multiply by 8 (same as logical left shift)
        W3 SHA 3            % Equivalent to W3 * 8
```

### Example 5: Variable shift count
```assembly
        % Arithmetic shift by variable amount
        W SHA B.VALUE, B.SHIFT_COUNT
```

### Example 6: Sign-preserving extraction
```assembly
        % Extract and sign-extend high bits
        W1 SHA -16          % Shift right 16, preserving sign
```

### Example 7: Fixed-point division
```assembly
        % Divide fixed-point value by 16
        W2 SHA -4
```

### Example 8: Normalize signed value
```assembly
        % Shift until MSB differs from sign bit
LOOP:
        W1 TEST I3
        IF<GO NEGATIVE
        % Positive: shift until MSB = 1
        W1 SHA 1
        IF>=GO LOOP
        GO DONE
NEGATIVE:
        % Negative: shift until MSB = 0
        W1 SHA 1
        IF<GO LOOP
DONE:
```

### Example 9: Sign-extend byte to word
```assembly
        % Load byte with sign extension
        BY MOVE SOURCE, I1  % Load byte
        W1 SHA 0            % Triggers sign bit evaluation
```

### Example 10: Zero shift (no-op)
```assembly
        % Legal but does nothing
        W1 SHA 0            % No change, flags updated
```

---

## Performance Notes

- **Execution**: 2-3 cycles depending on shift count
  - Small shifts (1-8): ~2 cycles
  - Large shifts (>8): ~3 cycles
- **Sign preservation**: Right shifts preserve sign (critical for signed arithmetic)
- **Left shift**: Identical to SHL (no difference for left shifts)
- **Optimization**: Use SHA for signed division by powers of 2

**Arithmetic vs Logical shift:**
```assembly
% For negative numbers, SHA preserves sign:
W1 := -16           % 0xFFFFFFF0
W1 SHA -2           % 0xFFFFFFFC = -4 (sign preserved)

% SHL fills with zeros:
W2 := -16           % 0xFFFFFFF0
W2 SHL -2           % 0x3FFFFFFC (large positive - sign lost)

% For positive numbers, both are identical:
W3 := 16            % 0x00000010
W3 SHA -2           % 0x00000004 = 4
W4 := 16            % 0x00000010
W4 SHL -2           % 0x00000004 = 4 (same result)
```

**Division equivalents:**
```assembly
% Signed division by powers of 2:
W1 SHA -1           % ÷ 2
W1 SHA -2           % ÷ 4
W1 SHA -3           % ÷ 8
W1 SHA -4           % ÷ 16

% Note: For negative numbers with remainder, result rounds toward zero
% Example: -7 SHA -1 = -3 (not -4)
%         -7 / 2 mathematically = -3.5, SHA rounds to -3
```

**Shift count validation:**
```assembly
% Valid shifts (W type, 32-bit):
W1 SHA 31           % OK - shift left 31 positions
W1 SHA -31          % OK - shift right 31 positions
W1 SHA 0            % OK - no-op

% Invalid (traps):
W1 SHA 32           % TRAP - IOV
W1 SHA -32          % TRAP - IOV

% For halfword (16-bit):
H1 SHA 15           % OK
H1 SHA 16           % TRAP - IOV

% For byte (8-bit):
BY1 SHA 7           % OK
BY1 SHA 8           % TRAP - IOV
```

**When to use SHA vs SHL:**
- **SHA**: For signed integer arithmetic (division, scaling)
- **SHL**: For unsigned arithmetic or logical bit manipulation
- **Left shifts**: No difference (use either)
- **Right shifts**:
  - SHA for signed values (preserves sign)
  - SHL for unsigned values (fills with zero)

**Comparison table:**
| Operation | Value | SHA -2 | SHL -2 |
|-----------|-------|--------|--------|
| Positive  | +16   | +4     | +4     |
| Negative  | -16   | -4     | 1073741820 |
| Zero      | 0     | 0      | 0      |

---

## Reference Manual

**Section:** §10.25
**Title:** Arithmetical shift

---

## See Also

- [SHL](shl.md) - Logical shift (zeros fill, no sign preservation)
- [SHR](shr.md) - Rotational shift (bits wrap around)
- [DIV](div.md) - Division (slower than SHA for powers of 2)
- [MUL](mul.md) - Multiplication (slower than SHA for powers of 2)
