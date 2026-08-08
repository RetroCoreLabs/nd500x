# TUTTI - Enable Process Switch

## Overview

**Mnemonic:** `tutti`
**Function:** Enable process switching (end atomic section)
**Class:** CONTROL
**Privilege:** user

**Format:** `TUTTI`

---

## Description

Re-enables process switching after a SOLO instruction, terminating the atomic execution section. TUTTI must be paired with every SOLO to allow the operating system to resume normal process scheduling and context switching.

**Operation:**
```
Re-enable process switch
End atomic section started by SOLO
Resume normal process scheduling
```

**Key Characteristics:**
- No operands (operates on process switch state)
- Must be paired with preceding SOLO instruction
- Terminates atomic execution section
- Allows OS to resume process switching
- No timeout restrictions (instant re-enable)

**Common Use Cases:**
- Terminating atomic operations started with SOLO
- Ending critical sections
- Completing lock acquisition/release sequences
- Finalizing synchronized data structure updates
- Closing mutual exclusion regions

**Operands:** 0 (no operands)
**Variants:** 1 opcode

---

## Variants

Total variants: 1

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFE01 | TUTTI |

---

## Trap Conditions

None (TUTTI cannot cause traps).

---

## Data Status Bits

No flags affected.

---

## Examples

### Example 1: Atomic increment

```assembly
        % Atomic increment of shared variable
        SOLO
        W1 := COUNTER
        W1 INC
        W1 =: COUNTER
        TUTTI
```

**Explanation:** TUTTI terminates the atomic section, allowing other processes to access COUNTER.

### Example 2: Spinlock release

```assembly
        % Release spinlock
        SOLO
        W1 := 0
        W1 =: LOCK
        TUTTI
```

**Explanation:** TUTTI after clearing lock ensures atomic release.

### Example 3: Test-and-set with retry

```assembly
ACQUIRE:
        SOLO
        W1 := LOCK
        W1 TEST
        IF><GO HELD
        % Acquire lock
        W1 := 1
        W1 =: LOCK
        TUTTI
        GO ACQUIRED
HELD:
        TUTTI                       % Must TUTTI before retry
        CALL SHORT_DELAY
        GO ACQUIRE
ACQUIRED:
```

**Explanation:** TUTTI required even on failed acquisition path to re-enable switching.

### Example 4: Compare-and-swap completion

```assembly
        SOLO
        W1 := SHARED
        W1 COMP EXPECTED
        IF><GO CAS_FAIL
        NEW_VALUE =: SHARED
        W1 := 1                    % Success
        GO CAS_END
CAS_FAIL:
        W1 := 0                    % Failure
CAS_END:
        TUTTI
```

**Explanation:** TUTTI after both success and failure paths.

### Example 5: Queue operation

```assembly
        % Enqueue item
        SOLO
        W1 := TAIL
        W2 =: (W1)
        W1 := W1 + SIZE
        W1 =: TAIL
        TUTTI
```

**Explanation:** TUTTI completes atomic queue update.

### Example 6: Reference count update

```assembly
        % Increment refcount
        SOLO
        W1 := OBJ.REFCOUNT
        W1 INC
        W1 =: OBJ.REFCOUNT
        TUTTI
```

**Explanation:** TUTTI allows other threads to access object's refcount.

### Example 7: Nested SOLO prevention

```assembly
        % Check if already in SOLO before entering
        W1 := IN_SOLO_FLAG
        W1 TEST
        IF><GO ALREADY_ATOMIC
        % Not in SOLO, enter
        W1 := 1
        W1 =: IN_SOLO_FLAG
        SOLO
        % ... atomic operations ...
        TUTTI
        W1 := 0
        W1 =: IN_SOLO_FLAG
ALREADY_ATOMIC:
```

**Explanation:** Preventing nested SOLO (which would cause timeout). TUTTI restores state.

---

## Performance Notes

- **Execution:** 1-2 cycles
- **Overhead:** Minimal - just re-enables context switch hardware
- **Implementation:** Hardware-level process switch re-enable

**Usage recommendations:**
- Always pair with preceding SOLO
- Execute TUTTI on all code paths (success and failure)
- Don't forget TUTTI before loops that might retry
- No side effects - safe to execute multiple times
- Essential for proper OS scheduling resumption

**Critical pattern enforcement:**
```
Every SOLO must have matching TUTTI
Bad:
  SOLO
  IF condition GO EARLY_EXIT
  TUTTI
  ...
EARLY_EXIT:  % BUG: No TUTTI!

Good:
  SOLO
  IF condition GO EARLY_EXIT
  TUTTI
  ...
EARLY_EXIT:
  TUTTI  % Correct: TUTTI on all paths
```

**Pairing requirements:**
- One TUTTI for every SOLO
- TUTTI on all execution paths
- TUTTI before any branch that exits atomic section
- No nested SOLO/TUTTI pairs (causes timeout)

---

## Reference Manual

**Section:** §16.2
**Title:** Enable process switch

---

## See Also

- [SOLO](solo.md) - Disable process switch (begin atomic section)
- [ENTT](entt.md) - Enter trap handler
- [WAIT](wait.md) - Wait for event
- [CONTROL instruction reference](../../instruction-reference/CONTROL.md)
