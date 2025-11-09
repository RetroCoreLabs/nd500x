# STZ - Store Zero

**Mnemonic:** `stz`  
**Function:** Store zero to operand  
**Class:** MOVE  
**Format:** `t STZ <operand>`

**Description:** Replaces contents of destination operand with zero.

**Operands:** 1  
**Variants:** 6

**Examples:**

```assembly
% Zero a variable
W STZ B.COUNT

% Clear array element
W STZ B.ARRAY(I2)

% Reset record field
W STZ R.VALUE
```

**Reference:** §10.17 Store zero  
**See Also:** [CLR](clr.md)
