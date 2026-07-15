# HANDOFF: implement MON 412B FSCNT (FileAsSegment) with REAL file-backed segment mapping

**Full path:** `/home/ronny/repos/nd500x/docs/HANDOFF_412B_FSCNT_FileAsSegment.md`
Date: 2026-07-14
Audience: the LLM implementing MON-call handlers in nd500x (C11 codebase at `/home/ronny/repos/nd500x`).

## TL;DR

`MON 412B FSCNT (FileAsSegment)` in nd500x is BOOKKEEPING-ONLY today - it flips flags in the open-file
table and never backs the logical segment with the file's bytes. Make it actually MAP an open file's
bytes into a real MMU/PST-backed logical segment in the caller's domain. This is the SINGLE blocker
that stops the ND-500 C compiler back-end (CAT-500) from generating object code. Also finish its
counterpart `MON 413B FSCDNT (FileNotAsSegment)` (write-back + free).

## Why this matters (context)

The ND C compiler is a two-part pipeline: the front end `nc-a06.dom` (NC) preprocesses/parses C and
writes a machine-independent intermediate ("CAT code") into the always-open scratch file
(SINTRAN file number 0100 octal = 64 = 0x40 = `SCRATCHnn:DATA`, host-backed by
`.../build/nc_sandbox/SCRATCH/SCRATCH64.DATA`). It then invokes the shared back-end code generator
`CAT-500` (`/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06.dom`, "CAT-500 - Version B06 - 1988-01-05") which
reads that intermediate and emits the `:NRF` object.

CAT-500 does NOT read the intermediate with RFILE. It CONNECTS the scratch file AS A SEGMENT via
412B FSCNT, then reads the CAT code with ordinary MEMORY LOADS from that logical segment (wrapper at
CAT-500 `0x0801F080`, 4 callers). With FSCNT bookkeeping-only, those loads hit unmapped/garbage
memory, CAT-500 prints "can't map scratch file into memmory", and no NRF is produced. Fixing FSCNT is
what lets CAT-500 read its input.

Front half is PROVEN working: NC's intermediate is real and captured at
`/home/ronny/repos/nd500x/build/nc_sandbox/cat_intermediate.dat` (2048 bytes; begins with a numeric
header then `options m2  a4  f-  r4  l+  d+ ...`).

Full CAT-500 MON contract + disassembly: `/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06-MON-contract.md`,
`/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06.asm`. Cross-emulator design: `/home/ronny/repos/nd500x/docs/CAT500_UECOM_CSHARP_HANDOFF.md`.

## Current state (what to replace)

`/home/ronny/repos/nd500x/src/libmon/handlers/mon_412B_FileAsSegment.c` - after validating args it just
does (lines 88-90 + 96):
```
    entry->mapped_as_segment = true;
    entry->mapped_segment_no = log_segment_no;
    entry->segment_access_type = (uint8_t)access_type;
    ...
    ctx->set_error_code(ctx->cpu, log_segment_no);   /* returns seg no in W1 */
```
No physical segment is created; no file bytes are loaded. That is the bug.

`/home/ronny/repos/nd500x/src/libmon/handlers/mon_413B_FileNotAsSegment.c` - PARTIAL; only clears the
flag. Must write the segment back to the file (if it was mapped writable) and free the segment.

## MANDATORY: critical compare against ground truth BEFORE and WHILE implementing

Do NOT implement from this document alone or from the current (buggy) nd500x handler. The current
handler already has a WRONG contract (see AccessType below). Cross-check every semantic against ALL
THREE authoritative sources and reconcile discrepancies explicitly in your commit message / a short
notes file:

