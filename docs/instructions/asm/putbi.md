# PUTBI - Put Bit

## Overview

**Mnemonic:** `putbi`
**Function:** Store register bit into operand bit
**Class:** BITFIELD
**Privilege:** user

**Format:** `tn PUTBI <operand/w/t>,<bit No./r/BY>`

---

## Description

Stores bit 0 of the specified register into a single bit of the operand. Only the specified bit in the operand is modified; other bits remain unchanged.

**Key Characteristics:**
- Single-bit storage from register bit 0
- Read-modify-write operation (preserves other bits)
- Supports 3 data types: BY (8-bit), H (16-bit), W (32-bit)
- 12 variants (3 types × 4 registers) for flexible allocation
- Z flag reflects stored bit value (0 or 1)
- Essential for flag setting and hardware control
- Not atomic (requires synchronization in multiprocessor systems)
- Traps on out-of-range bit numbers

The bit number specifies which bit to modify, with bit numbering following the ND-500 convention where bit 0 is the rightmost (least significant) bit. The operand can be a byte (BY), halfword (H), or word (W) data type.

Even when the operand is a word register, only the specified bit is affected - the upper bits of the destination register remain unchanged. This makes PUTBI ideal for manipulating individual flag bits without affecting neighboring bits.

An illegal operand value trap (IOV) is triggered if:
- `<bit No.>` is negative
- `<bit No.>` >= number of bits in the data type (8 for BY, 16 for H, 32 for W)

This instruction is essential for:
- Setting/clearing individual flag bits
- Implementing boolean operations
- Writing hardware control bits
- Updating packed boolean arrays

**Operands:** 2
**Variants:** 12 opcode(s)

---

## Variants

Total variants: 12 (3 data types × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly Notation |
|---------|--------|-----------|----------|-------------------|
| 1/12 | 0xFDD4 | BY | 1 | BY1 PUTBI |
| 2/12 | 0xFDD5 | BY | 2 | BY2 PUTBI |
| 3/12 | 0xFDD6 | BY | 3 | BY3 PUTBI |
| 4/12 | 0xFDD7 | BY | 4 | BY4 PUTBI |
| 5/12 | 0xFDD8 | H | 1 | H1 PUTBI |
| 6/12 | 0xFDD9 | H | 2 | H2 PUTBI |
| 7/12 | 0xFDDA | H | 3 | H3 PUTBI |
| 8/12 | 0xFDDB | H | 4 | H4 PUTBI |
| 9/12 | 0xFDDC | W | 1 | W1 PUTBI |
| 10/12 | 0xFDDD | W | 2 | W2 PUTBI |
| 11/12 | 0xFDDE | W | 3 | W3 PUTBI |
| 12/12 | 0xFDDF | W | 4 | W4 PUTBI |

---

## Operands

### Operand 1 (Destination Operand)

The operand into which bit 0 of the register will be stored. Only the specified bit is modified.

**Type:** BY, H, or W (determined by instruction prefix)
**Access:** Write (read-modify-write)

**Supported modes:**
- **LOCAL** - Local variable
- **RECORD** - Record field
- **REGISTER** - Register (cannot use CONSTANT)
- **PRE_INDEXED** - Indexed addressing
- **ABSOLUTE** - Absolute memory address

**Note:** CONSTANT mode is not allowed for write operations.

### Operand 2 (Bit Number)

The bit position where register bit 0 will be stored (0 = rightmost/LSB).

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

- **Z (Zero):** Set if transferred bit = 0, cleared if bit = 1
- **S (Sign):** Not affected
- **O (Overflow):** Not affected
- **C (Carry):** Not affected

---

## Examples

### Example 1: Set flag bit from register

```assembly
        % Store bit 0 of R4 into bit 4 of local byte variable FLAGS
        BY4 PUTBI B.FLAGS, 4
        % Bit 4 of B.FLAGS now contains the value from R4 bit 0
        % Other bits of B.FLAGS unchanged
```

### Example 2: Update boolean array

```assembly
        % Store boolean value into packed array
        % B.BOOL_ARRAY contains packed bits, 32 per word
        % W1 contains boolean value (0 or 1) in bit 0
        % B.BIT_INDEX contains target bit position
        % Calculate word index and bit position
        W2 := B.BIT_INDEX
        W3 := I2
        W SHR I3, 5              % Divide by 32 for word index
        W2 AND I2, 31            % Modulo 32 for bit position
        % Store the bit
        W1 PUTBI B.BOOL_ARRAY(I3), I2
```

### Example 3: Set hardware control bit

```assembly
        % Enable device interrupt (set bit 5 of control register)
        W1 := 1                  % Load 1 into R1 bit 0
        W1 PUTBI @DEVICE_CTRL, 5
        % Bit 5 of device control register now set
```

### Example 4: Toggle bit implementation

```assembly
        % Toggle bit N in FLAGS
        W1 GETBI B.FLAGS, N      % Read current bit
        W XOR I1, 1              % Toggle bit 0
        W1 PUTBI B.FLAGS, N      % Write back
```

### Example 5: Conditional bit update

```assembly
        % Set error flag if condition met
        W COMP B.STATUS, B.ERROR_CODE
        IF<>GO NO_ERROR
        W1 := 1
        W1 PUTBI B.FLAGS, B.ERROR_BIT
        GO CONTINUE
NO_ERROR:
        W1 := 0
        W1 PUTBI B.FLAGS, B.ERROR_BIT
CONTINUE:
```

### Example 6: Copy bit between variables

```assembly
        % Copy bit 3 from SOURCE to bit 7 in DEST
        W1 GETBI B.SOURCE, 3
        W1 PUTBI B.DEST, 7
```

---

## Performance Notes

- **Typical cycles:** 5-8 cycles depending on operand addressing modes
- **Best case:** 5 cycles (register and constant operands)
- **Worst case:** 8+ cycles (memory operands with indexed addressing, read-modify-write)

**Note:** PUTBI performs a read-modify-write operation to preserve other bits in the destination operand. This ensures only the targeted bit is modified.

**Bit numbering:** Remember that ND-500 uses little-endian bit numbering - bit 0 is the LSB.

**Atomicity:** PUTBI is not atomic. In multi-processing environments, use synchronization primitives to protect shared flag variables.

---

## Reference Manual

**Section:** §10.28
**Title:** Put bit

---

## See Also

- [GETBI](getbi.md) - Get bit (extract operand bit into register)
- [SETBI](setbi.md) - Set bit to 1 unconditionally
- [CLEBI](clebi.md) - Clear bit to 0 unconditionally
- [PUTBF](putbf.md) - Put bit field (multi-bit storage)
- [TSET](tset.md) - Test and set bit
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
