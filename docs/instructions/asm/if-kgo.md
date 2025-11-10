# IF -K GO - Conditional Jump Flag Clear

## Overview

**Mnemonic:** `if-kgo`
**Function:** Conditional jump flag clear
**Class:** BRANCH
**Privilege:** user

**Format:** `IF -K GO <<displacement>>`

---

## Description

Transfers control if K=0. Used after comparison/test operations.

**Operation:** `if K=0 then PC += displacement`

**Operands:** 1
**Variants:** 2

---

## Variants

| Variant | Opcode | Displacement |
|---------|--------|--------------|
| 1/2 | 0x00D2 | Byte |
| 2/2 | 0x00D3 | Halfword |

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
        CLRK
        IF -K GO K_CLEAR
```

### Example 2: Loop control

```assembly
LOOP:
        % loop body
        IF -K GO:B LOOP
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
