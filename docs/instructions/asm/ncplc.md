# NCPLC - Convert ND-500 descriptor to PLANC descriptor

## Overview

**Mnemonic:** `ncplc`  
**Class:** SYSTEM (the grouping used by this project)  
**Privilege:** not stated in the manual  
**Extension:** '87 extension (ND-500 Reference Manual ND-05.009.4, June 1987 edition; the manual says it also runs on ND-500/1 and ND-500/2)  
**Manual section:** 15.16, page 269

## Variants

Total opcodes: 1

| Opcode | Assembly |
|--------|----------|
| 0xFFFE | w ncplc |

The opcodes are from the octal column of the manual's code table. The operand rules the assembler uses were copied from the nearest existing instruction with the same format; they are not stated in the manual (see the list of unproven points in the plan).

---

## From the ND-500 Reference Manual

Copied from the manual's Markdown; the manual's tables are as printed.

**Format:** W NCPLC `<source/r/W>,<destination/w/W>`

| Assembly notation | Name                      | Hex code | Octal code  |
|-------------------|---------------------------|----------|-------------|
| W NCPLC           | convert to PLANC descriptor | FFFEH    | 177776B     |

**Operation:**

A -> a
O -> l
N - 1 -> u
If u-l+1 < 0, 0 -> N

**Description:**

Convert ND-500 descriptor to planc descriptor.

The descriptors are as shown below:

| ND-500 descriptor      | Planc descriptor  |
|------------------------|-------------------|
| Number of elements (N) | address (a)       |
| Address (A)            | lower (l)         |
|                        | upper (u)         |

**Data Status Bits:**

Upper = O -> Z
Signbit -> S

---

## See Also

- [ND-5000 '87 extension instructions](../../ND5000-87-EXTENSION-INSTRUCTIONS.md) - all 25, with counts
- [SYSTEM instruction reference](../../instruction-reference/SYSTEM.md)
