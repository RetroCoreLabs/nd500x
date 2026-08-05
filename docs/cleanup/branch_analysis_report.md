# Branch Instruction Analysis Report

Generated from ND-500 execution trace analysis.

## Summary

- **Total unique branch locations:** 133
- **Total branch occurrences:** 695
- **Branches always taken:** 67
- **Branches never taken:** 41
- **Branches with varying outcomes:** 25
- **Potential issues detected:** 16

## Branch Type Distribution

| Branch Type | Occurrences | Condition |
|-------------|-------------|-----------|
| if><go | 502 | Not Equal (Z=0) |
| if=go | 165 | Equal (Z=1) |
| if>=go | 8 | Signed >= (S=0) |
| if<go | 6 | Signed < (S=1) |
| if>>=go | 5 | Unsigned >= (C=1) |
| if<<go | 5 | Unsigned < (C=0) |
| if-kgo | 4 | K Clear (K=0) |

## Branch Details by PC Address

### 0x08025A08: if><go

**Operand:** `$0xB8`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3925 | `h comp2      $0x801AF5E,#0xFFFF` | `[PdzsCko]` | `[PdZsCko]` | No |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

### 0x08025A51: if><go

**Operand:** `$0x17`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3960 | `h comp2      $0x801AF4E,$0x3` | `[PdzsCko]` | `[PdzScko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x08025A71: if<<go

**Operand:** `$0x4F`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3962 | `h comp2      $0x801AF56,#0x40` | `[PdzScko]` | (unchanged) | Yes |

**Analysis:** C=0 (Unsigned < requires C=0) -> TAKEN

**Potential Issues:**
- Line 3962: Preceding instruction 'h' has no flag change recorded

### 0x080292C1: if><go

**Operand:** `$0x14`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3113 | `w test       IND(b.0x14) ; 0x800` | `[PdzsCko]` | (unchanged) | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 3113: Preceding instruction 'w' has no flag change recorded

### 0x080292EA: if><go

