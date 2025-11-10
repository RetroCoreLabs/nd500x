# SETBI - Set Bit

## Overview

**Mnemonic:** `setbi`
**Function:** Set bit to 1 unconditionally
**Class:** BITFIELD
**Privilege:** user

**Format:** `t SETBI <operand/w/t>,<bit No./r/BY>`

---

## Description

Sets a specified bit in an operand to 1, regardless of its previous value. Only the specified bit is affected; all other bits in the operand remain unchanged.

The bit number specifies which bit to set, with bit numbering following the ND-500 convention where bit 0 is the rightmost (least significant) bit. The operand can be a byte (BY), halfword (H), or word (W) data type.

Unlike PUTBI which stores a register bit value, SETBI unconditionally sets the target bit to 1. This makes it more efficient when you know you want to set a bit without first loading a register.

An illegal operand value trap (IOV) is triggered if:
- `<bit No.>` is negative
- `<bit No.>` >= number of bits in the data type (8 for BY, 16 for H, 32 for W)

This instruction is essential for:
- Enabling flag bits
- Setting hardware control bits
- Implementing bit sets
- Marking status flags

**Operands:** 2
**Variants:** 3 opcode(s)

---

## Variants

Total variants: 3 (one per data type, no register variants)

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/3 | 0xFE80 | BY | BY SETBI |
| 2/3 | 0xFE81 | H | H SETBI |
| 3/3 | 0xFE82 | W | W SETBI |

---

## Operands

### Operand 1 (Destination Operand)

The operand in which a bit will be set to 1. Only the specified bit is modified.

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

The bit position to set to 1 (0 = rightmost/LSB).

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

- **Z (Zero):** Cleared (always 0)
- **S (Sign):** Cleared (always 0)
- **O (Overflow):** Cleared (always 0)
- **C (Carry):** Cleared (always 0)

**Note:** All status bits are unconditionally cleared, distinguishing SETBI from PUTBI.

---

## Examples

### Example 1: Enable interrupt flag

```assembly
        % Enable interrupts by setting bit 7 in control register
        W SETBI B.CTRL_REG, 7
        % Bit 7 of B.CTRL_REG now set to 1, other bits unchanged
```

### Example 2: Set error flag

```assembly
        % Set error flag (bit 3) in status word
        W SETBI B.STATUS, 3
```

### Example 3: Mark bit in bitmap

```assembly
        % Mark resource as allocated in bitmap
        % B.RESOURCE_ID contains the resource number
        W1 := B.RESOURCE_ID
        W2 := I1
        W SHR I2, 5              % Divide by 32 for word index
        W1 AND I1, 31            % Modulo 32 for bit position
        W SETBI B.BITMAP(I2), I1
```

### Example 4: Set multiple flags

```assembly
        % Set various status flags
        W SETBI B.FLAGS, B.READY_BIT
        W SETBI B.FLAGS, B.ENABLED_BIT
        W SETBI B.FLAGS, B.INITIALIZED_BIT
```

### Example 5: Set bit with ALT prefix

```assembly
        % Set FAILURE bit in word argument EXCEPTIONS on alternative domain
        W SETBI ALT(IND(B.EXCEPTIONS)), FAILURE
```

### Example 6: Conditional bit set

```assembly
        % Set error bit if error detected
        W COMP B.RESULT, 0
        IF>=GO NO_ERROR
        W SETBI B.ERROR_FLAGS, B.OVERFLOW_BIT
NO_ERROR:
```

---

## Performance Notes

- **Typical cycles:** 4-7 cycles depending on operand addressing modes
- **Best case:** 4 cycles (simple addressing)
- **Worst case:** 7+ cycles (complex indexed addressing with page fault)

**Note:** SETBI is slightly more efficient than loading a 1 into a register and using PUTBI, as it doesn't require a register and performs the operation in a single instruction.

**Bit numbering:** Remember that ND-500 uses little-endian bit numbering - bit 0 is the LSB.

**Atomicity:** SETBI is not atomic. In multi-processing environments, use synchronization primitives to protect shared flag variables.

---

## Reference Manual

**Section:** §10.30
**Title:** Set bit

---

## See Also

- [CLEBI](clebi.md) - Clear bit to 0 unconditionally
- [PUTBI](putbi.md) - Put bit (set from register value)
- [GETBI](getbi.md) - Get bit (extract bit into register)
- [TSET](tset.md) - Test and set bit (atomic)
- [OR](or.md) - Logical OR (can set multiple bits with mask)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
