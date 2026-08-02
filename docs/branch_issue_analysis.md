# Deep Analysis of 16 Potential Branch Issues

Generated from analysis of `$ND500X_WORK/nd-trace-500x.txt`

## Executive Summary

After deep analysis of all 16 flagged issues, **NO BUGS WERE FOUND** in the branch or flag-setting code. All branches behave correctly according to the ND-500 architecture. The issues flagged are expected behavior patterns:

- **Always-taken/never-taken branches**: Normal behavior for specific test data
- **Missing flag change lines**: Tracer only outputs `->` when flags actually change
- **Consistent behavior**: Code paths that always take/skip in this specific trace

---

## Issue Analysis

### Issue 1: 0x08029309 - NEVER_TAKEN (6x)
**Type:** `if=go` (branch if equal, Z=1)

**Code Pattern:**
```
h comp2      b.0x76,#0x40     ; Compare halfword at b.0x76 with 64
  -> [PdzScko]                 ; S=1 (negative result), Z=0 (not equal)
if=go        $0xB              ; Branch if Z=1
```

**Analysis:** Loop counter at b.0x76 is compared with 0x40 (64). In this trace, the counter never reaches 64, so Z=0 and branch is never taken. This is a loop termination check.

**Verdict:** CORRECT - Normal loop behavior

---

### Issue 2: 0x080293A4 - NEVER_TAKEN (5x)
**Type:** `if=go` (branch if equal, Z=1)

**Code Pattern:**
```
h comp2      b.0x72,#0x40     ; Compare halfword with 64
  -> [PdzScko]                 ; S=1, Z=0
if=go        $0xB
```

**Analysis:** Same pattern as Issue 1. Different loop variable, same limit (0x40).

**Verdict:** CORRECT - Normal loop behavior

---

### Issue 3: 0x0802B52B - NEVER_TAKEN (11x)
**Type:** `if=go` (branch if equal, Z=1)

**Code Pattern:**
```
h comp2      b.0x2E,$0x10     ; Compare halfword with 16
  -> [PdzScko]                 ; S=1, Z=0
if=go        $0xB
```

**Analysis:** Counter at b.0x2E never reaches 0x10 (16) in this trace. Loop runs 11 iterations without hitting the limit.

**Verdict:** CORRECT - Normal loop behavior

---

### Issue 4: 0x0802CF16 - ALWAYS_TAKEN (4x)
**Type:** `if><go` (branch if not equal, Z=0)

**Code Pattern:**
```
w3 :=        b.0x14           ; Load word from b.0x14
  -> I3=0000083A              ; I3 = 0x83A (non-zero)
w test       W3               ; Test W3 against zero
  -> [PdzsCko]                 ; Z=0 (not zero), C=1
if><go       $0x5             ; Branch if Z=0
```

**Analysis:** W3 is always loaded with non-zero values (0x83A, 0x1), so test sets Z=0 and branch is always taken.

**Verdict:** CORRECT - Non-null pointer check that always succeeds

---

### Issue 5: 0x0802D68D - NEVER_TAKEN (15x)
**Type:** `if><go` (branch if not equal, Z=0)

**Code Pattern:**
```
w1 :=        b.0x18           ; Load array base
  -> [PdzsCko]
w test       IND(b.0x14)(r1)  ; Test array[index]
  -> [PdZsCko]                 ; Z=1 (value is zero)
if><go       $0xC             ; Branch if Z=0
```

**Analysis:** Tests array elements that are all zero in this trace. The branch would be taken if a non-zero element is found, but none exist in the test data.

**Verdict:** CORRECT - Sparse array handling (no non-zero elements)

---

### Issue 6: 0x0802D93C - NEVER_TAKEN (10x)
**Type:** `if><go` (branch if not equal, Z=0)

**Code Pattern:**
```
w test       $0x801D7BC       ; Test fixed memory location
  -> [PdZsCko]                 ; Z=1 (value is zero)
if><go       $0x32            ; Branch if Z=0
```

