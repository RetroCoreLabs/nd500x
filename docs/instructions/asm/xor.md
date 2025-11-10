# XOR - Bitwise Exclusive OR

## Overview

**Mnemonic:** `xor`
**Function:** Bitwise exclusive OR operation between register and operand
**Class:** LOGICAL
**Privilege:** user
**Format:** `tn XOR <operand>`

---

## Description

Performs a bitwise logical XOR (exclusive OR) operation between the contents of a specified register and an operand, storing the result back in the register. Each bit in the result is set to 1 if the corresponding bits in the register and operand differ, and 0 if they are the same.

**Operation:**
```
Rn = Rn ^ operand
result = 0 → Z flag
result.signbit → S flag
```

**Common Use Cases:**
- Toggle specific bits in a register or variable
- Simple encryption/decryption operations
- Checksum and parity calculations
- Bit difference detection
- Swapping values without temporary variable (A XOR B; B XOR A; A XOR B)
- Zero detection via self-XOR (Rn XOR Rn → 0)

**Operands:** 1 (source, register is implicit destination)
**Variants:** 16 opcodes (4 data types × 4 registers)

---

## Variants

| Variant | Opcode | Type | Register | Assembly |
|---------|--------|------|----------|----------|
| 1-4 | 0xFDFC-0xFDFF | BI | I1-I4 | BIn XOR |
| 5-8 | 0xFCA0-0xFCA3 | BY | I1-I4 | BYn XOR |
| 9-12 | 0xFCA4-0xFCA7 | H | I1-I4 | Hn XOR |
| 13-16 | 0x00A4-0x00A7 | W | I1-I4 | Wn XOR |

---

## Operands

**Operand 1** (Source, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix (BI/BY/H/W)
- **Role**: Second operand for XOR operation
- **Note**: Upper bits zero-filled for BI, BY, H types

**Register Rn** (Destination, Read-Modify-Write):
- **Implicit operand**: Specified by instruction variant (I1-I4)
- **Role**: First operand and result destination
- **Note**: Upper part zero-filled for sub-word types

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation

---

## Data Status Bits

- **Z (Zero)**: Set if result = 0, cleared otherwise
- **S (Sign)**: Set to result's sign bit
- **C (Carry)**: Unaffected
- **V (Overflow)**: Unaffected

---

## Examples

### Example 1: Toggle specific bits
```assembly
        % Flip bits 0, 4, 8, 12 in halfword register
        H4 XOR 0x1111
```

### Example 2: Simple XOR encryption/decryption
```assembly
        % Encrypt data with XOR key
        W2 XOR B.KEY

        % Decrypt by XOR with same key
        W2 XOR B.KEY        % Returns to original value
```

### Example 3: Toggle flag bits
```assembly
        % Toggle specific control flags
        BY1 XOR 0x0C        % Flip bits 2 and 3
```

### Example 4: Clear register to zero
```assembly
        % Self-XOR always produces zero
        W1 XOR W1           % W1 = 0, Z flag set
```

### Example 5: Parity calculation
```assembly
        % Accumulate XOR for parity check
        W1 CLR
        W1 XOR DATA1
        W1 XOR DATA2
        W1 XOR DATA3
        % Final W1 contains XOR of all values
```

### Example 6: Bit difference detection
```assembly
        % Find which bits differ between two values
        W3 MOVE ORIGINAL
        W3 XOR MODIFIED     % Result has 1s where bits differ
```

### Example 7: Array element XOR
```assembly
        % XOR with indexed array element
        W1 XOR ARRAY(W2)
```

### Example 8: XOR-based swap (no temp variable)
```assembly
        % Swap A and B without temporary
        W1 MOVE B.A
        W2 MOVE B.B
        W1 XOR W2           % A = A ^ B
        W2 XOR W1           % B = B ^ (A ^ B) = A
        W1 XOR W2           % A = (A ^ B) ^ A = B
        W1 MOVE B.B
        W2 MOVE B.A
```

---

## Performance Notes

- **Execution**: 1-2 cycles
  - Register operand: 1 cycle
  - Memory operand: 2 cycles
- **Optimization**: Use constant operands when possible for fastest execution
- **Upper bits**: For BI, BY, H types, upper register bits are zero-filled

**Common patterns:**
- Toggle bits: `Wn XOR mask` (flips bits where mask = 1)
- Clear register: `Wn XOR Wn` (always produces 0)
- No-op: `Wn XOR 0` (leaves register unchanged)
- Invert all bits: `Wn XOR 0xFFFFFFFF` (same as INV instruction)

**XOR properties:**
- Commutative: A XOR B = B XOR A
- Associative: (A XOR B) XOR C = A XOR (B XOR C)
- Identity: A XOR 0 = A
- Self-inverse: A XOR A = 0
- Double XOR: (A XOR B) XOR B = A

---

## Reference Manual

**Section:** §10.23
**Title:** Exclusive or

---

## See Also

- [OR](or.md) - Bitwise OR (set bits)
- [AND](and.md) - Bitwise AND (clear/mask bits)
- [INV](inv.md) - Bitwise NOT (invert all bits)
- [TEST](test.md) - Test bits without modifying register
