# TOSSP - Special load of TOS

## Overview

**Mnemonic:** `tossp:=`  
**Class:** MOVE (the grouping used by this project)  
**Privilege:** not stated in the manual  
**Extension:** '87 extension (ND-500 Reference Manual ND-05.009.4, June 1987 edition; the manual says it also runs on ND-500/1 and ND-500/2)  
**Manual section:** 16.30, page 313

## Variants

Total opcodes: 1

| Opcode | Assembly |
|--------|----------|
| 0xFE9F | tossp:= |

The opcodes are from the octal column of the manual's code table. The operand rules the assembler uses were copied from the nearest existing instruction with the same format; they are not stated in the manual (see the list of unproven points in the plan).

---

## From the ND-500 Reference Manual

Copied from the manual's Markdown; the manual's tables are as printed.

**Format:**
TOSSP := <operand/r/W>

| Assembly notation | Name               | Hex code | Octal code |
|-------------------|--------------------|----------|------------|
| TOSSP             | special load of TOS| FE9F     | 177237B    |

**Operation:**
<operand> -> TOS

**Description:**

The TOS register is loaded from the operand. Before the value is loaded, a check on magnitude greater than B.SP is performed. If true, a stack overflow trap condition exists.

**Trap condition:** Stack overflow trap

**Data Status Bits:**
<operand> = 0 -> Z
<operand>.signbit -> S

---

## See Also

- [ND-5000 '87 extension instructions](../../ND5000-87-EXTENSION-INSTRUCTIONS.md) - all 25, with counts
- [MOVE instruction reference](../../instruction-reference/MOVE.md)
