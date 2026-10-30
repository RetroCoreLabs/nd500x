# REXT - Read from device external to CPU

## Overview

**Mnemonic:** `rext`  
**Class:** IO (the grouping used by this project)  
**Privilege:** privileged (the manual says "Privileged instruction")  
**Extension:** '87 extension (ND-500 Reference Manual ND-05.009.4, June 1987 edition; the manual says it also runs on ND-500/1 and ND-500/2)  
**Manual section:** 16.28, page 311

## Variants

Total opcodes: 4

| Opcode | Assembly |
|--------|----------|
| 0xFFE8 | w1 rext |
| 0xFFE9 | w2 rext |
| 0xFFEA | w3 rext |
| 0xFFEB | w4 rext |

The opcodes are from the octal column of the manual's code table. The operand rules the assembler uses were copied from the nearest existing instruction with the same format; they are not stated in the manual (see the list of unproven points in the plan).

---

## From the ND-500 Reference Manual

Copied from the manual's Markdown; the manual's tables are as printed.

**Format:** Wn REXT \<device/r/W\>

| Assembly notation | Name              | Hex code | Octal code      |
|-------------------|-------------------|----------|-----------------|
| Wn REXT           | read from external| FFE8H    | 177750B+n-1     |

**Operation:** \<device\> -> In

**Description:**

Privileged instruction.

Information is read from external device into the specified register. Further devices will be supported in later versions.

**Device numbers:**

Device = 0 : OCTO-bus / ACCP.

**Data Status Bits:**

Nothing read 1 -> K
else 0 -> K

---

## See Also

- [ND-5000 '87 extension instructions](../../ND5000-87-EXTENSION-INSTRUCTIONS.md) - all 25, with counts
- [IO instruction reference](../../instruction-reference/IO.md)
