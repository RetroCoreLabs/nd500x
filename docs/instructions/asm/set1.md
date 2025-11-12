# SET1 - Set to One

## Overview

**Mnemonic:** `set1`
**Function:** Set operand to constant value 1
**Class:** CONTROL
**Privilege:** user

**Format:** `t SET1 <operand>`

---

## Description

Replaces the contents of the destination operand with the value one (1). This instruction provides a convenient shorthand for initialization and counter setup. All data status bits are cleared after execution.

**Operation:**
```
1 → <operand>
```

**Key Characteristics:**
- Sets operand to exactly 1
- Works with all data types: BI, BY, H, W, F, D
- All status flags cleared
- Single operand instruction
- Efficient for loop counter initialization
- Float/double get 1.0 value

**Common Use Cases:**
- Loop counter initialization
- Boolean flag setting
- Reset counters to 1
- Initialize accumulators
- Set iteration start value

**Operands:** 1 (destination, write-only)
**Variants:** 6 opcodes (one per data type)

---

## Examples

### Example 1: Initialize loop counter

```assembly
        % Start loop at 1
        W1 SET1 B.COUNTER
LOOP:   % Loop body
        W1 ADD 1, B.COUNTER
        W1 COMP 10, B.COUNTER
        IF<=GO LOOP
```

**Explanation:** Initialize loop counter to 1 instead of 0.

### Example 2: Set float to 1.0

```assembly
        % Initialize float multiplier
        F SET1 IND(B.START)
        % F register indirectly sets value to 1.0
```

**Explanation:** Set floating-point argument to 1.0.

### Example 3: Boolean flag

```assembly
        % Mark as active/true
        BY SET1 ACTIVE_FLAG
        % Check later
        BY COMP 1, ACTIVE_FLAG
        IF=GO IS_ACTIVE
```

**Explanation:** Use 1 as true value for boolean flag.

### Example 4: Reset counter

```assembly
        % Reset iteration counter
        W SET1 RETRY_COUNT
        % Start retry loop
```

**Explanation:** Reset counter to initial value of 1.

### Example 5: Initialize array element

```assembly
        % Set array entry to 1
        W2 MOVE INDEX
        H SET1 ARRAY(W2)
```

**Explanation:** Initialize indexed array element to 1.

---

## Trap Conditions

- **Addressing traps**: Invalid operand address

---

## Data Status Bits

- **All cleared**: Z=0, S=0, C=0, V=0, all flags cleared

---

## Reference Manual

**Section:** §10.18
**Title:** Set to one

---

## See Also

- [CLR](clr.md) - Clear to zero
- [MOVE](move.md) - Move data
- [LOAD](load.md) - Load constant
