# INTR - Integer Register Operation

## Overview
**Mnemonic:** `intr`
**Function:** Integer register operation
**Class:** SYSTEM
**Privilege:** supervisor
**Format:** `t INTR <operand>`

---

## Description

Performs integer register operations at system level. Similar to INT but specifically operates on register values with privileged access. Used for system-level register manipulation and integer operations requiring supervisor mode.

**Common Use Cases:**
- Register-based type conversions
- System register operations
- Privileged register arithmetic
- Integer format transformations in registers

**Operands:** 1
**Variants:** 8 opcodes

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1-8 | 0xFE68-0xFE6F | INTR |

---

## Operands

**Operand 1** (Register Source, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Varies by prefix
- **Role**: Register value to process

**Result**: Processed register value

---

## Trap Conditions
- **Addressing traps**: Invalid address
- **Privilege violation**: User mode execution
- **Register errors**: Invalid register access

---

## Data Status Bits
- **Z,S,C,V**: Set based on result

---

## Examples

### Example 1: Register conversion
```assembly
F INTR I1
```

### Example 2: System register op
```assembly
W INTR SYSTEM_REG
```

### Example 3: Type transform
```assembly
D INTR E1
```

### Example 4: Register arithmetic
```assembly
H INTR A1
```

### Example 5: Privileged register
```assembly
BI INTR FLAGS
```

---

## Performance Notes
- Execution: 2-4 cycles (privileged)
- Register-based (faster than memory)
- Requires supervisor mode

---

## Reference Manual
**Section:** §16.x
**Title:** Integer register operation

---

## See Also
- [INT](int.md) - Integer operation
- [BYCONR](byconr.md) - Byte conversion
- [WCONR](wconr.md) - Word conversion
