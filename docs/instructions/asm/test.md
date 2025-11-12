# TEST - Test Against Zero

## Overview

**Mnemonic:** `test`
**Function:** Test if operand is zero
**Class:** COMPARE
**Privilege:** user
**Format:** `t TEST <operand>`

---

## Description

Tests an operand against zero and sets condition flags based on the result. This is equivalent to comparing the operand with zero (COMP operand, 0) but does not require specifying the zero explicitly. The operand itself is not modified.

TEST is frequently used before conditional branches to check if a value is zero, positive, or negative.

**Operation:**
```
operand - 0  (result discarded, only flags set)
operand = 0 → Z flag
operand < 0 → S flag
C flag cleared
V flag cleared
```

**Key Characteristics:**
- Non-destructive zero comparison (operand unchanged)
- Sets flags without modifying source
- Supports all 6 data types (BI, BY, H, W, F, D)
- More efficient than COMP for zero testing
- Clears C and V flags explicitly
- Essential for NULL checks and sign testing
- Common before conditional branches
- Faster than explicit comparison with zero constant

**Common Use Cases:**
- Check if register or variable is zero
- Check sign (positive/negative) of value
- Validate pointers (NULL check)
- Loop termination tests
- Conditional execution based on value state
- Function return value checking

**Operands:** 1 (source to test)
**Variants:** 6 opcodes (6 data types)

---

## Variants

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/6 | 0x0041 | Bit | BI TEST |
| 2/6 | 0x0042 | Byte | BY TEST |
| 3/6 | 0x0043 | Halfword | H TEST |
| 4/6 | 0x0044 | Word | W TEST |
| 5/6 | 0x0045 | Float | F TEST |
| 6/6 | 0x0046 | Double | D TEST |

---

## Operands

**Operand 1** (Source, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix (BI/BY/H/W/F/D)
- **Role**: Value to test against zero
- **Access**: Read-only (operand is never modified)

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation

---

## Data Status Bits

- **Z (Zero)**: Set if operand = 0, cleared otherwise
- **S (Sign)**: Set if operand < 0, cleared otherwise
- **C (Carry)**: Cleared
- **V (Overflow)**: Cleared

---

## Examples

### Example 1: Test register for zero
```assembly
        % Test if register is zero
        W TEST I1
        IF=GO ISZERO
```

### Example 2: Check variable value
```assembly
        % Branch if variable is not zero
        W TEST B.FLAG
        IF><GO NOTZERO
```

### Example 3: NULL pointer check
```assembly
        % Test pointer before use
        W TEST B.PTR
        IF=GO NULL
        % Safe to use pointer
        W MOVE (B.PTR), I1
```

### Example 4: Check sign of value
```assembly
        % Branch based on sign
        W TEST B.VALUE
        IF<GO NEGATIVE
        IF=GO ZERO
        % Falls through if positive
POSITIVE:
```

### Example 5: Loop counter check
```assembly
        % Check loop counter
LOOP:
        % ... loop body ...
        W1 DEC
        W TEST I1
        IF><GO LOOP      % Continue if not zero
```

### Example 6: Function return value
```assembly
        % Check error code
        CALL FUNCTION
        W TEST I1
        IF=GO SUCCESS    % Zero = success
        % Handle error
```

### Example 7: Byte flag testing
```assembly
        % Test byte-sized flag
        BY TEST FLAGS
        IF=GO NOT_SET
        % Flag is set (non-zero)
```

### Example 8: Floating-point zero check
```assembly
        % Test float for zero
        F TEST B.RESULT
        IF=GO ZERO_RESULT
        IF<GO NEGATIVE_RESULT
        % Positive result
```

### Example 9: Array bounds validation
```assembly
        % Validate array index
        W TEST INDEX
        IF<GO INVALID    % Negative index
        W COMP INDEX, ARRAY_SIZE
        IF>=GO INVALID   % Index too large
        % Valid index
```

### Example 10: Multi-condition check
```assembly
        % Check multiple values
        W TEST VAR1
        IF><GO SKIP1
        W TEST VAR2
        IF><GO SKIP2
        % Both are zero
```

---

## Performance Notes

- **Execution**: 1-2 cycles
  - Register operand: 1 cycle
  - Memory operand: 2 cycles
- **No modification**: Faster than COMP in some cases (no second operand fetch)
- **Optimization**: Use TEST instead of `COMP <operand>, 0`

**Comparison with COMP:**
- `W TEST I1` is equivalent to `W COMP I1, 0` but more efficient
- TEST is clearer in intent when checking for zero
- TEST always clears C and V flags; COMP may set them

**Common patterns:**
```assembly
% Pattern 1: Zero check with branch
W TEST I1
IF=GO ZERO_CASE

% Pattern 2: Non-zero check
W TEST I1
IF><GO NON_ZERO_CASE

% Pattern 3: Sign check
W TEST I1
IF<GO NEGATIVE
IF>GO POSITIVE

% Pattern 4: Pointer validation
W TEST B.PTR
IF=GO NULL_PTR
W MOVE (B.PTR), I2   % Safe access
```

---

## Reference Manual

**Section:** §10.11
**Title:** Test against zero

---

## See Also

- [COMP](comp.md) - Compare two operands
- [TSET](tset.md) - Test and set (atomic operation)
- [IF](if.md) - Conditional branch instructions
- [CLR](clr.md) - Clear register to zero
