# ENTF - Enter Fortran Subroutine

## Overview

**Mnemonic:** `entf`
**Function:** Enter Fortran-style subroutine with fixed local data area
**Class:** CALL
**Privilege:** user

**Format:** `ENTF <address of local data area/r/W>`

---

## Description

Enters a Fortran-style subroutine where local variables are allocated in a fixed data area rather than on the stack. This matches Fortran's static local variable semantics.

ENTF:
1. Saves return address
2. Sets up frame pointer to point to the fixed data area
3. Does NOT allocate stack space for locals (locals are static)

This is used for compiled Fortran code where local variables retain their values between calls (static storage class in C terms).

**Operands:** 1
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFD84 | ENTF |

---

## Operands

### Operand 1 (Local Data Area Address)

Address of the static data area for local variables.

**Type:** Word
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation

---

## Examples

### Example 1: Fortran subroutine with static locals

```assembly
FSUB:   ENTF LOCALS_AREA    % Point to static locals
        % Local variables retain values between calls
        W INCR B.COUNTER    % Increment static counter
        RETK                % Simple return

LOCALS_AREA:
        COUNTER: 0          % Static local variable
```

### Example 2: Fortran function

```assembly
FFUNC:  ENTF FLOCALS
        % Computation
        W1 := B.RESULT
        RETK

FLOCALS:
        RESULT: 0
        TEMP1: 0
        TEMP2: 0
```

---

## Reference Manual

**Section:** §13.10
**Title:** Subroutine entry points

---

## See Also

- [ENTFN](entfn.md) - Fortran entry with argument count
- [RETK](retk.md) - Simple return
- [ENTS](ents.md) - Simple entry
