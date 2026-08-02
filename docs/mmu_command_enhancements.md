# MMU Command Enhancements for ND-500 Emulator

This document describes the MMU-related command enhancements implemented in the C version (nd500x) that should be ported to the C# implementation.

## Background: Address Decomposition Fix

The ND-500 virtual address format was incorrectly implemented as a 16-bit page number. The correct format per the ND-05.009.4 Reference Manual (p53-54) is:

```
 31-27   26-20    19-11      10-0
+-------+-------+---------+---------+
|  SEG  |   L1  |    L2   | OFFSET  |
| 5 bit | 7 bit |  9 bit  | 11 bit  |
+-------+-------+---------+---------+
```

### Address Decomposition Constants

| Component | Bits | Mask | Shift | Range | Purpose |
|-----------|------|------|-------|-------|---------|
| Segment | 31-27 | 0x1F | 27 | 0-31 | Selects logical segment |
| L1 Index | 26-20 | 0x7F | 20 | 0-127 | First-level page table index (PS_ADI) |
| L2 Index | 19-11 | 0x1FF | 11 | 0-511 | Second-level page table index (PS_ASI/PS_ADI) |
| Offset | 10-0 | 0x7FF | 0 | 0-2047 | Byte offset within 2KB page |

### PST Index Mode Maximum Sizes

| Mode | Name | Description | Max Pages | Max Size |
|------|------|-------------|-----------|----------|
| PS_AZI (0) | Direct | No paging, single physical page | 1 | 2KB |
| PS_ASI (1) | Single-level | L2 index only (L1 must be 0) | 512 | 1MB |
| PS_ADI (2) | Two-level | Full L1+L2 indexing | 65,536 | 128MB |

---

## Command Enhancements

### 1. `showmmu` - MMU Status Display

**Enhancement:** Added address format documentation to help users understand virtual address decomposition.

**New Output Section:**
```
Virtual Address Format (per ND-05.009.4):
  31-27: Segment (5 bits)  - 32 segments per domain
  26-20: L1 Index (7 bits) - 128 entries for PS_ADI
  19-11: L2 Index (9 bits) - 512 entries for PS_ASI/PS_ADI
  10-0:  Offset (11 bits)  - 2KB page size

PST Index Modes:
  PS_AZI (0): Direct - 1 page max (2KB), L1=0 L2=0 required
  PS_ASI (1): Single-level - 512 pages max (1MB), L1=0 required
  PS_ADI (2): Two-level - 65536 pages max (128MB)
```

**C# Implementation Notes:**
- Add this documentation section to the ShowMMU command output
- Helps users understand address translation without consulting manual

---

### 2. `showpst <psn>` - PST Entry Details

**Enhancement:** Added maximum addressable size information based on index mode.

**New Output Lines:**
```
Index Mode:       PS_ASI (Single-level paging)
  - Max Pages:    512 (L2 index 0-511)
  - Max Size:     1 MB
  - L1 Index:     Must be 0
```

For PS_ADI:
```
Index Mode:       PS_ADI (Two-level paging)
  - Max Pages:    65536 (L1: 0-127, L2: 0-511)
  - Max Size:     128 MB
```

For PS_AZI:
```
Index Mode:       PS_AZI (Direct addressing)
  - Max Pages:    1
  - Max Size:     2 KB
  - L1 Index:     Must be 0
  - L2 Index:     Must be 0
```

**C# Implementation Notes:**
- After displaying the index mode, add detailed information about max pages, max size, and index constraints
- This clarifies what addresses can be translated through each PST entry

---

### 3. `listpst` - PST Entry Listing

**Enhancement:** Added "Max Size" column to the table output.

**Old Output:**
```
PSN   Mode  PFN     Physical Address
----  ----  ------  ----------------
 100  AZI   0x1000  0x00800000
```

**New Output:**
```
PSN   Mode  PFN     Physical Address  Max Size
----  ----  ------  ----------------  --------
 100  AZI   0x1000  0x00800000        2KB
 101  ASI   0x2000  0x01000000        1MB
 102  ADI   0x3000  0x01800000        128MB
```

**Footer Enhancement:**
```
Index Modes: AZI=Direct(2KB), ASI=Single-level(1MB), ADI=Two-level(128MB)
```

