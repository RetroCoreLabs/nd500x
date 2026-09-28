# IF < GO - Conditional Jump Less (Signed)

## Overview

**Mnemonic:** `if<go`
**Function:** Conditional jump less (signed)
**Class:** BRANCH
**Privilege:** user

**Format:** `IF < GO <<displacement>>`

---

## Description

Transfers control if S=1 (sign flag set), indicating a negative result from signed comparison. Used after COMP or arithmetic operations to branch when first operand is less than second operand in signed comparison.

**Operation:**
```
if S=1 then PC += displacement
```

**Key Characteristics:**
- Tests Sign flag (S=1) for negative result
- Signed comparison semantics
- Two variants: byte and halfword displacement
- Common after COMP for less-than tests
- Sign-extended displacement

**Common Use Cases:**
- Signed integer comparisons (a < b)
- Loop bounds checking
- Range validation
- Sorting algorithms
- Conditional execution

**Operands:** 1 (signed displacement)
**Variants:** 2 opcodes

---

## Variants

| Variant | Opcode | Displacement |
|---------|--------|--------------|
| 1/2 | 0x00CA | Byte |
| 2/2 | 0x00CB | Halfword |

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
        IF < GO LESS
```

### Example 2: Loop control

```assembly
LOOP:
        % loop body
        IF < GO:B LOOP
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
