# WHOLE - write to NUCLEUS hole

## Overview

**Mnemonic:** `whole`  
**Class:** IO (the grouping used by this project)  
**Privilege:** not stated in the manual  
**Extension:** '87 extension (ND-500 Reference Manual ND-05.009.4, June 1987 edition; the manual says it also runs on ND-500/1 and ND-500/2)  
**Manual section:** 16.26.2, page 302

## Variants

Total opcodes: 1

| Opcode | Assembly |
|--------|----------|
| 0xFE9D | by whole |

The opcodes are from the octal column of the manual's code table. The operand rules the assembler uses were copied from the nearest existing instruction with the same format; they are not stated in the manual (see the list of unproven points in the plan).

---

## From the ND-500 Reference Manual

Copied from the manual's Markdown; the manual's tables are as printed.

Format: BY WHOLE `<string/r/by/I1>`, `<hole/w/by/I2>`

| Assembly notation | Name       | Hex code | Octal code |
|-------------------|------------|----------|------------|
| BY WHOLE          | Write hole | FE9DH    | 177235B    |

**Operation:**
while not end of strings do
S(I1) -> D(I2), I1+1 -> I1, I2+1 -> I2
enddo

**Description:**

Bytes are moved from source string to destination hole until either source is empty or until destination is full.

**String descriptor:**

| Length of source string |
|-------------------------|
| Start address of string |

**Hole descriptor:**

| Hole number |
|-------------|
| Reserved    |

**Trap Conditions:**

- No access to hole : PV trap. Nothing moved, registers unchanged.
- The hole is not a message : IOV trap. Nothing moved, registers unchanged.
- Outside source or destination : Descriptor Range Trap.

**Data Status Bits:**

- Outside source : K = 0, I1, I2 Unchanged, DR trap condition.
- Outside destination : K = 1, I1, I2 Unchanged, DR trap condition.
- Source empty : K = 0, I1, I2 next element.
- Destination full : K = 1, I1, I2 next element.

---

## See Also

- [ND-5000 '87 extension instructions](../../ND5000-87-EXTENSION-INSTRUCTIONS.md) - all 25, with counts
- [IO instruction reference](../../instruction-reference/IO.md)
