# Handoff: NC codegen crash at 0x08023EA4 (resume point)

**Full path:** `/home/ronny/repos/nd500x/docs/HANDOFF_NC_CRASH_0x08023EA4.md`
Date: 2026-07-11. Status: paused; DAP being added to nd500x to continue the trace.

## Goal
Get the ND-500 C compiler (`/mnt/d/ND/500/FraTor/nc/nc-a06.dom`) to run to a
complete compile producing a real .NRF, then link it. The last blocker before
real object bytes is a crash during code generation.

## The crash (reproduced identically on BOTH emulators)
- Fires after ~1.5-1.87M instructions of code generation.
- Faulting instruction (same PC on C and C#):
  `0x08023EA1  42 F4 00  by test R1.0x0000` - dereferences R1.
  Reported PC is `0x08023EA4` (the next instruction) due to PC-advance-before-
  dispatch (`cpu.c` advances PC then dispatches).
- R1 is a wild pointer: C side `0x81D881C8`, C# side `0x30000002`. Different
  garbage per emulator; same code, same crash site.

## What the crash is (from C#'s measured backward-trace)
Full C# trace: `/mnt/d/ND/500/nc-codegen-crash-0x08023EA4-trace.md`.
It is a linked-structure (list/tree) walk:
```
0x08023E9C  w1 := B.0x14     ; R1 := arg1 = root pointer 0x18003000
0x08023E9E  w1 := R1.0x2     ; R1 := mem[R1 + 2 words] = the "+2 link" field
0x08023EA1  by test R1.0x0   ; dereference R1  -> FAULT when +2 field is garbage
```
- For real heap nodes (0x1802xxxx) the +2 link is always a valid seg-0x18
  pointer (6 times in the trace).
- Only when the walk reaches the GLOBAL ROOT (0x18003000) is the +2 field
  garbage (1 time) -> crash.
- Anchors: root pointer stored to global @ 0x08000204 at ~instr 6755
  (`0x0800801D w1 =: 0x08000204` storing 0x18003000). Crashing routine is
  0x08023DB7 (outer 0x08024352), both take args (0x18003000, x).
- C# measured: the wild value appears exactly once in the whole trace, only as
  this read's result - on the C# side NO instruction ever writes mem[0x18003002].

## What the C (nd500x) side established (measured, this session)
Ruled OUT the static-loader / DOM-image class of causes:
1. The DOM has ONLY Segment 1 (PROG+DATA at load_addr 0). Segment 3 (where
   0x18003000 lives - top 5 bits = 3) is NOT in the DOM image.
2. At load time all of segment 3 is unmapped (every access traps).
3. Segment 3 is RUNTIME-established: by ~instr 8000 it is mapped (memmap shows
   `18000000-18018FFF seg 3 ASI Paged PFN 1261+`), to fresh high physical pages.
4. RAM is calloc-zeroed (`src/machine/io.c:40`). The seg-3 physical pages read
   all-zero when fresh (`m! 0x00275800` -> all 00). So NOT physical aliasing at
   setup; a clean unwritten +2 field would read 0 (a valid list terminator).
5. NEW DIVERGENCE vs C#: on the C side the +2 field IS actively written - at
   instr 8000, `m 0x18003000` shows word +2 = 0x00051800 (not zero, not garbage
   yet). C# measured their equivalent field is NEVER written. So either
   0x18003000 hosts different structures at instr 8000 vs at the 1.5M crash
   (heap reuse), OR the two emulators diverge on whether the field is written.

Conclusion: NOT a static loader/DOM bug. It is a runtime data-flow issue inside
the runtime-established segment 3 - i.e. how the root structure's +2 link is
(or is not) populated during code generation.

## The one experiment that settles it (DO THIS ON RESUME)
Hold a WRITE WATCHPOINT on the root's +2 link field across the FULL ~1.5M-
instruction NC run, to capture the LAST writer before the crash (or prove
nothing writes it).
- CLI instrument (works today, no DAP needed):
  `wp <addr> 4 write` in `./build/bin/nd500x --debug`.
- DAP instrument (being added): data breakpoint - nicer, IDE-driven.
- Requires driving NC to codegen: feed NC console input via `-i <file>` (the
  `compile B,B,B` or `check`+`generate-code` command); the debugger REPL reads
  stdin separately, so the two input streams do not collide.
- Reproduction source used by C#: `setit(){}` (compiles clean, 2048-byte :CAT),
  then `check B,B,B` + `generate-code B,BOUT`. Crash also fires on empty CAT.

Key question to answer with the watchpoint: what writes the wild value into the
root's +2 field (or, if nothing does, why the walk reaches the root at all
instead of terminating on a 0 link) - that decides shared-CPU-bug vs
shared-runtime-init vs missing-MON-side-effect.

