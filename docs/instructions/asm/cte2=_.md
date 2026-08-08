# CTE2=: - Store Child trap enable 2

## Overview

**Mnemonic:** `cte2=`
**Function:** Store Child trap enable 2
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `CTE2=: <operand>`

---

## Description

Stores the CTE2 (Child Trap Enable 2) register to the specified operand. The CTE2 register provides additional trap enable bits for child processes, extending the trap control mask beyond CTE1.

**Operation:**
```
CTE2 → <operand>
```

**Key Characteristics:**
- Stores extended child trap enable mask
- Complements CTE1 register
- Hierarchical trap control
- Supervisor-level register
- Extended trap coverage

**Common Use Cases:**
- Process context switching
- Extended trap configuration
- Complete trap state preservation
- Security policy management
- OS trap system administration

**Operands:** 1 (destination)
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Child trap enable 2 |

---

## Operands

### Operand 1 (Destination)

Where to store Child trap enable 2 value.

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
        CTE2=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        CTE2=: I1
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

- [CTE2:=](cte2_=.md) - Load Child trap enable 2
- [MOVE instruction reference](../../instruction-reference/MOVE.md)
