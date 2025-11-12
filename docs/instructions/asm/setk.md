# SETK - Set K Flag

## Overview

**Mnemonic:** `setk`
**Function:** Set K status flag bit
**Class:** CONTROL
**Privilege:** user

**Format:** `SETK`

---

## Description

Sets the K (user flag) bit in the CPU status register to 1. The K flag is a general-purpose user flag that can be set, cleared, and tested by software for application-specific purposes. Unlike other status bits (Z, S, C, V) which are set automatically by arithmetic/logical operations, the K bit is entirely under software control.

**Operation:**
```
K flag = 1
```

**Key Characteristics:**
- User-controlled flag (not set by arithmetic operations)
- Single-cycle execution (very fast)
- Cannot trap (always succeeds)
- Independent of other flags (Z, S, C, V unchanged)
- Paired with CLRK (clear) and IFKGO/IF-KGO (test)
- Essential for custom control flow and state tracking
- Common in subroutine return status (RETK uses K=1 for success)
- Useful for boolean state across operations

**K Flag Uses:**
- **Custom Condition Flags**: Storing boolean state across operations
- **Semaphores/Locks**: Simple synchronization primitive
- **State Machines**: Tracking current state in algorithms
- **Error Indicators**: Marking error conditions in subroutines
- **Feature Flags**: Enabling/disabling optional behavior
- **Communication**: Passing single-bit information between routines
- **Loop Control**: Custom loop termination conditions (with IFKGO)

The K bit can be tested with conditional branches like IFKGO (if K set, goto) and IF-KGO (if K clear, goto), making it useful for custom control flow patterns not directly supported by standard comparison flags.

**Operands:** 0
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFE02 | SETK |

---

## Operands

**None**: Operates directly on K bit in status register.

**Result**: K flag bit = 1

---

## Trap Conditions

**None**: This instruction cannot trap

---

## Data Status Bits

- **K (User flag)**: Set to 1
- **Z, S, C, V**: Unaffected

---

## Examples

### Example 1: Set error flag

```assembly
% Mark error condition
VALIDATE:
        W1 COMP INPUT, MAX
        IF>GO SET_ERROR
        CLRK              % Clear error flag
        RET
SET_ERROR:
        SETK              % Set error flag
        RET
```

### Example 2: Simple semaphore

```assembly
% Acquire lock
ACQUIRE_LOCK:
        IFKGO LOCKED      % If K=1, lock already held
        SETK              % Set lock
        % Critical section
        RET
LOCKED:
        % Wait or handle contention
```

### Example 3: Feature toggle

```assembly
% Enable optional feature
        SETK              % K=1 means feature enabled

PROCESS:
        % Do main work
        IFKGO DO_OPTIONAL % If K set, do optional work
        RET
DO_OPTIONAL:
        % Optional feature code
        RET
```

### Example 4: State machine

```assembly
% Two-state machine (K=0: idle, K=1: active)
IDLE_STATE:
        % Wait for event
        IF=GO EVENT_OCCURRED
        RET
EVENT_OCCURRED:
        SETK              % Enter active state
        GO ACTIVE_STATE

ACTIVE_STATE:
        % Process while active
        IF=GO DONE
        RET
DONE:
        CLRK              % Return to idle
        GO IDLE_STATE
```

### Example 5: Subroutine success indicator

```assembly
% Return success/failure via K flag
OPEN_FILE:
        % Try to open file
        W1 COMP RESULT, 0
        IF<GO OPEN_FAILED
        SETK              % Success: K=1
        RET
OPEN_FAILED:
        CLRK              % Failure: K=0
        RET

% Caller checks K flag
        CALL OPEN_FILE
        IFKGO FILE_OPENED
        % Handle failure
```

### Example 6: Loop termination with custom condition

```assembly
% Process until K flag set (by external event/interrupt)
        CLRK              % Clear flag initially
LOOP:
        % Process work
        IFKGO DONE        % Exit if K set
        GO LOOP
DONE:
```

### Example 7: Boolean result passing

```assembly
% Set K based on complex condition
CHECK_CONDITION:
        W1 COMP A, B
        IF<>GO NOT_EQUAL
        W2 COMP C, D
        IF<>GO NOT_EQUAL
        SETK              % Both equal: K=1
        RET
NOT_EQUAL:
        CLRK              % At least one not equal: K=0
        RET
```

---

## Performance Notes

- **Size**: 2 bytes (single-word instruction)
- **Execution**: 1 cycle (direct status register write)
- **No Side Effects**: Only K bit modified, all other state preserved
- **vs Other Flags**: K is the only user-controlled flag (Z/S/C/V are automatic)
- **Typical Use**: Infrequent - mainly for special-purpose control flow

---

## Reference Manual

**Section:** §15.11
**Title:** Set flag

---

## See Also

- [CLRK](clrk.md) - Clear K flag
- [IFKGO](ifkgo.md) - Branch if K flag set
- [IF-KGO](if-kgo.md) - Branch if K flag clear
