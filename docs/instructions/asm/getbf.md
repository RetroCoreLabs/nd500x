# GETBF - Get Bit Field

## Overview

**Mnemonic:** `getbf`
**Function:** Extract bit field from operand into register
**Class:** BITFIELD
**Privilege:** user

**Format:** `tn GETBF <operand/r/t>,<bit No./r/BY>,<field size/r/BY>`

---

## Description

Extracts a bit field from an operand and loads it into the lower bits of the specified register. The upper bits of the destination register are zero-filled.

**Operation:**
```
field = (operand >> bit_no) & ((1 << field_size) - 1)
register = field  // Upper bits zero-filled
```

**Key Characteristics:**
- Extract bit field into register with zero-extension
- 12 variants (3 data types: BY/H/W × 4 registers)
- Bit 0 = LSB (rightmost), little-endian bit numbering
- IOV trap if bit_no < 0 or field_size ≤ 0
- IOV trap if (bit_no + field_size) exceeds data type width
- Sets Z flag if extracted field = 0
- Sets S flag to leftmost bit of extracted field
- Essential for packed structures and hardware registers
- 5-8 cycles depending on addressing modes

The bit field is specified by a starting bit number and a field size. Bit numbering follows the ND-500 convention where bit 0 is the rightmost (least significant) bit. The field extraction starts at `<bit No.>` and extends to higher-numbered bits for `<field size>` bits.

The operand can be a byte (BY), halfword (H), or word (W) data type. The bit number and field size parameters are interpreted as signed byte integers.

An illegal operand value trap (IOV) is triggered if:
- `<bit No.>` is negative
- `<field size>` is zero or negative
- `<bit No.>` + `<field size>` exceeds the number of bits in the data type (8 for BY, 16 for H, 32 for W)

This instruction is essential for:
- Packing multiple flags or small values into a single word
- Accessing hardware register fields
- Implementing bitfield structures in high-level languages
- Decoding packed binary protocols

**Operands:** 3
**Variants:** 12 opcode(s)

---

## Variants

Total variants: 12 (3 data types × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly Notation |
|---------|--------|-----------|----------|-------------------|
| 1/4 | 0xFDE0 | BY | 1 | BY1 GETBF |
| 2/4 | 0xFDE1 | BY | 2 | BY2 GETBF |
| 3/4 | 0xFDE2 | BY | 3 | BY3 GETBF |
| 4/4 | 0xFDE3 | BY | 4 | BY4 GETBF |
| 5/12 | 0xFDE4 | H | 1 | H1 GETBF |
| 6/12 | 0xFDE5 | H | 2 | H2 GETBF |
| 7/12 | 0xFDE6 | H | 3 | H3 GETBF |
| 8/12 | 0xFDE7 | H | 4 | H4 GETBF |
| 9/12 | 0xFDE8 | W | 1 | W1 GETBF |
| 10/12 | 0xFDE9 | W | 2 | W2 GETBF |
| 11/12 | 0xFDEA | W | 3 | W3 GETBF |
| 12/12 | 0xFDEB | W | 4 | W4 GETBF |

---

## Operands

### Operand 1 (Source Operand)

The operand containing the bit field to extract.

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

The starting bit position of the field to extract (0 = rightmost bit).

**Type:** Byte (signed)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local variable containing bit number
- **RECORD** - Record field containing bit number
- **CONSTANT** - Immediate bit number
- **REGISTER** - Register containing bit number
- **PRE_INDEXED** - Indexed addressing
- **ABSOLUTE** - Absolute memory address

### Operand 3 (Field Size)

The number of bits to extract.

**Type:** Byte (signed)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local variable containing field size
- **RECORD** - Record field containing field size
- **CONSTANT** - Immediate field size
- **REGISTER** - Register containing field size
- **PRE_INDEXED** - Indexed addressing
- **ABSOLUTE** - Absolute memory address

**Note:** Field size must be positive and (bit number + field size) must not exceed the operand's bit width.

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **Illegal operand value (IOV):**
  - Negative `<bit No.>`
  - Zero or negative `<field size>`
  - `<bit No.>` + `<field size>` > bits in data type

---

## Data Status Bits

- **Z (Zero):** Set if extracted bit field = 0, cleared otherwise
- **S (Sign):** Set to leftmost bit of extracted field
- **O (Overflow):** Not affected
- **C (Carry):** Not affected

---

## Examples

### Example 1: Extract 8-bit field from word

```assembly
        % Extract bits 11-18 from word variable
        % Field is 8 bits starting at bit 11
        W2 GETBF R.16, 11, 8
        % W2 now contains the 8-bit field in lower byte, upper bits zero
```

### Example 2: Extract flag bits

```assembly
        % Extract 3 flag bits from status word
        % Flags are in bits 5-7
        W1 GETBF B.STATUS, 5, 3
        % W1 contains 0-7 depending on flag state
        W COMP I1, 7
        IF=GO ALL_FLAGS_SET
```

### Example 3: Decode packed structure

```assembly
        % Packed word contains: type(4 bits) | size(8 bits) | flags(4 bits)
        % Extract type field (bits 0-3)
        W1 GETBF B.PACKET, 0, 4
        % Extract size field (bits 4-11)
        W2 GETBF B.PACKET, 4, 8
        % Extract flags (bits 12-15)
        W3 GETBF B.PACKET, 12, 4
```

### Example 4: Extract variable-width field

```assembly
        % Extract field with runtime-determined position and size
        % B.BIT_POS and B.BIT_LEN determined at runtime
        W1 GETBF B.DATA, B.BIT_POS, B.BIT_LEN
        % Handle extraction error
        IF-KGO EXTRACT_OK
        % IOV trap occurred - invalid parameters
        CALL ERROR_HANDLER
EXTRACT_OK:
```

### Example 5: Extract halfword bitfield

```assembly
        % Extract 5-bit value from halfword device register
        % Hardware status in bits 8-12 of 16-bit register
        H1 GETBF R.DEVICE_STATUS, 8, 5
        % H1 contains 0-31 status value
        W COMP I1, B.EXPECTED_STATUS
        IF<>GO STATUS_ERROR
```

---

## Performance Notes

- **Typical cycles:** 5-8 cycles depending on operand addressing modes
- **Best case:** 5 cycles (register and constant operands)
- **Worst case:** 8+ cycles (memory operands with indexed addressing)

**Note:** GETBF performs bit-level masking and shifting operations. Performance is consistent regardless of field size, as the operation is implemented in hardware.

**Bit numbering:** Remember that ND-500 uses little-endian bit numbering within bytes/words - bit 0 is the LSB, bit 31 is the MSB of a word.

---

## Reference Manual

**Section:** §10.31
**Title:** Get bit field

---

## See Also

- [PUTBF](putbf.md) - Put bit field (store register into bit field)
- [GETBI](getbi.md) - Get bit (extract single bit)
- [PUTBI](putbi.md) - Put bit (set single bit)
- [SETBI](setbi.md) - Set bit to 1
- [CLEBI](clebi.md) - Clear bit to 0
- [Trap System](../../ND-500-TRAPS.md)
