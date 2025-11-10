# TSET - Test and Set (Atomic)

## Overview

**Mnemonic:** `tset`
**Function:** Atomic test and set operation
**Class:** CONTROL
**Privilege:** user
**Format:** `BY TSET <operand>`

---

## Description

Performs an atomic test-and-set operation used for implementing multiprocessor synchronization primitives. The instruction reads a byte operand, sets status flags based on its current value, then unconditionally sets all bits in the operand to 1 (0xFF). These operations are performed atomically - no other processor or DMA channel can access the memory location between the read and write.

The atomicity is guaranteed by the memory system hardware, making TSET suitable for implementing:
- Mutual exclusion locks (mutexes)
- Semaphores
- Spinlocks
- Critical section protection
- Resource arbitration in multiprocessor systems

**Operation:**
```
LOCK memory bus
temp = operand          % Read current value
operand.value = 0 → Z   % Set flags from current value
operand.signbit → S
operand = 0xFF          % Set all bits to 1
UNLOCK memory bus
```

**Common Use Cases:**
- Implementing mutex locks
- Process/thread synchronization
- Hardware resource arbitration
- Critical section protection
- Semaphore operations
- Lock-free data structures

**Operands:** 1 (byte operand, read-modify-write)
**Variants:** 1 opcode (byte only)

---

## Variants

| Variant | Opcode | Data Type | Assembly |
|---------|--------|-----------|----------|
| 1/1 | 0xFD40 | BY | BY TSET |

**Note:** TSET operates only on byte (BY) operands. No word, halfword, or other data type variants exist.

---

## Operands

**Operand 1** (Semaphore Byte, Read-Modify-Write):
- **Addressing modes**: LOCAL, RECORD, PRE_INDEXED, ABSOLUTE
- **Data type**: Byte (BY) only
- **Role**: Lock variable/semaphore
- **Access**: Atomic read-modify-write
- **Restrictions**:
  - REGISTER mode illegal (causes IOS trap)
  - CONSTANT mode illegal (causes IOS trap)
  - Must be in main memory (not register or cache-only)

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation
- **Illegal operand specifier (IOS)**: Register or constant operand used

---

## Data Status Bits

- **Z (Zero)**: Set if operand was 0 **before** the set operation, cleared otherwise
- **S (Sign)**: Set if operand was negative **before** the set operation (bit 7 = 1)
- **C (Carry)**: Unaffected
- **V (Overflow)**: Unaffected

**Important:** Flags reflect the **original** value before modification, allowing the caller to determine if the lock was free or already held.

---

## Examples

### Example 1: Acquire mutex lock
```assembly
        % Try to acquire lock
ACQUIRE:
        BY TSET MUTEX_LOCK
        IF><GO LOCKED       % Z=0 means already locked
        % Lock acquired (was 0, now 0xFF)
        GO CRITICAL_SECTION
LOCKED:
        % Lock was already held, spin or wait
        GO ACQUIRE
```

### Example 2: Spinlock implementation
```assembly
        % Spinlock with busy-wait
SPIN:
        BY TSET B.SPINLOCK
        IF=GO ACQUIRED      % Z=1 means we got it
        % Busy wait
        GO SPIN
ACQUIRED:
        % Critical section
        % ...
        % Release lock
        BY MOVE 0, B.SPINLOCK
```

### Example 3: Semaphore decrement (P operation)
```assembly
        % Atomic semaphore wait
WAIT:
        BY TSET SEMAPHORE
        IF=GO GOT_RESOURCE  % Was 0, now we have it
        % Resource in use, wait and retry
        CALL YIELD          % Yield to other processes
        GO WAIT
GOT_RESOURCE:
        % Resource acquired
```

### Example 4: Lock with timeout
```assembly
        % Try to acquire with timeout
        W1 := TIMEOUT_COUNT
TRYLOCK:
        BY TSET LOCK_VAR
        IF=GO SUCCESS       % Got the lock
        W1 DEC
        IF><GO TRYLOCK
        % Timeout - lock not acquired
        GO LOCK_FAILED
SUCCESS:
        % Lock acquired
```

