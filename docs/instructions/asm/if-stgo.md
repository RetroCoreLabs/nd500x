# IF -ST GO - Conditional Jump Status Bit Clear

## Overview

**Mnemonic:** `if-stgo`
**Function:** Conditional jump status bit clear
**Class:** BRANCH
**Privilege:** user

**Format:** `IF -ST GO <<displacement>>`

---

## Description

Transfers control if the specified status bit is clear (bit=0). Used to test individual bits in status registers or flags after bit manipulation operations.

**Operation:**
```
if bit=0 then PC += displacement
```

**Key Characteristics:**
- Tests individual status bit (not flag register)
- Requires bit position as additional operand
- Useful for testing specific hardware status conditions
- Two displacement ranges (byte: ±127, halfword: ±32767)
- Pipeline flush on taken branch
- Common in I/O polling and hardware status checking

**Operands:** 1
**Variants:** 2

---

## Variants

| Variant | Opcode | Displacement |
|---------|--------|--------------|
| 1/2 | 0xFD65 | Byte |
| 2/2 | 0xFC84 | Halfword |

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
        IF -ST GO 5, HANDLER
```

### Example 2: Loop control

```assembly
LOOP:
        % loop body
        IF -ST GO:B LOOP
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
