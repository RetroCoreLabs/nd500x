# WEXT - Write to device external to CPU

## Overview

**Mnemonic:** `wext`  
**Class:** IO (the grouping used by this project)  
**Privilege:** privileged (the manual says "Privileged instruction")  
**Extension:** '87 extension (ND-500 Reference Manual ND-05.009.4, June 1987 edition; the manual says it also runs on ND-500/1 and ND-500/2)  
**Manual section:** 16.29, page 312

## Variants

Total opcodes: 4

| Opcode | Assembly |
|--------|----------|
| 0xFFEC | w1 wext |
| 0xFFED | w2 wext |
| 0xFFEE | w3 wext |
| 0xFFEF | w4 wext |

The opcodes are from the octal column of the manual's code table. The operand rules the assembler uses were copied from the nearest existing instruction with the same format; they are not stated in the manual (see the list of unproven points in the plan).

---

## From the ND-500 Reference Manual

Copied from the manual's Markdown; the manual's tables are as printed.

**Format:** Wn WEXT <device/r/W>

| Assembly notation | Name                     | Hex code | Octal code         |
|-------------------|--------------------------|----------|--------------------|
| Wn WEXT           | write to external device | FFEC     | 177754B+n-1        |

**Operation:**
In -> <device>

**Description:**

Privileged instruction.

Information is written into external device from the specified register. Further devices will be supported in later versions.

Register 'n' is written to 'device'.

Device numbers:

Device = 0 : OCTO-bus / ACCP.

**Data Status Bits:**

Unable to write data 1 -> K
else 0 -> K

---

## See Also

- [ND-5000 '87 extension instructions](../../ND5000-87-EXTENSION-INSTRUCTIONS.md) - all 25, with counts
- [IO instruction reference](../../instruction-reference/IO.md)
