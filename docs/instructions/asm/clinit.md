# CLINIT - Initialize local clock

## Overview

**Mnemonic:** `clinit`  
**Class:** SYSTEM (the grouping used by this project)  
**Privilege:** privileged (the manual says "Privileged instruction")  
**Extension:** '87 extension (ND-500 Reference Manual ND-05.009.4, June 1987 edition; the manual says it also runs on ND-500/1 and ND-500/2)  
**Manual section:** 15.17, page 270

## Variants

Total opcodes: 1

| Opcode | Assembly |
|--------|----------|
| 0xFF1E | clinit |

The opcodes are from the octal column of the manual's code table. The operand rules the assembler uses were copied from the nearest existing instruction with the same format; they are not stated in the manual (see the list of unproven points in the plan).

---

## From the ND-500 Reference Manual

Copied from the manual's Markdown; the manual's tables are as printed.

| Assembly notation | Name                | Hex code | Octal code |
|-------------------|---------------------|----------|------------|
| CLINIT            | initialize CPU's clock | FF1EH    | 177436B    |

**Operation:** 0 -> <clock>

**Description:**

Privileged instruction

The CPU contains a local clock running at 1 microsecond cycle time.

Clock is reset and started.

**Trap Conditions:** None

**Data Status Bits:** Unaffected

---

## See Also

- [ND-5000 '87 extension instructions](../../ND5000-87-EXTENSION-INSTRUCTIONS.md) - all 25, with counts
- [SYSTEM instruction reference](../../instruction-reference/SYSTEM.md)
