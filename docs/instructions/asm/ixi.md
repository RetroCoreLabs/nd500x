# IXI - Index Extension

## Overview
**Mnemonic:** `ixi`
**Function:** Index extension operation
**Class:** ARITHMETIC
**Privilege:** user
**Format:** `t IXI <op1>, <op2>`

---

## Description

Extends index calculations for array and pointer operations. Provides enhanced indexing capabilities beyond basic addressing modes. Supports floating-point, double, and register-based index computations for complex data structure access.

**Operation:**
```
result = index_extend(<op1>, <op2>)
```

**Key Characteristics:**
- Index extension for complex addressing
- 12 variants (F, D, R data types)
- Enhanced multi-dimensional array support
- Beyond basic addressing mode capabilities
- 2-3 cycles with hardware acceleration
- Sets Z, S, C, V flags based on result
- Essential for complex pointer arithmetic
- Supports scaled index calculations
- Common in dynamic array access

**Common Use Cases:**
- Multi-dimensional array indexing
- Complex pointer arithmetic
- Scaled index calculations
- Dynamic array access

**Operands:** 2
**Variants:** 12 opcodes

---

## Variants

| Variant | Opcode | Data Type | Assembly |
|---------|--------|-----------|----------|
| 1-12 | 0xFCC8-0xFCD3 | F/D/R | IXI |

---

## Operands

**Operand 1** (Base, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches prefix

**Operand 2** (Index, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word

**Result**: Extended index value

---

## Trap Conditions
- **Addressing traps**: Invalid address
- **Index overflow**: Index out of range
- **Type errors**: Invalid type combination

---

## Data Status Bits
- **Z,S,C,V**: Set based on result

---

## Examples

### Example 1: Array indexing
```assembly
F IXI ARRAY_BASE, I1
```

### Example 2: Pointer extension
```assembly
D IXI DATA_PTR, OFFSET
```

### Example 3: Scaled index
```assembly
W IXI MATRIX, ROW_IDX
```

### Example 4: Complex addressing
```assembly
F IXI STRUCT(I2), I1
```

### Example 5: Dynamic access
```assembly
R IXI TABLE, INDEX
```

---

## Performance Notes
- Execution: 2-3 cycles
- Optimized for indexed operations
- Hardware acceleration available

---

## Reference Manual
**Section:** §11.x
**Title:** Index extension

---

## See Also
- [AXI](axi.md) - Arithmetic index
- [CIND](cind.md) - Conditional index
- [LIND](lind.md) - Load index
