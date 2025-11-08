# IF <<= GO - Conditional Jump Less/Equal Magnitude

## Overview

**Mnemonic:** `if<<=go`
**Function:** Conditional jump less/equal magnitude
**Class:** BRANCH
**Privilege:** user

**Format:** `IF <<= GO <<displacement>>`

---

## Description

Transfers control if C=0 or Z=1. Used after comparison/test operations.

**Operation:** `if C=0 or Z=1 then PC += displacement`

**Operands:** 1
**Variants:** 2

---

## Variants

| Variant | Opcode | Displacement |
|---------|--------|--------------|
| 1/2 | 0x00DA | Byte |
| 2/2 | 0x00DB | Halfword |

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
        IF <<= GO LE_MAG
```

### Example 2: Loop control

```assembly
LOOP:
        % loop body
        IF <<= GO:B LOOP
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