**Analysis:** Memory at 0x801D7BC is always zero in this trace. This is likely a global flag or pointer that isn't set.

**Verdict:** CORRECT - Uninitialized/null global check

---

### Issue 7: 0x0802D947 - NEVER_TAKEN (9x)
**Type:** `if=go` (branch if equal, Z=1)

**Code Pattern:**
```
w comp2      b.0xC,$-0x1      ; Compare with -1 (0xFFFFFFFF)
  -> [Pdzscko]                 ; Z=0 (not equal)
if=go        $0x27            ; Branch if Z=1
```

**Analysis:** Value at b.0xC is never -1 in this trace. This is a sentinel value check that never triggers.

**Verdict:** CORRECT - End-of-list sentinel check

---

### Issue 8: 0x0802D94F - NEVER_TAKEN (9x)
**Type:** `if><go` (branch if not equal, Z=0)

**Code Pattern:**
```
w1 :=        b.0xC            ; Load pointer
  -> I1=0801DCE8
w test       r1.0x4           ; Test pointer+4 (e.g., ->next field)
  -> [PdZsCko]                 ; Z=1 (null)
if><go       $0xB             ; Branch if Z=0
```

**Analysis:** Field at offset 4 of structure is always null. Likely a "has child" or "next" pointer check.

**Verdict:** CORRECT - Linked list traversal (no children/next)

---

### Issue 9: 0x0802D957 - ALWAYS_TAKEN (9x)
**Type:** `if=go` (branch if equal, Z=1)

**Code Pattern:**
```
h test       r1.0x8           ; Test halfword at r1+8
  -> [PdZsCko]                 ; Z=1 (zero)
if=go        $0x17            ; Branch if Z=1
```

**Analysis:** Halfword at r1+8 is always zero. Branch always taken to skip processing.

**Verdict:** CORRECT - Zero-field skip optimization

---

### Issue 10: 0x0802D976 - ALWAYS_TAKEN (9x)
**Type:** `if><go` (branch if not equal, Z=0)

**Code Pattern:**
```
w comp2      b.0xC,$-0x1      ; Compare with -1
  -> [Pdzscko]                 ; Z=0 (not -1)
if><go       $0x1A            ; Branch if Z=0
```

**Analysis:** Value at b.0xC is never -1, so branch always taken. This is the inverse of Issue 7 (skip if NOT sentinel).

**Verdict:** CORRECT - Non-sentinel path always taken

---

### Issue 11: 0x0802D9A0 - ALWAYS_TAKEN (9x)
**Type:** `if><go` (branch if not equal, Z=0)

**Code Pattern:**
```
w1 =:        $0x801DC8C       ; Store W1 to memory
w comp2      $0x801DC8C,$-0x1 ; Compare memory with -1
                              ; (no -> line, flags unchanged from previous)
if><go       $0x5             ; Branch if Z=0
```

**Analysis:** The comp2 instruction has no flag change output because the flags are already `[Pdzscko]` (Z=0) from a previous instruction, and comp2 produces the same flags. The tracer only outputs changes.

**NOTE:** This pattern is suspicious - the comp2 SHOULD change flags, but if the result happens to be the same, no output is shown. The value at 0x801DC8C is never -1.

**Verdict:** CORRECT - Tracer optimization (same flags = no output)

---

### Issue 12: 0x0802D9AB - ALWAYS_TAKEN (9x)
**Type:** `if><go` (branch if not equal, Z=0)

**Code Pattern:**
```
w test       $0x801DC8C       ; Test memory
  -> [PdzsCko]                 ; Z=0 (non-zero), C=1
if><go       $0x6             ; Branch if Z=0
```

**Analysis:** Memory at 0x801DC8C is always non-zero (contains valid pointer/value).

**Verdict:** CORRECT - Non-null check always succeeds

---

### Issue 13: 0x0802DA87 - ALWAYS_TAKEN (8x)
**Type:** `if><go` (branch if not equal, Z=0)

