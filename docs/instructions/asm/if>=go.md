# IF >= GO - Conditional Jump Greater/Equal (Signed)

## Overview

**Mnemonic:** `if>=go`
**Function:** Conditional jump greater/equal (signed)
**Class:** BRANCH
**Privilege:** user

**Format:** `IF >= GO <<displacement>>`

---

## Description

Transfers control if S=0 (sign flag clear), indicating a non-negative result from signed comparison. Used after COMP operations to branch when first operand is greater than or equal to second operand in signed comparison.

**Operation:**
```
if S=0 then PC += displacement
```

**Key Characteristics:**
- Tests for non-negative (S=0)
- Signed comparison semantics
- Two variants: byte and halfword displacement
- Common after COMP for >= tests
- Sign-extended displacement

**Common Use Cases:**
- Signed integer comparisons (a >= b)
- Non-negative value checks
- Loop continuation
- Boundary validation
- Minimum threshold testing

**Operands:** 1 (signed displacement)
**Variants:** 2 opcodes

---

## Variants

| Variant | Opcode | Displacement |
|---------|--------|--------------|
| 1/2 | 0x00CC | Byte |
| 2/2 | 0x00CD | Halfword |

---

## Operands

### Operand 1 (Displacement)

Signed displacement.

---

## Trap Conditions

- **Branch trap (BT):** If enabled

---

## Data Status Bits

Unaffected.

---

## Examples

### Example 1: Basic usage

```assembly
        W COMP B.A, B.B
        IF >= GO GE
```

### Example 2: Loop control

```assembly
LOOP:
        % loop body
        IF >= GO:B LOOP
```

---

## Performance Notes

- **Cycles:** 2-3 (not taken), 3-4 (taken)

---

## Reference Manual

**Section:** §13.3
**Title:** Conditional Jump

---

## See Also

- [IF=GO](if=go.md) - Jump if equal
- [COMP](comp.md) - Compare
- [GO](go.md) - Unconditional jump
