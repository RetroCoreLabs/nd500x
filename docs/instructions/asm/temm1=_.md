# TEMM1=: - Store Trap enable modification mask 1

## Overview

**Mnemonic:** `temm1=`
**Function:** Store Trap enable modification mask 1
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `TEMM1=: <operand>`

---

## Description

Stores the TEMM1 (Trap Enable Modification Mask 1) register to the specified operand. TEMM1 controls which bits of OTE1 (bits 0-15) can be modified by user-level code versus supervisor-only operations.

**Operation:**
```
TEMM1 → <operand>
```

**Key Characteristics:**
- Controls modification permissions for OTE1 bits 0-15
- Security mechanism for trap enable control
- Essential for privilege separation
- Paired with TEMM2 for full 32-bit mask
- Supervisor-level register
- Prevents user code from disabling critical traps

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Trap enable modification mask 1 |

---

## Operands

### Operand 1 (Destination)

Where to store Trap enable modification mask 1 value.

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
        TEMM1=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        TEMM1=: I1
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

- [TEMM1:=](temm1_=.md) - Load Trap enable modification mask 1
- [Context Switching](../ContextSwitching.md)
