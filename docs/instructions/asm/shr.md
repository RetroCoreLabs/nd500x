# SHR - Rotational Shift (Rotate)

## Overview

**Mnemonic:** `shr`
**Function:** Rotational shift (rotate left or right)
**Class:** SHIFT
**Privilege:** user
**Format:** `t SHR <operand>, <count>`

---

## Description

Performs a rotational shift (rotate) operation on a byte, halfword, or word operand. Unlike logical shifts, bits that are shifted out from one end wrap around and re-enter from the other end. The shift count is interpreted as a signed byte value:
- **Positive count**: Rotate RIGHT (bits move toward LSB, wrap to MSB)
- **Negative count**: Rotate LEFT (bits move toward MSB, wrap to LSB)
- **Zero count**: No operation (operand unchanged)

This is a circular shift where no bits are lost - they wrap around to the opposite end.

> **Note:** The ND-500 Reference Manual §10.26 states "Positive shiftcount implies left shift". However, empirical testing against nd500-as and validated test cases shows that positive count actually performs RIGHT rotation. This discrepancy is documented but the emulator matches verified behavior.

**Operation:**
```
For positive count (rotate right):
  operand rotated right by count positions
  Bits shifted out from LSB re-enter at MSB
  Example: SHR(0x80000001, 1) = 0xC0000000

For negative count (rotate left):
  operand rotated left by |count| positions
  Bits shifted out from MSB re-enter at LSB

result = 0 → Z flag
result.signbit → S flag
```

**Key Characteristics:**
- Rotational shift (circular, no bits lost)
- Bidirectional: positive count = RIGHT, negative count = LEFT (opposite of SHL convention)
- Bits wrap around from one end to the other
- Preserves all bits (lossless operation)
- Supports byte, halfword, and word types (no bit/float/double)
- Sets Z and S flags based on result
- Variable rotate count (runtime-determined via signed byte)
- Common in cryptography, checksums, and bit manipulation

**Common Use Cases:**
- Bit permutation and rearrangement
- Circular buffer operations
- Byte/nibble swapping
- Cryptographic operations
- Checksum calculations
- Endian conversion helpers
- Bit pattern manipulation

**Operands:** 2 (operand to rotate, rotate count)
**Variants:** 3 opcodes (BY, H, W - no bit or float types)

---

## Variants

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/3 | 0xFCAE | Byte | BY SHR |
| 2/3 | 0xFCAF | Halfword | H SHR |
| 3/3 | 0xFCB0 | Word | W SHR |

**Note:** Despite the name "SHR" (shift right), this performs rotational shift in both directions based on sign of count.

---

## Operands

