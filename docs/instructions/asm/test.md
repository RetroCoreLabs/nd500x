# TEST - Test Against Zero

**Mnemonic:** `test`  
**Function:** Test if operand is zero  
**Class:** COMPARE  
**Format:** `t TEST <operand>`

**Description:** Compares operand against zero. Equivalent to COMP with implicit zero second operand.

**Operands:** 1  
**Variants:** 6

**Examples:**

```assembly
% Test register for zero
W TEST I1
IF=GO ISZERO

% Check variable value
W TEST B.FLAG
IF><GO NOTZERO

% Test before use
W TEST B.PTR
IF=GO NULL
```

**Reference:** §10.11 Test against zero  
**See Also:** [COMP](comp.md)
