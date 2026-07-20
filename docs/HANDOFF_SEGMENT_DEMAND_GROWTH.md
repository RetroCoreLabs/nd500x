# HANDOFF: MON-connected segments are demand-grown (PS_ADI), not sized to the file

Date: 2026-07-20
Files changed:
- `/home/ronny/repos/nd500x/src/cpu/nd500_segment_alloc.c`
- `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.c`
- `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.h`

Cross-emulator target: RetroCore `Emulated.HW/ND/CPU/ND500` MMU + MON 412B/422B.

## Symptom

Loading an object file in the ND Linker B01 died with a hard page fault:

```
[MMU] TRAP: PS_ASI page fault! L1=4 must be 0! vaddr=0x18402004
[STOP] page fault at PC=0xB001F826 data=0x18402004
```

VA `0x18402004` decodes (ND-500 VA = `[Segment(5)|L1(7)|L2(9)|Offset(11)]`) as
**segment 3, ~4.2 MB in**. Segment 3 is the linker's output domain, connected via
`MON 412B FSCNT` with `AccessType=1`.

## Root cause

Two independent errors, both in `alloc_backed_segment()`:

1. **The segment was sized to the backing file's current length.** For the domain
   file that was 4096 bytes (2 pages). `412B_FileAsSegment.yaml` defines
   `AccessType 1` as *"uninitialized empty file"* - the file is empty **by
   definition**, so its length says nothing about how much the caller will
   address. The linker writes megabytes into it.

2. **The segment was built in `PS_ASI` (single-level) mode**, which caps a
   segment at 512 pages = 1 MB because L1 must be 0. Even a correct size guess
   above 1 MB could not have been represented. That is precisely the `L1=4 must
   be 0` in the trap.

The ND-500 Reference Manual (ND-05.009.4, line 1237) describes the intended model:

> A physical segment is divided into blocks of 2k bytes called pages, and **may
> have any size from 2\*\*11 to 2\*\*27 bytes** in units of 2k bytes (1 page).
> **Pages can be moved (swapped) between main memory and secondary storage as the
> need arises.**

i.e. real hardware demand-pages a segment rather than committing its extent at
connect time.

## Fix

MON-allocated segments (412B FSCNT and 422B GSWSP) are now built in **`PS_ADI`**
(two-level) mode, spanning the architectural 128 MB (L1 = 7 bits = 128 tables x
L2 = 9 bits = 512 pages x 2 KB). Only the pages actually needed are mapped at
creation; a data fault inside such a segment allocates the missing L2 table
and/or page and lets the access retry. Untouched pages cost nothing.

New pieces in `nd500_segment_alloc.c`:

- `GrowableSegment g_growable[64]` - registry of (domain, segment) -> L1 table.
- `nd500_segment_grow_on_fault(cpu, vaddr, domain)` - called from the MMU.
- `growable_map_page()` / `nd500_segment_write_bytes()` - page-aware mapping and
  content preload.

Hooked into `nd500_mmu.c` at **both** `PS_ADI` miss points (L1 invalid and L2
invalid), data accesses only - an instruction fetch still traps as before.

### Two subtleties worth copying

**The L1 entry must stay writable even for a read-only segment.** `PS_ADI` checks
the protection bit at *both* levels on a write:

```c
if (is_write && (l1_pte.protection != 0 || l2_pte.protection != 0)) -> trap
```

so a read-only L1 entry would deny writes to every page beneath it. The segment's
RO/RW protection is applied at the L2 (data) entry only.

**Physical page allocation had to stop using `find_highest_used_pfn()`.** That
helper only walks `PS_ASI` page tables, so it is structurally blind to a
`PS_ADI` segment's data pages and would hand the same physical page out twice.
Allocation now comes from a monotonic watermark (`g_next_free_pfn`), seeded once
per machine from `find_highest_used_pfn()` and only ever moving up.

A consequence: **a segment's pages are no longer physically contiguous**, so
there is no single physical base to `memcpy` into. `alloc_backed_segment()` now
returns `*out_phys_base = 0`, and FSCNT preloads file content through
`nd500_segment_write_bytes()`, which walks the mapping page by page.

## Verification

- `test/diag_fscnt.c`: **PASS** - the mapped segment's first 32 bytes still match
  the file byte-for-byte through the new non-contiguous path.
- `test_mmu_translation`, `test_mmu_separate_id`: exit 0.
- `test_instruction_validation --continue`: ALL TESTS PASSED.
- `ctest`: 16/18, with the same two pre-existing failures (`float_arithmetic`,
  `mon_calls`) - `mon_calls` reports the identical 3 sub-failures before and
  after this change.
- ND Linker, from `/home/ronny/repos/nd500x/build/link_sandbox`:
  ```
  ../bin/diag_linkdrive /mnt/d/ND/500/nd-linker/linker-b01.dom \
      "OPEN-DOMAIN NEWDOM;;LOAD CATLIB;;EXIT" 80000000
  ```
  Before: page fault at instr 423946.
  After: no fault; all three segments report `mode=ADI`; the load runs to
  instr 530271 and EXIT produces real linker output -
  `1 undefined entries on current domain.` and an
  `Entry selection (Undefined,Defined,All)` prompt.

## Still open (NOT fixed here)

The linker still rejects every NRF fed to it with
`*** ERROR - "4" in module  is illegal control byte. (0054:16)` - including the
genuine vendor library `/mnt/d/ND/500/c-libs/CAT-LIB-B06.NRF`. That is a separate
defect; before this change CATLIB simply page-faulted before reaching the parse.

## What the C# side must mirror

- Build MON-connected segments in `PS_ADI`, not `PS_ASI`; never derive a
  segment's extent from the backing file's current length (fatal for
  `AccessType 1`, "uninitialized empty file").
- Grow on data page-fault inside such a segment; keep L1 entries writable and put
  RO/RW protection on the L2 entry.
- If any equivalent of `find_highest_used_pfn()` exists, it must not be the
  allocator's source of truth once two-level segments exist.
