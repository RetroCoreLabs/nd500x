# ENTIER - SIMULA Entier function

## Overview

**Mnemonic:** `entier`  
**Class:** FLOAT_MATH (the grouping used by this project)  
**Privilege:** not stated in the manual  
**Extension:** '87 extension (ND-500 Reference Manual ND-05.009.4, June 1987 edition; the manual says it also runs on ND-500/1 and ND-500/2)  
**Manual section:** 10.37

## Variants

Total opcodes: 2

| Opcode | Assembly |
|--------|----------|
| 0xFDC7 | f entier |
| 0xFDC8 | d entier |

The opcodes are from the octal column of the manual's code table. The operand rules the assembler uses were copied from the nearest existing instruction with the same format; they are not stated in the manual (see the list of unproven points in the plan).

---

## From the ND-500 Reference Manual

Copied from the manual's Markdown; the manual's tables are as printed.

**Format:** t ENTIER `<source/r/t1>,<destination/w/w>`

| Assembly notation | Name                | Hex code | Octal code |
|-------------------|---------------------|----------|------------|
| F ENTIER          | float entier        | FDC7H    | 176707B    |
| D ENTIER          | double float entier | FDC8H    | 176710B    |

**Operation:**

    if int(source) > source then
        int(source) - 1 -> destination
    else
        int(source) -> destination
    endif

**Description:**

The function calculates the integer part of the source in accordance to the SIMULA Entier definition and stores it as a 32 bit integer in the destination.

**Data Status Bits:**

- result = 0 -> Z
- <result>.signbit -> S
- integer overflow -> O

| Page | Title            |
|------|------------------|
| 160  | ND-500 Reference Manual |

**Chapter 11**

#### Arithmetical Instructions

---

## See Also

- [ND-5000 '87 extension instructions](../../ND5000-87-EXTENSION-INSTRUCTIONS.md) - all 25, with counts
- [FLOAT_MATH instruction reference](../../instruction-reference/FLOAT_MATH.md)
