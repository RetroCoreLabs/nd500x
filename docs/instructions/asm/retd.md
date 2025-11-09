# RETD - Return with Display

## Overview

**Mnemonic:** `retd`
**Function:** Return from ENTD with display cleanup
**Class:** CALL
**Privilege:** user

**Format:** `RETD`

---

## Description

Returns from a subroutine entered with ENTD. Restores display registers and deallocates stack frame.

RETD:
1. Restores display register to previous nesting level
2. Deallocates stack frame
3. Restores previous frame pointer
4. Returns to caller

**Operands:** 0
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFDB0 | RETD |

---

## Examples

### Example 1: Return from display procedure

```assembly
PROC:   ENTD 20
        % Procedure body
        RETD                % Cleanup and return
```

---

## Reference Manual

**Section:** §13.11
**Title:** Return instructions

---

## See Also

- [ENTD](entd.md) - Enter with display
- [RET](ret.md) - Simple return
