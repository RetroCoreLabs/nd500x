# ST1=: - Store First status register

## Overview

**Mnemonic:** `st1=`
**Function:** Store First status register
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `ST1=: <operand>`

---

## Description

Stores the ST1 (First Status Register) to the specified operand. ST1 contains various CPU status and control bits including interrupt enables, privilege level, and system modes.

**Operation:**
```
ST1 → <operand>
```

**Key Characteristics:**
- Stores CPU status and control bits
- Contains interrupt enable flags
- Includes privilege level indicators
- Essential for context switching
- Supervisor-level register
- Used in exception handlers and OS context management

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store First status register |

---

## Operands

### Operand 1 (Destination)

Where to store First status register value.

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
        ST1=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        ST1=: I1
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

- [ST1:=](st1_=.md) - Load First status register
- [Context Switching](../ContextSwitching.md)
