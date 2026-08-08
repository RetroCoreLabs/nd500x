# CLEBI - Clear Bit

## Overview

**Mnemonic:** `clebi`
**Function:** Clear bit to 0 unconditionally
**Class:** BITFIELD
**Privilege:** user

**Format:** `t CLEBI <operand/w/t>,<bit No./r/BY>`

---

## Description

Clears a specified bit in an operand to 0, regardless of its previous value. Only the specified bit is affected; all other bits in the operand remain unchanged.

**Key Characteristics:**
- Unconditional bit clear operation (always sets to 0)
- Single-instruction bit manipulation (no register required)
- Supports 3 data types: BY (8-bit), H (16-bit), W (32-bit)
- More efficient than PUTBI for known bit clears
- Z flag always set to 1 (distinguishes from PUTBI)
- Essential for flag clearing and hardware control
- Not atomic (requires synchronization in multiprocessor systems)
- Traps on out-of-range bit numbers

The bit number specifies which bit to clear, with bit numbering following the ND-500 convention where bit 0 is the rightmost (least significant) bit. The operand can be a byte (BY), halfword (H), or word (W) data type.

Unlike PUTBI which stores a register bit value, CLEBI unconditionally clears the target bit to 0. This makes it more efficient when you know you want to clear a bit without first loading a register.

An illegal operand value trap (IOV) is triggered if:
- `<bit No.>` is negative
- `<bit No.>` >= number of bits in the data type (8 for BY, 16 for H, 32 for W)

This instruction is essential for:
- Disabling flag bits
- Clearing hardware control bits
- Implementing bit clears
- Resetting status flags

**Operands:** 2
**Variants:** 3 opcode(s)

---

## Variants

Total variants: 3 (one per data type, no register variants)

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/3 | 0xFE7D | BY | BY CLEBI |
| 2/3 | 0xFE7E | H | H CLEBI |
| 3/3 | 0xFE7F | W | W CLEBI |

---

## Operands

### Operand 1 (Destination Operand)

The operand in which a bit will be cleared to 0. Only the specified bit is modified.

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

The bit position to clear to 0 (0 = rightmost/LSB).

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

- **Z (Zero):** Set to 1 (always)
- **S (Sign):** Not affected
- **O (Overflow):** Not affected
- **C (Carry):** Not affected

**Note:** Z flag is always set to 1, distinguishing CLEBI from PUTBI.

---

## Examples

### Example 1: Clear register bit

```assembly
        % Clear bit N of word register R1
        W CLEBI R1, N
        % Bit N of R1 now 0, other bits unchanged
```

### Example 2: Disable interrupt flag

```assembly
        % Disable interrupts by clearing bit 7 in control register
        W CLEBI B.CTRL_REG, 7
        % Bit 7 of B.CTRL_REG now cleared to 0
```

### Example 3: Clear error flag

```assembly
        % Clear error flag (bit 3) in status word
        W CLEBI B.STATUS, 3
```

### Example 4: Clear bit in bitmap

```assembly
        % Mark resource as free in bitmap
        % B.RESOURCE_ID contains the resource number
        W1 := B.RESOURCE_ID
        W2 := I1
        W SHR I2, 5              % Divide by 32 for word index
        W1 AND I1, 31            % Modulo 32 for bit position
        W CLEBI B.BITMAP(I2), I1
```

### Example 5: Clear multiple flags

```assembly
        % Clear various status flags
        W CLEBI B.FLAGS, B.ERROR_BIT
        W CLEBI B.FLAGS, B.BUSY_BIT
        W CLEBI B.FLAGS, B.PENDING_BIT
```

### Example 6: Mask implementation

```assembly
        % Clear specific bits to create a mask
        W1 := -1                 % All bits set
        W CLEBI I1, 0            % Clear bit 0
        W CLEBI I1, 15           % Clear bit 15
        W CLEBI I1, 31           % Clear bit 31
        % W1 now contains mask with holes at bits 0, 15, 31
```

### Example 7: Conditional bit clear

```assembly
        % Clear ready bit if timeout occurred
        W COMP B.TIMER, B.TIMEOUT
        IF<GO NO_TIMEOUT
        W CLEBI B.STATUS, B.READY_BIT
NO_TIMEOUT:
```

---

## Performance Notes

- **Typical cycles:** 4-7 cycles depending on operand addressing modes
- **Best case:** 4 cycles (simple addressing)
- **Worst case:** 7+ cycles (complex indexed addressing with page fault)

**Note:** CLEBI is slightly more efficient than loading a 0 into a register and using PUTBI, as it doesn't require a register and performs the operation in a single instruction.

**Bit numbering:** Remember that ND-500 uses little-endian bit numbering - bit 0 is the LSB.

**Atomicity:** CLEBI is not atomic. In multi-processing environments, use synchronization primitives to protect shared flag variables.

---

## Reference Manual

**Section:** §10.29
**Title:** Clear bit

---

## See Also

- [SETBI](setbi.md) - Set bit to 1 unconditionally
- [PUTBI](putbi.md) - Put bit (clear from register value)
- [GETBI](getbi.md) - Get bit (extract bit into register)
- [AND](and.md) - Logical AND (can clear multiple bits with mask)
- [Trap System](../../ND-500-TRAPS.md)
