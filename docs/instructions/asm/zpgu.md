# ZPGU - Clear Page Used Bit

## Overview

**Mnemonic:** `zpgu`
**Function:** Clear Page Used table bit
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `BI ZPGU <bit no.>`

---

## Description

Clears (sets to 0) a specified bit in the Page Used (PGU) table. This instruction is the write counterpart to RPGU and is used by operating system page management code after loading new pages from disk into physical memory, or when implementing page aging algorithms.

**Operation:**
```
0 → PGU_table[page_number]
Clears both program and data PGU tables simultaneously
```

**Key Characteristics:**
- Privileged instruction (supervisor mode only)
- Clears PGU bit in both program AND data tables
- Used after page swapping operations
- Essential for clock and second-chance page replacement algorithms
- Installation-dependent (requires knowledge of physical memory configuration)

**Important Hardware Detail:** ND-500 systems have separate PGU tables for program and data memory. ZPGU clears the specified bit in BOTH tables simultaneously. This means an ND-500 system cannot have physically separate memory for program and data at the same physical address.

**Common Use Cases:**
- Resetting page-used bits after loading pages from disk
- Implementing clock/second-chance page replacement algorithms
- Page aging in virtual memory management
- Working set reset after sampling period
- Page table maintenance in swapper routines

**Operands:** 1 (bit/page number to clear)
**Variants:** 1 opcode (BI prefix only)

---

## Variants

Total variants: 1

| Variant | Opcode | Data Type | Assembly |
|---------|--------|-----------|----------|
| 1/1 | 0xFE90 | BI | BI ZPGU |

**Note:** Only BI (bit) prefix is supported. Use RPGU with H prefix to read 16 bits at once, but ZPGU only clears one bit at a time.

---

## Operands

### Operand 1 (Page Number/Bit Number)

**Type:** Word (physical page number)
**Access:** Read (specifies which PGU bit to clear)

The operand specifies the physical memory page number whose PGU bit should be cleared.

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
- **Illegal instruction code (IIC):** Invalid opcode
- **Illegal operand value (IOV):** Page number out of range
- **Addressing traps:** Invalid address for operand

---

## Data Status Bits

No flags affected (Z, S, C, V unaffected).

---

## Examples

### Example 1: Clear PGU bit after loading page from disk

```assembly
        % Page loaded from disk to physical page PHYS_PAGE
        BI ZPGU PHYS_PAGE
        % PGU bit now clear, page marked as "not recently used"
```

**Explanation:** After swapper loads a page from disk, clear its PGU bit so it starts with "not accessed" status.

### Example 2: Clock algorithm - clear bit for second chance

```assembly
        % Clock algorithm: give page second chance
        BI1 RPGU CANDIDATE_PAGE
        IF=GO EVICT_PAGE            % Not used, evict it
        % Page was used, give second chance
        BI ZPGU CANDIDATE_PAGE
        % Continue to next candidate
```

**Explanation:** Classic clock/second-chance algorithm: check PGU bit, if set give page second chance by clearing bit.

### Example 3: Reset working set after sampling period

```assembly
        % Reset all PGU bits after sampling
        W1 := 0                     % Start at page 0
RESET_LOOP:
        BI ZPGU W1
        W1 INC
        W1 COMP MAX_PAGES
        IF<GO RESET_LOOP
        % All PGU bits cleared for next sample period
```

**Explanation:** Periodic reset of all PGU bits to measure page usage in next time interval.

### Example 4: Selective reset for aging

```assembly
        % Age pages: clear PGU bits of old pages
        W2 := PAGE_LIST
AGING_LOOP:
        W1 := (W2)                  % Get page number
        IF=GO AGING_DONE
        BI ZPGU W1                  % Clear PGU bit
        W2 := W2 + 4                % Next page
        GO AGING_LOOP
AGING_DONE:
```

**Explanation:** Implement page aging by selectively clearing PGU bits of pages in specific lists.

### Example 5: Indexed page table update

```assembly
        % Clear PGU bit using page table index
        W3 := PT_INDEX
        BI ZPGU PAGE_TABLE(W3)
```

**Explanation:** Use indexed addressing to clear PGU bit based on page table entry.

### Example 6: Swapper main loop

```assembly
        % Swapper: load page and reset PGU
        CALL LOAD_PAGE_FROM_DISK    % I1 = physical page loaded
        BI ZPGU I1                  % Clear PGU bit
        % Update page table entry
```

**Explanation:** Standard swapper pattern: load page, immediately clear PGU bit.

### Example 7: Working set protection

```assembly
        % Protect working set: clear PGU only for non-WS pages
        BI1 RPGU PAGE_NUM
        IF=GO SKIP_CLEAR            % Not accessed, already clear
        W1 := PAGE_NUM
        CALL IS_IN_WORKING_SET
        IF><GO SKIP_CLEAR           % In WS, don't clear
        BI ZPGU PAGE_NUM            % Not in WS, clear for aging
SKIP_CLEAR:
```

**Explanation:** Selective PGU clearing: protect working set pages from eviction.

---

## Performance Notes

- **Execution:** 2-3 cycles
  - Hardware table update: 1-2 cycles
  - Both program and data tables updated: +1 cycle
- **Implementation:** PGU tables maintained by MMU hardware
- **Overhead:** Minimal - direct hardware table write

**Usage recommendations:**
- Use after every page load from disk (swapper requirement)
- Combine with RPGU for clock/second-chance algorithms
- Periodic bulk reset for working set analysis
- Essential for proper virtual memory operation

**Algorithm integration:**
```
Clock/Second-Chance:
  1. RPGU - check if page used
  2. If used: ZPGU - clear bit, continue
  3. If not used: evict page

Aging:
  1. Sample period: let PGU bits accumulate
  2. RPGU - read all bits
  3. ZPGU - clear all bits
  4. Repeat
```

**Relationship to other instructions:**
- `ZPGU` clears, `RPGU` reads PGU bits
- Used together for all page replacement algorithms
- Part of ND-500 virtual memory instruction set

---

## Reference Manual

**Section:** §16.21
**Title:** Clear Page Used bit

---

## See Also

- [RPGU](rpgu.md) - Read Page Used table (companion read instruction)
- [RPHS](rphs.md) - Read from physical segment
- [WPHS](wphs.md) - Write to physical segment
- [SYSTEM instruction reference](../../instruction-reference/SYSTEM.md)
