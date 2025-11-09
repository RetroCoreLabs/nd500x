# COMP - Compare

**Mnemonic:** `comp`  
**Function:** Compare register against operand  
**Class:** COMPARE  
**Format:** `t COMP <operand>`

**Description:** Subtracts operand from register without storing result. Sets status bits for conditional branching. True comparison - sign bit changes on overflow.

**Operands:** 1  
**Variants:** 24

**Examples:**

```assembly
% Compare for loop termination
W COMP I2, B.SIZE
IF>=GO DONE

% Compare with zero
W COMP B.VALUE, 0
IF=GO ISZERO

% Float comparison
F COMP A1, B.THRESHOLD
IF>GO EXCEEDED
```

**Reference:** §10.9 Compare  
**See Also:** [TEST](test.md), [COMP2](comp2.md), conditional jumps
