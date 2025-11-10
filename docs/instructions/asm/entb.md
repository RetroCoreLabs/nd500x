# ENTB - Enter Block Subroutine

## Overview

**Mnemonic:** `entb`
**Function:** Enter block-structured subroutine (buddy allocator entry)
**Class:** CALL
**Privilege:** user

**Format:** `ENTB <log size/r/BY>`

---

## Description

Enters a block-structured subroutine by allocating a stack frame from the buddy system heap. The frame size is 2^`<log size>` words.

ENTB is used for subroutines that need dynamically-sized stack frames allocated from the heap rather than the contiguous stack. This is typically used in:
- Recursive algorithms with unpredictable depth
- Subroutines requiring large local variable space
- Dynamic memory management routines

The instruction allocates a block using the buddy system (similar to GETB), saves the return address, and sets up the new stack frame pointer.

**Operands:** 1
**Variants:** 1 opcode(s)

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
