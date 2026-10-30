# CLREAD - Read local clock

## Overview

**Mnemonic:** `clread`  
**Class:** SYSTEM (the grouping used by this project)  
**Privilege:** not stated in the manual  
**Extension:** '87 extension (ND-500 Reference Manual ND-05.009.4, June 1987 edition; the manual says it also runs on ND-500/1 and ND-500/2)  
**Manual section:** 15.18, page 271

## Variants

Total opcodes: 1

| Opcode | Assembly |
|--------|----------|
| 0xFF1F | clread |

The opcodes are from the octal column of the manual's code table. The operand rules the assembler uses were copied from the nearest existing instruction with the same format; they are not stated in the manual (see the list of unproven points in the plan).

---

## From the ND-500 Reference Manual

Copied from the manual's Markdown; the manual's tables are as printed.

| Assembly notation | Name              | Hex code | Octal code |
|-------------------|-------------------|----------|------------|
| CLREAD            | read CPU's clock | FF1FH    | 177437B    |

#### Operation:
`<clock> -> W1`

#### Description:
The clock value is read into register number 1. Time is an integer value giving the number of microseconds since the last CLINIT instruction.

Note that the clock counts for a period of 2**32 microseconds after which it starts from zero again.

#### Trap Conditions:
None

#### Data Status Bits:
- `<clock> = 0 -> Z`
- `<clock>.signbit -> S`

---

## See Also

- [ND-5000 '87 extension instructions](../../ND5000-87-EXTENSION-INSTRUCTIONS.md) - all 25, with counts
- [SYSTEM instruction reference](../../instruction-reference/SYSTEM.md)