**C# Implementation Notes:**
- Add "Max Size" column header and data
- Add index mode legend in footer
- Size values: PS_AZI="2KB", PS_ASI="1MB", PS_ADI="128MB"

---

### 4. `dumppt <psn>` - NEW COMMAND: Page Table Dump

**Purpose:** Displays page table entries for a PST entry that uses paging (PS_ASI or PS_ADI).

**Usage:**
```
dumppt <psn>                    # Dump page table for PSN
dumppt <psn> <start> <count>    # Dump specific range of entries
```

**Output for PS_ASI (L2 page table entries):**
```
=== Page Table for PSN 101 (PS_ASI) ===
Page Table Base: 0x01000000
Showing L2 entries (512 max)

Index  PTE Addr    Raw PTE     PFN     Physical    Prot  Valid
-----  ----------  ----------  ------  ----------  ----  -----
    0  0x01000000  0x00002001  0x0001  0x00000800  R/W   Yes
    1  0x01000004  0x00004001  0x0002  0x00001000  R/W   Yes
    2  0x01000008  0x00000000  0x0000  0x00000000  R/W   No
...
Total: 2 valid entries shown
```

**Output for PS_ADI (L1 page table entries):**
```
=== Page Table for PSN 102 (PS_ADI) ===
L1 Page Table Base: 0x01800000
Showing L1 entries (128 max) - each points to L2 table

Index  PTE Addr    Raw PTE     L2 Base PFN  L2 Phys Addr  Prot  Valid
-----  ----------  ----------  -----------  ------------  ----  -----
    0  0x01800000  0x00010001  0x0008       0x00004000    R/W   Yes
    1  0x01800004  0x00020001  0x0010       0x00008000    R/W   Yes
...
Use 'dumppt <psn> l2 <l1_index>' to view L2 entries
```

**Error Cases:**
- PSN not configured: "PST entry <psn> not configured"
- PS_AZI mode: "PST entry <psn> uses PS_AZI (direct addressing) - no page table"

**C# Implementation Notes:**
- New command to implement
- For PS_ASI: read 512 PTEs starting at (PFN << 11)
- For PS_ADI: read 128 L1 PTEs starting at (PFN << 11)
- PTE format: [31:2]=PFN, [1]=valid, [0]=protection (0=R/W, 1=R/O)
- Show both raw PTE value and decoded fields
- Support optional start/count parameters for large tables

---

### 5. `phyladr <addr>` - Address Translation

**Enhancement:** Now shows L1 and L2 indices instead of combined page number.

**Old Output:**
```
Virtual: 0x0802D455
  Segment: 1, Page: 16474, Offset: 0x455
```

**New Output:**
```
Virtual: 0x0802D455
Address Decomposition:
  Segment:  1
  L1 Index: 0 (0x00)
  L2 Index: 90 (0x5A)
  Offset:   0x455 (1109)
Translation:
  Physical: 0x00012455
```

**C# Implementation Notes:**
- Extract L1 index: (addr >> 20) & 0x7F
- Extract L2 index: (addr >> 11) & 0x1FF
- Display both decimal and hex values for indices

---

## Implementation Priority

1. **High Priority:**
   - Fix address decomposition in MMU translation (if not already done)
   - Update `phyladr` to show L1/L2 indices
   - Add Max Size column to `listpst`

2. **Medium Priority:**
   - Enhance `showmmu` with address format documentation
   - Enhance `showpst` with max addressable size info

3. **Lower Priority:**
   - Implement `dumppt` command for page table viewing

---

## Testing

After implementing these enhancements, verify with:

```
> mmusetup
> listpst
# Should show Max Size column

> showpst 1
# Should show max addressable size for the index mode

> phyladr 0x0802D455
# Should show Segment=1, L1=0, L2=90, Offset=0x455

> dumppt 1
# Should show L2 page table entries for PS_ASI segment
```

---

## Reference Files

- C implementation: `src/debugger/commands.c`
- MMU header: `src/cpu/nd500_mmu.h`
- MMU core: `src/cpu/nd500_mmu.c`
- Original fix documentation: `$RETROCORE/Emulated.HW/ND/CPU/ND500/docs/ND500_MMU_ADDRESS_DECOMPOSITION_FIX.md`
