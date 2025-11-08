# AND - Bitwise AND

## Overview

**Mnemonic:** `and`
**Function:** Bitwise AND operation
**Class:** LOGICAL
**Privilege:** user

**Format:** `tn AND <operand>`

---

## Description

Performs a bitwise AND operation between the contents of the specified register and the operand, storing the result back in the register.

The operation is: `Rn = Rn AND <operand>`

Each bit in the result is set to 1 only if the corresponding bits in both the register and operand are 1. This operation is commonly used for:
- Masking bits (clearing specific bits while preserving others)
- Testing specific bit patterns
- Extracting bit fields
- Implementing set intersection

When the data type is BI (bit), BY (byte), or H (halfword), the upper part of the register is zero-filled after the operation.

**Operands:** 1
**Variants:** 16 opcode(s)

---

## Variants

Total variants: 16 (4 data types × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly |
|---------|--------|-----------|----------|----------|
| 1/16 | 0xFDCC | BI | 1 | BI1 AND |
| 2/16 | 0xFDCD | BI | 2 | BI2 AND |
| 3/16 | 0xFDCE | BI | 3 | BI3 AND |
| 4/16 | 0xFDCF | BI | 4 | BI4 AND |
| 5/16 | 0xFC90 | BY | 1 | BY1 AND |
| 6/16 | 0xFC91 | BY | 2 | BY2 AND |
| 7/16 | 0xFC92 | BY | 3 | BY3 AND |
| 8/16 | 0xFC93 | BY | 4 | BY4 AND |
| 9/16 | 0xFC94 | H | 1 | H1 AND |
| 10/16 | 0xFC95 | H | 2 | H2 AND |
| 11/16 | 0xFC96 | H | 3 | H3 AND |
| 12/16 | 0xFC97 | H | 4 | H4 AND |
| 13/16 | 0x00E4 | W | 1 | W1 AND |
| 14/16 | 0x00E5 | W | 2 | W2 AND |
| 15/16 | 0x00E6 | W | 3 | W3 AND |
| 16/16 | 0x00E7 | W | 4 | W4 AND |

---

## Operands

### Operand 1 (Source)

The operand to AND with the register contents.

**Type:** Bit, Byte, Halfword, or Word (matching register type)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer register (I1-I4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

---

## Trap Conditions

- **Addressing traps:** Invalid address, descriptor range violation, page fault, protection violation

---

## Data Status Bits

- **Z (Zero):** Set if result = 0 (all bits cleared), cleared otherwise
- **S (Sign):** Set to sign bit of result (most significant bit)
- **O (Overflow):** Unaffected
- **K (Flag):** Unaffected
- **C (Carry):** Unaffected

---

## Examples

### Example 1: Mask bits using descriptor array

```assembly
        % AND W2 with R3rd element of array via descriptor
        W2 AND DESC(B.MASKS(R1))(R3)
```

### Example 2: Clear upper bits (byte mask)

```assembly
        % Mask lower 8 bits of word register
        W1 AND 0FFH              % Keep only bits 0-7
```

### Example 3: Test specific bits

```assembly
        % Test if bits 4 and 5 are set
        W1 := B.FLAGS
        W1 AND 030H              % Mask bits 4-5 (00110000)
        IF=GO BITS_CLEAR         % Branch if both bits are 0
        % Bits 4 or 5 are set
        GO CONTINUE
BITS_CLEAR:
        % Neither bit 4 nor 5 is set
CONTINUE:
```

### Example 4: Extract bit field

```assembly
        % Extract bits 8-15 from word
        W2 := B.DATA
        W2 AND 0FF00H            % Mask bits 8-15
        W2 SHR 8                 % Shift to low byte
        I2 =: B.EXTRACTED_FIELD
```

### Example 5: Implement set intersection

```assembly
        % Compute intersection of two bit sets
        W1 := B.SET_A            % Load first set
        W1 AND B.SET_B           % Intersect with second set
        I1 =: B.SET_INTERSECTION % Result contains only common bits
```

### Example 6: Clear specific bits (reverse mask)

```assembly
        % Clear bits 0, 2, 4 (preserve others)
        W3 := B.VALUE
        W3 AND 0FFFFFFEAH        % Mask: ...11101010 (clears bits 0,2,4)
        I3 =: B.VALUE
```

---

## Performance Notes

- **Typical cycles:** 3-4 cycles depending on addressing mode
- **Best case:** 3 cycles (register to register)
- **Worst case:** 4+ cycles (memory access with page fault)

**Note:** Bitwise operations are fast and execute in constant time regardless of data values.

---

## Reference Manual

**Section:** §10.21
**Title:** And

---

## See Also

- [OR](or.md) - Bitwise OR
- [XOR](xor.md) - Bitwise exclusive OR
- [INV](inv.md) - Bitwise NOT (invert)
- [GETBF](getbf.md) - Get bit field
- [PUTBF](putbf.md) - Put bit field
- [SHL](shl.md) - Shift left logical
- [SHR](shr.md) - Shift right logical
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
