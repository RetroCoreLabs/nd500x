# OR - Bitwise OR

## Overview

**Mnemonic:** `or`
**Function:** Bitwise OR operation between register and operand
**Class:** LOGICAL
**Privilege:** user
**Format:** `tn OR <operand>`

---

## Description

Performs a bitwise logical OR operation between the contents of a specified register and an operand, storing the result back in the register. Each bit in the result is set to 1 if either or both corresponding bits in the register and operand are 1.

**Operation:**
```
Rn = Rn | operand
Result bit = 1 if either input bit is 1
result = 0 → Z flag
result.signbit → S flag
```

**Key Characteristics:**
- Bitwise OR (logical union)
- Register-based operation (implicit destination in Rn)
- Supports 4 data types (BI, BY, H, W - no float/double)
- Works with 4 index registers (I1-I4)
- Upper bits zero-filled for BI/BY/H types
- Sets Z and S flags based on result
- Essential for bit setting and flag combination
- Common in control register manipulation

**Common Use Cases:**
- Setting specific bits in a register or variable (bit masking)
- Combining multiple flag values
- Building composite bit patterns
- Enable bits in control registers
- Bitwise accumulation operations

**Operands:** 1 (source, register is implicit destination)
**Variants:** 16 opcodes (4 data types × 4 registers)

---

## Variants

| Variant | Opcode | Type | Register | Assembly |
|---------|--------|------|----------|----------|
| 1-4 | 0xFDF8-0xFDFB | BI | I1-I4 | BIn OR |
| 5-8 | 0xFC98-0xFC9B | BY | I1-I4 | BYn OR |
| 9-12 | 0xFC9C-0xFC9F | H | I1-I4 | Hn OR |
| 13-16 | 0x00A0-0x00A3 | W | I1-I4 | Wn OR |

---

## Operands

**Operand 1** (Source, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix (BI/BY/H/W)
- **Role**: Second operand for OR operation
- **Note**: Upper bits zero-filled for BI, BY, H types

**Register Rn** (Destination, Read-Modify-Write):
- **Implicit operand**: Specified by instruction variant (I1-I4)
- **Role**: First operand and result destination
- **Note**: Upper part zero-filled for sub-word types

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation

---

## Data Status Bits

- **Z (Zero)**: Set if result = 0, cleared otherwise
- **S (Sign)**: Set to result's sign bit
- **C (Carry)**: Unaffected
- **V (Overflow)**: Unaffected

---

## Examples

### Example 1: Set specific bits in flags register
```assembly
        % Set bits 4 and 5 in word register W1
        W1 OR 0x0030
```

### Example 2: Combine bit masks
```assembly
        % Combine multiple permission flags
        W2 OR B.READ_MASK
        W2 OR B.WRITE_MASK
```

### Example 3: Enable control bits
```assembly
        % Enable interrupt bits 0, 2, 4
        BY1 OR 0x15        % Binary: 00010101
```

### Example 4: Build composite value
```assembly
        % Build configuration word from components
        W1 CLR
        W1 OR OPTION1
        W1 OR OPTION2
        W1 OR OPTION3
```

### Example 5: Set flag bits in array
```assembly
        % Set specific bit pattern in array element
        W3 OR FLAGS(W2)
```

### Example 6: Byte register OR
```assembly
        % OR byte register with octal constant
        BY1 OR 111B        % Octal 111 = 0x49
```

### Example 7: Accumulate bits from loop
```assembly
        % Accumulate OR of array elements
        W1 CLR
        W2 CLR
LOOP:
        W1 OR DATA_ARRAY(W2)
        W2 INC
        W2 COMP ARRAY_SIZE
        IF<GO LOOP
        % W1 now has OR of all elements
```

---

## Performance Notes

- **Execution**: 1-2 cycles
  - Register operand: 1 cycle
  - Memory operand: 2 cycles
- **Optimization**: Use constant operands when possible for fastest execution
- **Upper bits**: For BI, BY, H types, upper register bits are zero-filled

**Common patterns:**
- Set bits: `Wn OR mask` (sets bits where mask = 1)
- No-op: `Wn OR 0` (leaves register unchanged)
- Set all bits: `Wn OR 0xFFFFFFFF` (sets register to all 1s)

---

## Reference Manual

**Section:** §10.22
**Title:** Or

---

## See Also

- [AND](and.md) - Bitwise AND (clear/mask bits)
- [XOR](xor.md) - Bitwise XOR (toggle bits)
- [INV](inv.md) - Bitwise NOT (invert all bits)
- [TEST](test.md) - Test bits without modifying register
