# SETE - Set Bit in Trap Enable Register

## Overview
**Mnemonic:** `sete` | **Function:** Enable trap by setting OTE bit | **Class:** CONTROL | **Privilege:** user | **Format:** `SETE <bit no>`

## Description
Sets specified bit in Own Trap Enable (OTE) register to enable trap handling. Bit must be modifiable per TEMM mask or IOV trap occurs.

## Variants
| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFD39 | SETE |

## Examples
```assembly
SETE 9                         % Enable overflow trap
SETE 17                        % Enable single instruction trap
SETE I1                        % Enable trap via register
SETE B.TRAP_NUM                % Enable from variable
SETE 0                         % Enable trap 0
SETE TRAP_CONFIG(W1)           % Array-based enable
SETE 12                        % Enable specific trap
```

## Trap Conditions
- **Illegal operand value (IOV)**: Bit not modifiable per TEMM

## Data Status Bits
- **Z,S,C,V**: Unaffected

## Reference Manual
**Section:** §16.5

## See Also
- [CLTE](clte.md), [SETK](setk.md), [CLRK](clrk.md)
