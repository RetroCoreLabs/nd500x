# RETBK - Return Block (Keep)

## Overview

**Mnemonic:** `retbk`
**Function:** Return from buddy block without freeing
**Class:** CALL
**Privilege:** user

**Format:** `RETBK <log size/r/BY>`

---

## Description

Returns from ENTB but does NOT free the buddy block. The block remains allocated. Used when the block needs to persist after return.

RETBK:
1. Validates log size parameter
2. Restores frame pointer (without freeing block)
3. Returns to caller

The caller or another routine must later explicitly free the block using FREEB.

**Operands:** 1
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFDB5 | RETBK |

---

## Examples

### Example 1: Return keeping block allocated

```assembly
ALLOC:  ENTB 6
        % Initialize data in block
        W MOVE I1, B.DATA
        RETBK 6             % Return but keep block
```

---

## Reference Manual

**Section:** §13.11
**Title:** Return instructions

---

## See Also

- [ENTB](entb.md) - Enter block
- [RETB](retb.md) - Return block (freeing)
- [FREEB](freeb.md) - Free buddy element