### Example 5: Critical section protection
```assembly
        % Protect critical section
ENTER_CS:
        BY TSET CS_LOCK
        IF><GO ENTER_CS     % Spin until free
        % In critical section
        % ... critical code ...
        % Release lock
        BY MOVE 0, CS_LOCK
```

### Example 6: Producer-consumer flag
```assembly
        % Producer sets flag atomically
PRODUCE:
        % ... produce data ...
        BY TSET READY_FLAG  % Set flag
        % Consumer will see it
```

### Example 7: Multi-resource lock
```assembly
        % Lock multiple resources in order
        BY TSET LOCK_A
        IF><GO LOCK_A_BUSY
        BY TSET LOCK_B
        IF><GO LOCK_B_BUSY
        % Both locks acquired
        GO WORK
LOCK_B_BUSY:
        BY MOVE 0, LOCK_A   % Release A
        GO START
LOCK_A_BUSY:
        GO START
```

### Example 8: Array element lock
```assembly
        % Lock specific array element
        BY TSET LOCKS(W1)   % Indexed lock
        IF=GO ELEMENT_FREE
        % Element locked by another process
```

### Example 9: Resource reservation
```assembly
        % Reserve hardware resource
RESERVE:
        BY TSET DEVICE_LOCK
        IF=GO RESERVED
        % Device busy
        CALL WAIT_DEVICE
        GO RESERVE
RESERVED:
        % Use device
        % ... device operations ...
        % Release
        BY MOVE 0, DEVICE_LOCK
```

### Example 10: Atomic flag test
```assembly
        % Check and set flag atomically
        BY TSET STATUS_FLAG
        IF=GO WAS_CLEAR
        % Flag was already set
        % Handle already-set case
WAS_CLEAR:
        % Flag was clear, now set
```

---

## Performance Notes

- **Execution**: 5-8 cycles
  - Memory lock overhead: ~2 cycles
  - Actual operation: ~3-4 cycles
  - Cache bypass: ~1-2 cycles
- **Memory system**: Requires MPM-IV or later for true atomicity
- **Cache behavior**: Always reads from main memory, updates cache after
- **Bus locking**: Prevents other processors/DMA from accessing during operation

**Memory System Compatibility:**
- **MPM-IV and later**: True atomic operation with bus locking
- **MPM-III**: Algorithmically correct but **not truly atomic** - other accesses may interfere
- **Older systems**: Undefined behavior

**Spinlock vs Wait:**
```assembly
% Spinlock (busy-wait) - wastes CPU:
SPIN:
    BY TSET LOCK
    IF><GO SPIN
% Better for short waits

% Wait with yield - cooperative:
WAIT:
    BY TSET LOCK
    IF=GO GOT_IT
    CALL YIELD          % Give up CPU
    GO WAIT
% Better for longer waits
```

**Lock release pattern:**
```assembly
% WRONG - Don't use TSET to release:
BY TSET LOCK            % Sets to 0xFF, doesn't release!

% CORRECT - Use MOVE to release:
BY MOVE 0, LOCK         % Clear lock
```

**Flag interpretation:**
```assembly
% After TSET:
IF=GO ...   % Z=1: Lock was FREE (value was 0)
IF><GO ...  % Z=0: Lock was HELD (value was non-zero)

% Common pattern:
BY TSET LOCK
IF=GO ACQUIRED      % We got it!
% Else already locked
```

**Cache considerations:**
- TSET always accesses main memory (bypasses cache for read)
- Cache is updated after the operation
- Ensures visibility across processors
- Necessary for multiprocessor coherency

**Addressing mode restrictions:**
```assembly
% ILLEGAL - will trap:
BY TSET I1              % IOS - register not allowed
BY TSET 0xFF            % IOS - constant not allowed

% LEGAL:
BY TSET B.LOCK          % OK - local variable
BY TSET R.FIELD         % OK - record field
BY TSET GLOBAL_LOCK     % OK - absolute address
BY TSET LOCKS(W1)       % OK - indexed array
```

---

## Reference Manual

**Section:** §16.3
**Title:** Test and set

---

## See Also

- [INIT](init.md) - Initialize/clear lock
- [TEST](test.md) - Test value without modification
- [COMP](comp.md) - Compare values
- [MOVE](move.md) - For releasing locks (set to 0)
