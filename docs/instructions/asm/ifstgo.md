# IFSTGO - If Status Bit Set, Go

## Overview

**Mnemonic:** `ifstgo`
**Function:** Conditional jump on status register bit
**Class:** BRANCH
**Privilege:** user

**Format:** `IFSTGO <bit_number>, <displacement>`

---

## Description

Performs a conditional branch if a specified bit in the CPU status register is set (equals 1). This instruction allows testing arbitrary status register bits beyond the standard Z, S, C, V flags, enabling fine-grained control flow based on processor state.

**Branch Behavior:**
```
if (status_register[bit_number] == 1) then
    PC = PC + sign_extend(displacement)
else
    PC = PC + instruction_length
endif
```

The bit number operand specifies which bit (0-29) in the status register to test. Different bits represent different processor states including flags, interrupt masks, privilege levels, and other architectural state.

**Status Register Bits (typical):**
- Bits 0-7: Condition codes (Z, S, C, V, K, etc.)
- Bits 8-15: Interrupt mask bits
- Bits 16-23: Mode and privilege bits
- Bits 24-29: Reserved/implementation-specific

**Common Use Cases:**
- Testing specific interrupt mask levels
- Checking privilege mode bits
- Testing implementation-specific flags
- Fine-grained processor state checking
- Low-level system programming

**Operands:** 2 (bit number + displacement)
**Variants:** 2 opcodes (byte vs halfword displacement)

---

## Variants

| Variant | Opcode | Displacement | Assembly Notation | Range |
|---------|--------|--------------|-------------------|-------|
| 1/2 | 0xFC7B | Byte | IFSTGO:B <bit>, <disp> | -128 to +127 |
| 2/2 | 0xFD64 | Halfword | IFSTGO:H <bit>, <disp> | -32768 to +32767 |

---

## Operands

**Operand 1** (Bit Number, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Byte (BY)
- **Range**: 0-29 (bits 30-31 reserved)
- **Role**: Status register bit to test

**Operand 2** (Displacement):
- **Byte variant**: 8-bit signed displacement
- **Halfword variant**: 16-bit signed displacement
- **Role**: Offset added to PC if bit is set

**Result**: PC potentially modified based on status bit

---

## Trap Conditions

- **Addressing traps**: If bit number address calculation fails
- **Branch trap (BT)**: If target address protection violation
- **Illegal operand value (IOV)**: If bit number > 29

---

## Data Status Bits

**All flags unaffected**

---

## Examples

### Example 1: Test interrupt mask bit

```assembly
% Check if specific interrupt level enabled
        IFSTGO:B 10, INTERRUPT_ENABLED
        % Interrupt level 10 disabled
        GO SKIP_INT_HANDLER
INTERRUPT_ENABLED:
        % Handle interrupt
```

### Example 2: Check privilege mode

```assembly
% Branch if in supervisor mode (bit 20)
        IFSTGO:B 20, SUPERVISOR_MODE
        % User mode - access denied
        GO ACCESS_ERROR
SUPERVISOR_MODE:
        % Supervisor mode - allow privileged operation
```

### Example 3: Test custom status flag

```assembly
% Check implementation-specific flag (bit 25)
        IFSTGO:B 25, FEATURE_ENABLED
        % Feature not available
        GO FALLBACK_CODE
FEATURE_ENABLED:
        % Use optimized feature
```

### Example 4: Multi-bit testing loop

```assembly
% Test multiple status bits
        W1 CLR
TEST_LOOP:
        IFSTGO:B I1, BIT_SET
        % Bit clear
        GO NEXT_BIT
BIT_SET:
        % Record which bit is set
        W2 MOVE I1, B.BIT_ARRAY(I2)
        W2 ADD 1, I2
NEXT_BIT:
        W1 ADD 1, I1
        W1 COMP I1, 30
        IF<GO TEST_LOOP
```

### Example 5: Interrupt mask checking

```assembly
% Check if interrupt 5 is masked
        IFSTGO:B INT_MASK_5, MASKED
        % Not masked - enable handler
        GO ENABLE_HANDLER
MASKED:
        % Masked - skip
```

### Example 6: Trace mode detection

```assembly
% Check if trace mode enabled (bit 18)
        IFSTGO:B 18, TRACE_ACTIVE
        % Normal execution
        RET
TRACE_ACTIVE:
        % Log instruction for debugging
        CALL LOG_TRACE
        RET
```

### Example 7: Dynamic bit selection

```assembly
% Test bit specified in register
        W1 MOVE BIT_TO_TEST, I1
        IFSTGO:H I1, BIT_IS_SET
        % Bit clear
        CLRK
        RET
BIT_IS_SET:
        % Bit set
        SETK
        RET
```

---

## Performance Notes

- **Size**: 4-5 bytes (opcode + bit number + displacement)
- **Execution**: 2-3 cycles (bit test + conditional branch)
- **Range**: Bit number 0-29 only (30-31 reserved)
- **vs Standard Branches**: More flexible but slower than IF=GO/IF<GO
- **Use Case**: System-level programming, interrupt handling
- **Validation**: Bit number > 29 causes trap

---

## Reference Manual

**Section:** §13.3
**Title:** Conditional jump

---

## See Also

- [IFKGO](ifkgo.md) - Branch if K flag set
- [IF=GO](if=go.md) - Branch if equal
- [IF<GO](if<go.md) - Branch if less than
- [SETK](setk.md) - Set K flag
- [CLRK](clrk.md) - Clear K flag
