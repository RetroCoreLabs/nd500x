# RETB - Return from Block Subroutine

## Overview

**Mnemonic:** `retb`
**Function:** Return from ENTB and free buddy block
**Class:** CALL
**Privilege:** user

**Format:** `RETB <log size/r/BY>`

---

## Description

Returns from a subroutine entered with ENTB. Frees the buddy-allocated stack frame back to the heap and returns to caller.

RETB:
1. Frees buddy block of size 2^`<log size>` words
2. Restores previous frame pointer
3. Returns to caller address

The `<log size>` must match the value used in the corresponding ENTB.

**Operands:** 1
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFDB4 | RETB |

---

## Examples

### Example 1: Return from block

```assembly
PROC:   ENTB 6
        % Subroutine body
        RETB 6              % Free 64-word block
```

---

## Reference Manual

**Section:** §13.11
**Title:** Return instructions

---

## See Also

- [ENTB](entb.md) - Enter block
- [FREEB](freeb.md) - Free buddy element
- [RET](ret.md) - Simple return
