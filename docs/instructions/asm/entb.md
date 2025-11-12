# ENTB - Enter Block Subroutine

## Overview

**Mnemonic:** `entb`
**Function:** Enter block-structured subroutine (buddy allocator entry)
**Class:** CALL
**Privilege:** user

**Format:** `ENTB <log size/r/BY>`

---

## Description

Enters a block-structured subroutine by allocating a stack frame from the buddy system heap. The frame size is 2^`<log size>` words (e.g., log size 6 allocates 64 words).

**Operation:**
```
Allocate 2^<log size> words from buddy heap
Save return address and frame pointer
Set up new frame pointer to allocated block
```

**Key Characteristics:**
- Heap-based stack frames (not contiguous stack)
- Power-of-2 block sizes via buddy allocator
- Dynamic allocation at call time
- Paired with RETB or RETBK (automatic deallocation)
- Supports deep/unpredictable recursion
- Isolated from stack overflow

**Common Use Cases:**
- Recursive algorithms with unpredictable depth
- Subroutines requiring large local variable space
- Dynamic memory management routines
- Procedures needing isolated stack frames
- Algorithms with variable memory needs

**Operands:** 1 (log2 of frame size)
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0x00BD | ENTB |

---

## Operands

### Operand 1 (Log Size)

Base-2 logarithm of the stack frame size in words.

**Type:** Byte
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **Stack overflow (STO):** No blocks available on heap

---

## Examples

### Example 1: Enter with fixed frame size

```assembly
PROC:   ENTB 6              % Allocate 64-word frame
        % Subroutine body
        RETB 6              % Return and free frame
```

### Example 2: Recursive subroutine

```assembly
RECURSE:
        ENTB 5              % Allocate 32-word frame
        % Check termination condition
        W COMP B.DEPTH, 0
        IF=GO BASE_CASE
        % Recursive call
        W DECR B.DEPTH
        CALL RECURSE
BASE_CASE:
        RETB 5
```

---

## Reference Manual

**Section:** §13.10
**Title:** Subroutine entry points

---

## See Also

- [RETB](retb.md) - Return from block subroutine
- [GETB](getb.md) - Get buddy element
- [FREEB](freeb.md) - Free buddy element
- [ENTD](entd.md) - Enter with display
- [ENTS](ents.md) - Simple entry
