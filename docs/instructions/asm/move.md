# MOVE - Move Data

## Overview

**Mnemonic:** `move`
**Function:** Copy data from source to destination
**Class:** MOVE
**Privilege:** user
**Format:** `t MOVE <source>, <dest>`

---

## Description

The MOVE instruction is the most fundamental data transfer operation in the ND-500 architecture. It copies data from a source operand to a destination operand, leaving the source unchanged. This is the primary method for loading constants, copying variables, transferring data between memory and registers, and moving data between different memory locations.

MOVE supports all common data types (bit, byte, halfword, word, float, double) through different instruction prefixes. The number of bits transferred depends on the data type prefix used.

**Operation:**
```
source → destination
source.value = 0 → Z flag
source.signbit → S flag
```

**Key Characteristics:**
- Source operand is never modified
- Destination cannot be a constant (constants are read-only)
- Sets Z flag if source value is zero
- Sets S flag based on source sign bit
- Most frequently executed instruction in typical programs

**Common Use Cases:**
- Loading constants into registers or memory
- Copying variables
- Parameter passing
- Function return values
- Data structure initialization
- Register-to-memory transfers

**Operands:** 2 (source, destination)
**Variants:** 6 opcodes (BI, BY, H, W, F, D)

---

## Variants

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/6 | 0xFC0B | Bit | BI MOVE |
| 2/6 | 0x0019 | Byte | BY MOVE |
| 3/6 | 0xFC14 | Halfword | H MOVE |
| 4/6 | 0x001A | Word | W MOVE |
| 5/6 | 0x001B | Float | F MOVE |
| 6/6 | 0x002C | Double | D MOVE |

---

## Operands

**Operand 1** (Source, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix (BI/BY/H/W/F/D)
- **Role**: Source value to be copied
- **Access**: Read-only (source is never modified)

**Operand 2** (Destination, Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix (BI/BY/H/W/F/D)
- **Role**: Destination where value is stored
- **Access**: Write-only
- **Restriction**: CONSTANT addressing mode is illegal (cannot write to constant)

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation
- **Write protection**: Attempt to write to read-only memory
- **Illegal addressing mode**: CONSTANT used as destination

---

## Data Status Bits

- **Z (Zero)**: Set if source value = 0, cleared otherwise
- **S (Sign)**: Set to source value's sign bit
- **C (Carry)**: Unaffected
- **V (Overflow)**: Unaffected

---

## Examples

### Example 1: Load constant to variable
```assembly
        % Load constant 100 into word variable
        W MOVE 100, B.COUNT
```

### Example 2: Copy between variables
```assembly
        % Copy variable to another
        W MOVE B.SRC, B.DEST
```

### Example 3: Register to memory
```assembly
        % Store register I1 to memory location
        W MOVE I1, VALUE
```

### Example 4: Memory to register
```assembly
        % Load memory value into register
        W MOVE DATA, I2
```

### Example 5: Array element access
```assembly
        % Copy array element indexed by W2
        W MOVE ARRAY(W2), B.TEMP
```

### Example 6: Record field transfer
```assembly
        % Copy between record fields
        W MOVE R.FIELD1, R.FIELD2
```

### Example 7: Floating point move
```assembly
        % Copy double precision value to local variable
        D MOVE GLOBAL, B.LOCAL
```

### Example 8: Initialize loop counter
```assembly
        % Set up loop counter
        W MOVE 0, I1
LOOP:
        % ... loop body ...
        W1 INC
        W1 COMP 10
        IF<GO LOOP
```

### Example 9: Function parameter passing
```assembly
        % Pass parameter via local variable
        W MOVE PARAM_VALUE, B.ARG1
        CALL FUNCTION
```

### Example 10: Byte operations
```assembly
        % Copy byte value
        BY MOVE FLAGS, B.SAVED_FLAGS
```

---

## Performance Notes

- **Execution**: 1-3 cycles depending on addressing modes
  - Register-to-register: 1 cycle
  - Memory-to-register or register-to-memory: 2 cycles
  - Memory-to-memory: 3 cycles
- **Optimization**: Use registers for frequently accessed values
- **Best practices**:
  - Prefer register operands when possible for fastest execution
  - Use appropriate data type prefix (BY/H/W/F/D) to minimize memory bandwidth
  - For bulk copying, use BMOVE instead of multiple MOVE instructions

**Comparison with other instructions:**
- MOVE vs [:=](assignto.md): MOVE is explicit instruction, := is assembler pseudo-op
- MOVE vs BMOVE: MOVE for single elements, BMOVE for blocks
- MOVE vs SWAP: MOVE copies one-way, SWAP exchanges both values

---

## Reference Manual

**Section:** §10.7
**Title:** Move

---

## See Also

- [SWAP](swap.md) - Exchange two operands
- [BMOVE](bmove.md) - Block move (bulk transfer)
- [:=](assignto.md) - Assignment operator (assembler pseudo-op)
- [STZ](stz.md) - Store zero (optimized constant 0 move)
- [CLR](clr.md) - Clear register to zero
