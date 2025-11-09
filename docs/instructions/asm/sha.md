# SHA - Shift Arithmetic

**Mnemonic:** `sha`  
**Function:** Arithmetic right shift  
**Class:** SHIFT  
**Format:** `tn SHA <count>`

**Description:** Arithmetic shift right - preserves sign bit.

**Examples:**

```assembly
% Divide by 2 (signed)
W1 SHA 1

% Arithmetic shift
W2 SHA 4
```

**Reference:** §10.26 Shift arithmetic  
**See Also:** [SHL](shl.md), [SHR](shr.md)
