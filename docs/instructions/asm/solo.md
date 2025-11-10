# SOLO - Disable Process Switch

## Overview

**Mnemonic:** `solo`
**Function:** Disable process switching (begin atomic section)
**Class:** CONTROL
**Privilege:** user (with timeout restrictions)

**Format:** `SOLO`

---

## Description

Disables process switching for up to 256 micro-cycles, ensuring that instructions between SOLO and TUTTI execute as an indivisible atomic sequence. This is essential for synchronization, mutual exclusion, and implementing protection mechanisms in multi-process systems.

**Operation:**
```
Disable process switch for maximum 256 micro-cycles
Must be terminated with TUTTI instruction
Timeout error if exceeded in user mode
```

**Key Characteristics:**
- No operands (operates on process switch state)
- Must be paired with TUTTI to re-enable process switching
- User mode: 256 micro-cycle limit, timeout error if exceeded
- Supervisor mode: no duration limitation
- Critical for implementing atomic operations and locks
- No traps allowed during SOLO section (would cause timeout)

**Timeout Behavior:**
- **ND-500/2:** Measured in micro-cycles
- **ND-5000:** Measured in macro-instruction cycles
- User mode: Timeout after 256 cycles
- Supervisor mode: No timeout

**Common Use Cases:**
- Implementing mutual exclusion (locks/semaphores)
- Atomic read-modify-write operations
- Critical sections in concurrent code
- Synchronizing access to shared data structures
- Implementing test-and-set primitives
- Protecting non-reentrant code sequences

**Operands:** 0 (no operands)
**Variants:** 1 opcode

---

## Variants

Total variants: 1

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFE00 | SOLO |

---

## Trap Conditions

- **Disable process switch timeout (DT):** User mode exceeds 256 micro-cycles in SOLO
- **Disable process switch error (DE):** Trap occurs during SOLO section (fatal)

**Important:** No enabled trap conditions may occur during SOLO section. Any trap handling exceeds 256 micro-cycles and causes timeout. Non-ignorable and fatal traps cause disable process switch error trap.

---

## Data Status Bits

No flags affected.

---

## Examples

### Example 1: Atomic increment (test-and-set)

```assembly
        % Atomic increment of shared counter
        SOLO
        W1 := SHARED_COUNTER
        W1 INC
        W1 =: SHARED_COUNTER
        TUTTI
```

**Explanation:** Classic atomic increment. No process switch can interrupt between read and write.

### Example 2: Implement spinlock acquisition

```assembly
        % Spinlock: loop until lock acquired atomically
ACQUIRE_LOCK:
        SOLO
        W1 := LOCK_VARIABLE
        W1 TEST
        IF><GO LOCK_HELD           % Lock already held
        % Lock is free, acquire it
        W1 := 1
        W1 =: LOCK_VARIABLE
        TUTTI
        % Lock acquired
        GO LOCK_ACQUIRED
LOCK_HELD:
        TUTTI
        % Brief delay, then retry
        CALL SHORT_DELAY
        GO ACQUIRE_LOCK
LOCK_ACQUIRED:
```

**Explanation:** Spinlock: atomically test and set lock variable.

### Example 3: Release spinlock

```assembly
        % Release lock atomically
        SOLO
        W1 := 0
        W1 =: LOCK_VARIABLE
        TUTTI
```

**Explanation:** Atomically clear lock to release it.

### Example 4: Atomic compare-and-swap

```assembly
        % Compare-and-swap: update if matches expected value
        % Input: W2=expected, W3=new value
        % Output: W1=success (1) or failure (0)
        SOLO
        W1 := SHARED_VAR
        W1 COMP W2
        IF><GO CAS_FAILED          % Doesn't match expected
        % Matches, do swap
        W3 =: SHARED_VAR
        W1 := 1                    % Success
        GO CAS_DONE
CAS_FAILED:
        W1 := 0                    % Failure
CAS_DONE:
        TUTTI
```

**Explanation:** Atomic compare-and-swap primitive for lock-free algorithms.

### Example 5: Critical section protection

```assembly
        % Protect critical section from interruption
        SOLO
        W MOVE SHARED_DATA, I1
        % Compute with data
        W1 ADD 100
        I1 MOVE SHARED_DATA
        TUTTI
```

**Explanation:** Simple critical section: read-process-write atomically.

### Example 6: Queue enqueue operation

```assembly
        % Atomically add item to queue tail
        SOLO
        W1 := QUEUE_TAIL
        W2 := NEW_ITEM
        W2 =: (W1)                 % Store item at tail
        W1 := W1 + ITEM_SIZE
        W1 =: QUEUE_TAIL           % Update tail pointer
        TUTTI
```

**Explanation:** Atomic queue insertion prevents corruption from concurrent access.

### Example 7: Reference counting

```assembly
        % Atomically increment reference count
        SOLO
        W1 := OBJECT.REFCOUNT
        W1 INC
        W1 =: OBJECT.REFCOUNT
        TUTTI
        % Safe to use object
```

**Explanation:** Atomic reference count update for memory management.

---

## Performance Notes

- **Execution:** 1-2 cycles (SOLO overhead minimal)
- **Atomic section:** Overhead depends on instructions within SOLO/TUTTI
- **Timeout risk:** User mode limited to 256 micro-cycles
- **Implementation:** Disables hardware context switch mechanism

**Usage recommendations:**
- Keep SOLO sections as short as possible
- Avoid loops within SOLO/TUTTI (timeout risk in user mode)
- No I/O or trap-causing operations within SOLO
- Always pair SOLO with TUTTI
- Use for true atomic operations only, not general critical sections
- Consider higher-level OS primitives for longer critical sections

**Timing considerations:**
- Simple instruction: ~1 micro-cycle per operand
- SOLO/TUTTI pair overhead: ~5-10 cycles
- Maximum user mode SOLO section: ~256 micro-cycles
- Typical atomic operation (3-5 instructions): ~10-20 cycles total

**Common patterns:**
```
Pattern 1: Atomic update
  SOLO
  read shared variable
  modify
  write shared variable
  TUTTI

Pattern 2: Test-and-set lock
  SOLO
  test lock variable
  if free: set lock
  TUTTI

Pattern 3: Atomic exchange
  SOLO
  read old value
  write new value
  TUTTI
  return old value
```

---

## Reference Manual

**Section:** §16.1
**Title:** Disable process switch

---

## See Also

- [TUTTI](tutti.md) - Enable process switch (end atomic section)
- [ENTT](entt.md) - Enter trap handler
- [WAIT](wait.md) - Wait for event
- [Synchronization](../ND500_SYNCHRONIZATION.md) - Lock and semaphore primitives