## UPDATE 2026-07-11 (writer found; memory layer cleared)
- C# watch caught the writer: PC 0x0802CEFE `w1 =: r2.0` (32-bit WORD store)
  wrote 0x30000002 into mem[0x18003002] at instr 1,848,798 (~22k before crash).
  Reader 0x08023E9E `w1 := r1.2` is a 32-bit WORD load. Both CONFIRMED word-width
  by nd500x's own decoder. The earlier "no instruction writes it" and "halfword
  store" claims are both WRONG.
- Signature (C# correlation): stored = (low16(*0x0801D544) << 16) | 0x0002.
  For root 0x18003000, low16 = 0x3000 -> 0x30000002 (top byte 0x30 = unmapped
  segment -> PV when later dereferenced). NOT a clean halfword swap (that would
  be 0x30001800).
- list-head 0x0801D544 (seg 1, in DOM): ships as 0 in the DOM image; runtime sets
  it to a valid 0x18003004 by instr 8000; holds only valid 0x1800xxxx ptrs or 0
  (a push/pop "current node"). So it is NOT itself the corruption source.
- nd500x memory layer CLEARED: nd500_read/write_memory_32 (instruction_helpers.c
  112-159) are clean big-endian and round-trip unaligned-within-a-page correctly.
  So the corrupt value must arrive in W1 BEFORE the store (upstream operand/value
  chain), not from the store or the byte assembly.
- NEXT: capture W1 at 0x0802CEF8 (post indirect load `w1 := @b.0x24`) AND at
  0x0802CEFE (pre-store). If W1 already wrong post-load -> the load/operand
  resolution or an upstream instr is the bug; if clean post-load but wrong at
  store -> the store path. Also verify `.2` offset = +2 BYTES (not word-scaled).
- LATENT BUG (not this crash): read/write_memory_32 translate only the base vaddr
  then do paddr+1..3 in PHYSICAL space -> a 32-bit access crossing a 2048-byte
  page boundary corrupts. 0x18003002 stays in-page so not the culprit here; fix
  later. C# should check their equivalent path.

## UPDATE 2026-07-11 (ROOT CAUSE FOUND: node use-after-free)
Full writeup: `/home/ronny/repos/nd500x/docs/NC_CRASH_0x08023EA4_ROOTCAUSE.md`.
Established with `test/diag_nc_writer_watch.c` (standalone, in-sandbox, no DAP).
- The reader walks a list of heap nodes (0x1802xxxx). The SAME node 0x1802A1B0
  is walked healthy (node[+2]=valid ptr) then fatal (node[+2]=garbage) in one
  run - its memory was OVERWRITTEN between walks by NC's OWN store instructions
  (PCs 0x0802CF63/0x08024853/0x08024887/0x0802CF01), including a 0xF0F0F0F0
  poison free-fill. NC freed and REUSED the block while the reader's list still
  pointed at it -> use-after-free.
- `w1 := r1.2` confirmed = 32-bit read at byte offset +2 (unaligned); emulator
  offset is CORRECT.
- OVERTURNS the earlier writer/pack theories: the 0x0802CEFE writer (targets root
  word 0) is a red herring; C#'s (low16<<16)|2 "pack law" was coincidental heap
  state; wild value varies per run (reused-block content).
- Open question: is the reuse an NC-internal aliasing bug both emulators
  faithfully reproduce, or driven by an emulator heap/instruction defect upstream
  (GETB/FREEB/ENTB)? Next: instrument NC's node allocator / DAP write-watch the
  node's +2 field.

## Debugger facts (verified this session)
- `bp cond <addr> <condition>` - conditional breakpoints; evaluator knows PC,
  I1.. (`src/machine/breakpoints.c:284,266`).
- `wp <addr> [len] read|write|change`, `wp reg <PC|I1-I4|L|B|R>`
  (`src/machine/breakpoints.h`).
- `m` (data, MMU), `mp` (program), `m!` (physical, bypass MMU), `memmap`.
- `run` is ASYNC (background pthread) - a scripted `run` then `regs` shows stale
  state. Use `step N` for synchronous scripted inspection.
- Load: `loaddom <path>`. Start addr 0x08000004. Domain auto-allocated to 1.

## Related open items (not this crash)
- 321B error code: C uses 124, C# uses 129. Neither manual-grounded. OPEN -
  joint decision. Noted in `docs/NC_TOOLCHAIN_MON_PLAN.md`.
- `int x;` at file scope: C# NC rejects it ("error in SIMPLE_DECLARATOR").
  Whether nd500x NC rejects it too is UNMEASURED. Independent of the crash.
- Linker startup blockers (separate track): 144B MAGTP unimplemented, 50B OPEN
  empty-filename. C runtime libs NC-LIB/CAT-LIB are ABSENT from /mnt/d/ND media
  (needed for the link half). See `docs/HANDOFF_DAP_INTEGRATION.md` and
  `/mnt/d/ND/nd-linker/nd500-c-compile-and-link.md`.

## DAP integration (in progress by user)
See `/home/ronny/repos/nd500x/docs/HANDOFF_DAP_INTEGRATION.md`. Prioritize data
breakpoints (Phase 2) - that is the instrument this crash trace needs.
