# RPGU - Read Page Used Table

## Overview

**Mnemonic:** `rpgu`
**Function:** Read Page Used table bit or group
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `tn RPGU <bit or group no.>`

---

## Description

Reads a bit or 16-bit group from the Page Used (PGU) table into a specified register. The Page Used table tracks which physical memory pages have been accessed, supporting virtual memory management and page replacement algorithms.

**Operation:**
```
For BIn RPGU: PGU_table[page_number] → Rn (single bit)
For Hn RPGU: PGU_table[page_number/16 * 16 .. +15] → Rn (16 bits)
```

**Key Characteristics:**
- Privileged instruction (supervisor mode only)  
- Reads from hardware Page Used table
- BI prefix reads single bit (one page status)
- H prefix reads 16-bit group (16 pages at once)
- Operand specifies physical page number or group number

**Common Use Cases:**
- Virtual memory page replacement algorithms (LRU, clock)
- Identifying recently accessed pages
- Memory usage statistics collection
- Page fault handler optimization
- Working set analysis

**Operands:** 1 (page/group number)
**Variants:** 8 opcodes (2 data types × 4 registers)

---

## Variants

Total variants: 8 (2 data types × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly | Purpose |
|---------|--------|-----------|----------|----------|---------|
| 1-4 | 0xFE88-0xFE8B | BI | I1-I4 | BIn RPGU | Single page bit |
| 5-8 | 0xFE8C-0xFE8F | H | I1-I4 | Hn RPGU | 16-page group |

---

## Operands

### Operand 1 (Page/Group Number)

**Type:** Word (physical page number)
**Access:** Read

For BIn RPGU:
- Operand specifies physical memory page number
- Returns 1 if page has been used (accessed), 0 if not

For Hn RPGU:
- Operand specifies page number divided by 16  
- Returns 16 bits representing pages [n*16 .. n*16+15]

**Supported modes:**
- **LOCAL** - Local variable containing page number
- **RECORD** - Record field with page number
- **CONSTANT** - Immediate page number
- **REGISTER** - Register containing page number
- **PRE_INDEXED** - Indexed page table access
- **ABSOLUTE** - Absolute address containing page number

---

## Trap Conditions

- **Privilege violation:** If executed in user mode
- **Addressing traps:** Invalid address for operand

---

## Data Status Bits

No flags explicitly affected (implementation-dependent).

---

## Examples

### Example 1: Check if specific page was used

```assembly
        % Check if physical page 100 was accessed
        BI1 RPGU 100
        IF=GO PAGE_NOT_USED
        % Page was accessed
```

**Explanation:** Single-bit read to check if a specific physical page has been accessed.

### Example 2: Read group of 16 page bits

```assembly
        % Read PGU bits for pages 0-15
        H2 RPGU 0
        % H2 now contains bits for pages 0-15
```

**Explanation:** Halfword read fetches 16 page-used bits at once for efficient scanning.

### Example 3: Scan for unused pages

```assembly
        % Find first unused page in group
        H3 RPGU PAGE_GROUP
        W3 TEST
        IF=GO ALL_USED              % All 16 pages used
        % Find first zero bit for page replacement
```

**Explanation:** Page replacement algorithm checking which pages are candidates for eviction.

### Example 4: Working set analysis

```assembly
        % Count accessed pages in range
        W1 CLR                      % Counter
        W2 := FIRST_GROUP
LOOP:
        H4 RPGU W2
        % Count bits in H4
        W2 INC
        W2 COMP LAST_GROUP
        IF<=GO LOOP
```

**Explanation:** Scanning PGU table to analyze working set size.

### Example 5: LRU page replacement

```assembly
        % Find least recently used page
        BI1 RPGU CANDIDATE_PAGE
        IF><GO PAGE_RECENTLY_USED
        % This page is LRU candidate
        W1 := CANDIDATE_PAGE
        CALL EVICT_PAGE
```

**Explanation:** Using PGU bit to implement LRU page replacement policy.

### Example 6: Clear and sample pattern

```assembly
        % Sample PGU bits before clearing
        H1 RPGU 0
        H1 =: SAVED_PGU_0_15
        % (ZPGU would clear the bits after reading)
```

**Explanation:** Saving PGU state before resetting for next sampling interval.

### Example 7: Page table walk with PGU check

```assembly
        % Check if indexed page was used
        W1 := PAGE_INDEX
        BI2 RPGU PAGE_TABLE(W1)
        IF=GO PAGE_COLD
        % Page is hot (recently accessed)
```

**Explanation:** Combining indexed addressing with PGU check for page table management.

---

## Performance Notes

- **Execution:** 2-3 cycles
  - Hardware table lookup: 1-2 cycles
  - Register update: 1 cycle
- **Implementation:** PGU table maintained by MMU hardware
- **Overhead:** Minimal - direct hardware access

**Usage recommendations:**
- Use H prefix for bulk scanning (16x faster than individual BI reads)
- Clear PGU bits periodically with ZPGU for accurate aging
- Combine with ZPGU for clock/second-chance algorithms
- Essential for virtual memory paging algorithms

**Relationship to other instructions:**
- `RPGU` reads, `ZPGU` clears PGU bits
- Used with `RPHS`, `WPHS` for physical segment operations
- Part of ND-500 virtual memory instruction set

---

## Reference Manual

**Section:** §16.20
**Title:** Read Page Used Table

---

## See Also

- [ZPGU](zpgu.md) - Clear Page Used table bits
- [RPHS](rphs.md) - Read from physical segment
- [WPHS](wphs.md) - Write to physical segment
- [SYSTEM instruction reference](../../instruction-reference/SYSTEM.md)

