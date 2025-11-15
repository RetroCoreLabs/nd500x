# INIT Instruction - Issues Found During Implementation

## Issue 1: ND-500 Reference Manual Example Error (Page 229)

### What the Manual Says

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

### The Problem with the Example

**Manual Example** (Page 229, line 7789):
```assembly
INIT FRAME, 010000H, 01000H
```

**Manual Description** (line 7787):
> Initialize a new stack at FRAME, requiring 010000H stack locations for the system, 01000H for the main program

### Why This is WRONG

Given the example:
- `operand[0]` = FRAME (bottom of stack)
- `operand[1]` = 0x10000 (described as "for the system")
- `operand[2]` = 0x1000 (described as "for the main program")

But the operand definition says:
- `operand[1]` = **<stack demand of main program>**
- `operand[2]` = **<total system stack demand>**

The description contradicts the operand definition!

Furthermore, this would trigger the overflow trap:
```
stack_demand_main (0x10000) >= total_stack_demand (0x1000) → TRAP!
```

### The Correct Example Should Be

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

### Impact

**Low** - The operand definition and operation description are clear and unambiguous. Only the example is wrong. Both C and C# implementations follow the correct specification from the operation description.

---

## Issue 2: C# Implementation Has Extra Argument Handling

### What the Manual Specifies

The manual **ONLY** specifies these operations for INIT:

```
<<bottom of stack>> → B
<<bottom of stack>> + <total system stack demand> → TOS
<<bottom of stack>> + <stack demand of main program> → B.SP
0 → B.PREVB
0 → B.RETA → L
```

**No mention of B.N or argument addresses.**

### What the C# Implementation Does

**RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CONTROL/Init.cs** (lines 111-124):

```csharp
// STEP 5: If called with arguments (unusual for INIT), copy them
// Normally INIT is executed directly, not via CALL
uint argCount = cpu.PendingCallArgCount;
WriteMemory(regs.B + OFFSET_N, argCount, 4);

for (uint i = 0; i < argCount && i < 256; i++)
{
    uint argAddr = cpu.PendingCallArgAddresses[i];
    WriteMemory(regs.B + OFFSET_ARG1 + (i * 4), argAddr, 4);
}

// Clear pending call state
cpu.PendingCallReturnAddress = 0;
cpu.PendingCallArgCount = 0;
```

### Analysis

**Why this is technically incorrect:**

1. INIT is executed at **program startup** BEFORE any CALL instruction
2. At startup, `PendingCallArgCount` should be 0 (no CALL has executed yet)
3. The manual specification does NOT mention initializing B.N or argument addresses
4. This is different from ENTM, where the manual explicitly lists argument handling

**Why this doesn't cause bugs:**

1. If `PendingCallArgCount == 0`, the loop doesn't execute (harmless)
2. The code correctly clears the pending call state afterward
3. In practice, INIT is never preceded by CALL, so this code path is never taken with arguments

**Comparison with ENTM:**

ENTM (Page 231) explicitly lists in its operation:
```
number of arguments → B.N
addresses of arguments → B.arg
```

But INIT does NOT list this.

### Recommendation

**For C Implementation:**
- ✅ Follow manual specification strictly (no argument handling)
- ✅ Document this difference from C# as implementation note

**For C# Implementation:**
- ⚠️ Consider removing lines 111-124 to match manual spec
- ⚠️ OR add comment explaining this is defensive programming
- ⚠️ OR confirm if this is intentional behavior from Norsk Data engineers

### Impact

**Very Low** - In practice, this code is never executed with `argCount > 0` because INIT is never preceded by CALL. It's harmless defensive code, but not per spec.

---

## Issue 3: Missing B.AUX Initialization?

### What Other Entry Points Do

Looking at ENTS (Page 230):
```
B.AUX is initialized (offset 12)
```

### What INIT Specifies

The manual for INIT does NOT mention B.AUX (offset 12).

### Analysis

Since INIT only specifies:
- B.PREVB (offset 0) = 0
- B.RETA (offset 4) = 0
- B.SP (offset 8) = bottom + stack_demand_main

Offset 12 (B.AUX) is left **uninitialized** per spec.

However, looking at the stack frame layout (Chapter 3), B.AUX is part of the standard frame:
```
B+0  (PREVB)  Previous B register value
B+4  (RETA)   Return address
B+8  (SP)     Stack pointer
B+12 (AUX)    Auxiliary field  ← Not mentioned in INIT spec!
B+16 (N)      Argument count
B+20+        Argument addresses
```

### Recommendation

**Current Implementation:**
- Both C and C# do NOT initialize B.AUX (following manual spec)

**Possible Enhancement:**
- Consider initializing B.AUX = 0 for cleanliness
- Would match ENTS behavior (see Ents.cs line 118)
- But this goes beyond manual specification

### Impact

**Very Low** - B.AUX is only used by language processors and buddy subroutines. For initial stack frame created by INIT, it's unlikely to matter.

---

## Summary of Issues

| Issue | Severity | Component | Status |
|-------|----------|-----------|--------|
| Manual example has reversed operands | Low | ND-500 Manual Page 229 | Documented |
| C# copies arguments (not in spec) | Very Low | RetroCore Init.cs | Harmless, but not per spec |
| B.AUX not initialized | Very Low | Both implementations | Per spec, but could be cleaner |

## Implementations Status

| Implementation | Correctness | Notes |
|----------------|-------------|-------|
| **C (nd500x)** | ✅ **100% per spec** | Strictly follows manual operation description |
| **C# (RetroCore)** | ✅ **99% correct** | Extra argument handling not in spec (harmless) |

Both implementations correctly:
- ✅ Set B, TOS, B.SP per specification
- ✅ Initialize B.PREVB = 0, B.RETA = 0, L = 0
- ✅ Check stack overflow condition (main >= total)
- ✅ Trap on overflow with STO

---

## References

- ND-500 Reference Manual, Page 229: INIT instruction specification
- ND-500 Reference Manual, Page 230-231: ENTM instruction (for comparison)
- RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CONTROL/Init.cs
- nd500x/src/cpu/instructions/CONTROL/Init.c

## Date

2025-11-14
