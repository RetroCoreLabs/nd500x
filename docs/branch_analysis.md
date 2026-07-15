# Branch Instruction Analysis Report

Analysis of trace file `/mnt/d/nd-trace-500x.txt` containing 6,429 instructions.

## Summary

| Metric | Value |
|--------|-------|
| Total branch occurrences | 741 |
| Unique branch PCs | 161 |
| Bugs found | 1 (fixed) |

**Result: Found and fixed carry flag bug in COMP instruction.**

## ND-500 Carry Flag Convention

From ND-500 Reference Manual (ND-05.009.4 EN, Section 10.10):

> **Data status bits:**
> - carry from most significant bit -> C

For `COMP2 op1, op2` (performs op1 - op2):

| Condition | C Flag | Meaning |
|-----------|--------|---------|
| op1 >= op2 | C=1 | No borrow (carry out from MSB) |
| op1 < op2 | C=0 | Borrow occurred (no carry out) |

Implementation in `Comp2.c:59`:
```c
bool carry = (op1 >= op2);  // C=1 means NO borrow
```

## Branch Condition Reference

From ND-500 Reference Manual (Page 220):

| Instruction | Condition | Manual Name | Implementation |
|-------------|-----------|-------------|----------------|
| IF=GO | Z=1 | equal | `if (nd500_test_flag(cpu, ND500_FLAG_Z))` |
| IF><GO | Z=0 | unequal | `if (!nd500_test_flag(cpu, ND500_FLAG_Z))` |
| IF>GO | S=0 AND Z=0 | greater signed | `if (!S && !Z)` |
| IF<GO | S=1 | less signed | `if (nd500_test_flag(cpu, ND500_FLAG_S))` |
| IF>=GO | S=0 | greater or equal signed | `if (!nd500_test_flag(cpu, ND500_FLAG_S))` |
| IF<=GO | S=1 OR Z=1 | less or equal signed | `if (S \|\| Z)` |
| IF-KGO | K=0 | flag reset | `if (!nd500_test_flag(cpu, ND500_FLAG_K))` |
| IFKGO | K=1 | flag set | `if (nd500_test_flag(cpu, ND500_FLAG_K))` |
| IF>>GO | C=1 AND Z=0 | greater magnitude | `if (C && !Z)` |
| **IF>>=GO** | **C=1** | greater or equal magnitude | `if (nd500_test_flag(cpu, ND500_FLAG_C))` |
| **IF<<GO** | **C=0** | less magnitude | `if (!nd500_test_flag(cpu, ND500_FLAG_C))` |
| IF<<=GO | C=0 OR Z=1 | less or equal magnitude | `if (!C \|\| Z)` |

## Branch Types in Trace

| Mnemonic | Count | Condition | Verified |
|----------|-------|-----------|----------|
| if><go | 502 | Z=0 | CORRECT |
| if=go | 165 | Z=1 | CORRECT |
| go | 46 | Unconditional | CORRECT |
| if>=go | 8 | S=0 (signed) | CORRECT |
| if<go | 6 | S=1 (signed) | CORRECT |
| if>>=go | 5 | C=1 (unsigned) | CORRECT |
| if<<go | 5 | C=0 (unsigned) | CORRECT |
| if-kgo | 4 | K=0 | CORRECT |

## Detailed Verification of Unsigned Branches

Each unsigned branch was traced to verify correctness:

### IF>>=GO (branches when C=1)

| PC | Flags | C | Branch? | Next PC | Expected | Result |
|----|-------|---|---------|---------|----------|--------|
| 0x0802D48A | PdzsCko | SET | Yes | 0x0802D497 | branch | CORRECT |
| 0x0802D4A2 | PdzsCko | SET | Yes | 0x0802D4B1 | branch | CORRECT |
| 0x0802CD25 | PdzsCko | SET | Yes | 0x0802CD32 | branch | CORRECT |
| 0x0802CD4F | PdzsCko | SET | Yes | 0x0802CD5E | branch | CORRECT |
| 0x0802CA57 | Pdzscko | CLEAR | No | 0x0802CA59 | fall-thru | CORRECT |

### IF<<GO (branches when C=0)

| PC | Flags | C | Branch? | Next PC | Expected | Result |
|----|-------|---|---------|---------|----------|--------|
| 0x0802989E | PdZsCko | SET | No | 0x080298A1 | fall-thru | CORRECT |
| 0x08029ABC | PdzScko | CLEAR | Yes | 0x08029AD5 | branch | CORRECT |
| 0x08029E15 | PdzsCko | SET | No | 0x08029E18 | fall-thru | CORRECT |
| 0x08025A71 | PdzScko | CLEAR | Yes | 0x08025AC0 | branch | CORRECT |
| 0x0802CFBD | PdzsCko | SET | No | 0x0802CFC0 | fall-thru | CORRECT |

## Flag String Format

In trace output, flags are shown as `[PdzsCko]`:
- **Uppercase** = flag SET
- **Lowercase** = flag CLEAR

| Position | Flag | Set | Clear |
|----------|------|-----|-------|
| 1 | Privileged | P | p |
| 2 | PSD | D | d |
| 3 | Zero | Z | z |
| 4 | Sign | S | s |
| 5 | Carry | C | c |
| 6 | K flag | K | k |
| 7 | Overflow | O | o |

## Documentation Issue Found

The header documentation in the branch instruction C files has **incorrect comments** about the carry flag convention, but the **code implementation is correct**.

### Files with incorrect header comments:

1. `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfUnsignedGreaterEqualGo.c`
   - Header says: "C=0 (no borrow) means A >= B" - **WRONG**
   - Should say: "C=1 (no borrow) means A >= B"

2. `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfUnsignedLessGo.c`
   - Header says: "C=1 (borrow occurred) means A < B" - **WRONG**
   - Should say: "C=0 (borrow) means A < B"

**Note:** The code comments near the actual branch logic (lines 128-129) are CORRECT. Only the header documentation (lines 41-44) needs fixing.

## Conclusion

1. **All branch instructions execute correctly** - verified against ND-500 Reference Manual
2. **COMP2 correctly sets C=1 for no borrow (A >= B)**
3. **IF>>=GO correctly branches when C=1**
4. **IF<<GO correctly branches when C=0**
5. **Documentation fix needed** - header comments in unsigned branch files have reversed carry convention description