1. THE CARVE (SINTRAN L07 - the ONLY ground truth for what SINTRAN actually does):
   - `/mnt/e/Dev/Ronny/NDInsight/tools/sintran-segment-carver/versions/L-VSX-500/re/mon-analysis/412B-FileAsSegment/`
     (README.md, 412B-FileAsSegment.ASM, .pseudo.c, .bin) and `.../413B-FileNotAsSegment/`.
   - Related segment/scratch workers to compare mechanism against:
     `157B-SegmentToPageTable/`, `235B-ScratchOpen/`, `322B-GetSegmentNo/`, `416B-SaveND500Segment/`,
     `422B-GetScratchSegment/`, `53B-GetSegmentEntry/`, `005B-ReadScratchFile/`, `006B-WriteScratchFile/`.
   - CARVE CAVEAT (read it): for 412B the ROUTING is byte-proven (ND-500 System Monitor call via the
     S3SM5 0x60 vector table, slot 0x0274 -> body at `030-S3SM5.bin` offset 0x98dd), but the body
     entry `0x98dd` is SHARED with MON 127B and the disassembly past the entry loads is MISALIGNED -
     the carve's own .pseudo.c states "body semantics, argument-slot mapping, and status/error contract
     are UNVERIFIED". So use the carve to confirm ROUTING/EXISTENCE, NOT to copy an argument/error
     contract. MON dispatch is via `MCTAB @ 005620B` indexed by N.
2. THE MON SPEC (authoritative argument contract): the YAML
   `/mnt/e/Dev/Ronny/NDInsight/Developer/MON/calls/412B_FileAsSegment.yaml` and
   `.../413B_FileNotAsSegment.yaml`, plus the manual "SINTRAN III Monitor Calls" ND-860228.2 EN p.193.
3. THE CONSUMER (what actually has to work): the CAT-500 code + analysis -
   `/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06.asm`, `.../cat-cat5-b06-analysis.md`,
   `.../cat-cat5-b06-MON-contract.md`. CAT-500's 412B wrapper is at `0x0801F080`/`0x0801F08B`, 4
   callers, args (b.0x14 FileNo, b.0x18 LogSegmentNo, b.0x1C AccessType, @b.0x20 SegmentNo[out]).
   TRACE the ACTUAL runtime values CAT-500 passes (esp. which AccessType) and make the handler satisfy
   THAT - CAT-500 producing a valid NRF is the final arbiter, above any doc.

Known discrepancy already found by this compare (fix it): the current handler treats AccessType as
0=read/1=write/2=rdwr and REJECTS 3. The YAML + carve pseudo.c both say AccessType is
0=file-contains-initial-data, 1=uninitialized/empty, 2=primarily-sequential, 3=combination(1+2).
CAT-500's input mapping of the CAT scratch is "initial data" (=0). Do NOT reject 3.

## The contract (412B FSCNT) - per the YAML/manual, reconcile with the compare above

Args (ND-500 INTEGER = 32-bit W):
- [I] arg0 FileNo         : open file number (64-127 / 0100-0177 octal). Must be open.
- [I] arg1 LogSegmentNo   : wanted logical segment number in the domain; 0 = select first free.
- [I] arg2 AccessType     : 0 = file contains initial data, 1 = uninitialized/empty, 2 = primarily
                            sequential, 3 = combination of 1 and 2 (NOT read/write/rdwr).
- [O] arg3 SegmentNo (@)  : segment number actually selected (returned when LogSegmentNo was 0). Note
                            the YAML lists SegmentNo as an OUTPUT PARAM slot; the current handler
                            returns it in W1 via set_error_code - verify against CAT-500's `@b.0x20`
                            (an output-arg pointer) which of the two (W1 vs the arg slot) CAT-500 reads.

Rules: file must be open (else 132B); file number in mass-storage range (else 127B); segment must be
free / not already mapped; AccessType in 0..3. "File contains initial data" (0) means LOAD the file's
bytes into the segment; "uninitialized/empty" (1) means allocate an empty/zeroed segment (no load).
RFILE/WFILE are disallowed on a segment-connected file (caller contract).

## What to implement

### 1. A host callback that creates a FILE-BACKED segment
libmon handlers are emulator-agnostic and reach the CPU/MMU only through `MonContext` callbacks. There
is already `allocate_segment` (used by 422B GSWSP) wired at
`/home/ronny/repos/nd500x/src/cpu/nd500_indirect.c` (`ctx.allocate_segment = nd500_mon_allocate_segment;`)
and implemented at `/home/ronny/repos/nd500x/src/cpu/nd500_segment_alloc.c:113`. That function already
does the hard part: validate/auto-assign the segment number (via `nd500_mmu_get_data_capability`),
round the size up to 2 KB pages, find free physical memory, and wire the domain's DATA capability so
the logical segment translates to those physical pages.

