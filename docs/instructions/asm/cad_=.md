# CAD:= - Load Current alternative domain

## Overview

**Mnemonic:** `cad=`
**Function:** Load Current alternative domain
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `CAD:= <operand>`

---

## Description

Loads the CAD (Current Alternative Domain) register from the specified operand. The CAD register determines the alternate domain for memory access and protection.

**Operation:**
```
<operand> → CAD
```

**Key Characteristics:**
- Loads alternative domain identifier
- Controls alternate memory mapping
- Used in domain switching
- Supervisor-level register
- Part of protection mechanism

**Common Use Cases:**
- Domain context switching
- Process state restoration
- Memory protection setup
- OS initialization
- Multi-domain access control

**Operands:** 1 (source)
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Load Current alternative domain |

---

## Operands

### Operand 1 (Source)

Value to load into Current alternative domain.

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
        CAD:= B.SAVED_VALUE
```

### Example 2: Load from register

```assembly
        CAD:= I1
```

---

## Performance Notes

- **Cycles:** 3-4

---

## Reference Manual

**Section:** §16.33
**Title:** Load special register

---

## See Also

- [CAD=:](cad=_.md) - Store Current alternative domain
- [MOVE instruction reference](../../instruction-reference/MOVE.md)
