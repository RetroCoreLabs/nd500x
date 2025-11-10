# SCOMP - String Compare

## Overview
**Mnemonic:** `scomp` | **Function:** Compare strings with descriptors | **Class:** STRING | **Privilege:** user | **Format:** `t SCOMP <string1>, <string2>`

## Description
Compares strings element-by-element using descriptor bounds. Sets flags based on first difference or exhaustion. Index registers I1/I2 track positions.

## Variants
| Variant | Opcode | Type | Assembly |
|---------|--------|------|----------|
| 1-6 | 0xFD72-0xFD77 | BI/BY/H/W/F/D | t SCOMP |

## Examples
```assembly
BY SCOMP STR1, STR2            % Compare byte strings
W SCOMP ARRAY1, ARRAY2         % Compare word arrays
BY SCOMP INPUT, EXPECTED       % Validate input
H SCOMP DATA1, DATA2           % Halfword compare
W SCOMP KEY, TABLE(W1)         % Search comparison
F SCOMP CALC, EXPECTED         % Float comparison
BY SCOMP PASSWORD, STORED      % Password check
```

## Trap Conditions
- **Addressing traps**, **Descriptor range (DR)**

## Data Status Bits
- **Z**: Strings equal | **S,C**: Comparison result | **K**: Termination reason

## Reference Manual
**Section:** §14.4

## See Also
- [SMOVE](smove.md), [SFILL](sfill.md), [COMP2](comp2.md)
