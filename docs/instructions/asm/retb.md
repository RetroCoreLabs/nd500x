# RETB - Buddy Subroutine Return

## Overview

**Mnemonic:** `retb`
**Function:** Return from buddy subroutine (K flag cleared)
**Class:** CALL
**Privilege:** user

**Format:** `RETB`

---

## Description

Returns from a buddy subroutine, releasing the local data area to the heap. Clears the K flag bit in the status register. Restores the base register and return address from the current local data area.

**Operation:**
```
Local data area released to heap
0 → STATUS.K
B.RETA → PC → L
B.PREVB → B
```

**Key Characteristics:**
- Releases buddy-allocated local area to heap
- Clears K flag (K=0)
- Restores previous base register
- Restores return address to link register
- Paired with ENTB instruction
- Memory is freed automatically

**Common Use Cases:**
- Dynamic memory allocation procedures
- Procedures with variable-sized local data
- Buddy system memory management
- Heap-based stack frames

**Operands:** None
**Variants:** 1 opcode

---

## Examples

### Example 1: Simple buddy return

```assembly
PROC:   ENTB 6
        % Allocate 2^6 = 64-word buddy block
        % Procedure body
        RETB                % Free block and return (K=0)
```

**Explanation:** Return and free buddy-allocated local area.

### Example 2: Multiple return points

```assembly
FUNC:   ENTB 7
        % Try operation
        W1 COMP STATUS, 0
        IF<GO ERROR
        % Success path
        RETB                % Normal return (K=0)
ERROR:  RETBK               % Error return (K=1)
```

**Explanation:** Different return paths with different K flag states.

### Example 3: Heap management

```assembly
ALLOCATOR: ENTB 8
        % Allocate 256-word buddy block
        % Initialize data structures
        W1 := BLOCK_ADDR
        RETB                % Free and return
```

**Explanation:** Procedure using buddy allocation for temporary work area.

---

## Trap Conditions

None

---

## Data Status Bits

- **K (Flag)**: Cleared to 0
- **Other flags**: Unaffected

---

## Reference Manual

**Section:** §13.11
**Title:** Subroutine return

---

## See Also

- [RETBK](retbk.md) - Buddy return with K flag set
- [ENTB](entb.md) - Enter buddy subroutine
- [RET](ret.md) - Simple return
- [FREEB](freeb.md) - Free buddy block
