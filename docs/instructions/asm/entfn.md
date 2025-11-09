# ENTFN - Fortran Entry with Argument Count

## Overview

**Mnemonic:** `entfn`
**Function:** Enter Fortran subroutine with argument validation
**Class:** CALL
**Privilege:** user

**Format:** `ENTFN <address of local data area/r/W>,<max no. of arg./r/W>`

---

## Description

Enters a Fortran-style subroutine (static locals) with argument count validation. Combines ENTF's static local semantics with argument checking.

ENTFN:
1. Validates argument count ≤ `<max no. of arg.>`
2. Saves return address
3. Sets frame pointer to fixed data area
4. Does NOT allocate stack space (locals are static)

**Operands:** 2
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFD88 | ENTFN |

---

## Examples

### Example 1: Fortran subroutine with args

```assembly
FSUB:   ENTFN FLOCALS, 4    % Max 4 args, static locals
        % Process
        RETK

FLOCALS:
        LOCAL1: 0
        LOCAL2: 0
```

---

## Reference Manual

**Section:** §13.10
**Title:** Subroutine entry points

---

## See Also

- [ENTF](entf.md) - Fortran entry
- [ENTSN](entsn.md) - Simple entry with args
