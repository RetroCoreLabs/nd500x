# SHL - Shift Left (Logical Shift)

## Overview

**Mnemonic:** `shl`
**Function:** Logical left shift (or right shift with negative count)
**Class:** SHIFT
**Privilege:** user
**Format:** `t SHL <operand>, <count>`

---

## Description

Performs a logical shift operation on a byte, halfword, or word operand. The shift count is interpreted as a signed byte value:
- **Positive count**: Shifts left (bits move toward MSB)
- **Negative count**: Shifts right (bits move toward LSB)
- **Zero count**: No operation (operand unchanged)

In a logical shift, zeros are shifted in from the opposite end. Bits shifted out are discarded (the last bit shifted out goes to the carry flag).

**Operation:**
```
For positive count (left shift):
  operand << count → operand
  Zeros fill from right
  MSB bits shifted out to carry

For negative count (right shift):
  operand >> |count| → operand
  Zeros fill from left
  LSB bits shifted out to carry

result = 0 → Z flag
result.signbit → S flag
```

**Common Use Cases:**
- Multiplication/division by powers of 2
- Bit field alignment
- Mask generation
- Data packing/unpacking
- Fast arithmetic operations
- Bit manipulation

**Operands:** 2 (operand to shift, shift count)
**Variants:** 3 opcodes (BY, H, W - no bit or float types)

---

## Variants

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/3 | 0xFCA8 | Byte | BY SHL |
| 2/3 | 0xFCA9 | Halfword | H SHL |
| 3/3 | 0xFCAA | Word | W SHL |

**Note:** SHL is not available for BI (bit), F (float), or D (double) data types.

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

### Example 1: Multiply by 2 (shift left 1)
```assembly
        % Fast multiply by 2
        W1 := B.VALUE
        W1 SHL 1            % Equivalent to W1 * 2
```

### Example 2: Multiply by 256 (shift left 8)
```assembly
        % Shift left 8 positions
        W2 SHL 8            % Equivalent to W2 * 256
```

### Example 3: Shift right using negative count
```assembly
        % Divide by 4 (shift right 2)
        W3 SHL -2           % Equivalent to W3 / 4 (logical)
```

### Example 4: Shift byte value
```assembly
        % Shift byte left by 4 bits
        BY1 SHL 4
```

### Example 5: Variable shift count
```assembly
        % Shift by count in variable
        W SHL B.COUNT, TWOFACTORS
```

### Example 6: Align bit field
```assembly
        % Align 12-bit value to upper bits
        H1 SHL 4            % Move bits 0-11 to bits 4-15
```

### Example 7: Create bit mask
```assembly
        % Create mask with N ones
        W1 := 1
        W1 SHL B.NUM_BITS
        W1 DEC              % Now has NUM_BITS ones
```

### Example 8: Extract and align field
```assembly
        % Extract field from packed data
        W1 MOVE PACKED_DATA
        W1 SHL 16           % Align field to high bits
        W1 SHL -24          % Right-align 8-bit field
```

### Example 9: Power of 2 multiplication
```assembly
        % Multiply by 16 (2^4)
        W2 := B.VALUE
        W2 SHL 4
```

### Example 10: Zero shift (no-op)
```assembly
        % Legal but does nothing
        W1 SHL 0            % No change, flags updated
```

---

## Performance Notes

- **Execution**: 2-3 cycles depending on shift count
  - Small shifts (1-8): ~2 cycles
  - Large shifts (>8): ~3 cycles
- **Direction**: Negative count performs right shift
- **Optimization**: Use SHL for powers-of-2 multiplication instead of MUL
- **Trap overhead**: Shift count validation adds minimal overhead

**Multiplication equivalents:**
```assembly
% Instead of:
W1 MUL 2            % Slower

% Use:
W1 SHL 1            % Faster

% Powers of 2:
W1 SHL 2            % × 4
W1 SHL 3            % × 8
W1 SHL 4            % × 16
W1 SHL 5            % × 32
```

**Shift count validation:**
```assembly
% Valid shifts (W type, 32-bit):
W1 SHL 31           % OK - shift left 31 positions
W1 SHL -31          % OK - shift right 31 positions
W1 SHL 0            % OK - no-op

% Invalid (traps):
W1 SHL 32           % TRAP - IOV
W1 SHL -32          % TRAP - IOV

% For halfword (16-bit):
H1 SHL 15           % OK
H1 SHL 16           % TRAP - IOV

% For byte (8-bit):
BY1 SHL 7           % OK
BY1 SHL 8           % TRAP - IOV
```

**Logical vs Arithmetic shift:**
- SHL: Logical shift (zeros fill)
- SHA: Arithmetic shift (sign bit preserved on right shift)
- For left shifts, SHL and SHA behave identically
- For right shifts:
  - SHL fills with zeros
  - SHA fills with sign bit (preserves sign for signed numbers)

---

## Reference Manual

**Section:** §10.24
**Title:** Logical shift

---

## See Also

- [SHR](shr.md) - Shift right (if separate instruction exists)
- [SHA](sha.md) - Shift arithmetic (preserves sign)
- [ROL](rol.md) - Rotate left (if available)
- [ROR](ror.md) - Rotate right (if available)
- [MUL](mul.md) - Multiplication (slower than shift for powers of 2)
