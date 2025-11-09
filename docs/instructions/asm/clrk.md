# CLRK - Clear K Flag

## Overview

**Mnemonic:** `clrk`
**Function:** Clear K status flag bit
**Class:** CONTROL
**Privilege:** user

**Format:** `CLRK`

---

## Description

Clears the K (user flag) bit in the CPU status register to 0. The K flag is a general-purpose user flag under complete software control, complementing SETK which sets it to 1. This instruction is used to reset the K flag after it has been set or to ensure a known initial state.

**K Flag Uses:**
- **Reset Condition Flags**: Clear custom boolean states
- **Release Semaphores**: Clear lock indicators
- **Reset State Machines**: Return to initial/idle state
- **Clear Error Indicators**: Reset error flags after handling
- **Disable Features**: Turn off optional behavior
- **Initialize Algorithms**: Establish known starting state
- **Communication**: Signal completion or readiness

The K bit interacts with conditional branches IFKGO (branch if K=1) and IF-KGO (branch if K=0), enabling custom control flow. CLRK is typically used in pairs with SETK to toggle between two states or to initialize the flag before a sequence of operations.

**Operands:** 0
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFE03 | CLRK |

---

## Operands

**None**: Operates directly on K bit in status register.

**Result**: K flag bit = 0

---

## Trap Conditions

**None**: This instruction cannot trap

---

## Data Status Bits

- **K (User flag)**: Cleared to 0
- **Z, S, C, V**: Unaffected

---

## Examples

### Example 1: Clear error flag

```assembly
% Initialize - no error
        CLRK              % K=0 means no error

% After successful operation
        CLRK              % Clear any previous error
```

### Example 2: Release semaphore

```assembly
% Release lock
RELEASE_LOCK:
        CLRK              % K=0 means lock free
        % Lock released
        RET
```

### Example 3: Initialize feature flag

```assembly
% Disable optional feature at start
        CLRK              % K=0 means feature disabled

PROCESS:
        % Main work
        IFKGO DO_OPTIONAL % Skip if K=0
        RET
DO_OPTIONAL:
        % Not executed when K=0
        RET
```

### Example 4: State machine reset

```assembly
% Return to idle state
RESET:
        CLRK              % K=0 = idle state
        GO IDLE_STATE

IDLE_STATE:
        % Wait for activation
        IFKGO ACTIVE_STATE
        RET

ACTIVE_STATE:
        % Active processing
        RET
```

### Example 5: Clear success indicator

```assembly
% Indicate failure
OPERATION:
        % Try operation
        IF=GO FAILED
        SETK              % Success: K=1
        RET
FAILED:
        CLRK              % Failure: K=0
        RET
```

### Example 6: Loop initialization

```assembly
% Initialize loop control flag
        CLRK              % Start with K=0
LOOP:
        % Process
        % Some condition sets K
        IF<>GO CONTINUE
        SETK              % Signal done
CONTINUE:
        IFKGO LOOP_DONE
        GO LOOP
LOOP_DONE:
```

### Example 7: Boolean result initialization

```assembly
% Assume false initially
CHECK_ALL:
        CLRK              % Result = false
        % Check conditions
        IF<>GO CHECK_FAILED
        % More checks
        IF<>GO CHECK_FAILED
        SETK              % All passed: true
CHECK_FAILED:
        RET               % K indicates result
```

---

## Performance Notes

- **Size**: 2 bytes (single-word instruction)
- **Execution**: 1 cycle (direct status register write)
- **No Side Effects**: Only K bit modified
- **Paired with SETK**: Often used together to toggle state
- **Initialization**: Commonly used to establish known initial state

---

## Reference Manual

**Section:** §15.12
**Title:** Clear flag

---

## See Also

- [SETK](setk.md) - Set K flag
- [IFKGO](ifkgo.md) - Branch if K flag set
- [IF-KGO](if-kgo.md) - Branch if K flag clear