**Operand:** `$0xF1`
**Occurrences:** 64
**Status:** Taken 63/64 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3122 | `h comp2      b.0x76,#0x3F` | `[PdZsCko]` | `[PdzScko]` | Yes |
| 3131 | `h comp2      b.0x76,#0x3F` | `[PdZscko]` | `[PdzScko]` | Yes |
| 3140 | `h comp2      b.0x76,#0x3F` | `[PdZscko]` | `[PdzScko]` | Yes |
| 3149 | `h comp2      b.0x76,#0x3F` | `[PdZscko]` | `[PdzScko]` | Yes |
| 3158 | `h comp2      b.0x76,#0x3F` | `[PdZscko]` | `[PdzScko]` | Yes |
| ... | (59 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x080292EE: if=go

**Operand:** `$0x29`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3692 | `w test       b.0x28` | `[PdZsCko]` | `[PdzsCko]` | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x08029309: if=go

**Operand:** `$0xB`
**Occurrences:** 6
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3707 | `h comp2      b.0x76,#0x40` | `[Pdzscko]` | `[PdzScko]` | No |
| 3720 | `h comp2      b.0x76,#0x40` | `[Pdzscko]` | `[PdzScko]` | No |
| 3733 | `h comp2      b.0x76,#0x40` | `[Pdzscko]` | `[PdzScko]` | No |
| 3746 | `h comp2      b.0x76,#0x40` | `[Pdzscko]` | `[PdzScko]` | No |
| 3759 | `h comp2      b.0x76,#0x40` | `[Pdzscko]` | `[PdzScko]` | No |
| ... | (1 more occurrences) | | | |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x08029312: if><go

**Operand:** `$0xE2`
**Occurrences:** 6
**Status:** Taken 5/6 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3709 | `h comp2      b.0x76,b.0x26` | `[PdzScko]` | (unchanged) | Yes |
| 3722 | `h comp2      b.0x76,b.0x26` | `[PdzScko]` | (unchanged) | Yes |
| 3735 | `h comp2      b.0x76,b.0x26` | `[PdzScko]` | (unchanged) | Yes |
| 3748 | `h comp2      b.0x76,b.0x26` | `[PdzScko]` | (unchanged) | Yes |
| 3761 | `h comp2      b.0x76,b.0x26` | `[PdzScko]` | (unchanged) | Yes |
| ... | (1 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 3709: Preceding instruction 'h' has no flag change recorded
- Line 3722: Preceding instruction 'h' has no flag change recorded
- Line 3735: Preceding instruction 'h' has no flag change recorded

### 0x0802935C: if><go

**Operand:** `$0x14`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1877 | `w test       IND(b.0x14) ; 0x800` | `[PdzsCko]` | (unchanged) | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 1877: Preceding instruction 'w' has no flag change recorded

### 0x08029385: if><go

**Operand:** `$0xF1`
**Occurrences:** 64
**Status:** Taken 63/64 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1886 | `h comp2      b.0x72,#0x3F` | `[PdZsCko]` | `[PdzScko]` | Yes |
| 1895 | `h comp2      b.0x72,#0x3F` | `[PdZscko]` | `[PdzScko]` | Yes |
| 1904 | `h comp2      b.0x72,#0x3F` | `[PdZscko]` | `[PdzScko]` | Yes |
| 1913 | `h comp2      b.0x72,#0x3F` | `[PdZscko]` | `[PdzScko]` | Yes |
| 1922 | `h comp2      b.0x72,#0x3F` | `[PdZscko]` | `[PdzScko]` | Yes |
| ... | (59 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x08029389: if=go

**Operand:** `$0x29`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2456 | `w test       b.0x28` | `[PdZsCko]` | `[PdzsCko]` | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x080293A4: if=go

**Operand:** `$0xB`
**Occurrences:** 5
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2471 | `h comp2      b.0x72,#0x40` | `[Pdzscko]` | `[PdzScko]` | No |
| 2484 | `h comp2      b.0x72,#0x40` | `[Pdzscko]` | `[PdzScko]` | No |
| 2497 | `h comp2      b.0x72,#0x40` | `[Pdzscko]` | `[PdzScko]` | No |
| 2510 | `h comp2      b.0x72,#0x40` | `[Pdzscko]` | `[PdzScko]` | No |
| 2523 | `h comp2      b.0x72,#0x40` | `[Pdzscko]` | `[PdzScko]` | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x080293AD: if><go

**Operand:** `$0xE2`
**Occurrences:** 5
**Status:** Taken 4/5 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2473 | `h comp2      b.0x72,b.0x26` | `[PdzScko]` | (unchanged) | Yes |
| 2486 | `h comp2      b.0x72,b.0x26` | `[PdzScko]` | (unchanged) | Yes |
| 2499 | `h comp2      b.0x72,b.0x26` | `[PdzScko]` | (unchanged) | Yes |
| 2512 | `h comp2      b.0x72,b.0x26` | `[PdzScko]` | (unchanged) | Yes |
| 2526 | `h comp2      b.0x72,b.0x26` | `[PdzScko]` | `[PdZsCko]` | No |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 2473: Preceding instruction 'h' has no flag change recorded
- Line 2486: Preceding instruction 'h' has no flag change recorded
- Line 2499: Preceding instruction 'h' has no flag change recorded

### 0x08029476: if=go

**Operand:** `$0x5`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5226 | `bi test      W1` | `[PdZsCko]` | (unchanged) | Yes |
| 5399 | `bi test      W1` | `[PdZsCko]` | (unchanged) | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

**Potential Issues:**
- Line 5226: Preceding instruction 'bi' has no flag change recorded
- Line 5399: Preceding instruction 'bi' has no flag change recorded

### 0x0802947E: if=go

**Operand:** `$0x22`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5228 | `by test      b.0x17` | `[PdZsCko]` | (unchanged) | Yes |
| 5401 | `by test      b.0x17` | `[PdZsCko]` | (unchanged) | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

**Potential Issues:**
- Line 5228: Preceding instruction 'by' has no flag change recorded
- Line 5401: Preceding instruction 'by' has no flag change recorded

### 0x080294AE: if><go

**Operand:** `$0x8`
**Occurrences:** 2
**Status:** Taken 1/2 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5380 | `w comp2      r1.0x1F,b.0x1C` | `[PdzsCko]` | `[PdzScko]` | Yes |
| 5489 | `w comp2      r1.0x1F,b.0x1C` | `[PdzsCko]` | `[PdZsCko]` | No |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x080294C0: if=go

**Operand:** `$0x14`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5388 | `by test      b.0x17` | `[Pdzscko]` | `[PdZsCko]` | Yes |
| 5493 | `by test      b.0x17` | `[PdZsCko]` | (unchanged) | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

**Potential Issues:**
- Line 5493: Preceding instruction 'by' has no flag change recorded

### 0x080294D6: if><go

**Operand:** `$0x95`
**Occurrences:** 3
**Status:** Taken 2/3 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5218 | `w test       b.0x1C` | `[PdzsCko]` | (unchanged) | Yes |
| 5391 | `w test       b.0x1C` | `[PdZsCko]` | `[PdzsCko]` | Yes |
| 5495 | `w test       b.0x1C` | `[PdZsCko]` | (unchanged) | No |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 5218: Preceding instruction 'w' has no flag change recorded
- Line 5495: Preceding instruction 'w' has no flag change recorded

### 0x0802960D: if><go

**Operand:** `$0xE`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2545 | `by comp2     b.0x1B,$0x2` | `[PdzsCko]` | `[PdzScko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x08029629: if=go

**Operand:** `$0x1AE`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2552 | `by comp2     r1.0x25,$0x6` | `[Pdzscko]` | `[PdZsCko]` | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

### 0x080297E2: if=go

**Operand:** `$0xF`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2558 | `bi test      W1` | `[PdZsCko]` | (unchanged) | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

**Potential Issues:**
- Line 2558: Preceding instruction 'bi' has no flag change recorded

### 0x0802980F: if=go

**Operand:** `$0x25F`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2577 | `h2 comp      W1` | `[PdZsCko]` | `[PdzSCko]` | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x08029818: if><go

**Operand:** `$0xD`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2589 | `bi test      b.0x25` | `[Pdzscko]` | `[PdzsCko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802983E: if><go

**Operand:** `$0x57`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2604 | `h comp2      r1.0x23,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802989E: if<<go

**Operand:** `$0xBE`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2608 | `h comp2      $0x1,r1.0x27` | `[Pdzscko]` | `[PdZsCko]` | No |

**Analysis:** C=1 (Unsigned < requires C=0) -> NOT TAKEN

### 0x080298BF: if><go

**Operand:** `$0x2D`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2652 | `h comp2      b.0xCE,$0x1` | `[Pdzscko]` | `[PdzScko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x080298F2: if<go

**Operand:** `$0x3F`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2654 | `h comp2      b.0xCE,#0x40` | `[PdzScko]` | (unchanged) | Yes |

**Analysis:** S=1 (Signed < requires S=1) -> TAKEN

**Potential Issues:**
- Line 2654: Preceding instruction 'h' has no flag change recorded

### 0x08029937: if=go

**Operand:** `$0x14`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2658 | `bi test      W1` | `[PdzScko]` | `[PdzsCko]` | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x08029A7A: if><go

**Operand:** `$0xF`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2676 | `h2 comp      W1` | `[PdZscko]` | `[PdzSCko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x08029A9B: if=go

**Operand:** `$0x3A`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2685 | `by comp2     r1.0x25,$0x2` | `[PdzsCko]` | (unchanged) | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

**Potential Issues:**
- Line 2685: Preceding instruction 'by' has no flag change recorded

### 0x08029AA4: if=go

**Operand:** `$0x31`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2689 | `bi test      W1` | `[PdzsCko]` | (unchanged) | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

**Potential Issues:**
- Line 2689: Preceding instruction 'bi' has no flag change recorded

### 0x08029AAF: if=go

**Operand:** `$0x1E`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2694 | `by comp2     r1.0x26,$0x7` | `[PdzsCko]` | `[PdzScko]` | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x08029ABC: if<<go

**Operand:** `$0x19`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2699 | `h comp2      r1.0x27,#0x40` | `[Pdzscko]` | `[PdzScko]` | Yes |

**Analysis:** C=0 (Unsigned < requires C=0) -> TAKEN

### 0x08029BAA: if><go

**Operand:** `$0xD`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3796 | `by comp2     b.0x1F,#0xFF` | `[PdZsCko]` | `[Pdzscko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x08029BBA: if><go

**Operand:** `$0x5`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3798 | `w comp2      b.0x24,$-0x1` | `[Pdzscko]` | (unchanged) | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 3798: Preceding instruction 'w' has no flag change recorded

### 0x08029BC3: if><go

**Operand:** `$0xE`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3801 | `by comp2     b.0x1B,$0x2` | `[Pdzscko]` | `[PdzScko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x08029BDD: if=go

**Operand:** `$0x17A`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3808 | `by comp2     r1.0x25,$0x6` | `[Pdzscko]` | `[PdZsCko]` | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

### 0x08029D61: if=go

**Operand:** `$0xF`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3814 | `bi test      W1` | `[PdZsCko]` | (unchanged) | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

**Potential Issues:**
- Line 3814: Preceding instruction 'bi' has no flag change recorded

### 0x08029D8D: if=go

**Operand:** `$0x173`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3833 | `h2 comp      W1` | `[PdZsCko]` | `[PdzSCko]` | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x08029D96: if><go

**Operand:** `$0xD`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3845 | `bi test      b.0x29` | `[Pdzscko]` | `[PdzsCko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x08029DBB: if><go

**Operand:** `$0x52`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3860 | `h comp2      r1.0x23,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x08029E15: if<<go

**Operand:** `$0x11`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3864 | `h comp2      $0x2,r1.0x27` | `[Pdzscko]` | `[PdzsCko]` | No |

**Analysis:** C=1 (Unsigned < requires C=0) -> NOT TAKEN

### 0x08029E1B: if><go

**Operand:** `$0xB`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3867 | `by test      b.0x3B` | `[PdzsCko]` | `[PdZsCko]` | No |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

### 0x0802A094: if><go

**Operand:** `$0x3B`
**Occurrences:** 2
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1085 | `w test       IND(b.0x14) ; 0x800` | `[PdZscko]` | `[PdZsCko]` | No |
| 2742 | `w test       IND(b.0x14) ; 0x800` | `[PdZscko]` | `[PdZsCko]` | No |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

### 0x0802A09D: if><go

**Operand:** `$0x16`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1089 | `w test       b.0x40` | `[PdZsCko]` | `[PdzsCko]` | Yes |
| 2746 | `w test       b.0x40` | `[PdZsCko]` | `[PdzsCko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802A0C7: if=go

**Operand:** `$0x8`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1727 | `w test       b.0xB4` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| 2968 | `w test       b.0xB4` | `[PdzsCko]` | `[PdZsCko]` | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

### 0x0802A0E9: if=go

**Operand:** `$0xE`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1739 | `by comp2     r1.0x25,$0x2` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| 2980 | `by comp2     r1.0x25,$0x2` | `[PdzsCko]` | `[PdZsCko]` | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

### 0x0802A10A: if><go

**Operand:** `$0x2C`
**Occurrences:** 2
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1750 | `w test       r1.0x1F` | `[PdzsCko]` | `[PdZsCko]` | No |
| 2991 | `w test       r1.0x1F` | `[PdzsCko]` | `[PdZsCko]` | No |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

### 0x0802A120: if><go

**Operand:** `$0xD`
**Occurrences:** 2
**Status:** Taken 1/2 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1758 | `w test       $0x801CE00` | `[PdZsCko]` | (unchanged) | No |
| 2997 | `w test       $0x801CE00` | `[PdzsCko]` | (unchanged) | Yes |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

**Potential Issues:**
- Line 1758: Preceding instruction 'w' has no flag change recorded
- Line 2997: Preceding instruction 'w' has no flag change recorded

### 0x0802A156: if><go

**Operand:** `$0x22`
**Occurrences:** 2
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1781 | `h comp2      r1.0x14,$0x1` | `[PdzsCko]` | `[PdZsCko]` | No |
| 3017 | `h comp2      r1.0x14,$0x1` | `[PdzsCko]` | `[PdZsCko]` | No |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

### 0x0802A18E: if=go

**Operand:** `$0x3F1`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1802 | `by test      b.0x1B` | `[PdZscko]` | `[PdZsCko]` | Yes |
| 3038 | `by test      b.0x1B` | `[PdZscko]` | `[PdZsCko]` | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

### 0x0802A587: if><go

**Operand:** `$0xF`
**Occurrences:** 2
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1808 | `w test       b.0x2C` | `[PdzsCko]` | `[PdZsCko]` | No |
| 3044 | `w test       b.0x2C` | `[PdzsCko]` | `[PdZsCko]` | No |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

### 0x0802A5B0: if<go

**Operand:** `$0x17`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1821 | `w comp2      $0x801CEC0,#0x40` | `[PdzsCko]` | `[PdzScko]` | Yes |
| 3057 | `w comp2      $0x801CEC0,#0x40` | `[PdzsCko]` | `[PdzScko]` | Yes |

**Analysis:** S=1 (Signed < requires S=1) -> TAKEN

### 0x0802A684: if><go

**Operand:** `$0xF`
**Occurrences:** 2
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1838 | `h test       b.0x56` | `[PdzScko]` | `[PdZsCko]` | No |
| 3074 | `h test       b.0x56` | `[PdzScko]` | `[PdZsCko]` | No |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

### 0x0802A68A: if><go

**Operand:** `$0x9`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1841 | `bi test      b.0x49` | `[PdZsCko]` | `[PdzsCko]` | Yes |
| 3077 | `bi test      b.0x49` | `[PdZsCko]` | `[PdzsCko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802A69F: if=go

**Operand:** `$0x5`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1845 | `w test       b.0xB0` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| 3081 | `w test       b.0xB0` | `[PdzsCko]` | `[PdZsCko]` | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

### 0x0802A6A7: if=go

**Operand:** `$0x5`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1847 | `w test       b.0xA8` | `[PdZsCko]` | (unchanged) | Yes |
| 3083 | `w test       b.0xA8` | `[PdZsCko]` | (unchanged) | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

**Potential Issues:**
- Line 1847: Preceding instruction 'w' has no flag change recorded
- Line 3083: Preceding instruction 'w' has no flag change recorded

### 0x0802A6AF: if=go

**Operand:** `$0x5`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1849 | `w test       b.0xAC` | `[PdZsCko]` | (unchanged) | Yes |
| 3085 | `w test       b.0xAC` | `[PdZsCko]` | (unchanged) | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

**Potential Issues:**
- Line 1849: Preceding instruction 'w' has no flag change recorded
- Line 3085: Preceding instruction 'w' has no flag change recorded

### 0x0802A6B6: if=go

**Operand:** `$0xA`
**Occurrences:** 2
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1852 | `w test       b.0x34` | `[PdZsCko]` | `[PdzsCko]` | No |
| 3088 | `w test       b.0x34` | `[PdZsCko]` | `[PdzsCko]` | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x0802A6C3: if=go

**Operand:** `$0x87`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1857 | `h test       b.0x56` | `[PdZsCko]` | (unchanged) | Yes |
| 3093 | `h test       b.0x56` | `[PdZsCko]` | (unchanged) | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

**Potential Issues:**
- Line 1857: Preceding instruction 'h' has no flag change recorded
- Line 3093: Preceding instruction 'h' has no flag change recorded

### 0x0802B03C: if=go

**Operand:** `$0x12B`
**Occurrences:** 2
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5237 | `w test       b.0x14` | `[PdzsCko]` | (unchanged) | No |
| 5409 | `w test       b.0x14` | `[PdzsCko]` | (unchanged) | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

**Potential Issues:**
- Line 5237: Preceding instruction 'w' has no flag change recorded
- Line 5409: Preceding instruction 'w' has no flag change recorded

### 0x0802B049: if=go

**Operand:** `$0x105`
**Occurrences:** 2
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5243 | `by comp2     r1.0x25,$0x2` | `[PdzsCko]` | (unchanged) | No |
| 5415 | `by comp2     r1.0x25,$0x2` | `[PdzsCko]` | (unchanged) | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

**Potential Issues:**
- Line 5243: Preceding instruction 'by' has no flag change recorded
- Line 5415: Preceding instruction 'by' has no flag change recorded

### 0x0802B057: if=go

**Operand:** `$0xF7`
**Occurrences:** 2
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5250 | `by test      W1` | `[PdzSCko]` | (unchanged) | No |
| 5422 | `by test      W1` | `[PdzsCko]` | (unchanged) | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

**Potential Issues:**
- Line 5250: Preceding instruction 'by' has no flag change recorded
- Line 5422: Preceding instruction 'by' has no flag change recorded

### 0x0802B05F: if=go

**Operand:** `$0xEF`
**Occurrences:** 2
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5254 | `by test      r1.0x26` | `[PdzsCko]` | (unchanged) | No |
| 5426 | `by test      r1.0x26` | `[PdzsCko]` | (unchanged) | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

**Potential Issues:**
- Line 5254: Preceding instruction 'by' has no flag change recorded
- Line 5426: Preceding instruction 'by' has no flag change recorded

### 0x0802B06B: if=go

**Operand:** `$0xB`
**Occurrences:** 2
**Status:** Taken 1/2 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5259 | `bi test      W1` | `[PdzsCko]` | (unchanged) | No |
| 5431 | `bi test      W1` | `[PdZsCko]` | (unchanged) | Yes |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

**Potential Issues:**
- Line 5259: Preceding instruction 'bi' has no flag change recorded
- Line 5431: Preceding instruction 'bi' has no flag change recorded

### 0x0802B07D: if><go

**Operand:** `$0x4E`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5322 | `h comp2      r1.0x23,$-0x1` | `[Pdzscko]` | (unchanged) | Yes |
| 5436 | `h comp2      r1.0x23,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 5322: Preceding instruction 'h' has no flag change recorded

### 0x0802B0D4: if=go

**Operand:** `$0x3A`
**Occurrences:** 2
**Status:** Taken 1/2 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5328 | `bi test      W1` | `[Pdzscko]` | `[PdzsCko]` | No |
| 5442 | `bi test      W1` | `[PdZscko]` | `[PdZsCko]` | Yes |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x0802B0DD: if><go

**Operand:** `$0x31`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5333 | `by comp2     r1.0x26,$0x7` | `[PdzsCko]` | `[PdzScko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802B116: if=go

**Operand:** `$0x15`
**Occurrences:** 2
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5338 | `h comp2      r1.0x23,#0x40` | `[Pdzscko]` | `[PdzSCko]` | No |
| 5447 | `h comp2      r1.0x23,#0x40` | `[PdzsCko]` | `[PdzSCko]` | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x0802B184: if><go

**Operand:** `$0x16`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5273 | `w test       b.0x14` | `[PdzsCko]` | (unchanged) | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 5273: Preceding instruction 'w' has no flag change recorded

### 0x0802B1AD: if><go

**Operand:** `$0x76`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5287 | `h2 comp      W1` | `[PdzSCko]` | `[PdZscko]` | No |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

### 0x0802B1B6: if><go

**Operand:** `$0x6D`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5292 | `by comp2     r1.0x26,$0x1` | `[Pdzscko]` | `[PdZsCko]` | No |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

### 0x0802B1BE: if=go

**Operand:** `$0x65`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5297 | `w test       r1.0x1B` | `[PdzsCko]` | `[PdZsCko]` | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

### 0x0802B232: if><go

**Operand:** `$0xE0`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5308 | `h2 comp      W1` | `[PdzSCko]` | `[PdZscko]` | No |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

### 0x0802B23B: if><go

**Operand:** `$0xD7`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5313 | `by comp2     r1.0x26,$0x7` | `[Pdzscko]` | `[PdzScko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802B47C: if><go

**Operand:** `$0x91`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1602 | `w test       b.0x1C` | `[PdZscko]` | `[PdzsCko]` | Yes |
| 2828 | `w test       b.0x1C` | `[PdZscko]` | `[PdzsCko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802B52B: if=go

**Operand:** `$0xB`
**Occurrences:** 11
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1620 | `h comp2      b.0x2E,$0x10` | `[Pdzscko]` | `[PdzScko]` | No |
| 1635 | `h comp2      b.0x2E,$0x10` | `[Pdzscko]` | `[PdzScko]` | No |
| 1650 | `h comp2      b.0x2E,$0x10` | `[Pdzscko]` | `[PdzScko]` | No |
| 1665 | `h comp2      b.0x2E,$0x10` | `[Pdzscko]` | `[PdzScko]` | No |
| 1680 | `h comp2      b.0x2E,$0x10` | `[Pdzscko]` | `[PdzScko]` | No |
| ... | (6 more occurrences) | | | |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x0802B534: if><go

**Operand:** `$0xE4`
**Occurrences:** 11
**Status:** Taken 9/11 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1622 | `h comp2      b.0x2E,b.0x1A` | `[PdzScko]` | (unchanged) | Yes |
| 1637 | `h comp2      b.0x2E,b.0x1A` | `[PdzScko]` | (unchanged) | Yes |
| 1652 | `h comp2      b.0x2E,b.0x1A` | `[PdzScko]` | (unchanged) | Yes |
| 1667 | `h comp2      b.0x2E,b.0x1A` | `[PdzScko]` | (unchanged) | Yes |
| 1683 | `h comp2      b.0x2E,b.0x1A` | `[PdzScko]` | `[PdZsCko]` | No |
| ... | (6 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 1622: Preceding instruction 'h' has no flag change recorded
- Line 1637: Preceding instruction 'h' has no flag change recorded
- Line 1652: Preceding instruction 'h' has no flag change recorded

### 0x0802B553: if=go

**Operand:** `$0xE`
**Occurrences:** 2
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1699 | `by test      b.0x23` | `[PdZsCko]` | `[PdzsCko]` | No |
| 2940 | `by test      b.0x23` | `[PdZsCko]` | `[PdzsCko]` | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x0802B564: if=go

**Operand:** `$0xF`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1706 | `by test      b.0x37` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| 2947 | `by test      b.0x37` | `[PdzsCko]` | `[PdZsCko]` | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

### 0x0802CA57: if>>=go

**Operand:** `$0x8`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 3990 | `w1 comp      b.0x1C` | `[Pdzscko]` | (unchanged) | No |

**Analysis:** C=0 (Unsigned >= requires C=1) -> NOT TAKEN

**Potential Issues:**
- Line 3990: Preceding instruction 'w1' has no flag change recorded

### 0x0802CCDC: if<go

**Operand:** `$0x11`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1039 | `w comp2      b.0x1C,b.0x20` | `[Pdzscko]` | `[PdzScko]` | Yes |

**Analysis:** S=1 (Signed < requires S=1) -> TAKEN

### 0x0802CD08: if=go

**Operand:** `$0x14`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1169 | `w test       $0x801D534` | `[Pdzscko]` | `[PdZsCko]` | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

### 0x0802CD25: if>>=go

**Operand:** `$0xD`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1172 | `w comp2      $0x801D42C,#0x1000` | `[PdZsCko]` | `[PdzsCko]` | Yes |

**Analysis:** C=1 (Unsigned >= requires C=1) -> TAKEN

### 0x0802CD4F: if>>=go

**Operand:** `$0xF`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1182 | `w comp2      #0x7FFFFFF,$0x801D4` | `[Pdzscko]` | `[PdzsCko]` | Yes |

**Analysis:** C=1 (Unsigned >= requires C=1) -> TAKEN

### 0x0802CD8A: if-kgo

**Operand:** `$0x3B`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1191 | `w add2       $0x801D42C,#0x801` | `[Pdzscko]` | (unchanged) | Yes |

**Analysis:** K=0 (K-clear requires K=0) -> TAKEN

**Potential Issues:**
- Line 1191: Preceding instruction 'w' has no flag change recorded

### 0x0802CDE7: if><go

**Operand:** `$0xD`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1201 | `w test       $0x801D5A8` | `[Pdzscko]` | `[PdzsCko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802CE7C: if=go

**Operand:** `$0x20`
**Occurrences:** 24
**Status:** Taken 23/24 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1251 | `bi test      W1` | `[Pdzscko]` | `[PdZsCko]` | Yes |
| 1264 | `bi test      W1` | `[Pdzscko]` | `[PdZsCko]` | Yes |
| 1277 | `bi test      W1` | `[Pdzscko]` | `[PdZsCko]` | Yes |
| 1290 | `bi test      W1` | `[Pdzscko]` | `[PdZsCko]` | Yes |
| 1303 | `bi test      W1` | `[Pdzscko]` | `[PdZsCko]` | Yes |
| ... | (19 more occurrences) | | | |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

### 0x0802CEA8: if><go

**Operand:** `$0xCE`
**Occurrences:** 24
**Status:** Taken 23/24 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1257 | `by comp2     b.0x1B,$0x17` | `[Pdzscko]` | `[PdzScko]` | Yes |
| 1270 | `by comp2     b.0x1B,$0x17` | `[Pdzscko]` | `[PdzScko]` | Yes |
| 1283 | `by comp2     b.0x1B,$0x17` | `[Pdzscko]` | `[PdzScko]` | Yes |
| 1296 | `by comp2     b.0x1B,$0x17` | `[Pdzscko]` | `[PdzScko]` | Yes |
| 1309 | `by comp2     b.0x1B,$0x17` | `[Pdzscko]` | `[PdzScko]` | Yes |
| ... | (19 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802CF16: if><go

**Operand:** `$0x5`
**Occurrences:** 4
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1129 | `w test       W3` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 2786 | `w test       W3` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 4025 | `w test       W3` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 4075 | `w test       W3` | `[PdzsCko]` | (unchanged) | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 4075: Preceding instruction 'w' has no flag change recorded

### 0x0802CF2F: if=go

**Operand:** `$0xD`
**Occurrences:** 4
**Status:** Taken 1/4 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 1140 | `d4 comp      D1` | `[Pdzscko]` | (unchanged) | No |
| 2797 | `d4 comp      D1` | `[Pdzscko]` | (unchanged) | No |
| 4037 | `d4 comp      D1` | `[PdZscko]` | (unchanged) | Yes |
| 4086 | `d4 comp      D1` | `[Pdzscko]` | (unchanged) | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

**Potential Issues:**
- Line 1140: Preceding instruction 'd4' has no flag change recorded
- Line 2797: Preceding instruction 'd4' has no flag change recorded
- Line 4037: Preceding instruction 'd4' has no flag change recorded

### 0x0802CFBD: if<<go

**Operand:** `$0x22`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4120 | `h comp2      b.0x3E,b.0x3A` | `[Pdzscko]` | `[PdzsCko]` | No |

**Analysis:** C=1 (Unsigned < requires C=0) -> NOT TAKEN

### 0x0802CFDD: if><go

**Operand:** `$0xEA`
**Occurrences:** 37
**Status:** Taken 36/37 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4138 | `h comp2      b.0x32,b.0x3E` | `[Pdzscko]` | `[PdzScko]` | Yes |
| 4154 | `h comp2      b.0x32,b.0x3E` | `[Pdzscko]` | `[PdzScko]` | Yes |
| 4170 | `h comp2      b.0x32,b.0x3E` | `[Pdzscko]` | `[PdzScko]` | Yes |
| 4186 | `h comp2      b.0x32,b.0x3E` | `[Pdzscko]` | `[PdzScko]` | Yes |
| 4202 | `h comp2      b.0x32,b.0x3E` | `[Pdzscko]` | `[PdzScko]` | Yes |
| ... | (32 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802D197: if><go

**Operand:** `$0xF5`
**Occurrences:** 38
**Status:** Taken 37/38 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5715 | `by test      IND(b.0x14) ; 0x1800` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 5721 | `by test      IND(b.0x14) ; 0x1800` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 5727 | `by test      IND(b.0x14) ; 0x1800` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 5733 | `by test      IND(b.0x14) ; 0x1800` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 5739 | `by test      IND(b.0x14) ; 0x1800` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| ... | (33 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802D1E9: if=go

**Operand:** `$0x149`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5665 | `w test       $0x801D7B8` | `[PdzsCko]` | (unchanged) | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

**Potential Issues:**
- Line 5665: Preceding instruction 'w' has no flag change recorded

### 0x0802D1F3: if><go

**Operand:** `$0x2C`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5668 | `w comp2      $0x801D7D8,$0x3` | `[PdzsCko]` | `[PdzScko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802D248: if=go

**Operand:** `$0xEA`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5943 | `w test       $0x801D7BC` | `[PdZsCko]` | (unchanged) | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

**Potential Issues:**
- Line 5943: Preceding instruction 'w' has no flag change recorded

### 0x0802D339: if=go

**Operand:** `$0x107`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5946 | `w comp2      $0x801D7D8,$0x3` | `[PdZsCko]` | `[PdzScko]` | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x0802D35A: if=go

**Operand:** `$0x2D`
**Occurrences:** 1
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5984 | `w test       b.0x14` | `[Pdzscko]` | `[PdzsCko]` | No |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x0802D362: if=go

**Operand:** `$0x25`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5988 | `h test       r1.0x8` | `[PdzsCko]` | `[PdZsCko]` | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

### 0x0802D48A: if>>=go

**Operand:** `$0xD`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 18 | `w comp2      $0x801D7C0,#0x1000` | `[PdZscko]` | `[PdzsCko]` | Yes |

**Analysis:** C=1 (Unsigned >= requires C=1) -> TAKEN

### 0x0802D4A2: if>>=go

**Operand:** `$0xF`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 20 | `w comp2      #0x7FFFFFF,$0x801D7` | `[PdzsCko]` | (unchanged) | Yes |

**Analysis:** C=1 (Unsigned >= requires C=1) -> TAKEN

**Potential Issues:**
- Line 20: Preceding instruction 'w' has no flag change recorded

### 0x0802D4DD: if-kgo

**Operand:** `$0x55`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 30 | `w add2       $0x801D7C0,#0x801` | `[PdzsCko]` | `[Pdzscko]` | Yes |

**Analysis:** K=0 (K-clear requires K=0) -> TAKEN

### 0x0802D550: if><go

**Operand:** `$0x11`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 37 | `w test       $0x801D7CC` | `[Pdzscko]` | `[PdzsCko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802D685: if><go

**Operand:** `$0xB`
**Occurrences:** 33
**Status:** Taken 18/33 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 127 | `bi test      $0x801D9E4(r1)` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 151 | `bi test      $0x801D9E4(r1)` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 175 | `bi test      $0x801D9E4(r1)` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 199 | `bi test      $0x801D9E4(r1)` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 223 | `bi test      $0x801D9E4(r1)` | `[Pdzscko]` | `[PdZsCko]` | No |
| ... | (28 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802D68D: if><go

**Operand:** `$0xC`
**Occurrences:** 15
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 228 | `w test       IND(b.0x14)(r1) ; 0` | `[PdzsCko]` | `[PdZsCko]` | No |
| 281 | `w test       IND(b.0x14)(r1) ; 0` | `[PdzsCko]` | `[PdZsCko]` | No |
| 310 | `w test       IND(b.0x14)(r1) ; 0` | `[PdzsCko]` | `[PdZsCko]` | No |
| 339 | `w test       IND(b.0x14)(r1) ; 0` | `[PdzsCko]` | `[PdZsCko]` | No |
| 368 | `w test       IND(b.0x14)(r1) ; 0` | `[PdzsCko]` | `[PdZsCko]` | No |
| ... | (10 more occurrences) | | | |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

### 0x0802D6AA: if><go

**Operand:** `$0x5`
**Occurrences:** 33
**Status:** Taken 32/33 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 142 | `by comp2     b.0x1B,$0x1F` | `[Pdzscko]` | `[PdzScko]` | Yes |
| 166 | `by comp2     b.0x1B,$0x1F` | `[Pdzscko]` | `[PdzScko]` | Yes |
| 190 | `by comp2     b.0x1B,$0x1F` | `[Pdzscko]` | `[PdzScko]` | Yes |
| 214 | `by comp2     b.0x1B,$0x1F` | `[Pdzscko]` | `[PdzScko]` | Yes |
| 243 | `by comp2     b.0x1B,$0x1F` | `[Pdzscko]` | `[PdzScko]` | Yes |
| ... | (28 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802D6B4: if><go

**Operand:** `$0xC7`
**Occurrences:** 33
**Status:** Taken 32/33 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 144 | `by comp2     b.0x1B,#0x29` | `[PdzScko]` | (unchanged) | Yes |
| 168 | `by comp2     b.0x1B,#0x29` | `[PdzScko]` | (unchanged) | Yes |
| 192 | `by comp2     b.0x1B,#0x29` | `[PdzScko]` | (unchanged) | Yes |
| 216 | `by comp2     b.0x1B,#0x29` | `[PdzScko]` | (unchanged) | Yes |
| 245 | `by comp2     b.0x1B,#0x29` | `[PdzScko]` | (unchanged) | Yes |
| ... | (28 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 144: Preceding instruction 'by' has no flag change recorded
- Line 168: Preceding instruction 'by' has no flag change recorded
- Line 192: Preceding instruction 'by' has no flag change recorded

### 0x0802D908: if><go

**Operand:** `$0x2E`
**Occurrences:** 10
**Status:** Taken 8/10 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4757 | `w test       b.0x0` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 4813 | `w test       b.0x0` | `[PdzsCko]` | (unchanged) | Yes |
| 4870 | `w test       b.0x0` | `[PdzsCko]` | (unchanged) | Yes |
| 4927 | `w test       b.0x0` | `[PdzsCko]` | (unchanged) | Yes |
| 4984 | `w test       b.0x0` | `[PdzsCko]` | (unchanged) | Yes |
| ... | (5 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 4813: Preceding instruction 'w' has no flag change recorded
- Line 4870: Preceding instruction 'w' has no flag change recorded
- Line 4927: Preceding instruction 'w' has no flag change recorded

### 0x0802D918: if><go

**Operand:** `$0x1E`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5128 | `w1 comp      W2` | `[Pdzscko]` | (unchanged) | Yes |
| 5581 | `w1 comp      W2` | `[Pdzscko]` | (unchanged) | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 5128: Preceding instruction 'w1' has no flag change recorded
- Line 5581: Preceding instruction 'w1' has no flag change recorded

### 0x0802D93C: if><go

**Operand:** `$0x32`
**Occurrences:** 10
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4760 | `w test       $0x801D7BC` | `[PdzsCko]` | `[PdZsCko]` | No |
| 4816 | `w test       $0x801D7BC` | `[PdzsCko]` | `[PdZsCko]` | No |
| 4873 | `w test       $0x801D7BC` | `[PdzsCko]` | `[PdZsCko]` | No |
| 4930 | `w test       $0x801D7BC` | `[PdzsCko]` | `[PdZsCko]` | No |
| 4987 | `w test       $0x801D7BC` | `[PdzsCko]` | `[PdZsCko]` | No |
| ... | (5 more occurrences) | | | |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

### 0x0802D941: if=go

**Operand:** `$0x2D`
**Occurrences:** 10
**Status:** Taken 1/10 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4763 | `w test       b.0xC` | `[PdZsCko]` | `[PdzsCko]` | No |
| 4819 | `w test       b.0xC` | `[PdZsCko]` | `[PdzsCko]` | No |
| 4876 | `w test       b.0xC` | `[PdZsCko]` | `[PdzsCko]` | No |
| 4933 | `w test       b.0xC` | `[PdZsCko]` | `[PdzsCko]` | No |
| 4989 | `w test       b.0xC` | `[PdZsCko]` | (unchanged) | Yes |
| ... | (5 more occurrences) | | | |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

**Potential Issues:**
- Line 4989: Preceding instruction 'w' has no flag change recorded

### 0x0802D947: if=go

**Operand:** `$0x27`
**Occurrences:** 9
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4766 | `w comp2      b.0xC,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | No |
| 4822 | `w comp2      b.0xC,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | No |
| 4879 | `w comp2      b.0xC,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | No |
| 4936 | `w comp2      b.0xC,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | No |
| 5014 | `w comp2      b.0xC,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | No |
| ... | (4 more occurrences) | | | |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

### 0x0802D94F: if><go

**Operand:** `$0xB`
**Occurrences:** 9
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4771 | `w test       r1.0x4` | `[Pdzscko]` | `[PdZsCko]` | No |
| 4827 | `w test       r1.0x4` | `[Pdzscko]` | `[PdZsCko]` | No |
| 4884 | `w test       r1.0x4` | `[Pdzscko]` | `[PdZsCko]` | No |
| 4941 | `w test       r1.0x4` | `[Pdzscko]` | `[PdZsCko]` | No |
| 5019 | `w test       r1.0x4` | `[Pdzscko]` | `[PdZsCko]` | No |
| ... | (4 more occurrences) | | | |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

### 0x0802D957: if=go

**Operand:** `$0x17`
**Occurrences:** 9
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4776 | `h test       r1.0x8` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| 4832 | `h test       r1.0x8` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| 4889 | `h test       r1.0x8` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| 4946 | `h test       r1.0x8` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| 5024 | `h test       r1.0x8` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| ... | (4 more occurrences) | | | |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

### 0x0802D970: if=go

**Operand:** `$0x107`
**Occurrences:** 10
**Status:** Taken 1/10 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4779 | `w test       b.0xC` | `[PdZsCko]` | `[PdzsCko]` | No |
| 4835 | `w test       b.0xC` | `[PdZsCko]` | `[PdzsCko]` | No |
| 4892 | `w test       b.0xC` | `[PdZsCko]` | `[PdzsCko]` | No |
| 4949 | `w test       b.0xC` | `[PdZsCko]` | `[PdzsCko]` | No |
| 4991 | `w test       b.0xC` | `[PdZsCko]` | (unchanged) | Yes |
| ... | (5 more occurrences) | | | |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

**Potential Issues:**
- Line 4991: Preceding instruction 'w' has no flag change recorded

### 0x0802D976: if><go

**Operand:** `$0x1A`
**Occurrences:** 9
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4782 | `w comp2      b.0xC,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | Yes |
| 4838 | `w comp2      b.0xC,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | Yes |
| 4895 | `w comp2      b.0xC,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | Yes |
| 4952 | `w comp2      b.0xC,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | Yes |
| 5030 | `w comp2      b.0xC,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | Yes |
| ... | (4 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802D9A0: if><go

**Operand:** `$0x5`
**Occurrences:** 9
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4787 | `w comp2      $0x801DC8C,$-0x1` | `[Pdzscko]` | (unchanged) | Yes |
| 4843 | `w comp2      $0x801DC8C,$-0x1` | `[Pdzscko]` | (unchanged) | Yes |
| 4900 | `w comp2      $0x801DC8C,$-0x1` | `[Pdzscko]` | (unchanged) | Yes |
| 4957 | `w comp2      $0x801DC8C,$-0x1` | `[Pdzscko]` | (unchanged) | Yes |
| 5035 | `w comp2      $0x801DC8C,$-0x1` | `[Pdzscko]` | (unchanged) | Yes |
| ... | (4 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 4787: Preceding instruction 'w' has no flag change recorded
- Line 4843: Preceding instruction 'w' has no flag change recorded
- Line 4900: Preceding instruction 'w' has no flag change recorded

### 0x0802D9AB: if><go

**Operand:** `$0x6`
**Occurrences:** 9
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4790 | `w test       $0x801DC8C` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 4846 | `w test       $0x801DC8C` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 4903 | `w test       $0x801DC8C` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 4960 | `w test       $0x801DC8C` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| 5038 | `w test       $0x801DC8C` | `[Pdzscko]` | `[PdzsCko]` | Yes |
| ... | (4 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802D9BE: if><go

**Operand:** `$0x5`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5169 | `w comp2      r1.0x0,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | Yes |
| 5622 | `w comp2      r1.0x0,$-0x1` | `[PdzsCko]` | `[Pdzscko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802D9D1: if>=go

**Operand:** `$0x8B`
**Occurrences:** 2
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5173 | `w comp2      r1.0x4,$0x801DC80` | `[Pdzscko]` | `[PdzScko]` | No |
| 5626 | `w comp2      r1.0x4,$0x801DC80` | `[Pdzscko]` | `[PdzScko]` | No |

**Analysis:** S=1 (Signed >= requires S=0) -> NOT TAKEN

### 0x0802D9E2: if<go

**Operand:** `$0x7A`
**Occurrences:** 2
**Status:** Taken 1/2 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5178 | `w comp2      r1.0x8,$0x801DC80` | `[Pdzscko]` | `[PdzsCko]` | No |
| 5631 | `w comp2      r1.0x8,$0x801DC80` | `[Pdzscko]` | `[PdzScko]` | Yes |

**Analysis:** S=0 (Signed < requires S=1) -> NOT TAKEN

### 0x0802DA13: if=go

**Operand:** `$0x11`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5199 | `w test       $0x801DC92` | `[PdZscko]` | `[PdZsCko]` | Yes |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

### 0x0802DA5A: if><go

**Operand:** `$0xA4`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5191 | `h comp2      $0x801DC90,r1.0xC` | `[Pdzscko]` | `[PdzScko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802DA74: if><go

**Operand:** `$0xFF40`
**Occurrences:** 10
**Status:** Taken 2/10 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4795 | `w test       r1.0x0` | `[PdzsCko]` | `[PdZsCko]` | No |
| 4851 | `w test       r1.0x0` | `[PdzsCko]` | `[PdZsCko]` | No |
| 4908 | `w test       r1.0x0` | `[PdzsCko]` | `[PdZsCko]` | No |
| 4965 | `w test       r1.0x0` | `[PdzsCko]` | `[PdZsCko]` | No |
| 5043 | `w test       r1.0x0` | `[PdzsCko]` | `[PdZsCko]` | No |
| ... | (5 more occurrences) | | | |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

**Potential Issues:**
- Line 5165: Preceding instruction 'w' has no flag change recorded
- Line 5618: Preceding instruction 'w' has no flag change recorded

### 0x0802DA81: if=go

**Operand:** `$0x9`
**Occurrences:** 9
**Status:** Taken 1/9 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4800 | `w test       b.0xC` | `[PdzsCko]` | (unchanged) | No |
| 4856 | `w test       b.0xC` | `[PdzsCko]` | (unchanged) | No |
| 4913 | `w test       b.0xC` | `[PdzsCko]` | (unchanged) | No |
| 4970 | `w test       b.0xC` | `[PdzsCko]` | (unchanged) | No |
| 4997 | `w test       b.0xC` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| ... | (4 more occurrences) | | | |

**Analysis:** Z=0 (Equal requires Z=1) -> NOT TAKEN

**Potential Issues:**
- Line 4800: Preceding instruction 'w' has no flag change recorded
- Line 4856: Preceding instruction 'w' has no flag change recorded
- Line 4913: Preceding instruction 'w' has no flag change recorded

### 0x0802DA87: if><go

**Operand:** `$0x1F`
**Occurrences:** 8
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 4802 | `w test       IND(b.0xC) ; 0x801D` | `[PdzsCko]` | (unchanged) | Yes |
| 4858 | `w test       IND(b.0xC) ; 0x801D` | `[PdzsCko]` | (unchanged) | Yes |
| 4915 | `w test       IND(b.0xC) ; 0x801D` | `[PdzsCko]` | (unchanged) | Yes |
| 4972 | `w test       IND(b.0xC) ; 0x801D` | `[PdzsCko]` | (unchanged) | Yes |
| 5050 | `w test       IND(b.0xC) ; 0x8003` | `[PdzsCko]` | (unchanged) | Yes |
| ... | (3 more occurrences) | | | |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

**Potential Issues:**
- Line 4802: Preceding instruction 'w' has no flag change recorded
- Line 4858: Preceding instruction 'w' has no flag change recorded
- Line 4915: Preceding instruction 'w' has no flag change recorded

### 0x0802DA8C: if><go

**Operand:** `$0x14`
**Occurrences:** 1
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5000 | `w test       b.0x0` | `[PdZsCko]` | `[PdzsCko]` | Yes |

**Analysis:** Z=0 (Not Equal requires Z=0) -> TAKEN

### 0x0802DC22: if-kgo

**Operand:** `$0xA`
**Occurrences:** 2
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 5356 | `call $0xF8000023,$0x1,b.0x18 ; 0x10` | `[PdzSCko]` | (unchanged) | Yes |
| 5465 | `call $0xF8000023,$0x1,b.0x18 ; 0x10` | `[PdzSCko]` | (unchanged) | Yes |

**Analysis:** K=0 (K-clear requires K=0) -> TAKEN

### 0x0802DF32: if=go

**Operand:** `$0x2E`
**Occurrences:** 2
**Status:** Taken 1/2 times

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 2619 | `by test      $0x801DE53` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| 3940 | `by test      $0x801DE53` | `[PdzsCko]` | (unchanged) | No |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

**Potential Issues:**
- Line 3940: Preceding instruction 'by' has no flag change recorded

### 0x0802EC4E: if>=go

**Operand:** `$0x2E`
**Occurrences:** 6
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 6103 | `w test       b.0x24` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| 6144 | `w test       b.0x24` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| 6184 | `w test       b.0x24` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| 6308 | `w test       b.0x24` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| 6349 | `w test       b.0x24` | `[PdzsCko]` | `[PdZsCko]` | Yes |
| ... | (1 more occurrences) | | | |

**Analysis:** S=0 (Signed >= requires S=0) -> TAKEN

### 0x0802EC97: if><go

**Operand:** `$0xE8`
**Occurrences:** 6
**Status:** Never taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 6106 | `w test       b.0x24` | `[PdZsCko]` | (unchanged) | No |
| 6147 | `w test       b.0x24` | `[PdZsCko]` | (unchanged) | No |
| 6187 | `w test       b.0x24` | `[PdZsCko]` | (unchanged) | No |
| 6311 | `w test       b.0x24` | `[PdZsCko]` | (unchanged) | No |
| 6352 | `w test       b.0x24` | `[PdZsCko]` | (unchanged) | No |
| ... | (1 more occurrences) | | | |

**Analysis:** Z=1 (Not Equal requires Z=0) -> NOT TAKEN

**Potential Issues:**
- Line 6106: Preceding instruction 'w' has no flag change recorded
- Line 6147: Preceding instruction 'w' has no flag change recorded
- Line 6187: Preceding instruction 'w' has no flag change recorded

### 0x0802EC9E: if=go

**Operand:** `$0xD`
**Occurrences:** 6
**Status:** Always taken

| Line | Preceding Instruction | Flags Before | Flags After | Taken? |
|------|----------------------|--------------|-------------|--------|
| 6108 | `by comp2     b.0x2F,#0x20` | `[PdZsCko]` | (unchanged) | Yes |
| 6149 | `by comp2     b.0x2F,#0x20` | `[PdZsCko]` | (unchanged) | Yes |
| 6189 | `by comp2     b.0x2F,#0x20` | `[PdZsCko]` | (unchanged) | Yes |
| 6313 | `by comp2     b.0x2F,#0x20` | `[PdZsCko]` | (unchanged) | Yes |
| 6354 | `by comp2     b.0x2F,#0x20` | `[PdZsCko]` | (unchanged) | Yes |
| ... | (1 more occurrences) | | | |

**Analysis:** Z=1 (Equal requires Z=1) -> TAKEN

**Potential Issues:**
- Line 6108: Preceding instruction 'by' has no flag change recorded
- Line 6149: Preceding instruction 'by' has no flag change recorded
- Line 6189: Preceding instruction 'by' has no flag change recorded

## Potential Issues

### 0x08029309: NEVER_TAKEN

Branch at 0x08029309 (if=go) is NEVER taken (6 occurrences). May indicate dead code or redundant test.

### 0x080293A4: NEVER_TAKEN

Branch at 0x080293A4 (if=go) is NEVER taken (5 occurrences). May indicate dead code or redundant test.

### 0x0802B52B: NEVER_TAKEN

Branch at 0x0802B52B (if=go) is NEVER taken (11 occurrences). May indicate dead code or redundant test.

### 0x0802CF16: ALWAYS_TAKEN

Branch at 0x0802CF16 (if><go) is ALWAYS taken (4 occurrences). May indicate dead code or redundant branch.

### 0x0802D68D: NEVER_TAKEN

Branch at 0x0802D68D (if><go) is NEVER taken (15 occurrences). May indicate dead code or redundant test.

### 0x0802D93C: NEVER_TAKEN

Branch at 0x0802D93C (if><go) is NEVER taken (10 occurrences). May indicate dead code or redundant test.

### 0x0802D947: NEVER_TAKEN

Branch at 0x0802D947 (if=go) is NEVER taken (9 occurrences). May indicate dead code or redundant test.

### 0x0802D94F: NEVER_TAKEN

Branch at 0x0802D94F (if><go) is NEVER taken (9 occurrences). May indicate dead code or redundant test.

### 0x0802D957: ALWAYS_TAKEN

Branch at 0x0802D957 (if=go) is ALWAYS taken (9 occurrences). May indicate dead code or redundant branch.

### 0x0802D976: ALWAYS_TAKEN

Branch at 0x0802D976 (if><go) is ALWAYS taken (9 occurrences). May indicate dead code or redundant branch.

### 0x0802D9A0: ALWAYS_TAKEN

Branch at 0x0802D9A0 (if><go) is ALWAYS taken (9 occurrences). May indicate dead code or redundant branch.

### 0x0802D9AB: ALWAYS_TAKEN

Branch at 0x0802D9AB (if><go) is ALWAYS taken (9 occurrences). May indicate dead code or redundant branch.

### 0x0802DA87: ALWAYS_TAKEN

Branch at 0x0802DA87 (if><go) is ALWAYS taken (8 occurrences). May indicate dead code or redundant branch.

### 0x0802EC4E: ALWAYS_TAKEN

Branch at 0x0802EC4E (if>=go) is ALWAYS taken (6 occurrences). May indicate dead code or redundant branch.

### 0x0802EC97: NEVER_TAKEN

Branch at 0x0802EC97 (if><go) is NEVER taken (6 occurrences). May indicate dead code or redundant test.

### 0x0802EC9E: ALWAYS_TAKEN

Branch at 0x0802EC9E (if=go) is ALWAYS taken (6 occurrences). May indicate dead code or redundant branch.

## Flag Reference

| Flag | Meaning |
|------|---------|
| Z | Zero - result is zero |
| S | Sign - result is negative (MSB=1) |
| C | Carry - unsigned overflow/borrow |
| K | User/Key - programmable flag |
| O | Overflow - signed overflow |

**Branch type mapping:**
- **Signed comparisons:** Use S and Z flags (if<, if>, if<=, if>=, if=, if><)
- **Unsigned comparisons:** Use C and Z flags (if<<, if>>, if<<=, if>>=)
- **User flag:** Uses K flag (ifk, if-k)
