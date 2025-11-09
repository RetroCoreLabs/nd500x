# AXI - Arithmetic Index Operation

## Overview
**Mnemonic:** `axi`
**Function:** Arithmetic operation with index
**Class:** ARITHMETIC
**Privilege:** user
**Format:** `t AXI <op1>, <op2>`

---

## Description

Performs arithmetic operation combining operand with index register. This instruction provides indexed arithmetic capabilities for array processing and pointer manipulation. Supports floating-point (F), double (D), and register (R) data types.

**Common Use Cases:**
- Array element calculations
- Pointer arithmetic
- Index-based computations
- Loop variable updates

**Operands:** 2
**Variants:** 8 opcodes

---

## Variants

| Variant | Opcode | Data Type | Assembly |
|---------|--------|-----------|----------|
| 1-8 | 0xFCC0-0xFCC7 | F/D/R | AXI |

---

## Operands

**Operand 1** (Source, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches prefix

**Operand 2** (Index, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word

---

## Trap Conditions
- **Addressing traps**: Invalid address or protection violation
- **Floating-point exceptions**: For F/D types

---

## Data Status Bits
- **Z,S,C,V**: Set based on result

---

## Examples

### Example 1: Array index arithmetic
```assembly
F AXI ARRAY_BASE, I1
```

### Example 2: Pointer calculation
```assembly
W AXI PTR, OFFSET
```

### Example 3: Loop variable
```assembly
D AXI VALUE, STEP
```

### Example 4: Index update
```assembly
F AXI DATA(I2), I1
```

### Example 5: Multi-dimensional array
```assembly
W AXI MATRIX, ROW_INDEX
```

---

## Performance Notes
- Execution: 2-3 cycles
- Optimized for indexed operations

---

## Reference Manual
**Section:** §11.x
**Title:** Arithmetic index operation

---

## See Also
- [IXI](ixi.md) - Index operation
- [ADD](add.md) - Addition
