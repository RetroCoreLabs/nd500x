# INT - Integer Operation

## Overview
**Mnemonic:** `int`
**Function:** Integer type conversion/operation
**Class:** SYSTEM
**Privilege:** supervisor
**Format:** `t INT <operand>`

---

## Description

Performs integer conversion or operation as system-level instruction. This privileged instruction is used for type conversions and integer operations that require supervisor mode access. Supports multiple data type prefixes for flexible type handling.

**Common Use Cases:**
- Type conversion in system code
- Integer format transformations
- Privileged arithmetic operations
- System-level data manipulation

**Operands:** 1
**Variants:** 8 opcodes

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1-8 | 0xFE60-0xFE67 | INT |

---

## Operands

**Operand 1** (Source, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Varies by prefix
- **Role**: Value to convert/process

**Result**: Converted or processed value

---

## Trap Conditions
- **Addressing traps**: Invalid address or protection violation
- **Privilege violation**: If executed in user mode
- **Type conversion errors**: Invalid conversions

---

## Data Status Bits
- **Z,S,C,V**: Set based on result

---

## Examples

### Example 1: Integer conversion
```assembly
F INT FLOAT_VAL
```

### Example 2: Type transformation
```assembly
D INT DOUBLE_VAL
```

### Example 3: System operation
```assembly
W INT SYSTEM_REG
```

### Example 4: Format conversion
```assembly
H INT HALFWORD_VAL
```

### Example 5: Privileged operation
```assembly
BI INT BIT_FIELD
```

---

## Performance Notes
- Execution: 3-5 cycles (privileged)
- Requires supervisor mode
- Type-dependent performance

---

## Reference Manual
**Section:** §16.x
**Title:** Integer operation

---

## See Also
- [INTR](intr.md) - Integer operation variant
- [BYCONR](byconr.md) - Byte conversion
- [WCONR](wconr.md) - Word conversion
