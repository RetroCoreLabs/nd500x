# THA=: - Store Trap handler address

## Overview

**Mnemonic:** `tha=`
**Function:** Store Trap handler address
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `THA=: <operand>`

---

## Description

Stores the THA (Trap Handler Address) register to the specified operand. The THA register contains the base address of the trap handler table used for exception and interrupt processing.

**Operation:**
```
THA → <operand>
```

**Key Characteristics:**
- Stores trap handler table base address
- Used in exception processing
- Part of trap mechanism
- System-level register
- Critical for interrupt handling

**Common Use Cases:**
- Context switch save/restore
- Trap table inspection
- System debugging
- OS initialization verification
- Exception handler analysis

**Operands:** 1 (destination)
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Trap handler address |

---

## Operands

### Operand 1 (Destination)

Where to store Trap handler address value.

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
        THA=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        THA=: I1
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

- [THA:=](tha_=.md) - Load Trap handler address
- [Context Switching](../ContextSwitching.md)
