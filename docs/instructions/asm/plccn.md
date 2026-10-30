# PLCCN - Convert PLANC descriptor to ND-500 descriptor

## Overview

**Mnemonic:** `plccn`  
**Class:** SYSTEM (the grouping used by this project)  
**Privilege:** not stated in the manual  
**Extension:** '87 extension (ND-500 Reference Manual ND-05.009.4, June 1987 edition; the manual says it also runs on ND-500/1 and ND-500/2)  
**Manual section:** 15.15, page 268

## Variants

Total opcodes: 1

| Opcode | Assembly |
|--------|----------|
| 0xFFFD | w plccn |

The opcodes are from the octal column of the manual's code table. The operand rules the assembler uses were copied from the nearest existing instruction with the same format; they are not stated in the manual (see the list of unproven points in the plan).

---

## From the ND-500 Reference Manual

Copied from the manual's Markdown; the manual's tables are as printed.

Format: W PLCCN \<source/r/W\>,\<destination/w/W\>

| Assembly notation | Name                          | Hex code | Octal code |
|-------------------|-------------------------------|----------|------------|
| W PLCCN           | convert to ND-500 descriptor  | FFFDH    | 177775B    |

Operation:
(u-l+1) -> N
a + 1 -> A

Description:

A PLANC descriptor is converted to an ND-500 descriptor.

The descriptors are as shown below:

| Planc descriptor |   | ND-500 descriptor    |
|------------------|---|----------------------|
| address (a)      |   | Number of elements (N) |
| lower (l)        |   | Address (A)            |
| upper (u)        |   |                        |

Data Status Bits:
Number of elements = 0 -> Z
Signbit -> S

---

## See Also

- [ND-5000 '87 extension instructions](../../ND5000-87-EXTENSION-INSTRUCTIONS.md) - all 25, with counts
- [SYSTEM instruction reference](../../instruction-reference/SYSTEM.md)
