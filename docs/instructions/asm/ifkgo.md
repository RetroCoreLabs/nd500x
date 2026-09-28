# IFKGO - If K Flag Set, Go

## Overview

**Mnemonic:** `ifkgo`
**Function:** Conditional jump if K flag is set
**Class:** BRANCH
**Privilege:** user

**Format:** `IFKGO <displacement>`

---

## Description

Performs a conditional branch if the K (user flag) bit in the status register is set (K=1). If the condition is true, the sign-extended displacement is added to the program counter, transferring control to the target address. If K=0, execution continues with the next instruction.

This instruction is part of the ND-500's comprehensive conditional branch system. Unlike other conditional branches that test arithmetic flags (Z, S, C, V), IFKGO tests the user-controlled K flag, which is set/cleared explicitly by SETK/CLRK instructions. This enables custom control flow patterns based on application-specific conditions.

**Branch Behavior:**
```
if (K == 1) then
    PC = PC + sign_extend(displacement)
else
    PC = PC + instruction_length
endif
```

**Key Characteristics:**
- Tests user-controlled K flag (not arithmetic flags)
- Two displacement sizes: byte (±127), halfword (±32767)
- Paired with SETK/CLRK for custom control flow
- Independent of Z, S, C, V flags (orthogonal branching)
- Essential for semaphores, state machines, error flags
- Fast conditional branch (1-2 cycles)
- Assembler auto-selects displacement size for optimal code
- Common in event handling and feature toggles

**Displacement Encoding:**
- **Byte displacement** (0x00D0): -128 to +127 bytes (short jumps)
- **Halfword displacement** (0x00D1): -32768 to +32767 bytes (long jumps)

**Common Use Cases:**
- Testing custom condition flags set by SETK
- Semaphore/lock checking
- State machine transitions
- Error flag testing
- Feature enable/disable branching
- Event signaling in interrupt handlers

IFKGO is often used in conjunction with SETK/CLRK to implement boolean variables, simple locks, or custom state tracking that doesn't fit the standard arithmetic comparison model.

**Operands:** 1 (displacement)
**Variants:** 2 opcodes (byte vs halfword displacement)

---

## Variants

| Variant | Opcode | Displacement | Assembly Notation | Range |
|---------|--------|--------------|-------------------|-------|
| 1/2 | 0x00D0 | Byte | IFKGO:B <disp> | -128 to +127 |
| 2/2 | 0x00D1 | Halfword | IFKGO:H <disp> | -32768 to +32767 |

---

## Operands

**Operand 1** (Displacement):
- **Byte variant**: 8-bit signed displacement
- **Halfword variant**: 16-bit signed displacement
- **Role**: Offset added to PC if K=1

**Result**: PC potentially modified based on K flag

---

## Trap Conditions

- **Addressing traps**: If target address is invalid
- **Branch trap (BT)**: If branch target protection violation

---

## Data Status Bits

**All flags unaffected** (K, Z, S, C, V unchanged)

---

## Examples

### Example 1: Test error flag

```assembly
% Check if error occurred
        CALL OPERATION
        IFKGO:B ERROR_HANDLER
        % No error, continue
        RET

ERROR_HANDLER:
        % Handle error (K was set by OPERATION)
```

### Example 2: Semaphore check

```assembly
% Wait for lock to be free
WAIT_LOCK:
        IFKGO:B WAIT_LOCK   % Loop while K=1 (locked)
        % Lock is free (K=0), acquire it
        SETK
```

### Example 3: State machine

```assembly
% Branch based on state
CHECK_STATE:
        IFKGO:B ACTIVE_STATE
        % K=0: idle state processing
        GO IDLE_PROCESSING

ACTIVE_STATE:
        % K=1: active state processing
```

### Example 4: Optional feature execution

```assembly
% Execute feature if enabled
        IFKGO:B DO_FEATURE
        % Feature disabled (K=0), skip
        RET

DO_FEATURE:
        % Feature enabled (K=1), execute
        CALL OPTIONAL_CODE
        RET
```

### Example 5: Event handling

```assembly
% Check if event occurred
POLL_EVENT:
        % K set by interrupt handler when event occurs
        IFKGO:B EVENT_OCCURRED
        % No event yet, continue waiting
        GO POLL_EVENT

EVENT_OCCURRED:
        CLRK              % Clear event flag
        % Process event
```

### Example 6: Loop termination with custom flag

```assembly
% Process until K flag set
PROCESS_LOOP:
        % Do work
        CALL PROCESS_ITEM
        % Check termination flag
        IFKGO:B DONE
        GO PROCESS_LOOP

DONE:
        % K was set, processing complete
```

### Example 7: Function result check

```assembly
% Call function that returns status via K
        CALL CHECK_PERMISSION
        IFKGO:B PERMITTED
        % Permission denied (K=0)
        GO DENY_ACCESS

PERMITTED:
        % Permission granted (K=1)
        % Continue with operation
```

---

## Performance Notes

- **Size**: 3 bytes (byte disp) or 4 bytes (halfword disp)
- **Execution**: 1-2 cycles (faster if not taken)
- **Branch Prediction**: May benefit from static prediction on modern implementations
- **Range**: Use byte displacement for nearby targets (smaller, faster)
- **vs Arithmetic Branches**: IFKGO is orthogonal to Z/S/C/V flags
- **Typical Use**: Less common than IF=GO/IF<GO, but essential for custom logic

---

## Reference Manual

**Section:** §13.3
**Title:** Conditional jump

---

## See Also

- [SETK](setk.md) - Set K flag
- [CLRK](clrk.md) - Clear K flag
- [IF-KGO](if-kgo.md) - Branch if K flag clear
- [IF=GO](if=go.md) - Branch if equal
- [IF<GO](iflessthango.md) - Branch if less than
