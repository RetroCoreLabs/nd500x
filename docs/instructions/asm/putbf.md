# PUTBF - Put Bit Field

## Overview

**Mnemonic:** `putbf`
**Function:** Store register bit field into operand
**Class:** BITFIELD
**Privilege:** user

**Format:** `tn PUTBF <operand/w/t>,<bit No./r/BY>,<field size/r/BY>`

---

## Description

Stores a bit field from the lower bits of the specified register into a memory operand. Only the specified bit field in the operand is modified; other bits remain unchanged.

**Operation:**
```
mask = ((1 << field_size) - 1) << bit_no
field_value = register & ((1 << field_size) - 1)
operand = (operand & ~mask) | (field_value << bit_no)
```

**Key Characteristics:**
- Store register bit field into operand (read-modify-write)
- 12 variants (3 data types: BY/H/W × 4 registers)
- Bit 0 = LSB (rightmost), little-endian bit numbering
- Only specified field modified, other bits preserved
- IOV trap if bit_no < 0 or field_size ≤ 0
- IOV trap if (bit_no + field_size) exceeds data type width
- Sets Z flag if stored field = 0
- Sets S flag to leftmost bit of stored field
- Essential for packed structures and hardware registers
- 5-10 cycles (read-modify-write operation)

The bit field is specified by a starting bit number and a field size. Bit numbering follows the ND-500 convention where bit 0 is the rightmost (least significant) bit. The field storage starts at `<bit No.>` and extends to higher-numbered bits for `<field size>` bits.

The lower `<field size>` bits of the register (bits 0 to `<field size>-1`) are stored into the operand's bit field. The operand can be a byte (BY), halfword (H), or word (W) data type. The bit number and field size parameters are interpreted as signed byte integers.

An illegal operand value trap (IOV) is triggered if:
- `<bit No.>` is negative
- `<field size>` is zero or negative
- `<bit No.>` + `<field size>` exceeds the number of bits in the data type (8 for BY, 16 for H, 32 for W)

This instruction is essential for:
- Setting multiple flag bits in a single operation
- Writing to hardware register fields
- Implementing bitfield structures in high-level languages
- Encoding packed binary protocols

**Operands:** 3
**Variants:** 12 opcode(s)

---

## Variants

Total variants: 12 (3 data types × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly Notation |
|---------|--------|-----------|----------|-------------------|
| 1/12 | 0xFDEC | BY | 1 | BY1 PUTBF |
| 2/12 | 0xFDED | BY | 2 | BY2 PUTBF |
| 3/12 | 0xFDEE | BY | 3 | BY3 PUTBF |
| 4/12 | 0xFDEF | BY | 4 | BY4 PUTBF |
| 5/12 | 0xFDF0 | H | 1 | H1 PUTBF |
| 6/12 | 0xFDF1 | H | 2 | H2 PUTBF |
| 7/12 | 0xFDF2 | H | 3 | H3 PUTBF |
| 8/12 | 0xFDF3 | H | 4 | H4 PUTBF |
| 9/12 | 0xFDF4 | W | 1 | W1 PUTBF |
| 10/12 | 0xFDF5 | W | 2 | W2 PUTBF |
| 11/12 | 0xFDF6 | W | 3 | W3 PUTBF |
| 12/12 | 0xFDF7 | W | 4 | W4 PUTBF |

---

## Operands

### Operand 1 (Destination Operand)

The operand into which the bit field will be stored. Only the specified bit field is modified.

**Type:** BY, H, or W (determined by instruction prefix)
**Access:** Write

**Supported modes:**
- **LOCAL** - Local variable
- **RECORD** - Record field
- **REGISTER** - Register (cannot use CONSTANT)
- **PRE_INDEXED** - Indexed addressing
- **ABSOLUTE** - Absolute memory address

**Note:** CONSTANT mode is not allowed for write operations.

### Operand 2 (Bit Number)

The starting bit position where the field will be stored (0 = rightmost bit).

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

The number of bits to store from the register into the operand.

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

- **Z (Zero):** Set if stored bit field = 0, cleared otherwise
- **S (Sign):** Set to leftmost bit of stored field
- **O (Overflow):** Not affected
- **C (Carry):** Not affected

---

## Examples

### Example 1: Store 8-bit field into word

```assembly
        % Store 8-bit value into record variable at bits 8-15
        % W2 contains the value to store
        W2 PUTBF R.FLAGSET, 8, 8
        % Bits 8-15 of R.FLAGSET now contain lower 8 bits of W2
        % Other bits of R.FLAGSET unchanged
```

### Example 2: Set flag bits

```assembly
        % Set error flags (bits 5-7) to specific pattern
        W1 := 5              % Error code 5 = 0b101
        W1 PUTBF B.STATUS, 5, 3
        % Bits 5-7 of B.STATUS now contain 101
        % Other status bits unchanged
```

### Example 3: Encode packed structure

```assembly
        % Create packed word: type(4 bits) | size(8 bits) | flags(4 bits)
        W1 := 3              % Type = 3
        W1 PUTBF B.PACKET, 0, 4
        W2 := 127            % Size = 127
        W2 PUTBF B.PACKET, 4, 8
        W3 := 15             % Flags = all set
        W3 PUTBF B.PACKET, 12, 4
        % B.PACKET now contains complete packed structure
```

### Example 4: Set variable-width field

```assembly
        % Store field with runtime-determined position and size
        % B.BIT_POS and B.BIT_LEN determined at runtime
        W1 := B.VALUE
        W1 PUTBF B.DATA, B.BIT_POS, B.BIT_LEN
        % Handle storage error
        IF-KGO STORE_OK
        % IOV trap occurred - invalid parameters
        CALL ERROR_HANDLER
STORE_OK:
```

### Example 5: Update hardware register field

```assembly
        % Update device control register field
        % Set baud rate field (bits 8-11) to 9600 baud (value 7)
        W1 := 7
        W1 PUTBF R.UART_CTRL, 8, 4
        % Only baud rate bits modified, other control bits unchanged
        % Write to hardware
        W MOVE R.UART_CTRL, @UART_REG
```

### Example 6: Atomic flag update

```assembly
        % Atomically update multiple flag bits
        % Read current status
        W1 := B.STATUS
        % Extract current flags (bits 0-7)
        W2 GETBF I1, 0, 8
        % Modify specific flags (set bit 3, clear bit 5)
        W OR I2, 8           % Set bit 3
        W AND I2, -33        % Clear bit 5 (AND with ~32)
        % Write back
        W2 PUTBF B.STATUS, 0, 8
```

---

## Performance Notes

- **Typical cycles:** 5-10 cycles depending on operand addressing modes
- **Best case:** 5 cycles (register and constant operands)
- **Worst case:** 10+ cycles (memory operands with indexed addressing, read-modify-write)

**Note:** PUTBF performs a read-modify-write operation: it reads the current operand value, modifies only the specified bit field, then writes the result back. This ensures other bits remain unchanged.

**Bit numbering:** Remember that ND-500 uses little-endian bit numbering within bytes/words - bit 0 is the LSB, bit 31 is the MSB of a word.

**Atomicity:** PUTBF is not atomic. In multi-processing environments, use synchronization primitives to protect shared bitfields.

---

## Reference Manual

**Section:** §10.32
**Title:** Put bit field

---

## See Also

- [GETBF](getbf.md) - Get bit field (extract register from bit field)
- [PUTBI](putbi.md) - Put bit (set single bit)
- [GETBI](getbi.md) - Get bit (extract single bit)
- [SETBI](setbi.md) - Set bit to 1
- [CLEBI](clebi.md) - Clear bit to 0
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
