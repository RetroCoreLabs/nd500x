# + - Add (Operator Syntax)

## Overview

**Mnemonic:** `+`
**Function:** Add (operator syntax for register-based addition)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `tn + <operand>`

---

## Description

Adds the `<operand>` to the specified register and stores the result in that register. This is the operator syntax equivalent of ADD2, providing a more natural mathematical notation for register-based arithmetic.

**Operation:**
```
Rn = Rn + <operand>
```

**Key Characteristics:**
- Register-based addition (implicit destination in Rn)
- Operator syntax (`+`) for natural mathematical notation
- Supports 5 data types (BY, H, W, F, D)
- Works with 4 index registers (I1-I4)
- Sets carry (C) and overflow (V) flags for integers
- Floating-point overflow/underflow traps for F/D types
- More concise than ADD2 instruction for register operations
- Common in loop counters and accumulator patterns

For integer types (BY, H, W), carry and overflow flags are set appropriately. For floating point types (F, D), overflow and underflow traps may occur.

**Operands:** 1
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20 (5 data types × 4 registers)

| Data Type | Registers | Opcodes | Octal |
|-----------|-----------|---------|-------|
| BY | 1-4 | 0xFC34-0xFC37 | 176064B-176067B |
| H | 1-4 | 0xFC38-0xFC3B | 176070B-176073B |
| W | 1-4 | 0x0054-0x0057 | 124B-127B |
| F | 1-4 | 0x0058-0x005B | 130B-133B |
| D | 1-4 | 0x005C-0x005F | 134B-137B |

---

## Operands

### Operand 1 (Addend)

The operand to add to the specified register.

**Type:** Byte, Halfword, Word, Float, or Double Float (matching register type)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer or float register
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

---

## Trap Conditions

- **Addressing traps:** Invalid address, descriptor range violation, page fault, protection violation
- **Integer overflow (O):** Signed integer addition overflow (for BY, H, W types)
- **Floating overflow (FO):** Result too large to represent (for F, D types)
- **Floating underflow (FU):** Result too small to represent (for F, D types)

---

## Data Status Bits

- **Z (Zero):** Set if result = 0, cleared otherwise
- **S (Sign):** Set to sign bit of result
- **O (Overflow):** Set if integer overflow (BY, H, W types)
- **C (Carry):** Set if carry from most significant bit (integer types only)
- **FO (Floating Overflow):** Set if floating overflow (F, D types)
- **FU (Floating Underflow):** Set if floating underflow (F, D types)

---

## Examples

### Example 1: Add constant to register

```assembly
        % Add 10 to word register R1
        W1 + 10
```

### Example 2: Add memory value

```assembly
        % Add local variable to register
        W2 + B.INCREMENT
```

### Example 3: Float addition

```assembly
        % Add float value to register
        F3 + B.DELTA
```

### Example 4: Accumulate in loop

```assembly
        % Sum array elements
        W1 := 0
LOOP:
        W1 + B.ARRAY(I2)
        I2 + 1
        W COMP I2, B.SIZE
        IF<GO LOOP
```

---

## Performance Notes

- **Typical cycles:** 3-6 cycles
- **Best case:** 3 cycles (immediate)
- **Worst case:** 6+ cycles (memory with page fault)

**Note:** Operator syntax compiles to same code as ADD2.

---

## Reference Manual

**Section:** §11.1
**Title:** Add

---

## See Also

- [ADD2](add2.md) - Add two operands (explicit mnemonic)
- [ADD3](add3.md) - Add three operands
- [ADDC](addc.md) - Add with carry
- [-](-.md) - Subtract operator
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