**Code Pattern:**
```
w test       b.0xC            ; Test local variable
                              ; (no flag change - already Z=0)
if=go        $0x9             ; First branch: if Z=1
w test       IND(b.0xC)       ; Test indirect
                              ; (no flag change)
if><go       $0x1F            ; Branch if Z=0
```

**Analysis:** Both tests result in Z=0 (non-zero values). The tracer shows flags as `[PdzsCko]` which has Z=0, so if><go is always taken.

**Verdict:** CORRECT - Non-null indirect check

---

### Issue 14: 0x0802EC4E - ALWAYS_TAKEN (6x)
**Type:** `if>=go` (branch if signed greater-or-equal, S=0)

**Code Pattern:**
```
w2 -         b.0x18           ; Subtract from W2
  -> I2=00000009 [PdzsCko]    ; Result positive, C=1
by1 =:       IND(b.0x14)(r2)  ; Store byte
w test       b.0x24           ; Test local
  -> [PdZsCko]                 ; Z=1 (zero)
if>=go       $0x2E            ; Branch if S=0
```

**Analysis:** The test at b.0x24 sets Z=1, S=0 (zero is non-negative). The `if>=go` checks S=0, which is true.

**Verdict:** CORRECT - Non-negative check on zero value

---

### Issue 15: 0x0802EC97 - NEVER_TAKEN (6x)
**Type:** `if><go` (branch if not equal, Z=0)

**Code Pattern:**
```
w test       b.0x24           ; Test local
                              ; (flags from previous: [PdZsCko], Z=1)
if><go       $0xE8            ; Branch if Z=0
```

**Analysis:** Value at b.0x24 is always zero (Z=1), so `if><go` (requires Z=0) is never taken.

**Verdict:** CORRECT - Dead code path for non-zero case

---

### Issue 16: 0x0802EC9E - ALWAYS_TAKEN (6x)
**Type:** `if=go` (branch if equal, Z=1)

**Code Pattern:**
```
by comp2     b.0x2F,#0x20     ; Compare byte with 0x20 (space)
                              ; (no flag change - Z already 1)
if=go        $0xD             ; Branch if Z=1
```

**Analysis:** Byte at b.0x2F equals 0x20 (space character). Z=1 from previous test, and comp2 with equal values also sets Z=1.

**Verdict:** CORRECT - Space character detection

---

## Implementation Verification

### comp2 Implementation (Comp2.c)
Reviewed at `src/cpu/instructions/COMPARE/Comp2.c`:
- Correctly performs `op1 - op2`
- Sets Z=1 if result is zero (operands equal)
- Sets C=1 if no borrow (op1 >= op2 unsigned)
- Sets S = sign_bit XOR overflow (proper signed comparison)

### test Implementation (Test.c)
Reviewed at `src/cpu/instructions/COMPARE/Test.c`:
- Correctly compares operand against implicit zero
- Sets Z=1 if operand is zero
- Sets S=1 if operand sign bit is set
- Sets C=1 always for integer types

---

## Tracer Behavior Note

The tracer only outputs `->` lines when registers or flags CHANGE. This means:
- If an instruction sets flags to the same values they already have, no output is shown
- This can appear as "missing" flag changes but is correct behavior
- The flags shown on the NEXT instruction line are the current state

---

## Conclusion

All 16 flagged issues are **FALSE POSITIVES**. The branch instructions and their preceding flag-setting instructions work correctly. The patterns observed are:

1. **Loop counters not reaching limits** (Issues 1, 2, 3)
2. **Null/zero pointer checks succeeding** (Issues 4, 12, 13)
3. **Array elements all zero** (Issue 5)
4. **Global flags unset** (Issue 6)
5. **Sentinel values not found** (Issues 7, 10)
6. **Linked list fields null** (Issues 8, 9)
7. **Non-negative zero values** (Issues 14, 15)
8. **Character comparisons matching** (Issue 16)

The emulator's branch and comparison logic is **CORRECT**.
