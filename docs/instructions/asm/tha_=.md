# THA:= - Load Trap handler address

## Overview

**Mnemonic:** `tha=`
**Function:** Load Trap handler address
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `THA:= <operand>`

---

## Description

Loads the Trap handler address from the specified operand. This is a system register used for trap vector address.

**Operation:** `THA = <operand>`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Load Trap handler address |

---

## Operands

### Operand 1 (Source)

Value to load into Trap handler address.

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
        THA:= B.SAVED_VALUE
```

### Example 2: Load from register

```assembly
        THA:= I1
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

- [THA=:](tha=_.md) - Store Trap handler address
- [Context Switching](../ContextSwitching.md)
