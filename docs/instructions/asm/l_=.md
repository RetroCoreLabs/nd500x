# L:= - Load Link register

## Overview

**Mnemonic:** `l=`
**Function:** Load Link register
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `L:= <operand>`

---

## Description

Loads the L (Link) register from the specified operand. L contains the return address and stack frame link, used by subroutine call and return mechanisms.

**Operation:**
```
<operand> → L
```

**Key Characteristics:**
- Loads return address and frame link
- Used in subroutine return mechanisms
- Essential for context restoration
- Contains caller's frame pointer
- Enables non-standard call patterns
- Critical for stack unwinding and debugging

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Load Link register |

---

## Operands

### Operand 1 (Source)

Value to load into Link register.

---

## Trap Conditions

- **Addressing traps:** Invalid address
- **Privilege violation:** If supervisor-only

---

## Data Status Bits

Unaffected.

---

## Examples

### Example 1: Load from local variable

```assembly
        L:= B.SAVED_VALUE
```

### Example 2: Load from register

```assembly
        L:= I1
```

---

## Performance Notes

- **Cycles:** 3-4

---

## Reference Manual

**Section:** §16.7
**Title:** Load special register

---

## See Also

- [L=:](l=_.md) - Store Link register
- [MOVE instruction reference](../../instruction-reference/MOVE.md)
