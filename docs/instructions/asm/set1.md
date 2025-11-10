# SET1 - Set Bit Field (Single Operand)

## Overview
**Mnemonic:** `set1` | **Function:** Set bits in field | **Class:** BIT | **Privilege:** user | **Format:** `t SET1 <operand>`

## Description
Sets (ORs with 1) bits in specified field. Single-operand bit manipulation instruction for setting multiple bits simultaneously.

## Variants
| Variant | Opcode | Type | Assembly |
|---------|--------|------|----------|
| 1-6 | 0xFDA8-0xFDAD | BY/H/W | t SET1 |

## Examples
```assembly
BY SET1 FLAGS                  % Set byte flags
W SET1 STATUS_WORD             % Set word bits
H SET1 CONTROL_REG             % Set halfword
W SET1 B.BIT_FIELD             % Set local bits
BY SET1 MASK_VAL               % Set mask bits
W SET1 CONFIG(W1)              % Indexed set
H SET1 STATE_FLAGS             % Set state
```

## Trap Conditions
- **Addressing traps**

## Data Status Bits
- **Z,S**: Set based on result

## Reference Manual
**Section:** §10.x

## See Also
- [CLR](clr.md), [TEST](test.md), [OR](or.md)
