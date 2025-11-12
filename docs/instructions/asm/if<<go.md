# IF << GO - Conditional Jump Less Magnitude

## Overview

**Mnemonic:** `if<<go`
**Function:** Conditional jump less magnitude
**Class:** BRANCH
**Privilege:** user

**Format:** `IF << GO <<displacement>>`

---

## Description

Transfers control if C=0 (less magnitude), typically following a magnitude comparison. Tests carry flag clear, indicating the first operand has lesser absolute value than the second.

**Operation:**
```
if C=0 then PC += displacement
```

**Key Characteristics:**
- Tests magnitude comparison (unsigned/absolute value)
- Requires C=0 (carry clear)
- Opposite of IF>>GO (which tests C=1)
- Follows COMP or TEST instructions
- Two displacement ranges (byte: ±127, halfword: ±32767)
- Common in unsigned arithmetic and bounds checking

**Operands:** 1
**Variants:** 2

---

## Variants

| Variant | Opcode | Displacement |
|---------|--------|--------------|
| 1/2 | 0x00D8 | Byte |
| 2/2 | 0x00D9 | Halfword |

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
        IF << GO LESS_MAG
```

### Example 2: Loop control

```assembly
LOOP:
        % loop body
        IF << GO:B LOOP
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
