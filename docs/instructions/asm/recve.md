# RECVE - Receive from port

## Overview

**Mnemonic:** `recve`  
**Class:** IO (the grouping used by this project)  
**Privilege:** not stated in the manual  
**Extension:** '87 extension (ND-500 Reference Manual ND-05.009.4, June 1987 edition; the manual says it also runs on ND-500/1 and ND-500/2)  
**Manual section:** 16.26.4, page 304

## Variants

Total opcodes: 1

| Opcode | Assembly |
|--------|----------|
| 0x00B7 | w1 recve |

The opcodes are from the octal column of the manual's code table. The operand rules the assembler uses were copied from the nearest existing instruction with the same format; they are not stated in the manual (see the list of unproven points in the plan).

---

## From the ND-500 Reference Manual

Copied from the manual's Markdown; the manual's tables are as printed.

**Format:**
W1 RECVE <hole number/r/W>,<number of bytes/w/W>

| Assembly notation | Name             | Hex code | Octal code |
|-------------------|------------------|----------|------------|
| W1 RECVE          | receive from port| B7H      | 267B       |

**Operation:**
\<hole number> -> I1,
length of message -> \<number of bytes>

**Description:**

Receive message from hole number. Message is returned in register 1.
Size of message is returned in 'number of bytes'.

**Trap Conditions:**
Protect violation, Illegal operand specifier

**Data Status Bits:**
Unaffected

---

## See Also

- [ND-5000 '87 extension instructions](../../ND5000-87-EXTENSION-INSTRUCTIONS.md) - all 25, with counts
- [IO instruction reference](../../instruction-reference/IO.md)
