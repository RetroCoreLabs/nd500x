# THA:= - Load Trap handler address

## Overview

**Mnemonic:** `tha=`
**Function:** Load Trap handler address
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `THA:= <operand>`

---

## Description

Loads the THA (Trap Handler Address) register from the specified operand. THA contains the address of the trap vector table, used for exception and interrupt handling.

**Operation:**
```
<operand> → THA
```

**Key Characteristics:**
- Loads trap vector table address
- Points to exception handler entry point
- Essential for interrupt handling restoration
- Supervisor-level register
- Used in OS initialization and context switching
- Critical for exception dispatch mechanism

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
