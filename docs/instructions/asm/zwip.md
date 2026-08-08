# ZWIP - Clear Written In Page Bit

## Overview

**Mnemonic:** `zwip`
**Function:** Clear Written-In-Page (dirty bit)
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `BI ZWIP <bit no.>`

---

## Description

Clears (sets to 0) a specified bit in the Written-In-Page (WIP) table. This instruction is the write counterpart to RWIP and is used by operating system page management code after writing dirty pages to disk, ensuring the page's dirty bit is cleared after successful writeback.

**Operation:**
```
0 → WIP_table[page_number]
Clears both program and data WIP tables simultaneously
```

**Key Characteristics:**
- Privileged instruction (supervisor mode only)
- Clears WIP bit in both program AND data tables
- Used after page writeback to disk
- Essential for maintaining correct dirty page state
- Installation-dependent (requires knowledge of physical memory configuration)

**Important Hardware Detail:** ND-500 systems have separate WIP tables for program and data memory. ZWIP clears the specified bit in BOTH tables simultaneously. This means an ND-500 system cannot have physically separate memory for program and data at the same physical address.

**Common Use Cases:**
- Clearing dirty bit after successful page writeback to disk
- Page swapper routines after flushing dirty pages
- Maintaining correct page state in virtual memory system
- Preparing pages for eviction after disk sync
- Page table maintenance in OS kernel

**Operands:** 1 (bit/page number to clear)
**Variants:** 1 opcode (BI prefix only)

---

## Variants

Total variants: 1

| Variant | Opcode | Data Type | Assembly |
|---------|--------|-----------|----------|
| 1/1 | 0xFE9C | BI | BI ZWIP |

**Note:** Only BI (bit) prefix is supported. Use RWIP with H prefix to read 16 bits at once, but ZWIP only clears one bit at a time.

---

## Operands

### Operand 1 (Page Number/Bit Number)

**Type:** Word (physical page number)
**Access:** Read (specifies which WIP bit to clear)

The operand specifies the physical memory page number whose WIP bit should be cleared.

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

### Example 1: Clear WIP bit after page writeback

```assembly
        % Page written to disk, clear dirty bit
        W1 := PHYS_PAGE
        CALL WRITE_PAGE_TO_DISK
        BI ZWIP W1
        % Page now marked as clean
```

**Explanation:** After successfully writing dirty page to disk, clear its WIP bit to mark it clean.

### Example 2: Swapper flush routine

```assembly
        % Flush dirty page and clear WIP
FLUSH_PAGE:
        BI1 RWIP PAGE_NUM           % Check if dirty
        IF=GO ALREADY_CLEAN         % Not dirty
        % Page is dirty, write it back
        W1 := PAGE_NUM
        CALL SYNC_PAGE_TO_DISK
        BI ZWIP PAGE_NUM            % Clear dirty bit
ALREADY_CLEAN:
        % Page is clean, ready for reuse
```

**Explanation:** Standard pattern: check WIP, write if dirty, clear WIP bit after successful write.

### Example 3: Batch page flush with ZWIP

```assembly
        % Flush all dirty pages in range
        W2 := START_PAGE
FLUSH_LOOP:
        BI1 RWIP W2
        IF=GO NEXT_PAGE             % Not dirty, skip
        % Dirty page, flush it
        W1 := W2
        CALL FLUSH_PAGE_TO_DISK
        BI ZWIP W2                  % Clear WIP after write
NEXT_PAGE:
        W2 INC
        W2 COMP END_PAGE
        IF<=GO FLUSH_LOOP
```

**Explanation:** Loop through page range, flushing dirty pages and clearing WIP bits.

### Example 4: Page eviction with WIP handling

```assembly
        % Evict page, flushing if dirty
EVICT_PAGE_SAFELY:
        BI1 RWIP VICTIM_PAGE
        IF=GO CLEAN_EVICT           % Clean page, no flush needed
        % Dirty page, must flush first
        W1 := VICTIM_PAGE
        CALL WRITE_TO_DISK
        BI ZWIP VICTIM_PAGE         % Mark clean
CLEAN_EVICT:
        % Safe to evict (clean or flushed)
        CALL EVICT_PHYSICAL_PAGE
```

**Explanation:** Safe page eviction: flush dirty pages before eviction, clear WIP bit.

### Example 5: Indexed page table update

```assembly
        % Clear WIP for indexed page
        W3 := PT_INDEX
        CALL WRITEBACK_PAGE_AT_INDEX
        BI ZWIP PAGE_TABLE(W3)
```

**Explanation:** Use indexed addressing to clear WIP bit from page table.

### Example 6: Lazy writeback optimization

```assembly
        % Only flush if page is dirty
CHECK_AND_FLUSH:
        BI1 RWIP PAGE_TO_CHECK
        IF=GO NO_WORK_NEEDED        % Already clean
        % Dirty, needs flush
        W1 := PAGE_TO_CHECK
        CALL DISK_WRITE
        BI ZWIP PAGE_TO_CHECK
        % Return status: flushed
        W1 := 1
        RETURN
NO_WORK_NEEDED:
        % Return status: was already clean
        W1 := 0
        RETURN
```

**Explanation:** Conditional flush: only write and clear WIP if page is actually dirty.

### Example 7: Emergency page flush before shutdown

```assembly
        % Flush all dirty pages before system shutdown
        W1 := 0                     % Start page
SHUTDOWN_FLUSH:
        BI2 RWIP W1
        IF=GO NEXT_SHUTDOWN_PAGE    % Clean
        % Dirty page, emergency flush
        CALL URGENT_DISK_WRITE
        BI ZWIP W1                  % Clear WIP
NEXT_SHUTDOWN_PAGE:
        W1 INC
        W1 COMP MAX_PAGES
        IF<GO SHUTDOWN_FLUSH
        % All dirty pages flushed
```

**Explanation:** System shutdown: flush all dirty pages to ensure data consistency.

---

## Performance Notes

- **Execution:** 2-3 cycles
  - Hardware table update: 1-2 cycles
  - Both program and data tables updated: +1 cycle
- **Implementation:** WIP tables maintained by MMU hardware
- **Overhead:** Minimal - direct hardware table write

**Usage recommendations:**
- Always ZWIP after successful disk writeback
- Never ZWIP before disk write completes successfully
- Pair with RWIP for dirty page detection
- Essential for maintaining virtual memory consistency
- Critical for preventing data loss on page eviction

**Algorithm integration:**
```
Dirty Page Writeback:
  1. RWIP - check if page dirty
  2. If dirty: write to disk
  3. ZWIP - clear dirty bit after successful write
  4. Page now safe to evict

Page Eviction:
  1. RWIP - check dirty
  2. If dirty: flush to disk, ZWIP
  3. If clean: skip flush
  4. Evict page
```

**Relationship to other instructions:**
- `ZWIP` clears, `RWIP` reads WIP bits
- Similar to `ZPGU`/`RPGU` but for dirty page tracking
- Used together for all page replacement algorithms
- Part of ND-500 virtual memory instruction set

---

## Reference Manual

**Section:** §16.18
**Title:** Clear Written In Page bit

---

## See Also

- [RWIP](rwip.md) - Read Written-In-Page table (companion read instruction)
- [ZPGU](zpgu.md) - Clear Page Used bit
- [RPGU](rpgu.md) - Read Page Used table
- [RPHS](rphs.md) - Read from physical segment
- [WPHS](wphs.md) - Write to physical segment
- [SYSTEM instruction reference](../../instruction-reference/SYSTEM.md)
