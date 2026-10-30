# SEND - Send to port

## Overview

**Mnemonic:** `send`  
**Class:** IO (the grouping used by this project)  
**Privilege:** not stated in the manual  
**Extension:** '87 extension (ND-500 Reference Manual ND-05.009.4, June 1987 edition; the manual says it also runs on ND-500/1 and ND-500/2)  
**Manual section:** 16.26.3, page 303

## Variants

Total opcodes: 1

| Opcode | Assembly |
|--------|----------|
| 0x00B6 | w1 send |

The opcodes are from the octal column of the manual's code table. The operand rules the assembler uses were copied from the nearest existing instruction with the same format; they are not stated in the manual (see the list of unproven points in the plan).

---

## From the ND-500 Reference Manual

Copied from the manual's Markdown; the manual's tables are as printed.

**Format:** W1 SEND \<hole number/r/W>

| Assembly notation | Name        | Hex code | Octal code |
|------------------|-------------|----------|------------|
| W1 SEND          | send to port | B6H      | 266B       |

**Operation:**
I1 -> \<hole number>

**Description:**

Message of register 1 is sent to hole number as specified by the operand.

**Trap Conditions:** Protect violation, Illegal operand specifier

**Data Status Bits:** Unaffected

---

## See Also

- [ND-5000 '87 extension instructions](../../ND5000-87-EXTENSION-INSTRUCTIONS.md) - all 25, with counts
- [IO instruction reference](../../instruction-reference/IO.md)
