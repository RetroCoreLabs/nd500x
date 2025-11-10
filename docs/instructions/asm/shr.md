# SHR - Shift Right

**Mnemonic:** `shr`  
**Function:** Logical right shift  
**Class:** SHIFT  
**Format:** `tn SHR <count>`

**Description:** Shifts register right by count positions. Zeroes shift in from left.

**Examples:**

```assembly
% Divide by 2 (unsigned)
W1 SHR 1

% Extract high byte
W2 SHR 8
```

**Reference:** §10.25 Shift right  
**See Also:** [SHL](shl.md), [SHA](sha.md)
