# ENTFN - Fortran Entry with Argument Count

## Overview

**Mnemonic:** `entfn`
**Function:** Enter Fortran subroutine with argument validation
**Class:** CALL
**Privilege:** user

**Format:** `ENTFN <address of local data area>, <max no. of arguments>`

---

## Description

Enters a Fortran-style subroutine with static local variables and argument count validation. Validates that the number of actual arguments does not exceed the maximum specified. Uses a fixed data area for locals rather than stack allocation, following Fortran's static local variable semantics.

**Key Characteristics:**
- Validates argument count
- Static (non-stack) local variables
- Fixed data area for locals
- No stack frame allocation
- Paired with RETK instruction
- Fortran calling convention

**Common Use Cases:**
- Fortran subroutine entry
- Static local variable procedures
- Argument validation at entry
- Legacy code integration
- Pre-allocated data areas

**Operands:** 2 (data area address, max arguments)
**Variants:** 1 opcode

---

## Examples

### Example 1: Fortran subroutine with validation

```assembly
FSUB:   ENTFN FLOCALS, 4    % Max 4 args, static locals
        % Access ARG1 via calling convention
        % Process using static locals
        RETK                % Return (keep static frame)

FLOCALS:
        VAR1:   0
        VAR2:   0
        TEMP:   0
```

**Explanation:** Fortran subroutine with argument limit and static locals.

### Example 2: Mathematical function

```assembly
FMATH:  ENTFN MATH_DATA, 2  % 2 arguments maximum
        % arg1 and arg2 accessed via convention
        % Compute result using static workspace
        % Result in standard location
        RETK

MATH_DATA:
        WORK1:  0
        WORK2:  0
```

**Explanation:** Math function with fixed workspace.

### Example 3: Argument count checking

```assembly
FLEXFN: ENTFN FN_LOCALS, 6  % Up to 6 arguments
        % Trap if caller passed > 6 args
        % Process variable number of args
        RETK

FN_LOCALS:
        BUFFER: 0, 0, 0, 0
```

**Explanation:** Flexible argument handling with upper bound.

---

## Trap Conditions

- **Addressing traps**: Invalid data area address
- **Argument count trap**: Too many arguments provided by caller

---

## Data Status Bits

- **Unaffected**: Preserves caller's flags

---

## Reference Manual

**Section:** §13.10
**Title:** Subroutine entry points

---

## See Also

- [ENTF](entf.md) - Fortran entry without argument check
- [ENTSN](entsn.md) - Simple entry with argument count
- [RETK](retk.md) - Return keeping frame
- [ENT](ent.md) - Regular subroutine entry