Add a NEW callback that does the SAME segment allocation PLUS loads the file's bytes into the segment's
physical pages. Suggested signature (add to `MonContext` in
`/home/ronny/repos/nd500x/src/libmon/mon_types.h`, next to `allocate_segment`):
```
    int (*connect_file_as_segment)(void* cpu, void* machine, uint8_t domain,
        uint32_t requested_segment, uint32_t access_type,
        const char* host_path, uint32_t file_size_bytes,
        uint32_t* out_assigned_segment);
```
Implement it in `/home/ronny/repos/nd500x/src/cpu/nd500_segment_alloc.c` (model on
`nd500_mon_allocate_segment`):
1. Allocate an MMU/PST-backed data segment at `requested_segment` (or auto) sized to
   `file_size_bytes` rounded to pages - reuse/refactor the existing allocate logic so you do not
   duplicate it (per repo rule: never duplicate code).
2. Fill the segment per AccessType (per the corrected semantics above): AccessType 0 ("initial
   data") or 2/3 -> open `host_path`, read its bytes, and write them into the segment's physical
   pages (`nd500_bus_write8` at the physical base of each mapped page; the CAT code is a raw byte
   stream - copy verbatim, do not byte-swap). Zero-fill any tail past `file_size_bytes` within the
   last page. AccessType 1 ("uninitialized/empty") -> zero the pages, do not load.
3. Set the capability access mode (RO vs RW) from the FILE'S OPEN MODE (`entry->access_mode`), NOT
   from AccessType - AccessType selects initial-data-vs-empty, the open mode governs read/write. For
   CAT-500's input mapping the scratch is opened RANDOM READ/WRITE and read as initial data.
4. Return the assigned segment number via `out_assigned_segment`.

Wire it in `nd500_indirect.c` next to `allocate_segment`:
```
    ctx.connect_file_as_segment = nd500_mon_connect_file_as_segment;
```

### 2. Rewrite the 412B handler to use it
In `mon_412B_FileAsSegment.c`, after the existing validation and after fetching
`OpenFileEntry* entry = mon_file_table_get(file_no)`, call the callback:
```
    uint32_t assigned = 0;
    /* flush the host file so its bytes are on disk before we read them */
    if (entry->host_file) fflush(entry->host_file);
    int rc = ctx->connect_file_as_segment(ctx->cpu, ctx->machine,
                 /*domain=*/ <caller CED>, log_segment_no, access_type,
                 entry->host_path, entry->bytes_in_file, &assigned);
    if (rc != 0) { mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER); return MON_ERROR; }
    entry->mapped_as_segment   = true;
    entry->mapped_segment_no   = assigned;
    entry->segment_access_type = (uint8_t)access_type;
    ctx->set_error_code(ctx->cpu, assigned);   /* W1 = actual segment no */
    mon_set_success(ctx);
```
Notes: `OpenFileEntry` (`/home/ronny/repos/nd500x/src/libmon/mon_file_table.h`) has `host_path[256]`,
`FILE* host_file`, and `uint32_t bytes_in_file`. Use `bytes_in_file` for the size; if it is 0 for a
scratch, stat the host file / fseek-END instead. The caller domain (CED) is available from the CPU -
if libmon should not know CED, have the callback read it from `cpu` internally and drop that param.

### 3. Finish 413B FSCDNT (FileNotAsSegment)
On disconnect: if the file was mapped with write access, read the segment's physical pages back and
write them to `host_path` (truncate to the real length), then free the segment/capability
(counterpart to the allocate) and clear `entry->mapped_as_segment / mapped_segment_no /
segment_access_type`. For CAT-500's read-only input mapping this is just free+clear, but implement the
write-back path too (CAT-500 may map its object output writable).

## Validation

1. Unit: after 412B on a known file, read a few words from the mapped logical segment via the MMU and
   confirm they equal the file's first bytes.
2. End-to-end (the real goal): run CAT-500 against NC's captured intermediate and confirm it maps the
   scratch (no "can't map scratch file into memmory") and generates an object.
   - Intermediate: `/home/ronny/repos/nd500x/build/nc_sandbox/cat_intermediate.dat` (place/keep it as
     `.../SCRATCH/SCRATCH64.DATA` so CAT-500's auto-scratch open finds it; verify the scratch auto-open
     does NOT truncate an existing file).
   - Driver harness pattern: see `/home/ronny/repos/nd500x/test/diag_domload.c` (loads
     `/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06.dom`, steps to MON 0B LEAVE). Build diag harnesses with gcc
     against `build/lib/*.a` (NOT via make); run FROM `build/nc_sandbox` with `ND500X_PIN_CLOCK=1`.
   - Success = a non-empty `BOUT.NRF`; validate its structure against the golden ND-produced object
     `/mnt/d/ND/500/FraTor/test-real/test-real.nrf`.

## ND-500 memory management + ISOLATION (authoritative - ND-05.009.4 EN ND-500 Reference Manual)

Full detail on how logical segments, domains, capabilities, the PST and processes relate - this is
what determines whether a file-backed segment is correctly placed and isolated. All from the ND-500
Reference Manual (`/mnt/e/Dev/Ronny/NDInsight/Reference-Manuals/ND-05.009.4 EN ND-500 Reference Manual.md`,
sections around lines 851, 939-945, 1237-1262).

Address translation (logical -> physical), per access:
- A logical (virtual) address = [ logical segment no (5 bits, 0..31) | segment-relative address
  (27 bits) ]. Each domain therefore has 32 logical segments, each 27 address bits (up to 128 MB).
- The 27-bit logical segment address is translated by the memory-management system to a location in
  a PHYSICAL SEGMENT. A physical segment is divided into 2 KB PAGES and may be 2^11..2^27 bytes in
  1-page units. Pages swap between main memory and secondary storage on demand.
- All physical segments are described in the PHYSICAL SEGMENT TABLE (PST), which always resides in
  main memory; each physical segment has a 16-bit PST entry (an indexing mechanism handles multi-page
  segments). `PSTP` (Physical Segment Table Pointer) is ONE GLOBAL register for the whole system.

Domains, capabilities, the Process Segment (isolation core):
- A process may refer to up to 256 DOMAINS of data and instructions, connected in a DOMAIN TREE
  (mother/child links fixed at domain creation). A process may use up to 256*32 physical segments of
  program and an equal number of data.
- Per domain there is a DOMAIN INFORMATION TABLE holding a 32-entry PROGRAM capability table and a
  32-entry DATA capability table - one pointer per logical segment. Each pointer indicates the PST
  entry describing the physical segment that logical segment addresses, TOGETHER WITH the legal
  access-mode indicators. One (PST-pointer + access-mode) pair is a CAPABILITY. The domain info table
  also holds the trap/domain-call information.
- The domain info tables live inside a special physical segment, the PROCESS SEGMENT (PS) - ONE PS
  PER PROCESS; its size grows with the number of domains the process can use. The PS is itself an
  ordinary physical segment addressed through its PST entry; the per-process `PS` register points at
  that PST entry and is reloaded on every process switch.
- Per-process registers: each process has its OWN copy of `CED` (Current Executing Domain), `CAD`
  (Current Alternative Domain), and `PS`. `PSTP` is the only global one. So execution context =
  {CED, CAD, PS} + the register block.
- ISOLATION FALLS OUT OF THIS: a logical segment resolves ONLY through the executing domain's
  capability table within the executing process's PS. Two domains/processes are isolated UNLESS a
  capability in each points at the SAME PST entry (that is exactly and only how sharing is done). A
  Translation Speedup Buffer (TSB) caches (logical page -> physical page) keyed by logical address +
  DOMAIN number + PROCESS id, so even the cache is domain/process-scoped.

What this means for the two-program pipeline (NC front end -> CAT-500 back end):
- NC and CAT-500 are separate DOMAINS run sequentially by the ND-500 monitor (Loader Monitor
  ND-60.136.04A, sec 8.1 PLACE-DOMAIN / RECOVER-DOMAIN; "after execution control returns to the
  monitor command processor and another domain may be executed"). They do NOT need to co-reside in
  physical memory and do NOT share memory - they share the CAT intermediate through the SINTRAN FILE
  SYSTEM (the always-open scratch file 0100 octal). So a nested/sequential invocation model is
  faithful; physical isolation between the two is the architectural default.

What this means for FSCNT specifically:
- "Connect a file as a segment" = install a CAPABILITY in the CALLING domain's DATA capability table
  at logical segment `LogSegmentNo` (or first free), pointing at a PST entry for a physical segment
  whose pages hold the file's bytes, with the access mode set from the file's open mode. It is placed
  in the caller's (CED) domain only - isolated to that domain/process by construction.
- `nd500_mon_allocate_segment` (`/home/ronny/repos/nd500x/src/cpu/nd500_segment_alloc.c:113`) already
  performs this exact wiring for an EMPTY scratch segment (used by 422B GSWSP): it checks/auto-assigns
  the segment via `nd500_mmu_get_data_capability`, sizes to 2 KB pages, finds free physical memory,
  and installs the data capability. FSCNT is the SAME wiring with the pages PRE-LOADED from the file
  (AccessType 0/2/3) or zeroed (AccessType 1). Refactor a shared helper; do not duplicate.

## Do / Don't

- DO reuse the existing allocate/capability-wiring code (refactor a shared helper); do NOT duplicate it.
- DO keep the existing 412B arg validations (they match the carve/manual).
- Do NOT use Unicode in any C source/comment.
- Mirror this same fix in the C# RetroCore emulator per
  `/home/ronny/repos/nd500x/docs/CAT500_UECOM_CSHARP_HANDOFF.md` Part C (both emulators share this gap).

---

## IMPLEMENTED (2026-07-14, session 557c0950) - 412B is now real file-backed mapping

Done per this handoff, with the AccessType correction applied. Changes:
- `/home/ronny/repos/nd500x/src/cpu/nd500_segment_alloc.c`: refactored the allocation core into
  `static int alloc_backed_segment(..., int writable, ..., uint32_t* out_phys_base)` (shared - NO
  duplication). `nd500_mon_allocate_segment` is now a thin wrapper (writable=1). Added public
  `nd500_mon_connect_file_as_segment(cpu, machine, domain, requested_segment, access_type,
  writable, host_path, file_size_bytes, out_assigned_segment)`: allocates the segment then loads
  the file bytes into its physical pages (AccessType 0/2/3 load; 1 = leave zeroed). If size is 0
  it fseeks the host file for the real length. PTE protection + DC_WRP now honour `writable`.
- `/home/ronny/repos/nd500x/src/libmon/mon_types.h`: added the `connect_file_as_segment` callback
  to MonContext.
- `/home/ronny/repos/nd500x/src/cpu/nd500_indirect.c`: wired
  `ctx.connect_file_as_segment = nd500_mon_connect_file_as_segment;`.
- `/home/ronny/repos/nd500x/src/libmon/handlers/mon_412B_FileAsSegment.c`: AccessType validated
  0..3 (was 0..2, rejecting 3 - WRONG); `writable` derived from the file's OPEN mode
  (ACCESS_SEQ_READ/RAND_READ/RAND_READ_CTG -> RO, else RW), NOT from AccessType; fflush host file;
  call the callback; return the assigned segment in W1.

VALIDATED (handoff step 1): `/home/ronny/repos/nd500x/test/diag_fscnt.c` maps
`/mnt/d/ND/500/FraTor/test-real/test-real.nrf` (AccessType 0), enables the data MMU, and reads the
mapped logical segment via nd500_mmu_peek - the first 32 bytes MATCH the file
(`0A 00 01 70 44 00 ...`). PASS. Regression: linker still boots + reads commands (422B GSWSP uses
the same refactored core); full build clean.

STILL TODO (parallel session owns the end-to-end + can drive it):
- END-TO-END: I do not have `build/nc_sandbox/cat_intermediate.dat` in this tree, so I could not run
  CAT-500 -> BOUT.NRF. Please run it (place the intermediate as SCRATCH/SCRATCH64.DATA, drive
  cat-cat5-b06.dom) and confirm "can't map scratch file into memmory" is gone + BOUT.NRF is produced
  and structurally matches test-real.nrf.
- 413B FSCDNT: still only clears flags. Write-back (read segment pages -> host file when mapped
  writable) + free the segment/capability is NOT yet done - needed if CAT-500 maps its OUTPUT
  writable. Flagged for follow-up.
- Caveat: `connect_file_as_segment` loads the ENTIRE file up front (not demand-paged). Fine for the
  small CAT scratch; revisit for very large files.
- C# RetroCore mirror (CAT500_UECOM_CSHARP_HANDOFF.md Part C) still pending.
