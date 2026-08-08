# GETBI - Get Bit

## Overview

**Mnemonic:** `getbi`
**Function:** Extract single bit from operand into register
**Class:** BITFIELD
**Privilege:** user

**Format:** `tn GETBI <operand/r/t>,<bit No./r/BY>`

---

## Description

Extracts a single bit from an operand and loads it into bit 0 of the specified register. The upper bits of the register are unaffected - only bit 0 is modified.

**Key Characteristics:**
- Single-bit extraction to register bit 0
- Upper register bits preserved (non-destructive partial write)
- Supports 3 data types: BY (8-bit), H (16-bit), W (32-bit)
- 12 variants (3 types × 4 registers) for flexible allocation
- Z flag reflects extracted bit value (0 or 1)
- Essential for flag testing and boolean operations
- Efficient bit-level access without masks
- Traps on out-of-range bit numbers

The bit number specifies which bit to extract, with bit numbering following the ND-500 convention where bit 0 is the rightmost (least significant) bit. The operand can be a byte (BY), halfword (H), or word (W) data type.

An illegal operand value trap (IOV) is triggered if:
- `<bit No.>` is negative
- `<bit No.>` >= number of bits in the data type (8 for BY, 16 for H, 32 for W)

This instruction is essential for:
- Testing individual flag bits
- Implementing boolean operations
- Reading hardware status bits
- Accessing packed boolean arrays

**Operands:** 2
**Variants:** 12 opcode(s)

---

## Variants

Total variants: 12 (3 data types × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly Notation |
|---------|--------|-----------|----------|-------------------|
| 1/12 | 0xFCB4 | BY | 1 | BY1 GETBI |
| 2/12 | 0xFCB5 | BY | 2 | BY2 GETBI |
| 3/12 | 0xFCB6 | BY | 3 | BY3 GETBI |
| 4/12 | 0xFCB7 | BY | 4 | BY4 GETBI |
| 5/12 | 0xFCB8 | H | 1 | H1 GETBI |
| 6/12 | 0xFCB9 | H | 2 | H2 GETBI |
| 7/12 | 0xFCBA | H | 3 | H3 GETBI |
| 8/12 | 0xFCBB | H | 4 | H4 GETBI |
| 9/12 | 0xFDD0 | W | 1 | W1 GETBI |
| 10/12 | 0xFDD1 | W | 2 | W2 GETBI |
| 11/12 | 0xFDD2 | W | 3 | W3 GETBI |
| 12/12 | 0xFDD3 | W | 4 | W4 GETBI |

---

## Operands

### Operand 1 (Source Operand)

The operand containing the bit to extract.

**Type:** BY, H, or W (determined by instruction prefix)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local variable
- **RECORD** - Record field
- **CONSTANT** - Immediate value
- **REGISTER** - Register value
- **PRE_INDEXED** - Indexed addressing
- **ABSOLUTE** - Absolute memory address

### Operand 2 (Bit Number)

The bit position to extract (0 = rightmost/LSB).

**Type:** Byte (signed)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local variable containing bit number
- **RECORD** - Record field containing bit number
- **CONSTANT** - Immediate bit number
- **REGISTER** - Register containing bit number
- **PRE_INDEXED** - Indexed addressing
- **ABSOLUTE** - Absolute memory address

**Note:** Bit number must be 0 <= bitno < (8, 16, or 32 depending on operand type).

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **Illegal operand value (IOV):**
  - Negative `<bit No.>`
  - `<bit No.>` >= bits in data type

---

## Data Status Bits

- **Z (Zero):** Set if extracted bit = 0, cleared if bit = 1
- **S (Sign):** Not affected
- **O (Overflow):** Not affected
- **C (Carry):** Not affected

---

## Examples

### Example 1: Test status flag

```assembly
        % Load bit BITNO of word variable STATUS into R1
        W1 GETBI STATUS, BITNO
        % Test if bit is set
        W TEST I1
        IF<>GO BIT_SET
```

### Example 2: Read hardware status bit

```assembly
        % Read error flag (bit 7) from device status register
        W1 GETBI @DEVICE_STATUS, 7
        IF<>GO ERROR_OCCURRED
```

### Example 3: Extract bit from packed array

```assembly
        % Extract bit from boolean array
        % B.BOOL_ARRAY contains packed bits, 32 per word
        % Calculate word index and bit position
        W1 := B.BIT_INDEX
        W2 := I1
        W SHR I2, 5              % Divide by 32 for word index
        W1 AND I1, 31            % Modulo 32 for bit position
        % Extract the bit
        W3 GETBI B.BOOL_ARRAY(I2), I1
        % W3 bit 0 contains the boolean value
```

### Example 4: Test multiple flags

```assembly
        % Test multiple status flags
        W1 GETBI B.FLAGS, B.ERROR_BIT
        IF<>GO ERROR_SET
        W1 GETBI B.FLAGS, B.READY_BIT
        IF=GO NOT_READY
        W1 GETBI B.FLAGS, B.BUSY_BIT
        IF<>GO DEVICE_BUSY
```

### Example 5: Convert bit to integer

```assembly
        % Get bit and convert to 0/1 integer value
        W1 GETBI B.STATUS, 3
        % Clear upper bits to get clean 0 or 1
        W AND I1, 1
        % I1 now contains 0 or 1
```

---

## Performance Notes

- **Typical cycles:** 4-6 cycles depending on operand addressing modes
- **Best case:** 4 cycles (register and constant operands)
- **Worst case:** 6+ cycles (memory operands with indexed addressing)

**Note:** GETBI only modifies bit 0 of the destination register, preserving other register bits. This allows efficient testing with subsequent TEST/COMP instructions.

**Bit numbering:** Remember that ND-500 uses little-endian bit numbering - bit 0 is the LSB.

---

## Reference Manual

**Section:** §10.27
**Title:** Get bit

---

## See Also

- [PUTBI](putbi.md) - Put bit (store register bit 0 into operand bit)
- [SETBI](setbi.md) - Set bit to 1
- [CLEBI](clebi.md) - Clear bit to 0
- [GETBF](getbf.md) - Get bit field (multi-bit extraction)
- [TSET](tset.md) - Test and set bit
- [Trap System](../../ND-500-TRAPS.md)
