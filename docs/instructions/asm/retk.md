# RETK - Simple Return (Keep Frame)

## Overview

**Mnemonic:** `retk`
**Function:** Return without deallocating frame
**Class:** CALL
**Privilege:** user

**Format:** `RETK`

---

## Description

Returns to caller without deallocating the stack frame. Used with ENTF/ENTFN where locals are static (not on stack).

RETK simply:
1. Pops return address
2. Jumps to caller
3. Does NOT modify frame pointer or deallocate stack

**Operands:** 0
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFDB2 | RETK |

---

## Examples

### Example 1: Return from Fortran subroutine

```assembly
FSUB:   ENTF LOCALS
        % Body
        RETK                % Return, keep static frame
```

---

## Reference Manual

**Section:** §13.11
**Title:** Return instructions

---

## See Also

- [ENTF](entf.md) - Fortran entry
- [ENTFN](entfn.md) - Fortran entry with args
- [RET](ret.md) - Simple return