**Operand 1** (Destination, Read-Modify-Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix (BY/H/W)
- **Role**: Value to be rotated
- **Access**: Read original, write rotated result

**Operand 2** (Rotate Count, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Signed byte value
- **Role**: Number of positions to rotate
- **Range**: -(size-1) to +(size-1)
  - BY: -7 to +7
  - H: -15 to +15
  - W: -31 to +31
- **Trap**: Count >= operand size causes illegal operand value trap

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation
- **Illegal operand value (IOV)**: Rotate count >= operand bit size

---

## Data Status Bits

- **Z (Zero)**: Set if result = 0, cleared otherwise
- **S (Sign)**: Set to result's sign bit
- **C (Carry)**: Unaffected (or last bit rotated - implementation specific)
- **V (Overflow)**: Unaffected

---

## Examples

### Example 1: Rotate right by 1
```assembly
        % Rotate right by 1 (positive count = right)
        W1 SHR 1            % Rotate right 1 position
```

### Example 2: Exchange nibbles (swap 4-bit groups)
```assembly
        % Exchange nibbles of variable pointed at by R4
        BY SHR R4.0, 4      % Rotate byte by 4 bits (either direction swaps)
```

### Example 3: Rotate right by 8
```assembly
        % Extract high byte to low byte position (rotate right)
        W2 SHR 8            % Rotate right 8 bits
```

### Example 4: Swap bytes in halfword
```assembly
        % Byte swap for endian conversion
        H1 SHR 8            % Rotate 8 bits (swaps bytes, either direction works)
```

### Example 5: Rotate word left
```assembly
        % Rotate word left by 4 bits (negative count = left)
        W3 SHR -4
```

### Example 6: Circular bit extraction
```assembly
        % Extract bits with rotation
        W1 MOVE DATA
        W1 SHR ALIGNMENT    % Rotate to align bits
        W1 AND MASK         % Extract desired bits
```

### Example 7: Checksum rotation
```assembly
        % Rotating checksum accumulation
        W2 SHR 1            % Rotate accumulator
        W2 XOR NEXT_BYTE    % Mix in next byte
```

### Example 8: Zero rotation (no-op)
```assembly
        % Legal but does nothing
        W1 SHR 0            % No change, flags updated
```

### Example 9: Nibble extraction
```assembly
        % Get high nibble of byte
        BY1 MOVE FLAGS
        BY1 SHR 4           % Rotate high nibble to low position
        BY1 AND 0x0F        % Mask to get nibble
```

### Example 10: Bit pattern permutation
```assembly
        % Rearrange bit pattern
        H2 SHR B.SHIFT_AMT
```

---

## Performance Notes

- **Execution**: 2-3 cycles depending on rotate count
  - Small rotations (1-8): ~2 cycles
  - Large rotations (>8): ~3 cycles
- **No data loss**: All bits preserved (wrap around)
- **Direction**: Negative count rotates right, positive rotates left
- **Optimization**: Useful for bit manipulation without temporary storage

**Rotation properties:**
```assembly
% Rotating by operand size = no change:
BY1 SHR 8           % TRAP - illegal
BY1 SHR 7           % OK - almost full rotation
BY1 SHR -1          % Rotate right 1 bit

% Rotating twice in opposite directions restores original:
W1 SHR 5            % Rotate left 5
W1 SHR -5           % Rotate right 5 = back to original

% Rotation is commutative over full size:
W1 SHR 10
W1 SHR 15
% Equivalent to:
W1 SHR 25           % (10 + 15 = 25 mod 32)
```

**Difference from logical shift (SHL):**
```assembly
% Logical shift (SHL) - bits are lost, zeros fill:
W1 := 0x80000001
W1 SHL 1            % Result: 0x00000002 (MSB lost, zero fills LSB)

% Rotational shift (SHR) - bits wrap around:
W1 := 0x80000001
W1 SHR 1            % Result: 0x00000003 (MSB wraps to LSB)
```

**Common patterns:**
```assembly
% Pattern 1: Byte swap in halfword
H1 SHR 8            % or H1 SHR -8 (same result)

% Pattern 2: Nibble swap in byte
BY1 SHR 4           % or BY1 SHR -4 (same result)

% Pattern 3: Extract rotated bits
W1 SHR ALIGNMENT
W1 AND MASK

% Pattern 4: Circular shift for crypto/checksum
LOOP:
    W1 SHR 1
    W1 XOR DATA(W2)
    W2 INC
    W2 COMP SIZE
    IF<GO LOOP
```

**When to use SHR (rotate) vs SHL (logical shift):**
- Use SHR when you need to preserve all bits (rotation)
- Use SHL when you want zeros to fill (logical shift)
- SHR is ideal for endian swapping, bit permutations
- SHL is ideal for multiplication/division by powers of 2

---

## Reference Manual

**Section:** §10.26
**Title:** Rotational shift

---

## See Also

- [SHL](shl.md) - Logical shift (zeros fill, no wrap)
- [SHA](sha.md) - Arithmetic shift (sign-preserving)
- [ROL](rol.md) - Rotate left (if separate instruction exists)
- [ROR](ror.md) - Rotate right (if separate instruction exists)
