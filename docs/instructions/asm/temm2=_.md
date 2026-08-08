# TEMM2=: - Store Trap enable modification mask 2

## Overview

**Mnemonic:** `temm2=`
**Function:** Store Trap enable modification mask 2
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `TEMM2=: <operand>`

---

## Description

Stores the TEMM2 (Trap Enable Modification Mask 2) register to the specified operand. TEMM2 controls which bits of OTE2 (bits 16-31) can be modified by user-level code versus supervisor-only operations.

**Operation:**
```
TEMM2 → <operand>
```

**Key Characteristics:**
- Controls modification permissions for OTE2 bits 16-31
- Security mechanism for trap enable control
- Essential for privilege separation
- Paired with TEMM1 for full 32-bit mask
- Supervisor-level register
- Prevents user code from disabling critical traps

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Trap enable modification mask 2 |

---

## Operands

### Operand 1 (Destination)

Where to store Trap enable modification mask 2 value.

---

## Trap Conditions

- **Addressing traps:** Invalid address
- **Privilege violation:** If supervisor-only

---

## Data Status Bits

Unaffected.

---

## Examples

### Example 1: Store to local variable

```assembly
        TEMM2=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        TEMM2=: I1
```

---

## Performance Notes

- **Cycles:** 3-4

---

## Reference Manual

**Section:** §16.8
**Title:** Store special register

---

## See Also

- [TEMM2:=](temm2_=.md) - Load Trap enable modification mask 2
- [MOVE instruction reference](../../instruction-reference/MOVE.md)
