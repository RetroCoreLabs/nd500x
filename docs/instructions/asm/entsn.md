# ENTSN - Simple Entry with Argument Count

## Overview

**Mnemonic:** `entsn`
**Function:** Enter simple subroutine with variable argument validation
**Class:** CALL
**Privilege:** user

**Format:** `ENTSN <stack demand/r/W>,<max no. of arg./r/W>`

---

## Description

Enters a simple subroutine similar to ENTS but with additional validation of argument count. This supports variadic functions and runtime argument checking.

ENTSN:
1. Validates that actual argument count ≤ `<max no. of arg.>`
2. Saves return address and frame pointer
3. Allocates `<stack demand>` words for locals
4. Sets up new frame pointer

If more arguments are passed than `<max no. of arg.>`, behavior is implementation-defined (typically ignored).

**Operands:** 2
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFD78 | ENTSN |

---

## Operands

### Operand 1 (Stack Demand)

Number of words for local variables.

**Type:** Word
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

### Operand 2 (Max Arguments)

Maximum number of arguments accepted.

**Type:** Word
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

---

## Examples

### Example 1: Variadic function

```assembly
PRINTF: ENTSN 10, 8         % Max 8 args
        % Process arguments
        RET
```

---

## Reference Manual

**Section:** §13.10
**Title:** Subroutine entry points

---

## See Also

- [ENTS](ents.md) - Simple entry
- [ENTFN](entfn.md) - Fortran entry with args
