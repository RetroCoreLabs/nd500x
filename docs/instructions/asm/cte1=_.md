# CTE1=: - Store Child trap enable 1

## Overview

**Mnemonic:** `cte1=`
**Function:** Store Child trap enable 1
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `CTE1=: <operand>`

---

## Description

Stores the CTE1 (Child Trap Enable 1) register to the specified operand. The CTE1 register controls which traps are enabled for child processes/domains, providing hierarchical trap control.

**Operation:**
```
CTE1 → <operand>
```

**Key Characteristics:**
- Stores child process trap enable mask
- Hierarchical trap control
- Part of trap management system
- Supervisor-level register
- Process isolation mechanism

**Common Use Cases:**
- Process context switching
- Trap configuration management
- Parent/child isolation setup
- Security policy enforcement
- OS trap administration

**Operands:** 1 (destination)
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Child trap enable 1 |

---

## Operands

### Operand 1 (Destination)

Where to store Child trap enable 1 value.

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
        CTE1=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        CTE1=: I1
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

- [CTE1:=](cte1_=.md) - Load Child trap enable 1
- [MOVE instruction reference](../../instruction-reference/MOVE.md)
