# C# RetroCore Bugs Found During Migration

This document tracks discrepancies found between C# RetroCore implementation, YAML specifications, ASM documentation, and the ND-500 Reference Manual during the C migration process.

## Bug Tracking Status

**Total Bugs Found**: 1
**Fixed in C#**: 0
**Fixed in C Only**: 1
**Pending Review**: 0

---

## Issue #1: INIT Instruction - Manual Example Has Reversed Operands

**Date Found**: 2025-11-14
**Severity**: Low
**Status**: Documented (Manual error, not C# bug)

### Description

The ND-500 Reference Manual (Page 229) example for the INIT instruction has operands reversed.

### Manual Specification

**Operand Format** (Page 229, lines 7758-7760):
```
INIT <<bottom of stack/r/W>>,
     <stack demand of main program/r/W>,
     <total system stack demand/r/W>
```

**Operation** (Page 229, lines 7768-7774):
```
<<bottom of stack>> → B
<<bottom of stack>> + <total system stack demand> → TOS
<<bottom of stack>> + <stack demand of main program> → B.SP
0 → B.PREVB
0 → B.RETA → L
```

**Overflow Condition** (Page 229, line 7778):
> A value of <stack demand of main program> greater than or equal to <total system stack demand> will cause a stack overflow trap condition.

### The Problem

**Manual Example** (Page 229, line 7789):
```assembly
INIT FRAME, 010000H, 01000H
```

**Manual Description** (line 7787):
> Initialize a new stack at FRAME, requiring 010000H stack locations for the system, 01000H for the main program

This contradicts the operand definition:
- Operand 1: `010000H` (described as "for the system")
- Operand 2: `01000H` (described as "for the main program")

But the specification says:
- Operand 1 = **<stack demand of main program>**
- Operand 2 = **<total system stack demand>**

Furthermore, this would trigger the overflow trap:
```
stack_demand_main (0x10000) >= total_stack_demand (0x1000) → TRAP!
```

### Correct Example

```assembly
INIT FRAME, 0x1000, 0x10000
```

Description:
> Initialize a new stack at FRAME, requiring 0x1000 bytes for the main program, 0x10000 total system stack demand

This makes sense:
- B = FRAME
- B.SP = FRAME + 0x1000 (main program gets 4KB)
- TOS = FRAME + 0x10000 (total stack is 64KB)
- Stack has 60KB available for subroutine frames

### C# Implementation Status

✅ **C# is CORRECT** - RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CONTROL/Init.cs follows the specification correctly, not the manual example.

### C Implementation Status

✅ **C is CORRECT** - nd500x/src/cpu/instructions/CONTROL/Init.c follows the specification correctly.

### YAML Status

⚠️ **YAML HAD ERROR** - docs/instructions/yaml/init.yaml originally had the same reversed operands as the manual example. **Fixed on 2025-11-14**.

### ASM Documentation Status

✅ **ASM examples are CORRECT** - docs/asm/INIT* files have correct operand order.

### Impact

**Low** - The operand definition and operation description are clear and unambiguous. Only the manual example is wrong. Both C and C# implementations are correct.

### References

- ND-500 Reference Manual, Page 229: INIT instruction specification
- RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CONTROL/Init.cs
- nd500x/src/cpu/instructions/CONTROL/Init.c
- nd500x/docs/INIT_INSTRUCTION_ISSUES.md (detailed analysis)

---

## Issue Template

When adding new bugs, use this template:

```markdown
## Issue #N: [Instruction Name] - [Brief Description]

**Date Found**: YYYY-MM-DD
**Severity**: Low | Medium | High | Critical
**Status**: Pending | Fixed in C# | Fixed in C Only | Documented | Won't Fix

### Description

[Detailed description of the bug]

### Expected Behavior

[What the specification says should happen]

### Actual Behavior in C#

[What the C# code actually does]

### Root Cause

[Analysis of why the bug exists]

### Fix Applied

- [ ] C# RetroCore fixed
- [ ] C implementation fixed
- [ ] YAML spec updated
- [ ] ASM documentation updated
- [ ] Pull request submitted to RetroCore

### Impact

[Assessment of impact: Low/Medium/High]

### References

- ND-500 Reference Manual, Page X
- YAML: docs/instructions/yaml/instr.yaml
- ASM: docs/asm/INSTR*
- C#: RetroCore/path/to/Instr.cs
- C: nd500x/path/to/Instr.c
```

---

**Last Updated**: 2025-11-15
