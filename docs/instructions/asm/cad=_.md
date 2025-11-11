# CAD=: - Store Current alternative domain

## Overview

**Mnemonic:** `cad=`
**Function:** Store Current alternative domain
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `CAD=: <operand>`

---

## Description

Stores the CAD (Current Alternative Domain) register to the specified operand. The CAD register contains the domain number for alternate memory mappings and protection contexts.

**Operation:**
```
CAD → <operand>
```

**Key Characteristics:**
- Stores alternative domain identifier
- Part of domain-based protection system
- Used in context switching
- Supervisor-level register
- Multi-domain memory management

**Common Use Cases:**
- Process context switching
- Domain state preservation
- Memory protection management
- Debugging domain issues
- OS state capture

**Operands:** 1 (destination)
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Current alternative domain |

---

## Operands

### Operand 1 (Destination)

Where to store Current alternative domain value.

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
        CAD=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        CAD=: I1
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

- [CAD:=](cad_=.md) - Load Current alternative domain
- [Context Switching](../ContextSwitching.md)
