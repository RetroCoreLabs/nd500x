# MULAD - Multiply and Add (Fused Multiply-Add)

## Overview

**Mnemonic:** `mulad`
**Function:** Fused multiply-add operation
**Class:** ARITHMETIC
**Privilege:** user
**Format:** `tn MULAD <x>, <y>`

---

## Description

Performs a fused multiply-add operation that multiplies the contents of a register by the first operand, then adds the second operand to the product, storing the result back in the register. This is a single atomic instruction that combines multiplication and addition.

**Operation:**
```
Rn = Rn * x + y
result = 0 → Z flag
result.signbit → S flag
```

**Common Use Cases:**
- Time calculations (hours × 60 + minutes)
- Unit conversions with offsets
- Polynomial evaluation
- Linear interpolation
- Affine transformations
- DSP algorithms
- Matrix operations
- Fixed-point arithmetic

**Operands:** 2 (multiplier, addend)
**Variants:** 20 opcodes (5 data types × 4 registers)

---

## Variants

| Variant | Opcode | Type | Register | Assembly |
|---------|--------|------|----------|----------|
| 1-4 | 0xFCE8-0xFCEB | BY | I1-I4 | BYn MULAD |
| 5-8 | 0xFCEC-0xFCEF | H | I1-I4 | Hn MULAD |
| 9-12 | 0x00A8-0x00AB | W | I1-I4 | Wn MULAD |
| 13-16 | 0xFCF0-0xFCF3 | F | I1-I4 | Fn MULAD |
| 17-20 | 0xFCF4-0xFCF7 | D | I1-I4 | Dn MULAD |

---

## Operands

**Operand 1** (Multiplier, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix (BY/H/W/F/D)
- **Role**: Multiplication factor

**Operand 2** (Addend, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix (BY/H/W/F/D)
- **Role**: Value to add after multiplication

**Implicit Operand** (Register Rn, Read-Modify-Write):
- **Role**: Multiplicand and result destination

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation
- **Integer overflow (O)**: Result exceeds data type range (integer types)
- **Floating overflow (FO)**: Result exceeds floating-point range
- **Floating underflow (FU)**: Result too small for floating-point representation

---

## Data Status Bits

- **Z (Zero)**: Set if result = 0, cleared otherwise
- **S (Sign)**: Set to result's sign bit
- **C (Carry)**: Set if carry from MSB (integer types only)
- **O (Overflow)**: Set if integer overflow occurs
- **FU (Floating Underflow)**: Set if floating underflow
- **FO (Floating Overflow)**: Set if floating overflow

---

## Examples

### Example 1: Time conversion (hours to minutes)
```assembly
        % Convert hours to minutes: hours × 60 + minutes
        H2 := B.HOURS
        H2 MULAD 60:B, B.MINUTES
        % Result: total minutes in H2
```

### Example 2: Temperature conversion (Celsius to Fahrenheit)
```assembly
        % F = C × 9/5 + 32
        F1 := B.CELSIUS
        F1 MULAD 1.8, 32.0
        % Result: Fahrenheit in F1
```

### Example 3: Linear interpolation
```assembly
        % result = base × factor + offset
        W1 := B.BASE
        W1 MULAD SCALE, OFFSET
```

### Example 4: Array index calculation
```assembly
        % address = index × element_size + base_offset
        W2 := INDEX
        W2 MULAD ELEM_SIZE, BASE_ADDR
```

### Example 5: Polynomial evaluation (ax + b)
```assembly
        % Evaluate: a × x + b
        F3 := B.X
        F3 MULAD A_COEFF, B_COEFF
```

### Example 6: Price calculation with tax
```assembly
        % total = price × (1 + tax_rate)
        D1 := B.PRICE
        D1 MULAD TAX_RATE, B.PRICE
        % Result: price + (price × tax)
```

### Example 7: Distance calculation
```assembly
        % distance = velocity × time + initial_position
        F2 := B.VELOCITY
        F2 MULAD TIME, POSITION
```

### Example 8: Byte scaling with offset
```assembly
        % scaled = value × multiplier + offset
        BY1 := INPUT
        BY1 MULAD 3, 10
```

### Example 9: Fixed-point arithmetic
```assembly
        % Fixed-point: (value × scale) + bias
        W3 := SENSOR_VALUE
        W3 MULAD SCALE_FACTOR, BIAS
```

### Example 10: Matrix element calculation
```assembly
        % element = row × cols + col
        W4 := ROW_INDEX
        W4 MULAD NUM_COLS, COL_INDEX
```

---

## Performance Notes

- **Execution**: 4-6 cycles
  - Integer: ~4 cycles
  - Floating-point: ~6 cycles
- **Optimization**: Single instruction vs separate MUL + ADD (saves 2-3 cycles)
- **Precision**: Floating-point maintains full precision through operation

**Comparison with separate operations:**
```assembly
% MULAD (1 instruction, ~4-6 cycles):
W1 MULAD X, Y           % Rn = Rn × X + Y

% Separate (2 instructions, ~6-9 cycles):
W1 MUL X                % Rn = Rn × X
W1 ADD Y                % Rn = Rn + Y
```

**Fused operation advantages:**
- Fewer instructions
- Faster execution
- Single rounding for floating-point (more accurate)
- Atomic operation
- Better register utilization

**Common patterns:**
```assembly
% Pattern 1: Scale and offset
Rn MULAD SCALE, OFFSET

% Pattern 2: Time conversion
Hn MULAD 60, MINUTES    % Hours to minutes

% Pattern 3: Affine transform
Fn MULAD A, B           % y = ax + b

% Pattern 4: Index calculation
Wn MULAD SIZE, BASE     % Array indexing
```

**Floating-point precision:**
```assembly
% MULAD performs intermediate calculation at full precision:
F1 := 0.1
F1 MULAD 10.0, 0.1      % (0.1 × 10.0) + 0.1 = 1.1
% Single rounding vs double rounding with separate ops
```

**Type coercion:**
```assembly
% Force constant to byte to save instruction bytes:
H2 MULAD 60:B, B.MINUTES    % :B suffix forces byte constant

% Without coercion (larger instruction):
H2 MULAD 60, B.MINUTES      % Defaults to halfword constant
```

---

## Reference Manual

**Section:** §11.19
**Title:** Multiply and add

---

## See Also

- [PSUM](psum.md) - Sum of products (x × y + Rn → Rn)
- [MUL3](mul3.md) - Three-operand multiply
- [MUL](mul.md) - Two-operand multiply
- [ADD](add.md) - Addition
- [MULC](mulc.md) - Multiply with carry (if available)
