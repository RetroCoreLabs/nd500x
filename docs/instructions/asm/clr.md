# CLR - Clear Register

## Overview

**Mnemonic:** `clr`
**Function:** Clear register to zero
**Class:** MOVE
**Privilege:** user

**Format:** `tn CLR` (where t = data type prefix, n = register number 1-4)

---

## Description

Clears (zeros) the specified general-purpose register in a single operation. This instruction sets all bits of the destination register to zero, regardless of the data type prefix used. The register is treated as a logical register number (1-4) selected by the opcode variant, with the data type prefix controlling the operation size.

CLR is one of the most fundamental and frequently used instructions in the ND-500 architecture. It provides a fast, compact way to initialize registers, reset accumulators, and establish known states before computation.

**Key Characteristics:**

1. **No Operands**: Unlike assignment operators (`:=`), CLR has no source operand - it implicitly uses the constant zero
2. **Register-Only**: Operates exclusively on general-purpose registers (cannot clear memory directly)
3. **Size-Independent Result**: Regardless of prefix (BI, BY, H, W, F, D), the entire 32/64-bit register is zeroed
4. **Fast Execution**: Single-cycle operation on most implementations
5. **Status Flags**: Always sets Z (zero) flag; clears S (sign) flag

**Register Variants:**

The opcode range 0x0084-0x008F encodes 12 register-clearing operations with different register combinations. The 24 total variants account for different data type prefixes applied to these 12 base opcodes.

**Common Use Cases:**
- Loop counter initialization
- Accumulator reset before sum/product computation
- Register clearing before conditional assignment
- Zero-initialization of function return values
- Flag/state variable initialization

Unlike memory-clearing operations (which require address calculation and bus cycles), CLR operates entirely within the CPU register file, making it extremely fast.

**Operands:** 0 (register number encoded in opcode)
**Variants:** 24 opcode(s)

---

## Variants

| Variant | Opcode | Register | Assembly Notation | Note |
|---------|--------|----------|-------------------|------|
| 1/24 | 0x0084 | R1 | BI1/BY1/H1/W1/F1/D1 CLR | Clear register 1 |
| 2/24 | 0x0085 | R2 | BI2/BY2/H2/W2/F2/D2 CLR | Clear register 2 |
| 3/24 | 0x0086 | R3 | BI3/BY3/H3/W3/F3/D3 CLR | Clear register 3 |
| 4/24 | 0x0087 | R4 | BI4/BY4/H4/W4/F4/D4 CLR | Clear register 4 |
| ... | ... | ... | ... | (20 more variants with additional prefixes) |

**Note**: All data type prefixes (BI, BY, H, W, F, D) produce the same result (zero), but may be used for documentation/clarity.

---

## Operands

**None**: Register number is encoded in the opcode itself.

**Result**: Specified register is set to all zeroes (0x00000000 for 32-bit, 0x0000000000000000 for 64-bit).

---

## Trap Conditions

- **None**: This instruction cannot trap under normal execution

---

## Data Status Bits

- **Z (Zero)**: Always set to 1 (result is always zero)
- **S (Sign)**: Always cleared to 0 (zero has no sign bit set)
- **C (Carry)**: Unaffected
- **V (Overflow)**: Unaffected

---

## Examples

### Example 1: Loop counter initialization

```assembly
% Initialize loop counter to zero
        W1 CLR
LOOP:
        % Loop body
        W1 ADD 1, I1
        W1 COMP I1, LIMIT
        IF<GO LOOP
```

### Example 2: Accumulator clearing before sum

```assembly
% Sum array elements
        W1 CLR                 % Clear accumulator
        W2 CLR                 % Clear index
SUM_LOOP:
        W1 ADD ARRAY(I2), I1   % Add array element
        W2 ADD 1, I2           % Increment index
        W2 COMP I2, COUNT
        IF<GO SUM_LOOP
        % W1 now contains sum
```

### Example 3: Multiple register initialization

```assembly
% Clear multiple registers at function entry
FUNC:
        W1 CLR                 % Clear return value
        W2 CLR                 % Clear temp1
        W3 CLR                 % Clear temp2
        W4 CLR                 % Clear temp3
        % Function body
```

### Example 4: Floating-point zero initialization

```assembly
% Initialize floating-point accumulator
        F1 CLR                 % F1 = 0.0 (floating zero)
        D2 CLR                 % D2 = 0.0 (double zero)
        % Use in calculation
        F ADD VALUE1, F1, F1
        F ADD VALUE2, F1, F1
```

### Example 5: Conditional result initialization

```assembly
% Initialize result before conditional assignment
        W1 CLR                 % Default result = 0
        W2 COMP INPUT, THRESHOLD
        IF<GO SKIP             % If below threshold, keep zero
        W1 MOVE 1, I1          % Otherwise set to 1
SKIP:
```

### Example 6: Product accumulator

```assembly
% Calculate factorial (n!)
        W1 MOVE N, I1
        W2 MOVE 1, I2          % Product accumulator = 1 (not CLR!)
FACT_LOOP:
        W2 MUL I1, I2          % Multiply
        W1 SUB 1, I1           % Decrement
        W1 COMP I1, 1
        IF>GO FACT_LOOP

% For sum, use CLR:
        W2 CLR                 % Sum accumulator = 0
```

### Example 7: Multi-precision arithmetic initialization

```assembly
% Clear 64-bit result (using two 32-bit registers)
        W1 CLR                 % Low word
        W2 CLR                 % High word
        % Now perform 64-bit operation
```

---

## Performance Notes

- **Size**: 2 bytes (1 word) per instruction
- **Execution Time**: Typically 1 cycle (register file operation)
- **Memory Access**: None - purely register operation
- **vs Assignment**: `Wn CLR` is faster than `Wn MOVE 0, In` (no constant operand)
- **vs Memory**: Much faster than clearing memory locations
- **Code Density**: Most compact way to zero a register
- **Pipeline**: No dependencies, executes in parallel with other operations
- **Optimization**: Compiler often uses CLR instead of loading constant zero

---

## Reference Manual

**Section:** §10.16 (est.)
**Title:** Clear register

---

## See Also

- [:=](assignto.md) - Assignment operator (can assign any value)
- [STZ](stz.md) - Store zero to memory
- [INCR](incr.md) - Increment register
- [DECR](decr.md) - Decrement register
- [MOVE](move.md) - General register move
