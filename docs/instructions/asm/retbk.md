# RETBK - Buddy Subroutine Return with K Set

## Overview

**Mnemonic:** `retbk`
**Function:** Return from buddy subroutine (K flag set)
**Class:** CALL
**Privilege:** user

**Format:** `RETBK`

---

## Description

Returns from a buddy subroutine, releasing the local data area to the heap. Sets the K flag bit in the status register to 1. Restores the base register and return address from the current local data area. Used to indicate error or special return condition.

**Operation:**
```
Local data area released to heap
1 → STATUS.K
B.RETA → PC → L
B.PREVB → B
```

**Key Characteristics:**
- Releases buddy-allocated local area to heap
- Sets K flag (K=1)
- Restores previous base register
- Restores return address to link register
- Paired with ENTB instruction
- Used for error/status indication

**Common Use Cases:**
- Error returns from buddy procedures
- Status indication with automatic cleanup
- Conditional return with heap deallocation
- Memory management with error signaling

**Operands:** None
**Variants:** 1 opcode

---

## Examples

### Example 1: Error return with cleanup

```assembly
ALLOC:  ENTB 7
        % Try allocation
        W1 COMP SIZE, MAX_SIZE
        IF>GO TOO_LARGE
        % Success
        RETB                % Normal return (K=0)
TOO_LARGE:
        RETBK               % Error return (K=1)
```

**Explanation:** Return with error indication while freeing buddy block.

### Example 2: Validation function

```assembly
VALIDATE: ENTB 6
        % Validate data in buddy frame
        W1 COMP DATA, THRESHOLD
        IF<GO INVALID
        % Valid
        RETB                % K=0 (valid)
INVALID: RETBK              % K=1 (invalid)
```

**Explanation:** Boolean result via K flag with automatic cleanup.

### Example 3: Caller checks status

```assembly
        % Call buddy procedure
        CALL BUDDY_FUNC
        IF K GO ERROR_HANDLER
        % Success path
        GO CONTINUE
ERROR_HANDLER:
        % Handle error (buddy block already freed)
CONTINUE:
```

**Explanation:** Caller tests K flag after buddy return.

---

## Trap Conditions

None

---

## Data Status Bits

- **K (Flag)**: Set to 1
- **Other flags**: Unaffected

---

## Reference Manual

**Section:** §13.11
**Title:** Subroutine return

---

## See Also

- [RETB](retb.md) - Buddy return with K flag cleared
- [ENTB](entb.md) - Enter buddy subroutine
- [RETK](retk.md) - Regular return with K set
- [FREEB](freeb.md) - Free buddy block
