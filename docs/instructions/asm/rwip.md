# RWIP - Read Written In Page Table

## Overview

**Mnemonic:** `rwip`
**Function:** Read Written-In-Page (dirty bit) table
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `tn RWIP <bit or group no.>`

---

## Description

Reads a bit or 16-bit group from the Written-In-Page (WIP) table into a specified register. The WIP table tracks which physical memory pages have been modified (dirty pages) and must be written back to disk before being replaced. This is essential for virtual memory page replacement algorithms.

**Operation:**
```
For BIn RWIP: WIP_table[page_number] → Rn (single bit)
For Hn RWIP: WIP_table[page_number/16 * 16 .. +15] → Rn (16 bits)
Returns logical OR of program and data WIP tables
```

**Key Characteristics:**
- Privileged instruction (supervisor mode only)
- Reads from hardware Written-In-Page table
- BI prefix reads single bit (one page dirty status)
- H prefix reads 16-bit group (16 pages at once)
- Returns logical OR of separate program and data WIP tables
- Dirty bit automatically set by hardware on write operations

**Important Hardware Detail:** ND-500 has separate WIP tables for program and data memory. RWIP returns the logical OR of both tables, making them appear as one. This means an ND-500 system cannot have physically separate memory for program and data at the same physical addresses.

**Common Use Cases:**
- Page swapper routines (identifying dirty pages to write back)
- Virtual memory page replacement decision
- Determining which pages need disk writeback before eviction
- Working with page fault handlers
- Memory usage analysis and optimization

**Operands:** 1 (page/group number)
**Variants:** 8 opcodes (2 data types × 4 registers)

---

## Variants

Total variants: 8 (2 data types × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly | Purpose |
|---------|--------|-----------|----------|----------|---------|
| 1-4 | 0xFE94-0xFE97 | BI | I1-I4 | BIn RWIP | Single page dirty bit |
| 5-8 | 0xFE98-0xFE9B | H | I1-I4 | Hn RWIP | 16-page group |

---

## Operands

### Operand 1 (Page/Group Number)

**Type:** Word (physical page number)
**Access:** Read

For BIn RWIP:
- Operand specifies physical memory page number
- Returns 1 if page has been written to (dirty), 0 if clean

For Hn RWIP:
- Operand specifies page number divided by 16
- Returns 16 bits representing pages [n*16 .. n*16+15]

**Note:** Only lower 25 bits of bit number are significant. Reading bits representing non-existing memory returns zero.

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

### Example 1: Check if page is dirty before eviction

```assembly
        % Check if page needs writeback before eviction
        BI1 RWIP CANDIDATE_PAGE
        IF=GO CLEAN_PAGE            % Not dirty, safe to evict
        % Dirty page, must write to disk first
        CALL WRITE_PAGE_TO_DISK
CLEAN_PAGE:
        % Safe to reuse this page
```

**Explanation:** Before evicting a page, check WIP bit. If set, page must be written to disk first.

### Example 2: Scan for dirty pages in group

```assembly
        % Find dirty pages in group 0-15
        H2 RWIP 0
        W2 TEST
        IF=GO NO_DIRTY_PAGES        % All clean
        % At least one dirty page in group
```

**Explanation:** Halfword read efficiently checks 16 pages for any dirty pages.

### Example 3: Swapper main loop - check before evict

```assembly
        % Page replacement: check WIP before evicting
        BI1 RWIP VICTIM_PAGE
        IF><GO WRITEBACK_NEEDED     % Dirty page
        % Clean page, skip writeback
        GO EVICT_PAGE
WRITEBACK_NEEDED:
        W1 := VICTIM_PAGE
        CALL FLUSH_PAGE_TO_DISK
        BI ZWIP VICTIM_PAGE         % Clear WIP bit after write
EVICT_PAGE:
        % Reuse page for new data
```

**Explanation:** Standard swapper pattern: check WIP, writeback if dirty, clear WIP bit, evict page.

### Example 4: Count dirty pages for writeback stats

```assembly
        % Count dirty pages in range
        W1 CLR                      % Dirty page counter
        W2 := FIRST_GROUP
SCAN_LOOP:
        H4 RWIP W2
        % Count set bits in H4
        % (bit counting code here)
        W2 INC
        W2 COMP LAST_GROUP
        IF<=GO SCAN_LOOP
        % W1 now has total dirty pages
```

**Explanation:** Scanning WIP table to gather statistics on dirty pages.

### Example 5: Indexed page table dirty check

```assembly
        % Check if indexed page is dirty
        W1 := PAGE_INDEX
        BI2 RWIP PAGE_TABLE(W1)
        IF=GO PAGE_CLEAN
        % Page is dirty, needs writeback
```

**Explanation:** Use indexed addressing with WIP check for page table management.

### Example 6: Prioritize clean pages for eviction

```assembly
        % LRU with dirty page avoidance
TRY_NEXT_CANDIDATE:
        BI1 RPGU CANDIDATE          % Check if used
        IF><GO SKIP_CANDIDATE       % Recently used
        BI1 RWIP CANDIDATE          % Check if dirty
        IF=GO FOUND_VICTIM          % Clean and not recently used!
        % Dirty page, try next
SKIP_CANDIDATE:
        W1 := CANDIDATE
        W1 INC
        GO TRY_NEXT_CANDIDATE
FOUND_VICTIM:
        % Found clean, unused page
```

**Explanation:** Page replacement preferring clean pages (faster eviction, no disk I/O).

### Example 7: Working set writeback preparation

```assembly
        % Before taking working set snapshot, flush dirty pages
        H1 RWIP 0                   % Check first 16 pages
        W1 TEST
        IF=GO NO_FLUSH_NEEDED
        % Dirty pages exist, flush them
        CALL FLUSH_DIRTY_PAGES_0_15
NO_FLUSH_NEEDED:
```

**Explanation:** Checking for dirty pages before snapshot to ensure consistency.

---

## Performance Notes

- **Execution:** 2-3 cycles
  - Hardware table lookup: 1-2 cycles
  - Logical OR of program/data tables: +1 cycle
  - Register update: 1 cycle
- **Implementation:** WIP table maintained by MMU hardware (auto-set on writes)
- **Overhead:** Minimal - direct hardware access

**Usage recommendations:**
- Use H prefix for bulk scanning (16x faster than individual BI reads)
- Always check WIP before page eviction
- Combine with ZWIP after writeback to disk
- Essential for correct virtual memory operation
- Prefer evicting clean pages (WIP=0) to avoid disk I/O

**Relationship to other instructions:**
- `RWIP` reads, `ZWIP` clears WIP bits
- Similar to `RPGU`/`ZPGU` but tracks dirty pages instead of accessed pages
- Both WIP and PGU tables essential for page replacement algorithms
- Used together: RPGU (LRU), RWIP (dirty pages)

---

## Reference Manual

**Section:** §16.17
**Title:** Read Written In Page table

---

## See Also

- [ZWIP](zwip.md) - Clear Written-In-Page bits
- [RPGU](rpgu.md) - Read Page Used table (accessed pages)
- [ZPGU](zpgu.md) - Clear Page Used bit
- [RPHS](rphs.md) - Read from physical segment
- [WPHS](wphs.md) - Write to physical segment
- [SYSTEM instruction reference](../../instruction-reference/SYSTEM.md)
