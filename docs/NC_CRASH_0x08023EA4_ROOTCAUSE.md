# NC codegen crash 0x08023EA4 - root cause: node use-after-free / reuse

**Full path:** `/home/ronny/repos/nd500x/docs/NC_CRASH_0x08023EA4_ROOTCAUSE.md`
Date: 2026-07-11. Established on the nd500x (C) side with a purpose-built
synchronous diagnostic, reproduced in-sandbox (no DAP server needed).

## Tool
`/home/ronny/repos/nd500x/test/diag_nc_writer_watch.c` - standalone, links the
built static libs, drives the real NC compiler (nc-a06.dom) with
`COMPILE A,A,A\r` from `build/nc_sandbox`, and captures CPU registers + memory
at chosen PCs WITHOUT perturbing the run (probe reads clear any trap they raise).
Build:
```
gcc -O2 -o build/bin/diag_nc_writer_watch test/diag_nc_writer_watch.c \
  -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
  build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
  build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
```
Run: `cd build/nc_sandbox && ../bin/diag_nc_writer_watch <dom> "COMPILE A,A,A\r" 2000000`

## The crash (reproduced)
PV at PC 0x08023EA4 (fault on `0x08023EA1 by test r1.0`), R1 = wild pointer
(0xA1B8A1A8 this run; value varies per run - 0x81D881C8, 0x30000002, 0xA1B8A1A8
all seen). Deterministic instruction count ~1.49M on the C side.

## Confirmed facts (all MEASURED)
1. `w1 := r1.2` (reader, 0x08023E9E) reads a 32-bit word at byte offset +2
   (UNALIGNED). Verified: healthy node 0x1802A1B0 has node[+2]=0x1802A1A0 and the
   reader loads exactly 0x1802A1A0. So the emulator's read offset/scaling is
   CORRECT (+2 bytes, not +2 words).
2. The reader walks a linked list of heap nodes (0x1802xxxx). R1 is the current
   node, loaded from the previous node's +2 link field.
3. The SAME node 0x1802A1B0 is walked twice in one run:
   - step 1119537 (healthy): node[+0]=0x00021802, node[+2]=0x1802A1A0 (valid link)
   - step 1492915 (fatal):   node[+0]=0x1804A1B8, node[+2]=0xA1B8A1A8 (garbage)
   The node's own memory was OVERWRITTEN between the two walks.
4. The overwrites are NC's OWN store instructions (poll-watch of 0x1802A1B0):
   PC 0x0802CF63 (0xF0F0F0F0 -> 0), 0x08024853, 0x08024887, 0x0802CF01,
   0x0802402F, 0x08024036. 0xF0F0F0F0 is a poison/free fill. The node is put on
   a free list and REALLOCATED for other data several times between the walks.

## Root cause (class)
NODE USE-AFTER-FREE / REUSE. The reader's list still contains a pointer to node
0x1802A1B0, but NC's list-allocator (the 0x0802CFxx / 0x08024xxx routines that
also maintain the 0x0801D544 free-list head) freed and reused that block for
unrelated data. Walking the stale pointer reads the reused block's bytes as a
"next" pointer -> wild -> PV. This is the SAME CLASS as the earlier GETB "no
rewrite" bug (a heap block live in two structures at once).

## What this OVERTURNS
- C#'s "halfword pack law" stored = (low16(*0x0801D544)<<16)|0x0002 was
  coincidental heap state, NOT the mechanism. The per-run variation of the wild
  value (0x30000002 / 0x81D881C8 / 0xA1B8A1A8) is reused-block content, not a
  deterministic pack.
- The writer at 0x0802CEFE (w1 =: r2.0) targets root word 0 (0x18003000) and is
  a RED HERRING for this crash; it is not the corrupter of the walked node.
- "store-vs-upstream at 0x0802CEFE" is not the question; the memory layer is
  clean (verified) and the corrupting writes are ordinary NC stores.

## Instruction-level detail (added after deeper probing)
- The doomed node's link is stored AND read at byte offset +2 (UNALIGNED), a
  symmetric pair:
    writer 0x08024884  `20 E5 34  w1 =: IND(b.52)(r2)`  (32-bit word store @ +2)
    reader 0x08023E9E  `0C F4 02  w1 := r1.2`           (32-bit word load  @ +2)
  Both straddle the node's 16-bit halfword fields: a store puts W1.high into
  word0.low and W1.low into word1.high; the read reconstructs the pointer as
  (word0.low << 16) | (word1.high). For a valid node word0.low = 0x1802 (the
  seg/page tag of these 0x1802xxxx nodes) so the reconstructed link is a valid
  0x1802xxxx pointer.
- Fatal case: 0x08024884 stored W1 whose HIGH half was already 0xA1B8 (an
  offset-shaped value, not the 0x1802 seg tag) -> word0.low = 0xA1B8 ->
  reconstructed link 0xA1B8A1A8 -> PV. So W1 was ALREADY corrupt before the
  store; the store is faithful. W1 came from 0x08024874 `w1 := IND(b.48)(r1)`
  (indexed indirect load) - i.e. from the recycled node's own contents.
- Every instruction on this path (unaligned word load/store, indexed indirect
  load) behaves CONSISTENTLY: the SAME code produces valid pointers thousands of
  times and garbage exactly once (on the recycled node). That is the fingerprint
  of corrupt INPUT DATA (a reused node), not a miscomputing instruction. The
  emulator's arithmetic/addressing is exonerated here.
- Corrected node-0x1802A1B0 word0 lifecycle (writer PCs fixed for the poll's
  off-by-one), A,A,A run:
    1119028 PC=0x0802CF60  0xF0F0F0F0 -> 0            (free/poison-clear)
    1119130 PC=0x0802484F  0 -> 0x00020000            (build header hi)
    1119146 PC=0x08024884  0x00020000 -> 0x00021802   (build header lo = 0x1802)
    1119369 PC=0x0802CEFE  0x00021802 -> 0x1802A1D0   (set link, word store)
    ... node reused several times ...
    1412870 PC=0x0802CEFE  0x1802A1B8 -> 0x18038000
    1412947 PC=0x0802484F  0x18038000 -> 0x18048000
    1412963 PC=0x08024884  0x18048000 -> 0x1804A1B8   (word0.lo becomes 0xA1B8!)
    1492963 WALK reads node[+2]=0xA1B8A1A8 -> crash

## UPDATE 2: it is an OVERLAPPING HEAP ALLOCATION (likely an emulator heap bug)
Probing the corrupting store 0x08024884 `w1 =: IND(b.52)(r2)` showed it is part of
a BUFFER-INIT loop: target = *(B+0x34) + R2, base *(B+0x34) = 0x1802A1AE, R2
increments, W1 stored = 0x20202020 (space fill) and 0xF0F0F0F0 (poison). So NC
allocated a NEW buffer at 0x1802A1AE and is initializing it - and 0x1802A1AE
OVERLAPS the still-live list node at 0x1802A1B0 (starts 2 bytes into it). The
space/poison fill clobbers the node's +2 link -> wild pointer -> crash.

Key point: every address computation is FAITHFUL (target = *(B+0x34)+R2 computed
correctly each iteration). The emulator is not misdirecting stores; it faithfully
executes NC writing into a block ITS ALLOCATOR HANDED OUT while an overlapping
block (0x1802A1B0) was still in use. That is an OVERLAPPING ALLOCATION - the SAME
CLASS as the earlier GETB "no rewrite" bug (buddy allocator returning overlapping
blocks), which was a confirmed EMULATOR bug.

UPDATE 3 (corrects UPDATE 2's "likely buddy-heap bug"): NC does NOT use the
buddy-heap instructions for these nodes - the whole nc-a06.asm has GETB x1 and
ZERO FREEB/ENTB/RETB. So the allocator at 0x0802CFxx/0x08024xxx is NC's OWN
(ordinary loads/stores), which the emulator executes FAITHFULLY. The buddy-heap
"same class as GETB" hypothesis is therefore WRONG for this crash.

Revised assessment (is it an emulator bug? - honest, calibrated):
 - It is NOT the crash-site instructions, NOT the store/pack, NOT address
   arithmetic, and NOT the buddy heap - all verified faithful / not used.
 - The overlapping allocation (0x1802A1AE over live 0x1802A1B0) is produced by
   NC's OWN allocator logic, which the emulator runs correctly instruction by
   instruction. So a SIMPLE emulator bug is unlikely.
 - That leaves two live hypotheses, distinguishable only by the cross-emulator
   diff (or deep NC-allocator archaeology):
     (b) a SUBTLE upstream emulator instruction bug feeds NC's allocator one
         wrong value (e.g. a size/count/pointer), so NC then legitimately (by its
         own correct logic) computes an overlapping allocation; OR
     (c) NC relies on SINTRAN pre-seeding its heap/free-space bookkeeping that we
         do not provide, so NC's allocator free-pointer is wrong from the start.
   Real ND-500 shipped this compiler working, and BOTH emulators reproduce the
   crash - which leans toward (c) (missing environment/seeding) or a shared (b).

Decider: the cross-emulator node-lifecycle diff (below). If C and C# node
histories are byte/instruction identical -> not an instruction-level emulator
bug -> (c) or an NC-internal issue both reproduce. If they diverge -> the first
divergent instruction is the (b) emulator bug.

## UPDATE 4: memory helpers CLEARED (tested, not eyeballed)
Hypothesis: are the CPU memory helpers buggy (mis-assembling the unaligned +2
link or the halfword fields)? Tested two ways, both clear them for this crash:
 - In-page: test/diag_mem_helpers.c exercises read/write_memory_{8,16,32} at
   aligned/unaligned addresses incl. the EXACT NC pattern (two halfword stores
   building a word, then the unaligned +2 word read reconstructing the link).
   15/15 pass; big-endian byte order correct; unaligned round-trip correct.
   Both access paths agree (instruction_helpers.c and cpu_instr.c mmu_read/write
   -> nd500_bus_read/write are byte-wise big-endian, correct for in-page).
 - Page-crossing: instrumented the real operand path (mmu_read/write16/32) and
   ran NC to the crash - ZERO non-contiguous page-crossing accesses. The latent
   base-only-translation bug (logged as BUG-2 in DAP_BUG_REPORTS.md) never fires
   during NC, so it is not this crash either.
Conclusion: the memory layer is faithful; the corruption is genuinely NC's own
allocator (overlapping allocation), executed correctly by the emulator. This
reinforces (b)/(c) over any emulator memory-access bug.

## UPDATE 5: NC's allocator IS the buddy heap (GETB) - corrects UPDATE 3
Disassembling the allocator NC calls to get the overlapping block:
  0x08024842  call 0x0802CF08  (allocator, arg = size)
  0x0802CF08  ents; computes alog2(size+3) -> log_size; then:
  0x0802CF60  `w1 getb r1`   <-- THE buddy-heap GETB (the single GETB in NC)
So every NC allocation flows through ONE GETB call wrapped in a log2-size
routine. UPDATE 3's "buddy heap not involved" was WRONG - it IS the allocator.
NC uses no FREEB instruction; freeing (if any) is via direct freelist stores /
the STO trap handler.
MEASURED (instrumented nd500_heap_alloc_block, guarded DIAG_GETB, reverted):
GETB returns the SAME block addresses repeatedly (0x18002448, 0x18002450, ...
each handed out many times) and blocks covering the doomed node 0x1802A1B0 are
issued more than once. Since there is no FREEB, a re-issued live block = the
overlap that corrupts the reader's list.
OPEN (next): is the reuse LEGITIMATE (NC frees blocks back to the FLOG freelists
via direct stores, so GETB correctly re-issues them) or a BUG (GETB fails to
unlink, or NC's STO trap handler adds still-live memory to a freelist)? Decider:
trace the freelist head before/after each GETB for the doomed size, and watch the
STO trap handler (does STO fire? what does it push to FLOG?). If a block is
re-issued while a pointer to it is still live in the reader's list AND NC never
freed it, GETB/STO is the (b) emulator bug; if NC freed it, it is NC-internal/(c).

## UPDATE 6: buddy heap mechanically EXONERATED - NC frees a live block
Instrumented nd500_heap_alloc_block (guarded DIAG_GETB, reverted): after each
unlink, walked the freelist to detect double-membership/cycles, and counted STO.
MEASURED:
 - ZERO FL-DOUBLE and ZERO FL-CYCLE events -> GETB's unlink is correct; the
   freelists are never corrupted. The repeated block returns (UPDATE 5) are
   LEGITIMATE: NC frees blocks back to the freelist and GETB re-issues genuinely
   free blocks.
 - STO fires only TWICE, both early (instr 740 log=10, instr 77922 log=15) - far
   from the crash window (1.49M) and the doomed allocations (>1.1M). So the STO
   trap-handler heap extension is NOT the mechanism.
Conclusion: the buddy allocator is behaving correctly. The crash is NC FREEING
the doomed block while a pointer to it is still live in the reader's list, then
GETB correctly reallocating it. The bug is in the FREE decision (NC's), not GETB.
=> still (b) NC frees based on a bad upstream emulator value, or (c) NC-internal
/ missing seeding. Buddy heap is not the culprit (mechanically verified twice).
NEXT: catch the FREE of the doomed block - the NC store that pushes 0x1802A1B0's
covering block onto a FLOG freelist - and the register/condition driving it.
Cross-check that condition's inputs against C# (ties back to the 504B-class
divergence hunt). The cross-emulator convergence test (C# fixes 504B, both
re-run; converge => (c), diverge => (b)) remains the cleanest decider.

## UPDATE 7: FINAL local conclusion - dangling pointer to a scope-freed block
Two more measured checks (guarded probes, reverted):
 - Cross-level free-block overlap: ZERO (buddy heap consistent across FLOG levels
   too; 0x1802A1B0/16B and 0x1802A1B8/8B were free at DIFFERENT times, not
   simultaneously).
 - Live-overlap (assuming no free): 237 events starting at instr 8078 - BUT NC
   runs fine to 1.49M despite them. Reconciled: those overlaps are HARMLESS
   reuse, which proves NC DOES free (my "no FREEB => no free" assumption was too
   narrow). The allocator 0x0802CF08 manipulates TOS/THA and uses ents/entt/rett
   -> NC frees by SCOPE UNWINDING (heap-stack pop on routine return), not FREEB.
 - The FLOG writes seen in the free-catch were all GETB-internal split pushes
   (PC 0x0802CF63); NC's other block stores go to the stack (seg 0x10).

FINAL MECHANISM (as far as black-box tracing can go): the crash is a DANGLING
POINTER. NC stores a pointer to a heap block into a longer-lived structure (the
one the crash reader walks), that block's SCOPE later unwinds (freeing it), GETB
reuses the memory for an overlapping allocation, and the stale pointer is then
dereferenced -> wild -> PV. The buddy heap is CORRECT (verified 4 ways: no
same-level double-membership, no cycles, no cross-level free overlap, and the
live-overlaps are legitimate scope reuse). NOT the buddy heap, NOT GETB, NOT the
memory helpers, NOT addressing - all faithful.

WHY the pointer escapes its scope is NC-logic-internal and cannot be resolved by
black-box tracing. It is either (b) a subtle emulator value that makes NC store
the escaping pointer / mis-time the scope unwind, or (c) NC-internal / an
environment difference. The DECIDER remains the cross-emulator convergence test
(C# fixes its 504B I1 deviation, both re-run: converge => (c)/NC-internal;
diverge => the first divergent instruction is the (b) emulator bug). That test
needs the C# side and is the only way forward; local instrumentation is
exhausted.

## The open question (next session)
Is NC's reuse of a still-referenced node:
 (a) an NC-internal bug (its own two lists aliasing a node) that both emulators
     faithfully reproduce because NC is the same binary - i.e. NOT an emulator
     bug; or
 (b) driven by an emulator defect UPSTREAM that made NC's allocator believe the
     block was free.

DECISIVE NEXT STEP - cross-emulator lifecycle diff (cheap, no new theory):
The node-0x1802A1B0 word0 lifecycle above (instr counts + writer PCs) is a
deterministic fingerprint. Have the C# side dump the SAME node's lifecycle
(their TestNC_CodegenCrashWatch already has the hooks) and diff:
 - IDENTICAL sequence  -> no instruction-level emulator divergence; the bug is
   NC-internal or an environment/seeding difference BOTH emulators share. Then
   check whether NC expects SINTRAN to pre-seed its heap / free-list (a state we
   do not set up) - i.e. NC may rely on initial memory neither emulator provides.
 - DIVERGENT sequence  -> the first divergent instruction IS the emulator bug;
   fix that emulator.
Both emulators already crash at nearly the same instruction count (~1.49M C /
~1.87M C# - note that gap is itself worth explaining; it may be console-input or
MON-timing differences, not codegen), which leans toward (a).

Secondary probe: at the moment node 0x1802A1B0 is recycled (~instr 1332-1412k),
is it reachable from NC's free-list head (0x0801D544 chain)? If YES, NC
legitimately reused a "free" node while another list still pointed at it (NC bug
/ seeding). If NO, something put the node back into use without freeing it.
Instrument by walking the 0x0801D544 chain at that instant (diag tool can be
extended). DAP data breakpoints would also serve once a server is hosted.

## Cross-emulator note
Both nd500x (C) and RetroCore (C#) crash at the identical PC and similar instr
counts. If the root cause is (a) NC-internal, both are CORRECT and the "crash"
is NC compiling something it cannot (e.g. the specific source), or NC relies on
an initial memory/heap state neither emulator provides. That possibility (NC
needs a specific initial heap/free-list seeding from SINTRAN that we do not
set up) should be checked too.

## UPDATE 8: 504B/I1 divergence resolved against the SINTRAN manual (no emulator bug here)
Chased the documented decider (the MON 504B I1 divergence with C#) to its source
in the SINTRAN restart microcode and the monitor-call manual - authoritative per
project rules.

EVIDENCE (manual = TRUTH):
 - Param table, ND-860228.2 EN (OCR line 1745): DVOUTS 504B parameters are
   1=LDN/file (INT,I), 2=NoOfBytes (INT,I), 3=String (ARR,I) - ALL Input-only,
   plus an ERR return. Nothing legitimately returns a computed value in I1/W1.
 - Restart microcode (ND500-MONITOR-CALL-PARAMETER-PASSING.md, sec 5.1-5.2):
     OKMONICO (success): T:=0; A:=0; D:=0   -> FUNCV=0, KFLIP=0
     EMONICO  (error):   FUNCV = error code, KFLIP=1
   On a SUCCESSFUL monitor call SINTRAN forces the function value to 0 and clears
   the K flag; on error it writes the error code and sets K.

nd500x behavior (verified in code):
 - mon_set_success() (src/libmon/mon_params.c:265) clears K only; does NOT touch
   I1. The dispatch epilogue (src/cpu/nd500_indirect.c:257) writes the error code
   to I[0]=W1 ONLY when (error_flag && error_code != 0). So on 504B success I1 is
   LEFT UNCHANGED.
 - This matches the manual on the essential point: no Output param, K cleared.
   (nd500x preserves I1 rather than zeroing FUNCV, but FUNCV/A/D is the ND-500
   function-value register, not I1 - so preserving I1 is not a divergence source
   for anything that reads I1 as data.)

CONCLUSION for the cross-emulator hunt:
 - If C# writes a NON-ZERO value into I1 on a SUCCESSFUL 504B, that CONTRADICTS
   the manual (OKMONICO forces the function value to 0, and 504B has no Output
   param) and is the C#-side bug - NOT nd500x. nd500x's 504B leaves I1 correct.
 - Therefore the 504B I1 difference, if real, points at the C# side; it does not
   implicate an nd500x emulator bug. The crash decider still needs both sides
   re-run, but the manual now predicts convergence once C# stops mutating I1 on
   504B success.
 - Open follow-up (separate from the crash): confirm which register nd500x should
   use for the MON error code on ND-500. The restart doc says the function value/
   error code lives in the A/D (FUNCV) register, while nd500x uses I[0]=W1. NC has
   run correctly to 1.49M instructions with the W1 convention, so NC evidently
   reads its error status from W1/K here - but this A-D vs W1 question is worth a
   dedicated check against a MON call that NC actually tests for error.

## UPDATE 9: error-register convention clarified - UPDATE 8's "A/D vs W1" was a cross-CPU conflation
Followed the ND-500 monitor-call MECHANISM doc (ND500-MONITOR-CALL-MECHANISM.md)
to settle where the MON return/error value actually lives. Correction to UPDATE 8:

 - The `OKMONICO: A:=0; D:=0` / `EMONICO: FUNCV=error` code is ND-100-side NPL
   (the SINTRAN driver's OWN A/D/T accumulators), NOT ND-500 registers. UPDATE 8's
   "A/D-FUNCV vs W1" follow-up conflated the two CPUs - there is no ND-500 A/D-vs-W1
   discrepancy to chase.
 - Real mechanism: FUNCV (function value, double word) and KFLIP (error flag) live
   in the 5MPM SHARED-MEMORY message buffer (sec 2.2). ND-100 writes them there;
   ND-500 microcode copies them back on resume via the 3MONCO restart microfunction
   ("read return value from message", sec 9). No ND-500 CPU register is named by
   the driver code - the mapping is the ND-500 microcode's job.
 - nd500x abstracts this entire inter-processor round-trip into an inline handler:
   error_code -> W1 (I[0]) and KFLIP -> K flag. This is a MODEL, not the hardware
   path, but it is functionally validated: NC runs correctly to 1.49M instructions,
   so its runtime reads error status from W1/K exactly as nd500x provides it. If W1
   were the wrong register, NC would misbehave on the FIRST error-returning MON call,
   far before the crash window.

NET: the error-register convention is NOT a divergence source. Combined with
UPDATE 8 (504B leaves I1 correct per the manual), the MON-call return path on the
nd500x side is manual-consistent. The remaining crash decider is purely the
cross-emulator convergence RE-RUN, which needs the C# side; every locally
investigable MON-return angle is now closed.

## Local investigation status: CLOSED
All locally tractable leads are exhausted and documented (UPDATES 1-9):
 - Crash mechanism: dangling pointer via scope unwind (UPDATE 7) - characterized.
 - Buddy heap / GETB / memory helpers / addressing: mechanically exonerated.
 - 504B I1 decider: resolved against the manual, points to the C# side (UPDATE 8).
 - MON error-register convention: not a divergence source (UPDATE 9).
The one open item - the actual convergence re-run and the 1.49M-vs-1.87M instr-gap
explanation - fundamentally requires diffing the two emulator traces, i.e. the C#
side. It cannot be advanced by nd500x-local analysis alone.

## UPDATE 10: deterministic clock added - both emulators now pinnable for the PC-diff
The C# side fixed its own non-determinism (commit 34d3dc930, DeterministicClock)
and, re-running with the 504B I1 fix (commit ca8d18573), got PERSISTENT DIVERGENCE:
C# crashes at 1,870,520 / node 0x18003000 vs nd500x 1,492,964 / node 0x1802A1B0,
both at PC 0x08023EA4. Per the UPDATE 8 decider, a real emulator bug therefore
exists and the next step is a PC-level trace diff. For that diff to isolate the
REAL divergence and not light up on clock MON calls, both emulators must return the
SAME time stream.

nd500x time-source audit and fix (this session):
 - 114B TUSED: was CLOCK_MONOTONIC elapsed. 113B CLOCK / 142B: were time()+localtime.
   41B ROBJE dates: were host stat() st_ctime/atime/mtime. 11B was ALREADY
   deterministic (instruction_count/40000).
 - Added src/libmon/mon_clock.{h,c}: default OFF (real clock); env ND500X_PIN_CLOCK
   pins to 1990-01-01 12:00:00 UTC (epoch 631195200). When pinned: 113B/142B ->
   fixed UTC instant, 114B TUSED -> 0, ROBJE dates -> fixed instant. Routed all four
   sources through it. Full ctest 18/18 pass.
 - Verified: 6/6 fresh pinned runs bit-identical at 1,492,964. Unpinned stable at
   1,492,947 -> the clock shifts NC's stream by +17 (same magnitude as C#'s jitter).
   (A transient off-by-17 in pinned runs was leftover build/nc_sandbox/SCRATCH state
   from a prior unpinned run, not an in-process entropy source.)

Exact pinned values handed to C# (must match, esp. the ROBJE date encoding):
 114B=0; 113B=[2160000,0,0,12,1,1,90]; 41B ROBJE date word=0xA042C000
 (year-offset-from-1950 6b | month 4b | day 5b | hour 5b | min 6b | sec 6b).
Handoff: /mnt/d/ND/500/nc-crash-0x08023EA4-ND500X-clock-pinned-ANSWER.md

NEXT (both sides): confirm C#'s pinned 113B/114B/41B equal the table (watch the
ROBJE bit layout), then diff the checkpoint traces. The first divergent PC after
~7570 is the genuine (b) emulator bug driving the 1.49M-vs-1.87M crash-count gap.

## UPDATE 11: pinned checkpoint trace delivered; clean-tool crash count = 1,492,885
C# verified the clock contract (ROBJE 0xA042C000, 113B buffer, 114B=0 all match;
DateHelper.ToNDDate uses the identical bit layout) and asked for nd500x's pinned
full-run checkpoint trace to run the PC-diff.

Delivered: new NON-PERTURBING tracer test/diag_nc_checkpoints.c (does NOT probe
memory or clear traps, unlike diag_nc_writer_watch). Emits `instr PC B I1 I2 I3 I4`
(hex, stride 5000, from instr 0) -> /mnt/d/ND/500/nd500x-fullrun-checkpoints.txt.
Run pinned (ND500X_PIN_CLOCK=1) from the fixture sandbox; bit-reproducible.

Authoritative CLEAN crash count = 1,492,885 (PC 0x08023EA4, wild I1=0xA1B8A1A8).
The earlier 1,492,964 came from diag_nc_writer_watch, whose per-instruction probe
reads (with nd500_trap_clear) perturb the run by ~79 instructions. Use 1,492,885.

Crash re-confirmed REAL (not an instrumentation artifact): the non-probing tool
still crashes identically. Tail checkpoints show NC's free-fill poison 0xF0F0F0F0
in I1 at instr 1,480,000 and 1,490,000, just before the fatal walk - consistent
with the use-after-free (node freed/poisoned, then walked).

IMPORTANT sandbox caveat discovered here: the sandbox SCRATCH/GUEST state is a
LARGE determinant of the run, not just the +17 clock jitter. With GUEST emptied
(A.C removed) NC exits early at 1,085,641 via MON 0B LEAVE instead of crashing.
Both emulators must run from the SAME source fixture (GUEST/A.C present, empty
SCRATCH) for the diff to be valid.

Deterministic clock + tracer committed to main (4fa340a).
NEXT: C# runs the line diff by instruction number; the first divergent checkpoint
window brackets the split, then a per-instruction trace over that window (both
sides) pins the exact first divergent PC = the genuine emulator bug.
Handoff: /mnt/d/ND/500/nc-crash-0x08023EA4-ND500X-checkpoints-ready.md

## UPDATE 12: FIRST DIVERGENCE PINNED AND FIXED - comp2 halfword Z-flag bug
The C# checkpoint diff showed a split before instr 5000, but that was a FIXTURE
MISMATCH: nd500x used source A.C (`int x; main(){ x=1; }`, CR endings) + command
`COMPILE A,A,A`; C# used B.C (`setit(){}`, LF) + `check/generate-code/exit`. The
different source and different-length command shift NC's input loop. Adopting the
IDENTICAL C# fixture and re-diffing the per-instruction window [0,6000] gave a real
first divergence at instr 1610:
  08029839: FC 16 F4 23 3F  h comp2 r1.35,$-1   (instr 1608, sets flags)
  0802983E: C7 00 57        if><go  $87          (instr 1609, branch if NOT equal)
Probe: R1=0x18000000, halfword[R1+0x23]=0xFFFF, immediate=-1(=0xFFFF as H) => EQUAL,
so `if><` must NOT branch. nd500x branched (ST1 after compare = 0x02, Z bit5 clear);
C# fell through (correct).

ROOT CAUSE (nd500x Comp2.c): read both operands into 64-bit and tested result==0 on
the full 64-bit difference. Operand fetch delivered them differently extended -
memory halfword zero-extended (0x0000FFFF) vs immediate -1 sign-extended
(0xFFFFFFFFFFFFFFFF) - so 0xFFFF-0xFFFF...F = 0x10000 != 0 -> Z wrongly cleared.
FIX (commit 2848b05): mask both operands and the result to the datatype width
before deriving Z/C/S (comp2 compares operands OF THE DATATYPE per the manual).
VERIFIED: window [0,6000] now bit-identical to the C# trace; 39,598 validation
cases pass; ctest 18/18 pass.

DOWNSTREAM: comp2 was the FIRST divergence, not the only one. COMPILE A,A,A still
crashes at 0x08023EA4 (now 1,492,266). With the aligned setit fixture, nd500x
(post-fix) runs PAST 1.87M without crashing (to the 3M cap) while C# crashes at
1,870,520 - a NEW later divergence to chase. Protocol continues: re-diff full
checkpoints, pin divergence #2, fix whichever side the manual says is wrong.
Tools: test/diag_nc_pctrace.c (per-instruction window), diag_nc_checkpoints.c.
Handoff: /mnt/d/ND/500/nc-crash-0x08023EA4-FIRST-BUG-FOUND-comp2.md

## UPDATE 13: comp2 fix converges to instr ~1.095M; divergence #2 bracketed
Committed the prior TEMM/LREGBL + shift-doc work (87015f3) and MON 413B test
(723bbe3). Then drove the next convergence iteration:
 - Found+fixed a tooling bug: diag_nc_checkpoints ignored the ";;" command
   separator and fed NC a malformed one-line command, which had made nd500x
   appear to "run past 1.87M without crashing". With the correct aligned command
   (check;;generate-code;;exit) nd500x CRASHES at 1,393,773 (PC 0x08023EA4), i.e.
   EARLIER than C# (1,870,520) after the comp2 fix.
 - Re-diffed corrected full checkpoints: nd500x and C# are now BIT-IDENTICAL
   through instr 1,095,000 (comp2 fix moved agreement from 1610 -> 1.095M).
   First divergent checkpoint at 1,100,000: identical registers, PC 0800F653
   (nd500x) vs 0800F644 (C#) - loop-phase offset. Divergence #2 window =
   [1,095,000, 1,100,000].
 - Divergence region is a BYTE comp2 -> if><go loop at 0800F644..F653
   (by comp2 b.55,#127). comp2 BYTE path already masked to 0xFF by the fix, so the
   split is likely a small earlier instruction-count offset, not this compare.
Delivered nd500x per-instruction window trace
/mnt/d/ND/500/nd500x-window2-1095k-1100k.txt; awaiting C#'s matching window trace
to pin the exact first divergent instruction. Checkpoint tool ";;" fix committed.
Handoff: /mnt/d/ND/500/nc-crash-0x08023EA4-DIVERGENCE2-window-1095k.md

## UPDATE 14: divergence #2 = MON 117B RFILE EOF-on-short-block (nd500x manual-grounded)
C# pinned divergence #2 at step 1,096,233, PC 0802DE7C (a 117B RFILE call) + `if -k
go`: C# returns I1=0x1000/K=0 (success) vs nd500x I1=3/K=1 (EOF). Instrumented the
nd500x 117B call (DIAG_117B, reverted): the divergent call reads file 101B 'B'
(source B.C = setit(){}\n, 10 bytes), NoOfBytes=4096, bytesRead=10, block 0,
block_size 2048, pos 0->10, file_len 10 => a SHORT read that DRAINED the file (got
all 10 bytes, fewer than the 4096 requested, reached EOF). nd500x delivers the 10
bytes then returns code 3 / K=1.

Documentary support (manual ND-860228.2 EN), upgrading the earlier "undocumented
inference":
 (a) RFILE 117B parameter table has NO "number of bytes read" output (params: file,
     wait, buffer[O], block, NoOfBytes) - unlike InByte/In8Bytes/InputString/M8INB
     which all have one. So code 3 / K is the ONLY EOF signal an RFILE caller gets;
     success-on-short-read would leave the caller unable to detect EOF or know the
     count.
 (b) SMAX 73B (manual line 20175): "error code 3 is returned if you later try to
     read beyond this size. Error code 3 means end of file." Reading [0,4096) from a
     10-byte file reads beyond size -> code 3.
So nd500x's EOF-on-short-block is the manual-consistent reading; asked C# to adopt
it behind their flag and re-run (predict crash converges toward nd500x 1,393,773).

Also answered C#'s Comp.c question and FIXED it (commit 98dbb6a): Comp masked the
RESULT for Z/sign (Z was correct) but compared raw 64-bit register+operand for
carry - masked both to datatype width now (same class as the Comp2 fix). 39,598
validation + ctest 18/18 pass.
Handoff: /mnt/d/ND/500/nc-crash-0x08023EA4-RFILE-EOF-answer.md

## UPDATE 15: CONVERGENCE ACHIEVED - both emulators crash identically => NC-internal
Divergence #3 was nd500x's Scomp reading string bytes with nd500_bus_read8 (raw
PHYSICAL) instead of MMU-translated nd500_read_memory_8. With the MMU on it compared
the wrong bytes: two clearly different strings 'if      ' (0x0800A3E8) vs 'setit   '
(0x10000260) compared EQUAL, so scomp returned I1=I2=8/K=0/Z=1 instead of the
correct I1=I2=0/K=1 (differ at byte 0). C# was correct. FIX (commit): use
nd500_read_memory_8. After the fix scomp returns I1=0/I2=0/K=1, matching C#.

RESULT - the cross-emulator decider has delivered its verdict:
 - nd500x and C# are now BIT-IDENTICAL across all 247 shared stride-5000 checkpoints
   (through instr 1,230,000) and CRASH AT THE SAME INSTRUCTION: 1,230,764, PC
   0x080241FC (fault at 0x080241F9 `h2 := r2.0`, then call), same doomed node
   I1=0x1802A1B0, wild value 0x54312D42 = ASCII "T1-B".
 - Two INDEPENDENT emulators (C and C#) now reproduce the crash identically.
   Per the agreed decider (converge => (c)/NC-internal; diverge => emulator bug),
   the remaining crash is NOT a per-emulator bug. It is NC-internal or a shared
   environment/input gap.

Four real emulator bugs were found and fixed by this cross-emulator diff:
 1. Comp2 Z-flag: compared raw 64-bit diff, not masked to datatype width (2848b05).
 2. Comp carry: register operand not masked to datatype width (98dbb6a).
 3. RFILE 117B: EOF-on-short-block contract (manual-grounded; C# adopted).
 4. Scomp: MMU-bypass on string element reads (this update).
The crash PC moved 0x08023EA4 -> 0x080241FC as the trajectory corrected, but it is
the SAME use-after-free node 0x1802A1B0 (a heap node reused to hold the text "T1-B"
- a compiler temp/label - then walked as a pointer).

REMAINING QUESTION (no longer an emulator-diff problem): why does NC itself hit the
dangling node? Candidates: (a) NC needs SINTRAN heap/environment seeding neither
emulator provides; (b) the invocation (check B,B,B / generate-code B,BOUT / exit) is
not how NC is meant to be driven; (c) a gap BOTH emulators share (a MON call both
handle identically-wrong, or an instruction both treat the same). Also outstanding:
~16 other STRING instructions + Ppack/Ppackr still use nd500_bus_read8 (same latent
MMU-bypass) - to fix as NC exercises them.
Handoff: /mnt/d/ND/500/nc-crash-0x08023EA4-CONVERGED.md

## UPDATE 16: post-convergence (a) MMU sweep + (b) NC-invocation/node-lifecycle
(a) Swept the remaining STRING (Schpar/Scopa/Scopt/Scotr/Scpuno/Sfilln/Smatch/
Smovn/Smvtr/Smvtu/Smvun/Smvwh/Sscan/Sskip/Sspan/Sspar) + Ppack/Ppackr/Pupack/
Pupackr instructions off raw nd500_bus_read8/write8 onto MMU-translated
nd500_read_memory_8/write_memory_8 (Entt/Rett already translated explicitly).
39,598 validation + ctest 18/18 pass; NC run still converges (crash unchanged
1,230,764). Commit ca51292.

(b) Invocation: NC-INTERFACE.md documents that `check` writes a .CAT and then
TERMINATES the whole program (MON 0B LEAVE), so C#'s chained
`check;;generate-code;;exit` has generate-code/exit as DEAD CODE. Correct object-
code path is `compile src,list,obj` (one shot) OR `check src,list,cat` then a
SEPARATE program run of `generate-code cat,obj`. But `check` and `compile` reach
the SAME crash, so invocation form is not the trigger.

(b) Node lifecycle traced (poll of node[0x1802A1B0] near the crash):
 - instr 1110136: GETB (PC 0x0802CF63) hands out the block (0xF0F0F0F0 poison ->
   data) - allocated.
 - used as a heap list node (links 0x1802A1D0/1C0; writers 0x08024853/0x08024887).
 - instr 1207023 & 1227286: GETB (PC 0x0802CF01) ZEROES it - freed/recycled.
 - re-written as a node again each time; last write instr 1227520 (PC 0x0802A175)
   -> 0x1802A1E9.
 - instr 1230765: crash - a stale pointer R2 into the recycled region is
   dereferenced (`h2 := r2.0` at 0x080241F9), reading the symbol text "T1-B"
   (0x54312D42) as a pointer -> PV at 0x080241FC.
=> NC's own alloc/free recycles the node while a pointer stays live (matches the
UPDATE 6/7 mechanical exoneration of the buddy heap). Now emulator-INDEPENDENT
(both C and C# converge), so it is NC-internal or a shared environment gap.

Most likely explanation (unproven): NC crashing while preprocessing a trivial
`setit(){}` points at an ENVIRONMENT difference - real SINTRAN gives NC an initial
heap/segment/state that avoids the recycle collision - rather than a genuine NC
defect. Concrete next experiments (not yet run): (1) trace the "T1-B" symbol's
creation and which routine still holds R2 across the free; (2) compare nd500x's
MON 422B GSWSP returned segment size/layout against what NC's heap extender
(0x0802CCF7) seeds; (3) try providing the NC-A:INIT / compile-parameter init NC
may expect.

## UPDATE 17: REFRAME accepted - NC logic is correct, shared UPSTREAM defect; stubbed MON calls are prime suspects
C# reframed: NC crashes on EVERY minimal input (setit(){}, f(){}, main(){},
a(){}b(){} all crash @0x080241FC; a function WITH a body crashes ~1.1M later at the
original 0x08023EA4). A 1989 shipping compiler cannot genuinely crash on setit(){},
so both emulators SHARE a defect vs real hardware (convergence = they agree with each
OTHER, not with real SINTRAN). Confirmed by disassembly of nc-a06.asm - NC's logic is
CORRECT at all three crash-related sites:
 - 0x0802A171: `w1 laddr r1.(0x39); w1 =: @b.0x104` = deliberately writes an
   intra-record CHAR pointer (node+0x00 := &node[+0x39], the inline name buffer).
   Not garbage.
 - 0x0802CEFE: freelist PUSH (free): node.next=[head@0x0801D544]; head=node. Textbook.
 - 0x08024884: list allocation w/ post-increment. Normal.
So NC applies correct code to state already wrong upstream.

Captured full nd500x MON trace (101 calls, /mnt/d/ND/500/nd500x-montrace-full.txt)
for the cross-diff. PRIME SHARED-DEFECT SUSPECTS = MON calls we STUB/canned (both
emulators fake identically -> invisible to cross-diff, wrong vs real SINTRAN):
 - MON 317B ExecuteCommand: STUB - "doesn't execute it, just return success"
   (src/libmon/handlers/mon_317B_ExecuteCommand.c). Called instr 1,124,681. Ties
   directly to the "CMD_FILE" symbol in the recycled node.
 - MON 312B CheckMonCall: returns a "fake entry address" (0x4C), 6x.
 - MON 262B GetSystemInfo: returns fixed 0x0801CA58, 6x.
 - K=1 error returns to verify vs manual: 123B ReleaseResource (err 5 @47685),
   54B DeleteFile (err 124 @1193164).
 - 3x MON 422B GSWSP heap extensions (returns 0x40000/0x80000/0x100000, doubling).
No real-hardware NC trace oracle exists on disk (searched).
NEXT: both sides diff MON traces + audit each stubbed/canned return against the
SINTRAN manual (ND-860228.2). 317B/312B stubs are the likeliest shared defect.

## UPDATE 18: MON audit vs manual - 262B CPUST is the suspect, 422B is contract-correct
C# MON-trace diff (csharp 109 vs nd500x 101 calls) + manual audit (ND-860228.2):
 - 422B GSWSP: manual (p.274) specifies only (size, seg#, ret seg#); geometry is
   SINTRAN-internal, unconstrained. nd500x contract-correct; both auto-assign same
   segment and converge => NOT the shared defect. Deprioritize.
 - 262B CPUST (manual p.262 byte layout) is the SUSPECT and ties to the "NC runs
   nd-100/500 programs" insight:
     byte 2 CPU type: manual defines ONLY 2-5 (ND-100/110/120) - nothing for ND-500.
     byte 3 instruction set: manual defines ONLY 0-3 (ND-100 variants).
   Both emulators hardcode byte2=0, byte3=0 (ND-100 encodings). NC is cross-target
   and likely reads these to pick ND-100-vs-ND-500 codegen/behavior. nd500x's other
   fields OK: 6:7=500, 8=5(VSX-500), 9='L'. NC reads the buffer into a global at
   ~0x0801CB38. NEXT: find which byte offset NC branches on.
 - Real but non-shared (we DIFFER, not the crash): 503B DVINST count 12(C)/14(C#);
   123B RELES nd500x err5/K1 vs C# success; 54B MDLFI empty name nd500x 124 vs C# 46
   - reconcile each vs manual separately.
nd500x canned values sent to C#: 262B 24-byte buffer, 422B geometry, 503B semantics.

## UPDATE 19: DOM data reveals NC multi-pass CAT table; 317B rule-out was return-only
C# empirically ruled out all 4 canned MON RETURNS (crash invariant to 262B garbage,
317B error, 312B zero -> all reproduce instr 1,231,351). SINTRAN L oracle (user):
312B MOINF = capability probe (skip-return entry-or-0); 317B UECOM = executes a
command line synchronously WITH persistent FS side effect (no-op stub wrong);
422B GSWSP = lowest free seg 0..31, size rounded to 2048B pages; 256B/41B/50B layouts.

DOM-load audit (nc-a06.dom): NO fixups/relocations at load; Seg1 PROG LB=0x4000
SZ=0x2ECBE FLA=0, Seg1 DATA LB=0x33000 SZ=0x1E4F1 FLA=0, FUA=0 => NO BSS gap. Loads
map seg1 prog+data at 0x08000000. First 64B of DATA (file 0x33000) = NC's PASS/CAT
name table: "CAT-OPTI-B","CAT-COPY-B","CAT-NC-CROSS-A","CAT-CAT1-B",... => NC is a
MULTI-PASS compiler that runs each pass as a SEPARATE ND-500 program via 317B
ExecuteCommand and reads back the :CAT each pass writes.

KEY: C#'s 317B rule-out toggled the RETURN VALUE (stub-success vs error) - BOTH mean
"pass never ran", so both crash identically. It did NOT test the SIDE EFFECT (actually
running CAT-NC-CROSS/CAT-COPY). So the missing-pass side-effect is NOT ruled out and
is the new lead: NC reads a :CAT a pass should have produced, gets stale/absent data,
symbol walk derefs garbage. Matches oracle (317B must actually execute) + the
"CMD_FILE"/CAT-* symbols.
NEXT: (A) DOM-load diff w/ C# (artifacts produced); (B) trace NC's OPEN/RFILE for
"CAT-*" files near crash - does it read a pass output that never got written?;
(C) core-instruction fallback.

## UPDATE 20: the 317B pass-driver decoded - crash is IN the pass loop; command = "NC-A"
317B UECOM call site: wrapper 0x0802E116 (2 callers: 0x08023F17 in the crash region,
0x0802B379). The wrapper takes 3 args (pass-record fields), builds a command string at
b.0x24 via sprintf 0x0802E208 from a template (0x0801DE98) + two wconv'd numbers
(b.1A,b.1E), then MON 317B with arg descriptor {0x80, &built-string} at b.-0x5C.
Runtime capture (instr 1,124,681): built command = "NC-A" (4E 43 2D 41 27, apostrophe-
terminated). => NC launches each pass by executing the SINTRAN command "NC-A" (re-runs
the NC-A subsystem/domain) with pass params. Only ONE 317B launch before the crash.

BOTH handlers read the command as EMPTY: the 317B arg is a descriptor {maxlen=0x80,
ptr=&string}; mon_read_sintran_string reads the arg slot directly (gets 0x00...) instead
of decoding the descriptor. (My earlier "define-cat-copy" was a mis-deref of the 0x80
value as an address; the real command is "NC-A".)

CRASH IS IN THE PASS-DRIVER LOOP: 0x08023F14 `h2 := r2.0` (09 F5 00, IDENTICAL opcode
to the crash at 0x080241F9) immediately precedes `call 0x0802E116,$3,r1.2,$1,r2.0`.
NC walks a list of pass records (R1/R2), reads a halfword pass field (r2.0), launches
each via the wrapper. Crash = R2 is the corrupted "T1-B" pass record.
=> Fix path: (1) both decode the 317B descriptor + read the real command; (2) implement
317B to actually RUN the "NC-A" pass (re-invoke NC-A domain w/ params) so the :CAT gets
transformed; then NC's pass-record walk finds valid records instead of the stale one.

## UPDATE 21 (2026-07-13): Pass-selection mechanism decoded from real data tables

0x0802E116 is NOT a bespoke pass-driver. It is one entry in NC runtime's generic
MON-call wrapper family:
  0x0802E116  MON 317B UECOM (execute SINTRAN command)
  0x0802E169  MON 412B FSCNT
  0x0802E193  MON 413B FSCDNT
  0x0802E1A6  MON 504B DVOUTS
  0x0802E1CF  MON 503B DVINST
  0x0802E208  string-slice formatter (called by 0x0802E116)

Two adjacent DATA-segment tables found in nc-a06.dom (file offsets):
  0x33008  pass/CAT-file name table (NUL-separated):
           CAT-OPTI-B, CAT-COPY-B, CAT-NC-CROSS-A, CAT-CAT1-B,
           CAT-CAT5-B, CAT-CAT6-B, CAT-CAT3-B, CAT-CAT8-B
  0x3306c  command blob (CONCATENATED, no separators between some entries):
           "NC-A" (0x3306c,len4) "define-optimizer" "define-cat-copy"
           "define-cross" "define-nd-100-back-end" "define-nd-500-back-end"
           "define-68000/32000/80386-back-end" "define-user-interface" ...

Because entries are concatenated ("NC-Adefine-optimizer..."), the pass is selected
by a numeric (start,end) offset pair into the blob, NOT a null-split index and NOT
a :CAT field. 0x0802E116 does exactly this:
  r1 = wconv(b.1E)   ; END index
  r2 = wconv(b.1A)   ; START index
  len = r1-r2+1  -> b.0x20
  call 0x802E208(src=@b.0x14, start=r2, end=r1, out=b.0x24) ; slice
  descriptor {0x80, &b.0x24} -> MON 317B UECOM

Crash caller: 0x08023F17 `call 0x0802E116,$3,r1.2,$1,r2.0`. The END index is
supplied by r2.0 = the corrupted "T1-B" record. So the pass-selection index is read
FROM the corrupted record; the corruption is UPSTREAM of the UECOM call.

Corrects UPDATE-earlier + canonical-invocation.md #3: "define-cat-copy" etc. ARE
real command strings NC issues via 317B UECOM (self-re-invocation of sub-passes),
not purely internal symbols. The mis-deref worry was wrong; the strings are real.

STILL UNKNOWN (the actual root cause): what writes "T1-B" (0x54312D42) into the
record R2 points at. Decisive next experiment: write-watchpoint on that record slot.

## UPDATE 22 (2026-07-13): Phase-2 prep - offset-0 union collision (prediction, pending Phase-1 writer PC)

Freelist PUSH (free) 0x0802CEEB..0x0802CF07:
  0x0802CEEF w1 laddr $0x801D544+   ; &freelist_head
  0x0802CEFB w2 := @b.0x14          ; r2 = node being freed
  0x0802CEFE w1 =: r2.(0x0)         ; node.next = old_head   -> POINTER stored at OFFSET 0
  0x0802CF04 w1 =: @b.0x24          ; head = node
=> A list node's "next" link lives at OFFSET 0.

Name-copy (symbol record builder) 0x0802B500..0x0802B55D:
  0x0802B521 by1 =: @b.0x58+        ; byte-by-byte name copy INTO record @b.0x58 starting OFFSET 0
  name length capped at 0x10 (comp2 b.0x2E,$0x10)
  metadata written at r2.(0x14), r2.(0x16), r2.(0x19)  (r2 = b.0x58 record base)
=> A symbol record stores its NAME at OFFSET 0 (up to 16 bytes), metadata at 0x14+.

COLLISION: offset 0 is BOTH a list "next" pointer AND a symbol name field. The crash
`0x080241F9 h2 := r2.(0x0)` reads offset 0 of a record whose offset 0 holds the name
"T1-B" (temp symbol "T1" of module "B") -> 0x54312D42 dereferenced as a pointer.

PREDICTION to confirm with Phase-1 writer PC:
  writer PC ~= 0x0802B521 (name-copy loop), record base = frame b.0x58.
Root-cause shape: a symbol/name record is reachable from a list that NC walks expecting
link nodes -> either a use-after-free (record freed then name-reused, stale walker), or a
record placed on the wrong list. UPSTREAM trigger (what real HW had different) still TBD.

## UPDATE 23 (2026-07-13): Crash VERIFIED real + chain pinned; subagent premise-break was an artifact

A subagent claimed the committed build no longer reproduces the crash (said it faults
at 0x08023EA4/0xA1B8A1A8). VERIFIED FALSE - that was an artifact of running from the
wrong CWD (A:C did not resolve). Freshly rebuilt harness, run from build/nc_sandbox:

  COMPILE A,A,A : STOP PC=0x080241F9 data=0x4B6F7076 ("Kopv") steps=1,129,065
  CHECK   B,B,B : STOP PC=0x080241FC data=0x54312D42 ("T1-B") steps=1,232,403
  COMPILE B,B,B : runs off into uninit memory (PC=0) steps=1,232,980

Same bug across inputs: a recycled ASCII NAME dereferenced as a pointer at 0x080241F9.
Only the name differs by input. Confirms the offset-union hypothesis (UPDATE 22).

Crash chain (0x080241F4..0x080241F9), verified from nc-a06.asm:
  080241F4  w2 := b.0x14      ; r2_init = local record-base pointer
  080241F6  w2 := r2.(0x2)    ; r2 := mem[r2_init + 2]   = bad value (name bytes)
  080241F9  h2 := r2.(0x0)    ; derefs mem[r2 + 0]  -> PROTECTION VIOLATION

=> Corrupted slot S = r2_init + 2. It holds a symbol NAME where NC expects a
   child/next POINTER. r2_init (from frame b.0x14) points at a WRONG-TYPE record.

Register-level scanning (subagent method) CANNOT catch this: the bad value lives in
memory[S] and only transits I[1] for one instruction. The writer hunt must hook the
MEMORY WRITE to slot S (or trace b.0x14's provenance), run FROM build/nc_sandbox,
targeting the correct per-input value (0x4B6F7076 for COMPILE A,A,A).

STILL UNKNOWN (Phase 3): what makes b.0x14 point at a wrong-type record, OR what
writes the name into [r2_init+2]. Next: memory-write hook on S.

## UPDATE 24 (2026-07-13): Decode inconsistency - base register/slot S is UNKNOWN (stop guessing)

Crash-state registers (COMPILE A,A,A, instr 1,130,743, PC 0x080241F9):
  B=0x10000278  I1(I[0])=0x18028000  I2(I[1])=0x00000000  I3=0  I4=0x0802D7A7
Fault data address = 0x4B6F7076.

PREINDEXED EA in emulator = cpu->I[op->reg-1] + disp (cpu_instr.c:714-721).
For 09 F5 00: op->reg=(0xF5&3)+1=2 -> I[1]. But I[1]=0 at crash, disp=0 -> EA=0,
which CONTRADICTS the observed fault address 0x4B6F7076. So either op->reg decode,
the register array mapping, or a chained load (F6: 0D F5 02 w2:=r2.(0x2)) is producing
the address in a way static reading has NOT pinned. CONCLUSION: base register + slot S
are UNKNOWN. Do not assert them.

NEXT (single self-contained instrumented run, no register-transit assumption):
Add a hook at nd500_cpu_step for PC==0x080241F9 that dumps, from the emulator's OWN
decode: op->reg, computed EA, the 32-bit word actually read, and every register. Same
run also logs all memory writes whose value == 0x4B6F7076 with writer PC+instr#. This
replaces disassembly guessing with ground truth from the executing emulator.

## UPDATE 25 (2026-07-13): Ground-truth loop trace - seg-3 structure walk

Harness test/diag_crash_ea.c (build/run from build/nc_sandbox) logs regs at every
execution of 0x080241F9. It executes 7 times before the fatal 8th (COMPILE A,A,A):

  #1088131  I2=180024C8  EA=180024C8
  #1089733  I2=18021800  EA=18021800
  #1111588  I2=18021888  EA=18021888
  #1113190  I2=18021900  EA=18021900
  #1121743  I2=180218A0  EA=180218A0   I3=0000004B  <- 0x4B='K' (first byte of "Kopv")
  #1124185  I2=18021800  EA=18021800   I3=0000000C
  #1129038  I2=180024B4  EA=180024B4   I3=00000025
  (fatal 8th @1,130,742: I2=0x4B6F7076 -> deref -> protection violation)

Facts pinned:
- "r2" in `09 F5 00 h2 := r2.(0x0)` IS cpu.I[1] (mode=9 PREINDEXED, reg=2), EA=I[1]+0.
- The walked nodes live in NC's DATA segment (seg 3, 0x18xxxxxx = NC heap).
- I[1] is loaded from a node link field each visit; at the fatal visit the link field
  holds ASCII name bytes ("Kopv"=0x4B6F7076 / "T1-B"=0x54312D42) instead of a pointer.
- Register-scan and post-step inspection are masked by in-step trap handling (I[1]
  reads 0 post-trap); ground-truth must be captured at top-of-instruction.

REMAINING (Phase 3, the actual root cause): find the physical seg-3 slot holding the
name at crash time (scan RAM for 4B 6F 70 76), then find the instruction that WROTE
the name there (expected: name-copy routine near 0x0802B521, UPDATE 22) and WHY that
named node is reachable from the walked list (wrong-list insertion / use-after-free).
Harness note: diag_crash_ea.c PASS-B/ground-truth blocks segfault post-crash; the
ring-capture (PASS A) is the reliable part. MUST run from build/nc_sandbox.

## UPDATE 26 (2026-07-13): Root cause is UPSTREAM - a bogus base pointer passed as argument (NOT heap UAF)

Verified caller/callee chain (nc-a06.asm):
  caller 0x0802417E: w1 := b.0x14      ; p = caller local record pointer
         0x08024180: w1 := r1.(0x1)    ; arg = mem[p + 1]
         0x08024183: call 0x080241DC,$1,arg
  callee 0x080241DC (ents $0x1C):
         0x080241F4: w2 := b.0x14       ; base = arg = mem[p+1]
         0x080241F6: w2 := mem[base+2]
         0x080241F9: h2 := mem[base+0]  ; deref -> PROTECTION VIOLATION

Findings (run-backed by test/diag_slotS_writer.c, from build/nc_sandbox):
- The faulting deref uses base = mem[p+1], a BOGUS pointer:
    CHECK B,B,B : base = 0x00000034 (tiny int); mem[0x34+2]=mem[0x36] lands in the
                  STATIC DOM CAT pass-name table ("...CAT-CA""T1-B"), phys 0x0002F836,
                  present at instr#0. Value read = 0x54312D42 "T1-B".
    COMPILE A   : base = 0x4B6F7074 ("Kopt"), already ASCII from the heap options
                  string "Koptions m2 a4 ...". Value read = 0x4B6F7076 "Kopv".
- NOT use-after-free (no freelist-push touched these), NOT a built symbol record.
  The dereferenced bytes are innocent text/table data the bogus pointer lands on.
- Different garbage per input (0x34 vs "Kopt") => the field mem[p+1] is likely
  UNINITIALIZED or a mis-typed value, not a consistently mis-computed pointer.

THE FORK that decides the fix (next decisive experiment = write-watch mem[p+1]):
  (A) mem[p+1] is never written before the read -> missing initialization / a setup
      step (pass, MON side-effect, or instruction) we don't perform -> fix = do it.
  (B) mem[p+1] IS written with a wrong value -> upstream emulation bug -> trace writer.
Also open: subagent reported COMPILE-A heap PHYSICAL placement varies across runs under
the pinned clock - if real, that nondeterminism is itself a bug and supports (A).

## UPDATE 27 (2026-07-13): PROXIMATE ROOT CAUSE - NULL record pointer walked; the fork resolved

Run-backed by test/diag_watch_addr.c (CHECK B,B,B, from build/nc_sandbox), snapshots at
0x08024180 (arg-gen), 0x08024183 (call), 0x080241F4 (P-load):

  healthy #1115625: p(I0)=1802A180  mem[p+0]=001802A0  mem[p+1]=1802A0E8
          #1115626: CALL arg(I0)=1802A0E8   #1115634: mem[B+0x14]=1802A0E8  (all valid)
  fatal   #1232392: p(I0)=00000000  mem[p+0]=00000000  mem[p+1]=00000034
          #1232393: CALL arg(I0)=00000034  B=100001C4
          #1232401: mem[B+0x14]=00000034 (B=100001E0)

CHAIN (all downstream of a NULL):
  caller: I0 := mem[callerB+0x14] = p = 0x00000000   <-- NULL record pointer (THE BUG)
          I0 := mem[p+1] = mem[0x1] = 0x34           (garbage from low mem via data-MMU)
          call 0x080241DC, arg=0x34
  callee: base := arg = 0x34; mem[0x34+2]=mem[0x36]="T1-B" (static CAT table); deref -> PV

FORK RESOLVED:
- NOT a stale/inherited stack slot: the CALL at 0x08024183 WRITES callee B+0x14 with its
  argument (healthy visits show mem[B+0x14]==arg). C# "stale slot" theory refuted.
- NOT a missing base register: 7 harmless visits walked FULL seg-3 pointers via r2.(0x2)
  with no base; missing-base theory refuted.
- The defect is a NULL (0x00000000) where NC requires a record pointer: p = caller's
  b.0x14 = 0. NC dereferences it (mem[p+1]) with no null-check; 0x34/"T1-B"/"Kopv" are
  pure garbage-propagation artifacts, input-dependent, meaningless.

NEXT HOP (the actual origin): why is caller's b.0x14 = 0 on the CHECK-B path? Is it an
argument passed as 0 by the caller-of-the-caller, or a local that a lookup/alloc/MON
call should have populated (returned 0 = not-found/empty, unchecked)? Identify the
routine owning 0x0802417E, whether its b.0x14 is an arg or a computed local, and trace
the 0. That 0 is the real defect - likely an unchecked failed lookup or a value an
emulated MON/instruction produced as 0 where NC expected non-zero.

## UPDATE 28 (2026-07-13): NULL is propagated down a call chain; b.0x14 is an ARGUMENT

Static (nc-a06.asm). Routine owning 0x0802417E = 0x08024146 (ents $0x1C). Its b.0x14 is
an ARGUMENT (not a computed local), passed by:
  routine 0x080240CA (ents $0x2C):
    0802411D w1 := b.0x14              ; loads THIS routine's b.0x14 (also the record ptr)
    0802412D call 0x8024146,$1,r1.0    ; passes it down -> becomes 0x08024146's b.0x14
  routine 0x08024146 (ents $0x1C):
    0802415E by2 := @b.0x14            ; uses the record ptr
    08024161 call 0x80294D9,...        ; helper (visit?)
    08024173 call 0x802AE01,...        ; helper (next/child?)
    0802417E w1 := b.0x14  -> 08024183 call 0x080241DC (the crash walk)

So the NULL record pointer is PASSED IN from above (0x080240CA's b.0x14) and propagated,
not fetched here. Both routines pair helpers 0x80294D9 + 0x802AE01 == a tree/list
traversal. The origin is the TOPMOST frame that first obtained this pointer as 0 (an
empty-list / failed-lookup that should have had >=1 entry). C# confirms PATH A (mem[p+1]
never written; p itself NULL) and callerB=0x100001C4, p=0 - bit-identical to nd500x.

NEXT: walk the frame chain (B-register links) at the fatal call to find the routine that
FETCHED the record pointer as 0 (vs merely received it). That fetch site is the defect:
either NC took a legit "not found" path it shouldn't (missing upstream setup - a pass/
record never produced), or an emulated MON/instruction returned 0 where NC expected a
non-zero list head. Helpers to decode: 0x80294D9 (3-arg), 0x802AE01 (1-arg).

## UPDATE 29 (2026-07-13): Full MON-call inventory NC uses (CHECK B,B,B -> converged crash)

Captured live (test/diag_monlog.c, mon_log INFO, from build/nc_sandbox, crash 0x080241FC
instr 1,230,726). Distinct MON calls NC issues before the crash:

  504B DVOUTS x16   503B DVINST x12   73B SMAX x10     76B SETBS x9
  41B ROBJE x6      312B MOINF x6     262B CPUST x6    120B WFILE x6
  50B OPEN x5       43B CLOSE x4      422B GSWSP x3    117B RFILE x3
  62B RMAX x2       256B DEABF x2     143B RSIO x2     113B CLOCK x2
  64B ERMSG x1      54B MDLFI x1      317B UECOM x1    12B SETCM x1
  123B RELES x1     11B TIME x1       114B TUSED x1

317B UECOM: called ONCE at PC 0x0802E160, handler reads Command='' (EMPTY - the descriptor
-decode bug); NC's intended command was "NC-A" (a pass launch). Stub returns SUCCESS.
=> REFUTES the "multiple define-*-back-end via 317B at startup" theory: there is exactly
   ONE 317B, late, a single pass launch - not a back-end registry population loop.

Last data calls before crash: 43B CLOSE(file 101) -> 54B MDLFI(empty name, ERROR) ->
504B DVOUTS -> 41B ROBJE(File 100 -> 'SCRATCH64.DAT') -> 76B SETBS -> CRASH.

TOP SUSPECTS for the NULL (fork B = MON returns wrong/empty):
1. 312B MOINF x6 (PC 0x08029092), looped with 262B CPUST - backlog: "returns GOTAB entry
   or 0". If NC probes MON/capability availability and we answer wrong, its capability/
   config table is malformed. Present in BOTH crash paths (0x080241FC and 0x08023EA4).
2. 41B ROBJE x6 (Read Object Entry / directory) - the LAST data call before the crash,
   returned 'SCRATCH64.DAT'. If NC walks a list built from ROBJE/RFILE of a scratch file
   the un-run 317B "NC-A" pass should have populated, it reads empty -> NULL list.
3. 262B CPUST x6 - backlog: bytes 2-3 should be host ND-100/110 CPU id, we write 0.

INFERENCE (unproven) chain tying it together: 317B "NC-A" pass stubbed -> SCRATCH file not
produced -> 41B ROBJE/117B RFILE read it back empty -> empty record list -> NULL walk.
Decisive test still pending: frame-chain trace of the NULL's origin -> does it read from a
MON output buffer, and which call (312B? 41B?). SINTRAN ground truth for 312B/262B/41B/317B
is in segments-ref (NOT the TASK-05 bundle, which lacks these).

## UPDATE 30 (2026-07-13): ROOT CAUSE (fork B) - 312B MOINF wrongly reports 321B UEADM absent

C# pinned the 312B MOINF loop (PC 0x08029092, 6 probes): every probe asks ONE question -
"is MON 321B UEADM available?" Both emulators answer 0 (not implemented). NC then takes the
no-UEADM path and builds NO list entries -> the list parent fn 0x080240CA walks is empty ->
p = mem[callerB+0x14] = 0 (NULL element) -> crash. NC HAS a real 321B call site
(0x0802906C: call 0xF80000D1 = MON 321B UEADM), i.e. it uses UEADM when it believes it exists.

AUTHORITATIVE SINTRAN L (VSX-500) evidence (D:\ND\t\re):
- 312B MOINF = "CheckMonCall", worker MOINF=032600B (commoncode), byte-verified. It reports
  from MCTAB/9MCTA (the real monitor-call table, symbol @5620B; DATA in segment 044-S3IDPIT).
  MON-CALL-INDEX: "216 of 256 MCTAB slots populated, every populated slot lands on a named
  L07 symbol."
- 321B UEADM IS a real, named, implemented call: worker symbol UEADM @126111B (N500-SYMBOLS,
  in 026-S3IMPIT AND 030-S3SM5) and @65453B (commoncode SYMBOL-1-LIST). NOT deprecated/absent.
=> MOINF(321B) MUST return non-zero (available). Both emulators returning 0 is THE BUG (fork B).

FIX (both emulators):
1. 312B MOINF: return the populated MCTAB entry (non-zero) for 321B (and generally reflect the
   real MCTAB, not "unimplemented -> 0").
2. Implement 321B UEADM per its real handler (126111B / 65453B) so NC's subsequent
   0x0802906C call gets real data and builds the list.
Then re-run the converged crash and check it clears.

CAVEAT (honest): the exact MCTAB[321B] WORD is not readable here - MCTAB data is in the
uncarved segment 044-S3IDPIT. The named UEADM worker symbol + the "every populated slot is a
named symbol" rule make it near-certain MCTAB[321B] is populated. To be 100% byte-authoritative,
carve 044-S3IDPIT and read word (5620B+321B)=6141B.

## UPDATE 31 (2026-07-13): Byte-authoritative fix contract + correction to UPDATE 30

Source: E:\Dev\Ronny\NDInsight\tools\sintran-segment-carver\versions\L-VSX-500 (authoritative,
extended: 156 mon-analysis folders incl. 312B-CheckMonCall + 321B-UEAdministrator, 79 segments
incl. 044-S3IDPIT, resident images).

312B MOINF (CheckMonCall) authoritative contract: in=MonCallNumber, out=entry address
(0=not implemented); returns GOTAB[N]. Byte-verified: GOTAB[321B] word at commoncode 071554B
= bytes 94 FE = octal 112376 (NON-ZERO). => real MOINF(321B) returns non-zero.

NC logic (nc-a06.asm, verified by stepping/disasm):
  0x08029092 call MON 312B MOINF (b.0x14=MON#, b.0x1C=out)
  0x0802909C w test b.0x1C ; 0x0802909E if >< go +4  => NON-ZERO = take "available" branch
  0x0802906C call MON 321B UEADM,$3,b.0x14,b.0x18,b.0x34 ; 0x08029075 if -k go +6 (checks K)
Live MON log: 6x MOINF, ZERO UEADM calls -> because our MOINF returns 0, NC never calls UEADM,
builds no entries -> NULL list -> crash. THIS IS THE ROOT CAUSE, byte-confirmed.

CORRECTION to UPDATE 30: UEADM does NOT have a confirmed real handler body. New authoritative
data: GOTAB[321B]=112376B vectors to DIA6 (diagnostic stub, name mismatch); manual-named
UEADM=065453B is ZERO-FILLED / not carved; manual sec 2.16 lists 321B under "no longer
supported". Only confirmed fact: GOTAB[321B] != 0, so MOINF returns non-zero.

FIX (nd500x + C#):
1. 312B MOINF: return GOTAB[N] semantics; for 321B return NON-ZERO (real word 0o112376 or a
   non-zero sentinel - NC only tests !=0). Currently we return 0 == THE BUG.
2. 321B UEADM: NC will then call it (3 args, checks K). Must return success + data NC reads.
   UEADM body is NOT carved -> exact output contract UNKNOWN. Try success/minimal first,
   step NC to see what it consumes.

MISSING (forwarded to carving LLM): the MON 321B UEADM handler body + output-parameter
contract (3 params, K/success). Resident UE range (065453B) zero-filled/not carved; GOTAB
->DIA6 name-mismatch. Secondary: confirm MOINF returns GOTAB[N] vs MCTAB[N] (both non-zero
for 321B, not blocking).

## UPDATE 32 (2026-07-13): Fix implemented - MOINF+UEADM path now taken; UEADM output data still needed

Changes (committed pending): src/libmon/handlers/mon_312B_CheckMonCall.c returns non-zero
(GOTAB[321B]=0112376) for MON 321B; mon_321B_UEAdministrator.c returns SUCCESS + logs args.

Result (CHECK B,B,B, from build/nc_sandbox):
- 312B MOINF now EXIT SUCCESS (non-zero) for the 6 probes -> NC takes the "available" path.
- 321B UEADM now CALLED (was never called before). Args observed, called repeatedly:
    a0=0x1  a1=0x0  a2=0x30
    a0=0x2  a1=0x0  a2=0x30
    a0=0x1  a1=0x0  a2=0x30  ...
  (a0 = subcode/index 1|2; a1 = 0 likely OUTPUT slot; a2 = 0x30=48 buffer addr/size)
- STILL crashes at 0x080241FC (~instr 1,232,415), same NULL list walk, moved only ~a few
  instrs. => UEADM returning success-but-EMPTY is insufficient; NC needs real UEADM OUTPUT
  to populate its list.

MISSING DETAIL (for carving LLM, sharpened): MON 321B UEADM OUTPUT CONTRACT for
subcodes a0=1 and a0=2 - what it writes to arg1 (0x0 in) and the arg2 buffer (0x30), and its
K/success semantics, so NC builds real list entries instead of NULL. Body uncarved
(UEADM=065453B zero-filled; GOTAB->DIA6 diagnostic).

NEXT: step NC around 0x0802906C to trace how it consumes UEADM's output into the list that
0x080240CA later walks - derive the expected output values empirically if the carving can't
recover them.

## UPDATE 33 (2026-07-13): C# confirms - blocker isolated to UEADM buffer-fill contract

C# reproduced identically: MOINF-flip necessary but NOT sufficient. Their 312B now returns
Entry=0xF80000D1 for 321B (matches GOTAB[321B]=0o112376); NC takes available branch, calls
UEADM; crash PERSISTS at 0x080241FC (only instr count moves: baseline 1,230,764; +UEADM error
1,230,620; +UEADM success 1,230,776). NC CONSUMES the buffer UEADM fills; empty buffer =
empty list = same NULL.

EXACT UEADM CALL CONTRACT (C# + nd500x agree):
- UEADM called TWICE per pass: handle a0=1, then a0=2.
- buffer = 0x0801CA58 (NC DATA segment).
- size = 0x30 = 48 bytes = 24 halfword entries.
- NC feature-detect fn 0x08029058 computes size = (b.0x24 - b.0x20 + 1)*2, then a downstream
  builder turns that buffer into the list that 0x080240CA walks (crash when an element is 0).

REMAINING BLOCKER (single, precise): the MON 321B UEADM BUFFER-FILL contract - what
24-halfword structure the real worker writes into buffer[0x0801CA58] for handle=1 and handle=2,
such that NC builds a non-empty (or safely-terminated) list. Body uncarved.

PATHS: (a) carve UEADM body (065453B, currently zero-filled) - authoritative; (b) RE NC's
buffer parser (0x08029058 caller -> list builder -> 0x080240CA) to derive the expected
24-halfword shape empirically. C# will implement verbatim once the buffer contract is known.

## UPDATE 34 (2026-07-13): Causality test - UEADM param-1 output does NOT feed the crash list

Experiment: UEADM writes 0x0000ABCD to output param 1, re-run CHECK B,B,B.
Result: NO change - crash still 0x080241FC, p = mem[callerB+0x14] = 0x00000000 at fatal visit
(#1232404 ARGGEN p(I0)=0). So UEADM's PARAM-1 output is NOT the source of the NULL list.

Reconciliation: the MOINF fix DID move the crash (~1,230,726 -> ~1,232,415, +~1700 instrs), so
NC does take the UEADM path and run further - but the walked list still ends with a 0 element.
Per C#'s branch: "p stays 0 -> UEADM output feeds a DIFFERENT structure, re-target." So either
UEADM must fill a different arg/buffer (not param1), or the crash list is populated by another
mechanism the UEADM path converges to.

State kept: 312B MOINF non-zero-for-321B fix RETAINED (confirmed-correct, byte-authoritative).
321B UEADM = plain success (no fabricated buffer). Test pattern reverted.

NEXT (decisive): trace the crash list's ACTUAL fill source - what writes the parent list at
0x080240CA (cursor = parent-b.0x14 - 2) that yields p=0 - rather than assuming UEADM param1.
In parallel: carve UEADM buffer contract (which arg is the fill buffer; a1=0/a2=0x30 suggest
the buffer ptr is elsewhere).

## UPDATE 35 (2026-07-13): UEADM does NOT receive the output-buffer pointer in its args

Corrected causality experiment (nd500x, CHECK B,B,B). Logged each UEADM arg's EA + value:
  arg[0] EA=0x0801CA9C  *EA=1  (subcode; then 2)
  arg[1] EA=0x0801CAA0  *EA=0
  arg[2] EA=0x0801CABC  *EA=0x30 (size)
  (frame B=0x0801CA88: EAs = B+0x14 / B+0x18 / B+0x34)
NONE of the three arg VALUES points into data memory (all are 1/0/0x30), so no arg carries
the output-region pointer. C#'s region 0x0801CA58 = frame local b.0x1C's VALUE (b.0x1C EA =
B+0x1C = 0x0801CAA4), which is NOT one of UEADM's 3 args. => Real UEADM must locate its output
region by some means NOT visible in the call args (fixed/global cell, caller-frame walk, or a
descriptor we can't see). This is unrecoverable without the carved UEADM body.

Bounded outcome of this session's fix work:
- 312B MOINF non-zero-for-321B: DONE, byte-authoritative, retained (routes NC to UEADM path).
- 321B UEADM: plain SUCCESS + arg logging (no fabricated buffer). Retained.
- Crash NOT cleared: NC's 24-halfword region 0x0801CA58 stays zero -> NULL walk. Clearing it
  requires knowing (a) HOW UEADM locates that region, and (b) the 24-halfword structure it
  writes for subcode 1/2. Both need the carved UEADM body.

HARD ASK FOR CARVING LLM (blocking): recover MON 321B UEADM worker so we know (a) how it
addresses its output region (it is NOT passed as one of the 3 args: subcode, 0, size), and
(b) what 24-halfword structure it writes for subcode a0=1 and a0=2.

## UPDATE 36 (2026-07-13): Real fill source = a fill-loop count of N-1 (NOT UEADM)

C# causality on the region 0x0801CA58 + all 3 args: NEGATIVE (p stays 0). UEADM output is NOT
the NULL-list source. UEADM/MOINF availability fix stays (correct) but is not the crash fix.

Real source (C# trace + nd500x static disasm agree): the fatal element is read from a seg-0x18
HEAP block filled with 0xF0F0 poison (GETB alloc-fill 0x0802CE39), whose entries are written by
a fill loop at 0x08023F82..0x08023F9A (halfword store 0x08023F85 = h1 =: @b.0x1C - width CORRECT).

Fill-loop count (static, nc-a06.asm):
  08023F6D h1 := r1.(0x0)   ; N = mem[record+0] (halfword count), record = @b.0x14
  08023F70 w decr r1        ; N-1
  08023F72 w1 =: b.0x20     ; fill index/count = N-1
  08023F76 w1 * $0x4 ; 08023F78 call 0x802CF08(( N-1)*4)  ; alloc buffer (N-1)*4
  08023F82 h1 := b.0x22 ; 08023F85 h1 =: @b.0x1C          ; halfword fill
=> the loop covers N-1 entries. If the walk at 0x0802412A consumes N, the last entry is
   unfilled 0xF0F0 poison -> read as a null/garbage element -> p=0 -> crash.
Writers of the fatal slot match: 0x0802CEFE = freelist free-site (UPDATE 22); 0x08023F85 = this
halfword store.

OPEN (decides CPU-bug vs data-bug): is N (mem[record+0]) correct, and does the walk read N vs
fill N-1? If fill<walk by one on our emulator but not real HW, suspect a CPU-instruction bug in
the count path (decr / comp2 b.0x26,$1 loop bound / halfword read). If N itself is wrong,
trace what set mem[record+0]. NEXT: capture N + fill-count + walk-count at the fatal record.

## UPDATE 37 (2026-07-13): Bit-identical list-builder confirmed; count 0x302 is stale/garbage

nd500x list-builder (fn 0x08023F49) invocations (CHECK B,B,B) - IDENTICAL to C#:
  instr ~1,110,641  count=2      fills 1    allocBase=0x1802A0F8
  instr ~1,124,926  count=1      fills 0    allocBase=0x18003000
  instr ~1,217,198  count=0x302(770) fills 769 allocBase=0x1802B000  <-- fatal
Cursor = allocBase-2 = 0x1802AFFE (walk 0x0802411F laddr r1.(-2) starts one halfword before
the list). 0x0802484F confirmed = FC 10 h1 =: @b.0x20 (HALFWORD store, by design into GETB
0xF0F0F0F0 poison) -> the 0xF0F00302 shape is expected, NOT a store-width bug.

Defect: the fatal count 0x302 (770) is anomalous vs the other invocations (1,2). It is read as
count=mem16[mem[@b.0x14]+0] from a block that was FREED (0x0802CEFE, ~1,217,002) shortly before
(~1,217,114). => use-after-free / stale-header count. The count should be small; 0x302 is the
low half of a stale 0xF0F00302 alloc header left in a freed/reused block.

NEXT HOP: trace the record pointer @b.0x14 for the FATAL list-builder invocation - is it a
freed block, what set its count field to 0x302, and what freed it at 0x0802CEFE. Both emulators
produce 0x302 identically => shared emulation defect OR faithful NC behavior driven by our
inputs; decide by finding whether an earlier emulated op corrupts/mis-frees this heap block.

## UPDATE 38 (2026-07-13): -2 base is real NC code; crash = walk reads a FREED block's stale header

C# reframe confirmed: counts MATCH (fill 769 == walk 769). It is a BASE off-by-one, not count.
nc-a06.asm bytes: 0x0802411F = FD 3C FC FF FF FF FE = `w1 laddr r1.(0xFFFFFFFE)` = handle + (-2).
The -2 is in the REAL binary and both emulators decode it identically -> handle-2 = 0x1802AFFE.
So NOT an emulator base-decode divergence.

Structure: builder fills 769 halfword entries from handle=allocBase=0x1802B000. Walk reads from
handle-2 = 0x1802AFFE (the ALLOC-HEADER slot) as entry[0]. On real HW that header slot must hold
a value the walk survives; on our run it holds stale 0xF0F00302 (poison high half + prior header
low half) because block 0x1802B000/0x1802AFFE was FREED at 0x0802CEFE (~1,217,002) shortly before
the list-builder alloc'd/used it (~1,217,114) - a USE-AFTER-FREE / freed-then-reused block.

CONVERGED ROOT: the fatal block is freed (0x0802CEFE) then re-allocated by GETB and used, so its
header slot (handle-2) is stale poison instead of a valid value. Non-fatal invocations (count 1,2)
don't crash because their blocks weren't freed -> valid header. THE corrupting op = the free at
0x0802CEFE of block 0x1802B000 before the list-builder reuses it.

DECISIVE NEXT: trace the free at 0x0802CEFE for block 0x1802B000 - what NC logic frees it, and is
that free faithful (real NC also frees+reuses, so real GETB must re-init the header) or a shared
emulation defect (GETB alloc-fill leaves 0xF0F0 where real SINTRAN GETB zero-inits). Compare our
GETB alloc/free/re-init (0x0802CE39 fill, 0x0802CEFE free, 0x0802CF08 alloc) header handling vs
what real SINTRAN GETB does. That header re-init on realloc is the likely fix point.

## UPDATE 39 (2026-07-13): NOT use-after-free - a TYPE CONFUSION. Concrete chain pinned.

Dropped the "bit-identical = correct" assumption (both emulators are one author's - shared bugs).
Traced the count=770 corruption to ground truth:

- Corruption originates at instr 1,196,912-914 in alloc routine 0x08024829, which computes
  list size = mem16[recordA] + mem16[recordB] (h1+h2). Captured: h1=0x300, h2=0x2 -> 0x302 (770).
- recordA = 0x1802A1B0. Dumped its bytes: word[0]=0x03001802 -> mem16=0x0300. This is a TYPE-3
  OBJECT (constructor 0x0801FEAD writes byte tag 0x03 at offset 0). NC reads the tag 0x0300 as a
  count. recordB is a valid global count-record (count=2).
- NOT use-after-free: free/alloc timeline of cell 0x1802A1B0 = {1,110,451/455, 1,110,718/722,
  1,206,997/1,207,001, ...}. It is allocated at 1,110,722 and NOT freed until 1,206,997, so at the
  fatal read (1,196,912) it is LIVE. It is a live type-3 object read as a count-record.
- Fatal caller: alloc-entry L=0x080050D3 -> caller 0x080050C9:
    080050C3 w1 := $0x8000200
    080050C9 call 0x8024829,$2,@b.0x14,r1.(0x0)   ; recordA=*b.0x14 (=0x1802A1B0 type-3),
                                                    ; recordB=mem[0x8000200] (global, count=2)
  This is NC init/setup code building lists from globals 0x8000200/0x8000204.

DEFECT: caller 0x080050C9's b.0x14 points at a live TYPE-3 object where a count-record is expected.
mem16[type-3 tag]=0x0300 summed with 2 = 770 -> over-sized list -> walk hits unfilled 0xF0F0 ->
crash 0x080241FC. NOT UEADM, NOT store-width, NOT use-after-free, NOT the -2 base.

NEXT: trace caller 0x080050C9's b.0x14 provenance - what sets it to the type-3 object 0x1802A1B0,
and whether that pointer/branch is computed by a mis-executed instruction (the shared emulator bug).

## UPDATE 40 (2026-07-13): ROOT MECHANISM - node freed while still linked in global list

Traced b.0x14's source: PC 0x08005044 (instr 1,193,163) does w1:=mem[0x08000200] (absolute
load; disasm "$0x8000200" is misleading) then b.0x14:=w1. So the record = current value of
global list-head 0x08000200 = 0x1802A1B0.

Timeline (from diag_global + diag_uaf, CHECK B,B,B):
  1,110,309  mem[0x08000200] := 0x1802A1B0     (list head set to node 0x1802A1B0)
  1,110,451  FREE 0x1802A1B0 (0x0802CEFE)       <-- node freed while global still references it
  1,110,455  ALLOC 0x1802A1B0
  1,110,718  FREE 0x1802A1B0
  1,110,722  ALLOC 0x1802A1B0                    (this alloc lives to 1,206,997)
  1,142,883  cell built as TYPE-3 object (ctor 0x0801FEAD, tag byte 0x03 @off0)
  1,193,163  NC copies mem[0x08000200]=0x1802A1B0 into a local (b.0x14)
  1,196,914  alloc reads mem16[0x1802A1B0]=0x0300 (type-3 tag) as a COUNT -> +2 = 770
  1,230,739  over-sized 770-entry list walked -> hits 0xF0F0 poison -> PV 0x080241FC

ROOT: node 0x1802A1B0 is FREED (0x0802CEFE @1,110,451) while STILL LINKED in the list rooted
at global 0x08000200, then reused as a type-3 object; global is never updated -> dangling.
NC later walks the stale global list and reads the type-3 tag as a count.

STILL TO PIN (for a real fix): WHY node 0x1802A1B0 is freed at 1,110,451 while global 0x08000200
references it. Either (a) an emulator instruction bug makes NC free a still-referenced node /
mis-manage the freelist, or (b) NC updates the global elsewhere and an emulator bug drops that
update, or (c) upstream wrong value. Next: trace the free at 0x0802CEFE @1,110,451 - what list/
pointer NC thinks it is freeing, and whether the global should have been updated first.

NO VERIFIED FIX YET: mechanism fully pinned; root free-decision not yet isolated to a specific
mis-executed instruction. A symptom patch (clamp count, skip free) would mask corruption.

## UPDATE 41 (2026-07-13): 2nd owner found - producer/consumer mailbox, global left stale

Global 0x08000200 is a scratch "mailbox":
  PRODUCER routine 0x08008548 (PC 0x08008577): global 0x08000200 := list 0x1802A1B0 (instr 1,110,309)
  CONSUMER routine A (PC 0x08008A7C, instr 1,110,322): reads mem[0x08000200]=0x1802A1B0 as concat
    input rec2, concat FREES its buffer, final result stored to a DIFFERENT global 0x08010680
    (0x08008AA1). Global 0x08000200 is NOT updated/cleared -> dangling.
  Reader routine 0x08005024 (PC ~0x08005044, instr 1,193,163): reads stale mem[0x08000200]
    =0x1802A1B0 (now a recycled type-3 object) -> mem16 tag 0x0300 read as count -> +2 = 770 ->
    over-sized list -> crash 0x080241FC.

Between consume (1,110,322) and stale read (1,193,163) there is NO write to 0x08000200 (verified
diag_global). Global writers: 0x08002547, 0x08008AB9(re-init), 0x08008577, 0x080084F3, 0x08005106.
The re-init (0x08008AB9, unconditional store after call 0x80249BB) last ran at 1,106,917 - BEFORE
the consume - so the mailbox was consumed and never refreshed before the stale read.

Branches/flags verified correct so far (free-path x3, both flip candidates, comp2). Flip-finder
inconclusive (derailments). Float comp2 fixed (real bug, not this trigger).

NEXT: why is the stale 0x08000200 read at 1,193,163 without a refresh? Check whether a
conditional refresh/clear of the mailbox is SKIPPED (a guarded branch) - if so, that skipped
branch (and its flag) is the bug. Reader routine 0x08005024; producer/consumer 0x08008xxx.

## UPDATE 42: Manual-audit of freelist float-bucketing - CLEAN (prime suspect cleared)
Route chosen: audit the freelist bucket computation (alog2/dconv/int/shl float path at the
allocator wrapper 0x0802CEB3 -> 0x0802CF08) against the ND-500 Reference Manual.
Harness: build/bin/diag_bucket (test/diag_bucket.c), run FROM build/nc_sandbox, ND500X_PIN_CLOCK=1.

VERIFIED (against manual sections, not inferred):
1. ND-500 float FORMAT matches the emulator exactly. Manual sect 7.2.5/7.2.6/7.2.1:
   sign=bit63/31, 9-bit exponent bias 256, HIDDEN implicit MSB, stored mantissa 54/22 bits,
   M in [0.5,1), value = S * 2^(exp-256) * M. Emulator constants (instruction_helpers.c:1747-1758)
   and nd500_double_to_ieee754 (bias delta +766, M = 0.5 + mantissa/2^55) reproduce this.
   Decoded 4 LIVE alog2 inputs from the crash run; every one matches:
     size 0xB(11)->2.0  size 0x22(34)->8.0  size 0x2F(47)->11.0  size 0x83D(2109)->527.0
   (value fed to alog2 = size_bytes/4 = size in ND words.)
2. Bucket math faithful: bucket = ceil(log2(size_in_words)). Correct on all 4 samples
   (1,3,4,10). Combined with UPDATE 6-7 (buddy heap exonerated 4 ways), the freelist
   float-bucketing is NOT the crash trigger. Prime audit suspect CLEARED.

SIDE FINDING (real, minor, NOT this crash): manual 7.2.7 specifies float results are ROUNDED
(round-to-nearest, add 1 to LSB). nd500_double_from_ieee754 / nd500_float_from_ieee754
TRUNCATE the mantissa (mantissa>>2 / <<2, no round bit). LSB-level divergence from HW on
transcendental results. Does not affect the observed buckets and cannot cause the UAF
(buddy heap exonerated), but is a genuine correctness gap worth a separate fix.

NEXT (audit route continues): the float-bucketing subset of the 171 opcodes is cleared. The
UAF trigger remains NC storing an escaping pointer (mailbox 0x08000200 not refreshed at
1,193,163). Per UPDATE 41: check whether a conditional refresh/clear of the mailbox is a
SKIPPED guarded branch. That, not the allocator, is where the remaining divergence must live.

## UPDATE 43: guarded-branch hypothesis CLOSED (reader + mailbox both clean)
Route: UPDATE 41 asked whether a conditional refresh/clear of mailbox 0x08000200 is a SKIPPED
guarded branch. Answer: NO. Two harnesses settle it.

Harness build/bin/diag_readerbr (test/diag_readerbr.c): static disasm + per-instruction ST1
trace of reader routine 0x08005024..0x08005130 across the crash window.
 - The reader routine has ZERO conditional branches. Straight-line: ents; load mailbox
   [0x08000200] at 0x0800503E -> b.20 (ic 1193163, I1=1802A1B0); call 0x080050F5 which
   UNCONDITIONALLY refreshes the mailbox to the fresh cell 0x18003000 at 0x08005106.
 - So read-stale-then-refresh is the routine's DETERMINISTIC design (double-buffer: consume
   previous pointer, then publish current). The stale 0x1802A1B0 flows to the fatal concat at
   0x080050C9 (call 0x08024829, IND(b.20)=0x1802A1B0). No guard exists to skip.

Harness build/bin/diag_mailbox (test/diag_mailbox.c): logs every change of mailbox 0x08000200
(MMU-translated) over ic 1,100,000..1,200,000.
   1,106,916 08008AB9  180024B0 -> 1802A0D0
   1,109,584 08008577  1802A0D0 -> 1802A0F0
   1,109,968 080084F3  1802A0F0 -> 1802A0F8
   1,110,308 08008577  1802A0F8 -> 1802A1B0   <-- final publish
   (83,000 instrs, NO write to the mailbox)
   1,193,206 08005106  1802A1B0 -> 18003000   <-- reader consumes+refreshes
 - NO producer write in the 83k-instruction gap. The stale value is NOT held because a producer
   branch was skipped; NC deterministically leaves the mailbox at 0x1802A1B0. Buffer 0x1802A1B0
   is freed ~143 instrs AFTER publish (1,110,451) and never re-published before the consume.

CONCLUSION: the guarded-branch route is EXHAUSTED for this crash. Reader = no branches; mailbox
= no skipped producer write; free-path branches already verified (RULED OUT list). Every
instruction and branch is faithful to its operands; only the AGGREGATE (freeing a just-published
node) diverges. This is exactly the (b) upstream-timing vs (c) NC-latent-UAF ambiguity that
black-box tracing cannot resolve without an oracle.

REMAINING ROUTES (tracing/audit now exhausted for this crash):
 - Pragmatic: add the defensive type-tag guard (validate record type-tag before reading mem16 as
   a list count at the list-builder) to get PAST 0x080241FC and expose the NEXT blocker.
 - Oracle: independent reference trace / per-instruction manual audit of the ~143-instr free
   window at 1,110,308..1,110,451 (the concat 0x08024829 that frees the just-published node) to
   decide (b) vs (c). This is the only remaining diagnostic, and only if an oracle exists.

## UPDATE 44: type-tag guard implemented + fires, but does NOT unblock (crash signature identical)
Implemented the defensive type-tag guard (env-gated ND500X_NC_TYPETAG_GUARD, off by default) in
src/cpu/cpu.c nd500_cpu_step, at NC list-builder helper 0x08024829:
  0x08024834 h1 := IND(b.20)  ; h1 = mem16[recordA]. After this load, if the high byte of h1 is
  the type-3 tag 0x03, substitute count 0 (a type-3 object's mem16 is a tag, not a count).
Uses only the loaded register (no memory access) so it is host-safe.

Also fixed a REAL host bug found while wiring it: cpu.c used getenv() without <stdlib.h> ->
implicit int declaration truncated the 64-bit pointer -> when the env var was SET, getenv's
pointer became a wild non-null value and e[0] segfaulted the emulator (OFF safe because unset
returns 0). Added #include <stdlib.h>.

RESULT (build/bin/diag_run, CHECK B,B,B, from build/nc_sandbox, ND500X_PIN_CLOCK=1):
  OFF: [STOP] protection violation PC=0x080241FC data=0x54312D42 instr=1231450
  ON : [NC-GUARD] type-3 tag (mem16=0x0300) at record 0x1802A1B0 ... instr=1196912; count->0
       [STOP] protection violation PC=0x080241FC data=0x54312D42 instr=1210545
Guard fires exactly once and eliminates the 770-count symptom, but the crash PC and faulting
data are IDENTICAL (0x080241FC / 0x54312D42), only ~20k instrs earlier.

WHY the point-guard is insufficient: 0x54312D42 = ASCII "T1-B". The fatal deref is
  0x080241F9  h2 := r2.0   ; reads mem16[r2], r2 = 0x54312D42 ("T1-B")
i.e. a list ELEMENT holding the string "T1-B" is loaded into r2 and dereferenced as a pointer.
The type-3 object 0x1802A1B0 is a STRING wrongly placed in a pointer list; ANY walk of that list
dereferences "T1-B" -> PV, independent of the count. The guard logged only once, so the second
path to 0x080241FC does NOT pass through 0x08024834 - the corrupt string-object reaches the fatal
walk by another route. The count 770 was a SYMPTOM, not the cause.

CONCLUSION: point-guarding the count read is whack-a-mole; the type confusion (a string object
flowing where a pointer list is expected) is pervasive. To get past this would require guarding
the element DEREF at 0x080241F9 (skip/clamp when r2 is not a valid mapped pointer) - a symptom of
a symptom - or fixing the upstream reason the "T1-B" string object enters the pointer list, which
is the same (b)-vs-(c) divergence that needs an oracle (UPDATE 43). The guard is kept as an
opt-in diagnostic (default build unaffected); it is NOT a fix.

## UPDATE 45 (2026-07-14): CRITICAL - 0x080241FC was a DIRTY-SANDBOX ARTIFACT. Real crash = 0x08023EA4.
The NC sandbox (build/nc_sandbox/GUEST) is MUTATED by every run: NC writes B.CAT, B.LIST,
B.NRF output files that persist and change the NEXT run's behavior. All prior "crash at
0x080241FC / data 0x54312D42 / instr ~1,230,739" results (UPDATES 36-44, the type-3 /
count-770 / T1-B chain) were produced on a DIRTY sandbox carrying leftover B.CAT/B.LIST.

On a PRISTINE sandbox (reset from test/nc_fixtures/ before the run), the crash is DIFFERENT
and fully deterministic across commands:
  CHECK   B,B,B : [STOP] PV PC=0x08023EA4 data=0xA1B8A1A8 instr=1,480,988
  COMPILE B,B,B : [STOP] PV PC=0x08023EA4 data=0xA1B8A1A8 instr=1,487,418
  COMPILE A,A,A : [STOP] PV PC=0x08023EA4 data=0xA1B8A1A8 instr=1,480,639
Identical PC and data; instr count varies only by command. Reset recipe:
  rm -rf build/nc_sandbox; mkdir -p build/nc_sandbox/SCRATCH
  cp -r test/nc_fixtures/GUEST build/nc_sandbox/; cp -r test/nc_fixtures/expected build/nc_sandbox/
The handoff's claim "0x08023EA4 = wrong-CWD fixture-not-found" was WRONG; 0x08023EA4 is the
pristine-state crash. 0x080241FC only appears with dirty leftover state.

NOTE: ctest dom_nc_compile_b "PASSES" only because test_dom_integration's criterion is
--min-instructions 1200000 and the PV is at ~1.48M. The compile does NOT succeed - it PVs.
The green test is misleading.

The env-gated type-tag guard (UPDATE 44, ND500X_NC_TYPETAG_GUARD) targets the dirty-state
artifact (site 0x08024834) and is IRRELEVANT to the pristine crash (verified: guard ON,
pristine -> still PV 0x08023EA4 @ 1,480,988, guard never fires). Kept as opt-in diagnostic
only; it is NOT on any real path.

REAL CRASH STRUCTURE (pristine, aligned disasm):
  08023E8A call 0x08024829,$2,IND(b.40),IND(b.20),$0x524F5353("ROSS"),$0x4341542D("CAT-")
  08023E94 w1 =: b.20        ; b.20 = list-builder result record ptr
  08023E9E w1 := r1.2        ; r1 = mem32[record+2]  = 0xA1B8A1A8 (bad pointer)
  08023EA1 by test r1.0      ; reads mem8[0xA1B8A1A8] -> PV (trap reported at advanced PC 0x08023EA4)
Same shape as the artifact: mem32[record+2] should be a valid pointer but holds garbage.
0xA1B8A1A8 = halfwords 0xA1B8 / 0xA1A8 = the LOW halfwords of heap cells 0x1802A1B8 /
0x1802A1A8. I.e. a 32-bit pointer whose HIGH halfword (seg 0x1802) is clobbered by another
cell's low halfword => suspect a HALFWORD pack/store/load ordering defect, not a UAF.
Cross-check the 0x0801D544 "halfword pack law" note (this file line ~51).

NEXT: trace provenance of mem32[record+2]=0xA1B8A1A8 at the pristine crash - what writes the
record's +2 field, as one 32-bit store or two halfword stores, and whether a halfword goes to
the wrong offset/endianness. All work must reset the sandbox to pristine first.

## UPDATE 46 (2026-07-14): Writer trace - fatal record built by a -2-based copy loop (halfword shift)
Env-gated write-watch (ND500X_NC_WWATCH) added to nd500_write_memory_8/16/32(_domain) in
instruction_helpers.c and mmu_write* in cpu_instr.c. Watches virtual [0x1802A1B0,0x1802A1D0).
Run pristine (reset sandbox first). All 40 writes to the range captured; the fatal-state build:

  NON-FATAL build (instr 1320432, storing-instr PCs 0802402F/08024036):
    w16 [1802A1B0] <- 0001         ; tag=1  at base+0
    w32 [1802A1B2] <- 1802A1A8     ; CLEAN pointer at base+2
  FATAL build (instr 1400970, storing-instr PCs 08024853 / 08024884):
    w16 [1802A1B0] <- 1804         ; tag corrupt (0x1804)
    w32 [1802A1B2] <- A1B8A1A8     ; the wild pointer that crashes at 0x08023EA1
    w32 [1802A1B6] <- 18020000     ; 0x1802 segment halfword lands one WORD late
    w32 [1802A1BA] <- 0000F0F0
    w32 [1802A1BE] <- F0F00002

The fatal store advances PC to 0x08024887, so the store is 0x08024884 `w1 =: IND(b.52)(r2)`,
a WORD copy loop. Preceding it:
  08024867 w1 := b.20
  08024869 w1 := laddr r1.(-2)   ; SOURCE base = b.20 - 2 bytes
  08024870 w1 =: b.48
  08024874 w1 := IND(b.48)(r1)   ; load word from (source-2)+index*4
  08024879 w2 := laddr r2.(-2)   ; DEST base = b.32 - 2 bytes
  08024880 w2 =: b.52
  08024884 w1 =: IND(b.52)(r2)   ; store word to (dest-2)+index*4  -> +2,+6,+A,+E (odd)
So the fatal record is COPIED through a -2-based loop; stores land at odd offsets and the
0x1802 segment halfwords end up one word late => the halfword-shift corruption. The clean
non-fatal path writes the same record base-ALIGNED (+0 tag, +2 ptr) via a different routine
(0802402F). Inconsistent base convention (+0 vs -2) for the same structure, OR an emulator
laddr displacement-scaling bug (is `laddr rN.(-2)` -2 BYTES or -2 ELEMENTS?).

The value 0xA1B8A1A8 is LOADED from the source at 0x08024874 (it pre-exists in the source
record), so the corruption may originate one hop further up. But the decisive next check is
whether laddr/preindexed displacement should be datatype-scaled per the ND-500 manual:
 - cpu_instr.c:718 PREINDEXED = I[n] + displacement (UNSCALED bytes)
 - short forms LOCAL_SHORT/RECORD_SHORT scale embedded value *4 (cpu_instr.c:701,711)
If HW scales the extended displacement by element size, -2 should be -8 bytes and stores would
land aligned. Verify `laddr` + preindexed displacement units against the manual (this is the
"instruction variant" root-cause class). All runs MUST reset the sandbox to pristine first.

## UPDATE 47 (2026-07-14): laddr/preindexed displacement-scaling hypothesis DISPROVEN
Checked UPDATE 46's leading suspect against the ND-500 manual. The pre-indexed address codes
0xF4-0xF7 / 0xF8-0xFB / 0xFC-0xFF (byte/halfword/word) select the STORAGE WIDTH of the
displacement field (1/2/4 bytes after the address code), NOT a scale factor on the value.
Manual sect 8.5/8.9:
 - line 3122: "...byte, halfword and word displacement parts with the displacement stored in
   1, 2, or 4 byte(s) after the address code, displacement unit BYTE."
 - line 2929: "The displacement unit is always bytes, except for short displacements, where
   the unit is words."
So `laddr r1.(-2)` with address code 0xFC (4-byte field) = -2 BYTES, not -2 words. The
emulator computes I[n] + displacement (cpu_instr.c:718) = -2 bytes = CORRECT. The decoder
(cpu_instr.c:354-358) reads displacement FIELD WIDTH, not a scale. Hypothesis killed; do NOT
"fix" preindexed scaling.

CONSEQUENCE: the -2-based copy loop (0x08024867-0x08024884) is FAITHFUL (byte-correct), and it
copies 0xA1B8A1A8 from a SOURCE record (loaded at 0x08024874 from b.20-2) that ALREADY held the
corrupt value. The corruption is genuinely UPSTREAM of this copy. Next hop: trace the source
record b.20 at the fatal copy (instr ~1,400,970) - who built IT and wrote 0xA1B8A1A8 into it.
Reuse the write-watch (ND500X_NC_WWATCH) retargeted to the source record's address range, on a
PRISTINE sandbox.

## UPDATE 48 (2026-07-14): Junk pointer BIRTH pinned - record vs free-link overlap on one cell
Traced 0xA1B8A1A8 to its creation (pristine run). It is NOT made by one bad instruction; it is
two individually-CORRECT writes overlapping on the same 6-byte heap cell 0x1802A1B0:

  instr 1320435  store at ~0x08024033 `w1 =: r2.2`  -> [1802A1B2] = 1802A1A8  (record pointer at cell+2)
  instr 1320549  store at  0x0802CEFE `w1 =: r2.0`  -> [1802A1B0] = 1802A1B8  (free-list link at cell+0)

The record builder (routine 0x08024013): call GETB size=6; `h set1 IND(b.28)` tag at +0;
`w1 =: r2.2` pointer at +2. So NC intentionally packs a 6-byte record = [2B tag][4B pointer@+2].
The free (0x0802CEFE, freelist push, head table 0x0801D544) writes a 4-byte "next" link at cell+0,
which overwrites the tag (bytes 0,1) and the TOP 2 bytes of the +2 pointer (bytes 2,3). Reading
4 bytes at +2 afterward yields 0xA1B8A1A8 (low half of the pointer + low half of the link).

Everything the EMULATOR does here is correct/by-design:
 - 6-byte record with pointer at +2: preindexed disp unit is bytes (UPDATE 47), store is byte-correct.
 - free-list link at +0: normal freelist behavior.
 - later copy loops just propagate 0xA1B8A1A8 to other cells (0x18030000, then back to 0x1802A1B0
   at instr ~1400986); the crash reads the final copy at 0x08023EA1, instr 1480987.

=> Genuine USE-AFTER-FREE in NC's own logic: cell 0x1802A1B0 is built as a live record at 1320435
and FREED 114 instructions later at 1320549, while a pointer to it (as a record) stays live and is
dereferenced at the crash. NOT an emulator store/offset/scaling bug (all verified correct).

SAME WALL as always, now grounded on the RIGHT crash: is the free at 1320549 (a) NC's own bug /
premature free, or (b) driven by a subtle wrong emulator value upstream? Branches + instructions on
the path verify correct; both emulators are the same author's (no oracle). To go further needs the
free's trigger: trace the 114 instrs 1320435..1320549 - what NC logic decides to free cell
0x1802A1B0, and whether a pointer to it should have been cleared/updated at the free.
NEXT: put a breakpoint-style capture at 0x0802CEFE for r2==0x1802A1B0 and walk back its caller chain
(L/B) to the NC routine that frees it; check that routine's free condition against its inputs.

## UPDATE 49 (2026-07-14): Full chain closed - list-concat frees its input record (dangling pointer)
Walked the caller chain of the birth-free (instr 1320549, cell 0x1802A1B0) via frame chain
(PREVB@B+0, RETA@B+4). diag_freewalk (test/diag_freewalk.c), pristine run:
  free 0x0802CEFE  <- 0x080248E0 (routine 0x08024829)  <- 0x08008577 (producer)
    <- 0x0800850D <- 0x08005140 / 0x0800504C (reader 0x08005024) <- 0x080028D5 <- ... <- entry.

The free is invoked at:
  080248D2 h1 := IND(b.20)          ; size = count field of input record b.20
  080248D5 w1 * $4                  ; size bytes = count*4
  080248D7 call 0x0802CEB3(b.20, size)   ; FREE input record b.20 (=0x1802A1B0)   -> ret 0x080248E0
  080248E5 call 0x0802CEB3(b.24, size)   ; FREE the other input record b.24
So routine 0x08024829 is a list MERGE/CONCAT: merges two input records (b.20,b.24) into a new
list (the -2 copy loop, UPDATE 46/48) then FREES BOTH INPUTS. Cell 0x1802A1B0 is an input,
consumed+freed here. Legit ONLY if no other live pointer to it remains.

But a pointer to 0x1802A1B0 DOES survive (via the mailbox/holder set by producer 0x08008577;
see UPDATE 43 mailbox 0x08000200 chain) and is dereferenced at the crash (0x08023EA1, instr
1480987) -> 0xA1B8A1A8 -> PV 0x08023EA4. This is the ORIGINAL "concat frees input, holder not
updated -> dangling pointer" diagnosis, now proven on the CORRECT (pristine) crash with the exact
free call site.

Possible actionable thread (unverified): the record was ALLOCATED as 6 bytes (0x0802401E call
GETB size=6) but is FREED as count*4 (=4 if count=1) here. Alloc-size vs free-size mismatch could
put the cell in the wrong freelist bucket. Worth checking, but likely NC-internal, not the
dangling-pointer trigger.

FINAL STATE OF TRACING: the mechanism is fully mapped - list-concat 0x08024829 frees an input
record whose pointer stays live in the mailbox/holder, then it is read after free. Every
instruction, offset, branch, and the free itself verify CORRECT vs operands (emulator faithful).
Whether NC should NOT free it / should update the holder (NC's own latent bug) vs a subtle
upstream emulator value that makes the holder retain the stale pointer CANNOT be resolved by
tracing - both emulators are the same author's (no oracle), no real-HW trace. Needs the ND-500
manuals on the relevant NC operation, or real hardware. This is the wall.

## UPDATE 50 (2026-07-14): MON audit + C# ANSWER doc converge on 422B GSWSP as the cause
Spun 6 agents comparing our MON handlers to the carved L07 oracle (the REAL SINTRAN code, a true
oracle unlike the sibling emulator). Also found the C# side's own conclusion at
/mnt/d/ND/500/FraTor/nc/ANSWER-no-rewrite-analysis.md.

Key results:
- 321B UEADM: our return (success vs error 52B/174B) makes NO difference to the crash - tested
  live, crash identical at 0x08023EA4/1.487M. RULED OUT for this crash. (Committed handler
  returns error 174B; earlier success was an uncommitted hack.)
- NC-A:INIT missing open: RULED OUT (C# doc: pre-placing one changes nothing).
- 41B ROBJE: the field NC uses (file page count @word 32B) is CORRECT. Not the cause.
- 50B OPEN: writes file# into caller slot0 (outside ND-500 register contract) - target addrs are
  NC stack, likely harmless; low priority.
- 312B/143B/317B: minor/placeholder, not crash-relevant.

C# ANSWER doc (authoritative prior finding):
- The earlier "no rewrite" blocker (~instr 1.1M) was OUR GETB instruction's invented
  auto-initialization fallback re-seeding the whole 0x18000000 heap and handing 0x18000000 out
  again -> overlap -> wiped NC's OUTPUT file record. FIX (remove the fallback; always raise STO)
  is ALREADY IN OUR TREE (instruction_helpers.c nd500_heap_alloc_block: block_addr==0 ->
  trap_stack_overflow, no re-seed). That is why we now run to ~1.48M.
- The C# doc explicitly names OUR current crash as the NEXT blocker: "PV at PC 0x08023EA4, data
  0xA1B8A1A8 at instruction 1,501,427 ... most likely the emulator's MON 422B GSWSP /
  segment-extension path returns something NC's heap extender does not expect (garbage data
  address = a pointer fabricated from a bad GSWSP result)."

NC's heap extender: on GETB freelist exhaustion, GETB raises STO -> NC trap handler
entt 0x0802CF71 -> 0x0802CCF7 -> calls MON 422B GSWSP (first seen ~instr 77,922) to get more
segment space, paints new block 0xF0F0F0F0, seeds the freelist. If GSWSP returns a wrong
segment base/size/number (or installs the capability in the wrong domain), NC's freshly-seeded
heap region is wrong -> the halfword-shifted 0xA1B8A1A8 pointer -> PV.

422B GSWSP agent findings (src/libmon/handlers/mon_422B_GetScratchSegment.c + callback
src/cpu/nd500_segment_alloc.c):
 1. auto-assign scans from seg 2 (hard-coded), oracle says lowest free (0). Low severity.
 2. home-grown physical-PFN allocator: magic floor start_pfn>=1000 (0x1F4000) + multi-page
    page-table span undercount in find_highest_used_pfn -> latent overlap. Oracle model is a
    swap-file reservation, not hand-picked PFNs.
 3. STRONGEST (unverified): capability installed in domain cpu->CED - is CED NC's user domain at
    MON dispatch time, or the monitor domain? If wrong domain, NC's access to seg-3 0x18000000+
    resolves through a different/absent capability.

NEXT: verify #3 (log CED at the GSWSP callback vs the domain NC uses to access 0x18000000+), and
audit the GSWSP segment base/size mapping against what NC's heap extender at 0x0802CCF7 computes.
All runs reset sandbox to pristine first.

## UPDATE 51 (2026-07-14): 422B GSWSP segment mapping VERIFIED CLEAN - not this crash
Instrumented the GSWSP callback (nd500_segment_alloc.c, env ND500X_SEG_DUMP) on a pristine
COMPILE B,B,B. All segments allocated fresh, correctly timed, contiguous, NON-overlapping:
  ic=16    seg2 v=10000000 phys=[1F4000,235000) ptbl=[235000,235800)
  ic=760   seg3 v=18000000 phys=[235800,276800) ptbl=[276800,277000)   <- NC's heap
  ic=77901 seg4 v=20000000 phys=[277000,2F8000) ptbl=[2F8000,2F8800)   <- heap extension
(reqBytes 264193 = 1004001 OCTAL; the monlog logs %o. The 422B agent's "491 pages" misread the
octal as decimal - it is 130 pages.) NC's heap seg3 is allocated at ic 760 and used fine for
~1.48M instructions before the crash. No segment overlap, no domain mismatch (a wrong domain
would fault on first heap access, not at 1.48M).

DISPROVES the C# ANSWER-doc hypothesis ("garbage data address = pointer fabricated from a bad
GSWSP result"): the fault value 0xA1B8A1A8 = halfwords 0xA1B8/0xA1A8 = the LOW halves of seg3
heap cells 0x1802A1B8/0x1802A1A8. It is built from HEAP-INTERNAL cell addresses, NOT from any
GSWSP segment base (those are 0x18000000/0x20000000). So the corruption is heap-internal - the
NC concat-frees-live-cell UAF already traced in UPDATE 48/49 - inside a CORRECTLY-mapped heap.

So the memory/MON-return hypothesis (GETB + GSWSP) resolves to: GETB auto-init fallback was the
REAL bug (fixed, +400k instrs); GSWSP is clean. This specific crash (0x08023EA4) is NOT a MON /
segment bug. It is back to the NC-internal UAF wall: routine 0x08024829 (list concat) frees its
input record cell 0x1802A1B0 while a pointer to it stays live; the overlap of the record's +2
pointer and the free-link +0 write makes 0xA1B8A1A8. Every instruction/branch/offset verified
correct. Whether NC should not free it, or an upstream instruction/value makes it, is the same
oracle-needing wall.

GSWSP low-priority real fixes (not this crash): auto-assign scans from seg2 not seg0; home-grown
PFN allocator with magic floor + page-table span undercount (latent, not triggered by NC's
130-page segments). Left as-is.

## UPDATE 52 (2026-07-14): HEAP OVERLAP CONFIRMED - GETB hands out a block overlapping free sub-blocks
User's heap-overlap hypothesis is CORRECT. Added a freelist-consistency check inside GETB
(nd500_heap_alloc_block, env ND500X_GETB_TRACE): after handing out a block, walk every FLOG[k]
and report any free node whose range overlaps the block just allocated. Pristine COMPILE B,B,B:

At ic=1407133 GETB hands out a 32KB block at 0x1802A1B0 (log_size=13) while EIGHT smaller free
blocks INSIDE that range are still on their freelists:
  FLOG1 [1802A1B0,+8)  FLOG2 [1802A1C0,+16)  FLOG3 [1802A1E0,+32)  FLOG5 [1802A280,+128)
  FLOG6 [1802A300,+256) FLOG8 [1802A400,+1024) FLOG10 [1802B000,+4096) FLOG12 [1802C000,+16384)
=> the 32KB block overlaps 8 live/free sub-regions. GETB then re-hands sub-cells from FLOG1..12
that sit INSIDE the 32KB block -> two allocations, same memory -> the 0x1802A1B0 record clobber
-> 0xA1B8A1A8 -> PV 0x08023EA4.

NOT a two-heaps bug: GETB's heap_vars = TOS = 0x0801D538, TOS+12 = 0x0801D544 = NC's freelist
head table. Same structure. So the inconsistency is WITHIN the shared freelist.

MECHANISM (how an 8-byte cell lands on the 32KB list FLOG13): NC's free wrapper 0x0802CEB3
buckets a freed block by SIZE, and the size comes from the block's count/tag field (concat free:
call 0x0802CEB3(b.20, mem16[b.20]*4)). Cell 0x1802A1B0's count/tag was corrupted to 0x1804, so
the free computes a huge size -> bucket 13 (ceil(log2)) -> the 8-byte cell is pushed onto FLOG13.
This is SELF-REINFORCING: a corrupt count causes a wrong-bucket free, which causes an overlapping
GETB alloc, which corrupts more counts. The bucketing is the FLOAT log2 path audited in UPDATE 42
(alog2/dconv/int at 0x0802CEB3 -> ceil(log2(size))).

OPEN (the FIRST domino): where does the FIRST corrupt count/wrong-bucket come from? Either (a) a
genuine off-by-one in the float bucket at a size boundary (our alog2/ceil vs real HW - UPDATE 42
verified samples but not the boundary/free path), or (b) an earlier overlap seeded by heap init.
NEXT: instrument the FREE wrapper 0x0802CEB3 to log (block, size_arg, computed_bucket) and detect
the FIRST free that pushes a block onto a freelist bigger than the block's real allocated size -
that free is the root. Reset sandbox to pristine each run.

## UPDATE 53 (2026-07-14): ROOT = double-free amplified by size-read-from-freed-memory
Freelist-head trace (env ND500X_NC_WWATCH, [FLPUSH] on writes to head table 0x0801D544) shows
node 0x1802A1B0 pushed onto:
  1121238 FLOG2(16B)  1326721 FLOG1(8B, legit free)  1407065 FLOG13(32768B) <-- CORRUPTING
The 8-byte cell is pushed onto the 32KB freelist at 1407065. GETB then hands out a 32KB block
there (1407133) overlapping 8 live sub-blocks (UPDATE 52) -> record clobber -> crash.

WHY bucket 13: NC's free reads the block's size from its FIRST halfword (call site 0x080248D2
h1 := IND(b.20); free wrapper 0x0802CEB3 buckets by that size). But cell 0x1802A1B0 was already
FREED at 1326719, so its first word holds the freelist LINK 0x1802A1B8; mem16 = 0x1802 = 6146 ->
ceil(log2(6146)) = 13 -> FLOG13. Verified: the bogus "size" is the stale link's high halfword.

=> DOUBLE FREE: cell freed legitimately at 1326719, a stale/dangling pointer survives, and NC
frees it AGAIN at 1407065. The second free reads garbage size from the freed cell -> wrong bucket
-> 8-byte cell on 32KB list -> massive GETB overlap. The double-free is the TRIGGER; the
size-from-freed-memory is the AMPLIFIER that turns a dangling pointer into heap catastrophe.

Whether the double-free is NC's own dangling-pointer bug or driven by an upstream emulator value
is the same wall - BUT now there is a precise, detectable event to work back from: the free at
instr 1407065 that pushes 0x1802A1B0 to FLOG13. NEXT: walk the caller chain of the 1407065 free
(diag_freewalk pattern at PC 0x0802CEFE, node 0x1802A1B0, ic~1407065) to find the NC routine
double-freeing it, and check whether a pointer that should have been cleared at the first free
(1326719) is still live. Reset sandbox pristine each run.

## UPDATE 54 (2026-07-14): double-free caller chain = stale mailbox -> reader -> concat
diag_freewalk on the 1407063 double-free (node 0x1802A1B0, link 18038000, COMPILE B,B,B pristine):
  free 0x0802CEFE <- 0x080248E0 (concat routine 0x08024829) <- 0x080050D3 (reader 0x08005024)
    <- 0x080028D5 <- 0x080023A4 <- 0x080004F5 <- entry.
Same mailbox producer/consumer chain as UPDATE 43, now on the pristine crash: reader 0x08005024
consumes the stale mailbox pointer 0x08000200 = 0x1802A1B0 (freed at 1326719) and hands it to the
concat 0x08024829 which double-frees it. The mailbox/holder was never updated when the cell was
freed at 1326719.

FULL VERIFIED CHAIN (pristine COMPILE B,B,B, crash 0x08023EA4 @ ~1.487M):
 1326719  cell 0x1802A1B0 freed legitimately (FLOG1, 8B)
 (mailbox 0x08000200 still = 0x1802A1B0 -> dangling)
 1407063  reader 0x08005024 -> concat 0x08024829 DOUBLE-FREES 0x1802A1B0; free reads garbage size
          (0x1802 from the stale freelist link) -> bucket 13 -> pushed to FLOG13 (32KB)
 1407133  GETB hands out 32KB block at 0x1802A1B0 overlapping 8 live sub-blocks (FLOG1..12)
 ~1407140 record built in the overlapping block; 0xA1B8A1A8 formed
 1480xxx  0xA1B8A1A8 dereferenced -> PV 0x08023EA4

FIX DIRECTIONS:
 A. Root (NC-logic wall): why does mailbox 0x08000200 retain 0x1802A1B0 after the 1326719 free?
    Find the write that should clear/update it. Same (b)-vs-(c) question, but now with an exact
    detectable double-free event (1407063) and holder (mailbox 0x08000200) to work back from.
 B. Defensive/behavioural: make GETB validate freelist integrity (refuse to hand out a block that
    overlaps an existing free node) and raise STO instead - NC's STO handler extends the heap and
    may continue (matches the user's "interrupted and continues" intuition). Test whether real
    ND-500 GETB validates. Would convert the silent overlap into a recoverable trap.

## UPDATE 55 (2026-07-14): GETB overlap-guard test - prevents overlap, exposes next heap domino
Added env-gated GETB integrity guard (ND500X_GETB_GUARD): if the block GETB is about to return
overlaps a still-free block on any freelist, restore the head and raise STO instead of handing
out overlapping memory. Pristine COMPILE B,B,B, 8M step cap:
  [GETB-GUARD] ic=1407133 refused overlapping [1802A1B0,+32768) (FLOG1 [1802A1B0,+8)); raising STO
  [GETB-GUARD] ic=1407394 refused again
  [STOP] page fault at PC=0x0802CE43 data=0x30100000 instr=1407437
So preventing the overlap makes NC's STO handler extend the heap (GSWSP -> seg 6 @ 0x30000000),
then the heap FILL at 0x0802CE43 page-faults writing 0x30100000 - the extended segment's fill
runs past what is mapped. NOT a clean recovery; the next heap/segment domino.

CONCLUSION: the failure is a CHAIN of heap bugs (user's thesis confirmed): double-free (UPDATE
53/54) -> wrong-bucket -> GETB overlap (UPDATE 52) -> [if guarded] STO extend -> segment-fill
page fault. The clean fix is the ROOT (stop the double-free: mailbox 0x08000200 retaining the
stale 0x1802A1B0 after the 1326719 free). Alternatively work the cascade: (a) prevent wrong-bucket
free of an already-free cell, (b) fix segment-extension fill bounds (seg 6 page fault at
0x0802CE43 / GSWSP size). Guard left env-gated (off by default), diagnostic only.

## UPDATE 56 (2026-07-14): stale holder is a DRIVER-FRAME LOCAL, not the mailbox
Correction to the long-standing "stale mailbox" theory: on pristine COMPILE B,B,B the mailbox
0x08000200 is UPDATED CORRECTLY - it moves off 0x1802A1B0 (->18003000) at instr 1323358, BEFORE
the 1326719 free (diag_mailbox). So the mailbox is not the dangling holder.

The real holder (holder.c capture at the two frees): a long-lived DRIVER FRAME at B=0x10000110
(function returns to 0x080028D5) holds record ptr 0x1802A1B0 in its local b.14 throughout:
  FIRST free  ic=1326699: concat (called via producer 0x08008577) frees recordA=0x1802A1B0;
              driver frame B=0x10000110 b.14 = 0x1802A1B0 (deep in stack).
  DOUBLE free ic=1407042: concat (called via reader 0x080050D3) frees recordA=0x1802A1B0 AGAIN;
              same driver frame B=0x10000110 b.14 still = 0x1802A1B0.
So the driver puts 0x1802A1B0 in b.14, one path (producer concat) FREES it at 1326719, the driver
NEVER clears b.14, then feeds the stale b.14 down a second path (reader concat) -> double-free at
1407063 -> wrong-bucket -> GETB 32KB overlap -> crash. Classic use-after-free of a LOCAL the
driver fails to clear after a consumer frees the record it points to.

NEXT: identify the driver function owning frame B=0x10000110 (entered from ~0x080028D0, returns
0x080028D5). Determine whether NC legitimately frees the record once and the second use is NC's
own UAF, or whether an upstream emulator value makes the driver take the free-then-reuse path.
This is the exact, minimal root site: driver b.14 holds 0x1802A1B0 across a free. Reset pristine.

## UPDATE 57 (2026-07-14): FINEST ROOT - reader captures mailbox, producer frees it mid-run
The driver (0x080028CF) calls the reader 0x08005024 (returns 0x080028D5); the reader IS frame
B=0x10000110. Reader logic: 0x0800503E w1 := mem[mailbox 0x08000200]; 0x08005044 w1 =: b.20 (the
local at B+0x14); then it REFRESHES the mailbox at 0x08005106 to a fresh cell. So the reader
CAPTURES the old mailbox value into b.14 and refreshes the mailbox. This reader invocation:
 - starts before instr 1323358, reads mailbox=0x1802A1B0 -> b.14, refreshes mailbox->18003000
   (this IS the 1323358 mailbox change).
 - runs ~84,000 instructions.
 - DURING that run, the producer path frees 0x1802A1B0 at 1326719.
 - at ~1407042 the reader consumes its now-stale b.14 (=freed 0x1802A1B0) via concat 0x08024829
   -> double-free -> wrong-bucket(13) -> GETB 32KB overlap -> crash.

ROOT (finest grain): a producer/consumer ordering UAF in NC's list processing - the reader
captures a record pointer from the mailbox and holds it across ~84k instructions, during which
the producer frees that record. When the reader consumes the captured pointer, the cell is freed.
The mailbox itself is managed correctly; the stale copy is the reader's captured local b.14.

This is the terminal (b)-vs-(c) question at maximum resolution: either NC legitimately expects the
captured record to stay live and the producer's free (1326719) is wrong/mistimed (upstream
emulator value), or NC has a genuine capture-across-free UAF that real HW happened to tolerate
(freed memory not reused in time). Cannot be resolved further without an oracle for NC's intended
behaviour (no such manual; needs real-HW/ working-emulator diff at the 1326719 free decision).
Practical path: defensive fixes to get PAST it (see UPDATE 55 GETB guard + fix the seg-extension
page fault), or diff the 1326719 free against a working reference to decide if that free should
happen at all.

## UPDATE 58 (2026-07-14): NC HAS TRAP HANDLERS - PV dispatched to NC = clean exit (user was right)
Captured cpu->THA at the crash: NC installed a FULL trap-handler vector (THA) with handlers for
traps 18..41, INCLUDING the "non-ignorable" ones: THA[36]=PV handler 0x0802D817, THA[35]/[37]/[38]
(ISE/THM/PGF) etc. Each stub: `entt; w1 := <trap#>; go 0x0802D865` -> common dispatch
`store trap# @0x0801D92C; callg 0x0801D654(r1); rett` (per-trap handler table at 0x0801D654).
So NC EXPECTS to receive these traps via THA - it would not install handlers for traps it can
never get. Our emulator treated PV (and all bits 32-41) as an unconditional FATAL stop
(cpu.c raise_trap, TRAP_INTERRUPT_MASK) and never invoked THA. THAT is an emulator bug.

Experiment (env ND500X_TRAP_DISPATCH, cpu.c): route non-ignorable traps that have a THA handler
to invoke_trap_handler instead of halting. Result on pristine COMPILE B,B,B:
  [TRAP-DISPATCH] trap 36 -> NC handler 0802D817 (instr 1487157, faultPC 08023EA4)
  [STOP] MON 0B LEAVE (Program exit) instr 1488385
=> NC's PV handler runs and does a CLEAN PROGRAM EXIT instead of an emulator crash. Confirms the
user's "is there a crash handler" hypothesis: YES. This is a real correctness fix (halt -> NC's
own graceful error exit, faithful to HW). BUT B.NRF is still 0 bytes - NC catches its corrupted
state and bails; it does NOT compile. The heap double-free (UPDATE 52-57) is still the root that
must be fixed for a real compile.

Caveat: ND500X_TRAP_DISPATCH + ND500X_GETB_GUARD together -> infinite trap loop (trap 35 ISE at
0x0802D935, the handler region itself faults; our RETT retries trap_saved_PC=faultPC forever).
So the RETT return semantics for non-ignorable/handler-internal traps need work before this can be
a default. For now it is a diagnostic that (a) proves NC's trap handlers exist and work, (b) turns
the emulator crash into NC's clean exit.

Answers to the two side questions:
 - "CPU on free-of-a-free?" There is no CPU double-free check. NC frees via plain STORE
   instructions pushing a freelist; the CPU executes them faithfully and the freelist just
   corrupts. GETB (alloc) is a CPU instruction; FREEB exists but NC does not use it.
 - "MON carve on the free code?" The free/alloc is NOT a MON call - it is NC's own code
   (0x0802CExx freelist stores) + the GETB CPU instruction. The carve covers SINTRAN MON services
   only, so it does not describe the free. MON 422B GSWSP only supplies the heap SEGMENTS (clean).

## UPDATE 59 (2026-07-14): trap-dispatch made DEFAULT-ON + nesting guard (RETT loop fixed)
Reworked the non-ignorable trap path in cpu.c raise_trap:
 - Non-ignorable traps (bits 32-41) with an installed THA handler are now DISPATCHED to the
   program handler by DEFAULT (opt out with ND500X_NO_TRAP_DISPATCH=1), matching the ND-500
   architecture (a program installs THA handlers to receive these traps; NC sets THA[36]=PV etc.).
 - NESTING GUARD: only dispatch when !cpu->in_trap_handler. A fault while already inside a handler
   is a genuine double fault (our single-level trap_saved_* state cannot nest) -> fall through to
   halt. This fixes the infinite ISE loop (the loop was a RETT at 0x0802D935 running with
   in_trap_handler out of sync after a nested dispatch corrupted the single-level saved state).
 - If THA==0 or the slot is 0 -> no handler -> halt (Trap Handler Missing), as before.

VERIFIED:
 - test_instruction_validation: ALL PASSED (39,803 cases).
 - ote_instructions + trap/mmu unit tests: 100% pass.
 - Full ctest: only pre-existing mon_calls fails (asserts 321B deprecated; carve disproves;
   unrelated). dom_nc_compile_a/b pass.
 - COMPILE B,B,B (default, no env): NC's PV handler runs -> clean MON 0B exit at 1488385 (was an
   emulator PV crash at 0x08023EA4). Faithful: the emulator no longer halts on a fault the program
   is equipped to handle.
 - COMPILE + GETB-guard: infinite ISE loop GONE -> clean page-fault halt at 1407437 (double fault).

Net: emulator correctness fix. The PV crash is no longer an emulator halt - NC catches it and
exits gracefully (still no NRF; the root heap double-free remains the reason the compile fails).

## UPDATE 60 (2026-07-14): TWO carve-verified MON I/O fixes - functions now parse, was crashing
User's MON-call hypothesis confirmed by comparing our file-I/O handlers to the carved L07 code.

FIX 1 - 117B RFILE (mon_117B_ReadFromFile.c): was returning error 3 (EOF, K set) on ANY short
read (bytes_read < num_bytes). Carve (006-S3FS worker 102130B) proves error 3 is a RANGE check
(block entirely beyond file, SAA 3 @102403); a within-bounds short read RETURNS SUCCESS and writes
the ACTUAL transferred byte count back to the caller (success path 102433-102470: count->B+16->
rec[26]). NC reads 4096 bytes of a 23-byte source -> our old code returned ERROR -> NC treated the
source as unreadable -> rejected valid code. Fix: short read = SUCCESS, write actual bytes_read to
NoOfBytes param (arg 4); error 3 only when bytes_read==0.

FIX 2 - 73B SMAX (mon_73B_SetMaxBytes.c): was immediately ftruncate-ing the host file. Carve shows
SMAX only RECORDS the logical max-byte length (applied at CLOSE); premature truncate shortens the
scratch file under NC's feet. Fix: record entry->object_entry.bytes_in_file only; no ftruncate.

RESULT (pristine COMPILE, these fixes on):
 - main(){} , f(){} , main(){ ; }  -> "no errors detected" (CLEAN PARSE). Before the fixes these
   CRASHED (heap PV / invalid-instruction). The heap double-free crash on function programs is GONE.
 - Variable declarations with a type keyword still fail: `int x;`/`char c;` -> "error in
   SIMPLE_DECLARATOR / IDENTIFIER deleted"; `main(){int x;}` -> "error in PRIM0". Line endings
   (LF/CR/CRLF) make no difference. This is a separate, narrower remaining bug in the
   type-declaration parse path (not yet root-caused; could be another I/O detail on the scratch
   read-back, a symbol-table/heap path, or a CPU instruction in the declarator parser).

VERIFIED: test_instruction_validation ALL PASS (39,803). ctest: pre-existing mon_calls fails, and
dom_nc_compile_b now "fails" because NC's behaviour changed (baseline was the OLD broken output;
needs re-baselining). No K&R program produces NRF yet (all blocked on the type-declaration parse).
NEXT: root-cause the `int x;` declarator rejection (dump the scratch read-back bytes the parser
sees; if clean, trace NC's declarator parser at the error).

## UPDATE 61 (2026-07-14): remaining bug = NC type-keyword recognition entirely non-functional
After the 117B/73B fixes, functions compile clean. The ONE remaining blocker: NC does not
recognize ANY C type/storage keyword. Tested `main(){ <kw> x; }` for:
  int char float double long short unsigned void register static extern auto const -> ALL FAIL.
Control keywords WORK: if, while, sizeof. `int;` alone is OK (NC tokenizes "int" as an ordinary
identifier). `int a;` fails IDENTICALLY to `a b;` / `xyz a;` (two identifiers). So NC treats every
type keyword as a plain identifier -> the type-keyword table/lookup is systematically empty or
unread. Reads are byte-perfect (int a; = 69 6E 74 20 61 3B verified in hex, source+scratch). Not a
hash collision (all types fail, not some). Not the string-compare instruction (Scomp: 0 calls
during compile). Not a missing include (the C keyword strings I grepped in the DOM were false
positives - substrings of NC command names like "define-user-INTerface"). Mechanism/location of
NC's type-keyword recognition is not yet found. NEXT: find where NC classifies an identifier token
as a type - likely a predefined-type table populated at NC init; determine why it is empty/unread.

## CHANGE LOG for C# (RetroCore) sync - apply all of these to keep the two emulators in sync
(Deferred handoff per user; do it once bugs are closed. Each is a real fix or an opt-in diagnostic.)

REAL FIXES (must be ported to C#):
1. src/cpu/instructions/COMPARE/Comp2.c - F/D COMP2 compares as IEEE floats (was integer math on
   float bits). [pre-session, in working tree]
2. src/libmon/handlers/mon_50B_OpenFile.c - added missing `break;` after case -52 (was falling
   through to "file already open").
3. src/libmon/handlers/mon_312B_CheckMonCall.c - return MCTAB[321B]=065453B (was retracted
   0112376 GOTAB value).
4. src/libmon/handlers/mon_117B_ReadFromFile.c - RFILE: a within-bounds SHORT read now returns
   SUCCESS and writes the actual bytes-read count to the NoOfBytes param (arg 4); error 3 (EOF)
   only when bytes_read==0. Carve-verified (006-S3FS worker 102130B: error 3 is a range check;
   success path 102433-102470 returns the residual byte count). THIS made functions compile.
5. src/libmon/handlers/mon_73B_SetMaxBytes.c - removed the immediate ftruncate; SMAX only records
   entry->object_entry.bytes_in_file (carve: length applied at CLOSE, not now).
6. src/cpu/cpu.c raise_trap - non-ignorable traps (bits 32-41) with an installed THA handler are
   dispatched to the program handler by default (guard: only when !in_trap_handler; opt out
   ND500X_NO_TRAP_DISPATCH). NC installs THA[36]=PV handler 0x0802D817 and expects to receive PV;
   the machine no longer halts on it. Requires the ND-100/500 nesting/RETT semantics to match.
7. src/cpu/cpu.c - added #include <stdlib.h> (getenv was implicit-int, truncated the pointer,
   segfaulted the host when an env flag was set).

OPT-IN DIAGNOSTICS (env-gated, off by default; port optional):
 - ND500X_NC_TYPETAG_GUARD (cpu.c) - old artifact guard, irrelevant to the real crash.
 - ND500X_GETB_TRACE / ND500X_GETB_GUARD (instruction_helpers.c) - freelist overlap detector.
 - ND500X_NC_WWATCH (cpu_instr.c + instruction_helpers.c) - record-range write watch + [FLPUSH].
 - ND500X_SEG_DUMP (nd500_segment_alloc.c) - GSWSP segment layout dump.

STILL-OPEN C# divergences to note: 41B ROBJE object-entry middle fields are byte-shifted vs App
F.6 (access@28/device@32/dates/version) - deferred (NC only reads pages@52 which is correct).

## UPDATE 62 (2026-07-14): *** BREAKTHROUGH *** SCOMP S-flag inversion fixed -> C now PARSES
ROOT of the type-keyword failure FOUND and FIXED. NC recognizes C keywords via a BINARY SEARCH
over a sorted 8-byte-per-entry keyword table at vaddr 0x0800A378 (auto..while, token codes at
0x0800A45C), using SCOMP (byte string compare) + `if>=go`/`if=go` branches (lookup code at
0x0800E70A..0x0800E767). The search walked the WRONG way for type keywords.

Cause: SCOMP's byte-difference S flag was INVERTED. Verified against BOTH the ND-500 Reference
Manual (sect 14.10 p254, table lines 8727-8736: "smaller byte in source-1 -> S=1", "greater byte
-> S=0") AND against COMP/COMP2/PCOMP (all set S=1 <=> source1<source2). Our Scomp.c byte-diff
branch set S the opposite way (S=1 when source1>source2). The conditional branches are all CORRECT
per the manual (if>=go tests S=0, verified). So SCOMP alone was wrong.

FIX: src/cpu/instructions/COMPARE/Scomp.c byte-diff case - set S when string1_less (source1<
source2), clear otherwise. One-line polarity swap. (The K=0 length branch was already correct.)

RESULT: `int x; main(){ x = 1; }` -> "no errors detected" (PARSES CLEAN). Before: "error in
SIMPLE_DECLARATOR / IDENTIFIER deleted". Every type/storage keyword now recognized. This was the
last parse blocker - the entire chain (functions-only, then type keywords) is resolved.

SUITE: 11 SCOMP tests now FAIL (ByteDiff_S1Smaller/S1Greater) - they assert the OLD inverted
convention (generated from the C# emulator, which has the SAME bug). Manual-confirmed correct, so
these 11 need regenerating with S=1<=>source1<source2. Not a real regression.

NEW BLOCKER (next): after the clean parse, NC enters code generation and RUNS AWAY (no halt at
30M instructions, NRF still 0). PC concentrated in 0x0802B000-0x0802E000 (0x0802Bxxx ~71%/M) -
a probable loop in the codegen/output path. This is the next thing to chase; it is PAST the parse.

## C# SYNC - add to the change log (UPDATE 61):
8. src/cpu/instructions/COMPARE/Scomp.c - byte-difference S flag was inverted; set S=1 when
   source1<source2 (manual sect 14.10 / matches COMP). C# RetroCore has the same bug - fix there
   too, and REGENERATE the 11 SCOMP ByteDiff test cases (they encode the old wrong convention).

## UPDATE 63 (2026-07-14): CODEGEN RUNAWAY characterized (Phase 1)
Sandbox reset from test/nc_fixtures; `COMPILE A,A,A` (int x;main(){x=1;}) under ND500X_PIN_CLOCK=1.
Harnesses rebuilt: build/bin/diag_nc_checkpoints, build/bin/diag_nc_pctrace.

FINDINGS:
- NC parses clean then RUNS AWAY in codegen: 40,000,000 instrs, NO halt, NRF=0, stop=none.
- The runaway is an EXACT 92-instruction INFINITE LOOP. Proven: at instr 5,000,000 and
  5,000,092 the FULL sampled CPU state is byte-identical (PC=0x0802BE21, B=0x10000174,
  I1=0x00000000, I2=0x18000000). A deterministic machine in identical state repeats forever.
- Across the whole 40M run I3=0x00000006 and I4=0x0802D7A7 are CONSTANT; B oscillates over only
  3 frame values (0x10000154/74/B8), 51 distinct total - so NOT stack-overflow recursion.
- Hot code region: 0x0802B900-0x0802BE39 (many small ents/ret subroutines calling each other).
  PC histogram over the loop: 69% in 0x0802Bxxx, 25% in 0x0802Cxxx.
- Loop body walks a record via register R (r1.NN field reads): key decision branches are
  0x0802BA89 `w comp2 r1.27,W2` -> `if<<go` (D9, unsigned-less, tests C=0) [TAKEN, constant];
  0x0802B948 `by comp2 r1.38,$7`; 0x0802B951/B95C `h comp2 r1.39,#64/#128` -> if<</if>>=.
  Counter-advance candidates INSIDE loop: 0x0802BB0C `w add2 IND(b.60),$2` (increment node
  counter by 2), 0x0802BAE5 `w stz r1.27`, 0x0802BADC `w1 laddr r1.58; =: IND(b.60)`.
  Pointer global: 0x0801067C (b.56 mirrors it; stored at 0x0802BE33 and 0x0802B9DA `b=:`).
- Comp2.c + IfLessThanGo/IfUnsignedGreaterEqualGo verified correct for the common case (and the
  parser, which leans on comp2/branches heavily, now works). So a broad compare/branch bug is
  unlikely; suspect a STORE/advance that does not persist (wrong effective address for
  add2/stz/laddr through IND(b.NN)/r1.NN), OR R (node ptr) not advancing.

NEXT PROBE: log R register + the memory word at IND(b.60) target and 0x0801067C across loop
iterations. If R and that node are byte-identical each 92-instr cycle, the list "advance to next"
is the bug -> locate the load of node->next and the store back. (Mirror of the SCOMP crack: find
the single op whose result should change state but does not.)

## UPDATE 63b (2026-07-14): probe results - R and codegen-global are NULL
diag_codegen_loop (test/diag_codegen_loop.c) logs R/A0/L/B/TOS + [0x0801067C] + node dumps at
every hit of 0x0802BE21.
- 0x0802BE21 is a SHARED subroutine TAIL (ret at 0x0802BE39), NOT a unique loop head; hit every
  ~124 instrs starting as early as instr 3608 (loop is established almost immediately after the
  clean parse, well before the 5M sample).
- R = 0x00000000 CONSTANT and codegen pointer-global [0x0801067C] = 0x00000000 CONSTANT across
  all hits. So the r1.NN "record field" reads are NOT via R; and the codegen output pointer is
  never populated (NRF stays empty - consistent).
- B cycles among 3 frame values -> 3 nested/mutually-recursive routines form the loop.
- An early trap fires: raise_trap trapBit=0x08000000 (bit 27) trapPC=0x0802CF60 at instr ~740
  (and again ~77221). Bit 27 trap identity + relevance UNKNOWN - check next (0x0802CF60 is in the
  hot 0x0802Cxxx region; may be the trap-dispatch path re-entering codegen).
CORRECTION to U63: the r1.NN operands are record-relative to an operand register loaded from
b.24/b.60 (a LOCAL holding a pointer), not the R register. The real loop variable is one of those
locals or a memory cell - not yet identified.
NEXT: dump ALL registers (I/A/E/R/L/B/TOS/ST1) per instruction for ONE full period at ~instr
3763-3887 (124 instrs, B=0x100000FC), find the backward branch that closes the loop and the
compare that gates it; then read the exact memory operand that compare consumes. Also identify
the bit-27 trap at 0x0802CF60.

## UPDATE 63c (2026-07-14): full 124-instr period captured; probe needs MMU-aware reads
- Captured ONE FULL loop period, instr 3763->3887 (124 instrs, PC 0x0802BE21->0x0802BE21),
  state byte-identical at both ends (B=0x100000FC, I1=0, I2=1) -> INFINITE confirmed at period 124.
  Saved: scratchpad/period.txt.
- The controlling record is a VIRTUAL pointer in segment 3: buffer base 0x18002000; the codegen
  reads bytes at record+38 / record+39 (disasm `by comp2 r1.38,$7`, `h comp2 r1.39,#64/#128`).
  Address 0x18002026 (=0x18002000+38) is materialized as I1 at instr 3817. The "r1.NN" operands
  are INDEXED (W1 + NN) where W1 was loaded from local b.24 / b.60 (a pointer), NOT via the R reg.
- METHODOLOGY BUG in diag_codegen_loop: nd500_bus_read32 reads PHYSICAL memory; 0x18002000 (seg 3)
  and B-relative locals are VIRTUAL. So the "buf@18002000 all zeros / R=0 / [0x0801067C]=0"
  readings are UNRELIABLE (untranslated). Must translate via nd500_mmu_translate(cpu,vaddr,0,0)
  then bus_read. FIX THE PROBE before drawing conclusions about the record contents.
NEXT (concrete): in diag_codegen_loop, MMU-translate and dump: (a) local b.24 = [B+24] -> record
pointer, (b) record+0..+48 (esp +38/+39), (c) 0x18002000 region, across >=4 iterations. Determine
whether the record's +38/+39 type bytes and the loop's advance counter actually change. If they are
constant, find the store that should advance them and why it does not persist (wrong EA / wrong
segment / MMU write path). This is the live edge of the codegen-runaway root-cause.

## UPDATE 63d (2026-07-14): *** REFRAME - the loop DOES progress (counter is in memory) ***
Fixed diag_codegen_loop to read via nd500_mmu_translate. The seg-3 buffer at 0x18002000 CHANGES
every iteration - the loop is NOT state-invariant; only the sampled CPU REGISTERS return to the
same values each 124-instr period because the loop counter lives in MEMORY, not registers.
The buffer is a STRING/RECORD BUILDER:
  buf+0x00 = 0x18002039  (self-pointer to char data at buf+0x39)
  buf+0x04 = "CMD_FILE" (434D445F 46494C45)
  buf+0x1C = length/count: 0x00000118 -> 0x0218 -> 0x0318 -> 0x0418 ... (+0x100 each iteration)
  buf+0x38 = 0xF0F0F0F0 GETB heap poison (append is marching toward/into it)
  buf+0x39 = an ASCII digit that increments '1','2','3','4','5','6',... each iteration
So NC is in an UNBOUNDED string-append / emit loop: it appends one char per ~124 instrs and never
stops (322k+ iterations by 40M instrs), heading into 0xF0F0 poison. This is the SAME failure
family as the original 0x08023EA4 crash (over-long list/string walk into poison), now reproduced
on the clean-parse codegen path.
Loop termination test is the count at ~buf+0x1C compared via `w comp2 r1.27,W2 ; if<<go`
(0x0802BA89) [unsigned-less, tests C=0]. The append count/bound is wrong (too large or a
sign/width issue makes the compare never terminate).
NEXT: single-step ONE iteration around 0x0802BA89; capture the two comp2 operands (the running
count vs the target bound) and where the target comes from (an upstream computed value). Determine
whether (a) the bound is computed wrong upstream (mirror of the 770-count bug), or (b) our comp2/
if<<go mis-evaluates for these specific operands (width/sign). That decides MON/CPU vs upstream.
Probe: build/bin/diag_codegen_loop (now MMU-aware). Loop instrs 3763..3887 = one period.

## UPDATE 63e (2026-07-14): CORRECTIONS - probe perturbs; real runaway is LATER (onset ~1-2M)
Two important corrections to U63/63b/63c/63d:
1. PROBE ARTIFACT: diag_codegen_loop's MMU-aware reads used nd500_mmu_translate, which RAISES
   TRAPS as a side effect (nd500_mmu_translate_domain calls trap_page_fault/trap_protect_violation
   on failed/blocked translations). This spuriously page-faulted NC (the "fault at data=0x0802BACC
   / handler 0x0802D831" was NOT real NC behavior). The untouched diag_nc_checkpoints runs clean
   past 200k (PC 0x0800DE0D, stop=none). RULE CONFIRMED (assume nothing): verify probes don't
   perturb. FIX NEEDED: a side-effect-free virtual->physical peek (translate without raising
   traps) before trusting any memory dump; nd500_dbg_mem_read_raw is PHYSICAL-only (no translate).
2. NC MAKES REAL PROGRESS then commits to the terminal loop LATER. Region-vs-instr over the 40M
   trace: instr 0=0x08000 (startup), ~1M=0x0800Dxxx, then from ~2M through 40M it is STUCK in
   0x0802Bxxx/0x0802Cxxx (38M instrs). Onset of the terminal runaway is between 1M and 2M.
   -> The 0x0802BE21 "loop" I dissected at instr 3763 (U63b-d, "CMD_FILE" string builder) was a
   TRANSIENT EARLY phase (NC building its command/scratch record), NOT the terminal runaway.
   Real progress seen even in the perturbed run: buffer 0x18002000 evolved CMD_FILE-record ->
   codegen words (00026D33...) -> a "dummy" symbol record at 0x18004000. So codegen advances.
NEXT (non-perturbing): analyze the TERMINAL loop at instr ~10,000,000 using diag_nc_pctrace
(register-only, no memory reads = no perturbation): get its exact period + body + controlling
branch. Separately, find the 1M->2M transition (what NC does just before it locks up) - that is
where the real bug bites. Only after that, add a trap-free peek-translate to inspect the terminal
loop's memory operands.

## UPDATE 63f (2026-07-14): TERMINAL loop fully mapped (period 92, at instr 10M)
Non-perturbing register-only pctrace (scratchpad/term.txt) at instr 10,000,000:
- Terminal loop = EXACT 92 instrs, B=0x10000174, state byte-identical each period (I1=0,I2=8 at
  head 0x0802B9CA). Confirmed running unchanged 2M..40M (~87,000 iterations for a 1-statement
  program = clear runaway). This IS the loop from the very first 5M sample (the earlier
  instr-3763/period-124 analysis was the transient startup phase, discard it).
- Structure: entered via routine 0x0802B9C7 `ents #68`; nest of small subroutines
  (0x0802B900, 0x0802BA66-BAFA, 0x0802BD61-6D, 0x0802BD79-92, 0x0802BE06-39). The subroutine
  tail 0x0802BE21..BE39 ends `ret`. I1 cycles small constants that look like character codes
  (0x42='B',0x6D='m',0x64='d',0x33='3',0x40,0x0F,0x0B) -> still a string/emit builder.
- DECISIVE branch: 0x0802BA89 `w comp2 r1.27,W2 ; if<<go`(0x0802BA8D, unsigned-less, tests C=0)
  TAKEN every iteration -> loops. r1 base = W1 = 0x18000000 (seg-3 buffer, note now 0x18000000
  not 0x18002000), so it reads [0x18000000+27] vs W2. Secondary type tests earlier:
  0x0802B948 `by comp2 r1.38,$7`, 0x0802B951 `h comp2 r1.39,#64`, 0x0802B95C `h comp2 r1.39,#128`.
- Register state invariant => the real loop variable is in MEMORY (seg-3 buffer at 0x18000000).
NEXT: add a TRAP-FREE peek translate (new read-only fn nd500_mmu_peek(cpu,va)->phys, never raises
traps; does NOT alter existing behavior) so a harness can dump [0x18000000+0x18..+0x30] and the
comp2 operands across terminal iterations WITHOUT perturbing. Then: does the buffer's count/type
field actually advance? If yes -> unbounded-emit (wrong bound upstream); if the count is stuck ->
a store that doesn't persist. Either way, capture the two comp2 operands at 0x0802BA89 to see
count-vs-bound. This is the crux for the codegen runaway root cause.

## UPDATE 63g (2026-07-14): *** UPDATES 63-63f SUPERSEDED - the "codegen runaway" was a HARNESS ARTIFACT ***
Reconciled with memory nc-codegen-crash.md UPDATE 63/64 (parallel session 48520b34). VERIFIED
independently here: driving NC with a TERMINATING command sequence exits CLEAN via MON 0B:
  "CHECK B,B,B;;GENERATE-CODE B,BOUT;;EXIT;;"  -> MON halt (Program exit) at instr 1,902,381,
   creates GUEST/B.LIST (114 bytes) + GUEST/BOUT.NRF (0 bytes).
So the 0x0802B000-0x0802E000 "92-instr infinite loop" I dissected in U63-63f is NC's INTERACTIVE
COMMAND-INTERPRETER polling the console (MON 503B DVINST) for the NEXT command, which my
"COMPILE A,A,A\r" harness never supplied (no EXIT). NOT a codegen bug. The "INPUT record counter
to 0x400 then reset" is the command-line input-buffer poll. My U63-63f analysis is VOID (correct
observations, wrong frame). Retained only for the nd500_mmu_peek helper (kept - genuinely useful,
trap-free diagnostic translate added to src/cpu/nd500_mmu.c/.h this session).
REAL BLOCKER (adopt from memory U64): BOUT.NRF = 0 bytes. NC's object image is its work-file
SCRATCH-00001:NRF; our emulator auto-opens (SCRATCH)SCRATCH64 as file 0x40 and NC's object WFILEs
land in ./SCRATCH/SCRATCH64.DATA (deleted at exit); 41B ROBJE fabricates 'SCRATCH64.DAT' for 0x40.
The SCRATCH-00001:NRF (object) -> BOUT:NRF transfer is UNIDENTIFIED. NEXT: MON-log the
GENERATE-CODE drive; find which OPEN maps the object scratch, the file number NC WFILEs object
bytes to, and the copy/rename step to BOUT (0x41). Try mon_config auto_scratch_64 OFF.

## UPDATE 65 (2026-07-14): OBJECT-OUTPUT path MON-logged (real blocker, adopting memory U63/64)
Drive: "CHECK B,B,B;;GENERATE-CODE B,BOUT;;EXIT;;" (diag_monlog), clean exit at 1,902,381.
FILE-NUMBER MAP (NC uses OCTAL): file 100(oct)=0x40 = our auto-opened (SCRATCH)SCRATCH64.DATA;
file 101(oct)=0x41 = every explicit OPEN (B:CAT, B:C, B:LIST, BOUT:NRF all reuse internal slot 65).
OBSERVED:
- The OBJECT image (>=16KB, 8+ 2KB blocks via 120B WFILE) is written ENTIRELY to FILE 100
  (SCRATCH64.DATA). NOT ONE WFILE goes to BOUT.
- BOUT:NRF is OPENed 4x but WRITTEN 0x. One open sequence: OPEN(write)->SETBS->SMAX->CLOSE, no data.
- No RFILE-from-100, no copy, no rename. Run ends with 3x MON 54B MDLFI (delete) whose FileName
  reads EMPTY (the U64 wrong-string-reader bug: mon_read_sintran_string reads the descriptor
  length word instead of the pointed-to chars) -> all ERROR, then MON 0B LEAVE.
=> The object (in SCRATCH64.DATA=file 100) is never transferred to BOUT.NRF. Transfer mechanism
   still UNIDENTIFIED (matches memory U64). The empty-name MDLFI calls at exit are prime suspects
   (misread filenames -> we skip NC's intended scratch cleanup/rename).
CONCRETE BUG FOUND (real, but NOT sufficient alone): mon_73B_SetMaxBytes.c line 82
  `uint32_t new_size = max_byte_ptr + 1;`  NC calls SMAX on BOUT with
  MaxBytePointer=37777777777(oct)=0xFFFFFFFF (SINTRAN "unlimited/no-max" sentinel). 0xFFFFFFFF+1
  overflows uint32 to 0 -> we record max=0 -> CLOSE truncates BOUT.NRF to 0 bytes. FIX: treat
  max_byte_ptr==0xFFFFFFFF as "no maximum" (do not set max_bytes_set / do not truncate). (Even
  fixed, BOUT stays 0 until the object actually gets written to it - see transfer above.)
NEXT: (1) fix the 54B MDLFI / 317B UECOM STRING reader (use mon_read_descriptor_string like 50B
  OPEN) to reveal the real end-of-run filenames -> identify the scratch->BOUT step; (2) fix the
  SMAX 0xFFFFFFFF overflow; (3) test mon_config auto_scratch_64 OFF (does NC's object file then
  map to BOUT?). auto-scratch opens (SCRATCH)SCRATCH64 at mon_dispatch.c:67-68.

## UPDATE 66 (2026-07-14): *** REAL CPU BUG FIXED - SCOPA was the wrong instruction ***
src/cpu/instructions/STRING/Scopa.c implemented "string copy all with translation table"
(loading a descriptor from the 3rd operand's ADDRESS). The ND-500 Reference Manual ND-05.009.4 EN
sect 14.12 p256 (opcode 0xFDBE = 176676B) defines SCOPA = "string COMPARE with PAD": 3rd operand
is a PAD BYTE VALUE, not a table. The ND LINKER startup does `by scopa b.x,b.y,$0`; the old code
dereferenced the constant pad `$0` as address 0 -> protection violation at data=0 (linker instr
9432). REWROTE Scopa.c as SCOMP + padding (shorter string padded with pad byte; K/Z/S per manual
p256; S=1 <=> source1<source2, matching SCOMP/COMP; C,O cleared). VERIFIED: linker now runs PAST
the PV (9432 -> 11703); zero SCOPA test cases exist so no regression; full suite still 39792/11
(same known SCOMP baseline). C# RetroCore has the SAME wrong SCOPA (same author) - MUST fix there
too (mirror the manual: compare-with-pad, 3rd operand = pad byte value).
Linker NEXT blocker: unimplemented MON 313B (IBRISZ), 2 args, at instr 11703 / PC 0xB004DA96.

## UPDATE 67 (2026-07-14): *** ND LINKER NOW BOOTS + EXITS CLEAN *** (2nd STRING bug + 2 handlers)
Using the user-provided linker docs (/mnt/d/ND/500/nd-linker/: linker-b01.help, .analysis.md,
nd500-c-compile-and-link.md), drove the REAL linker (linker-b01.dom) via test/diag_linkmon.
Fixes this session that got it from "faults at instr 5419" to a CLEAN MON 0B exit at instr 58264:
  1. 144B MAGTP: provisional benign-success stub + registry status IN_PROGRESS (unblock startup).
  2. SCOPA (Scopa.c): was "copy-all w/ translation table"; MANUAL says compare-with-pad (3rd
     operand = pad byte value). Fixed -> cleared the null-descriptor PV at instr 9432. [UPDATE 66]
  3. 313B IBRISZ (InBufferState): implemented from carve - returns queued console input length as
     NoInBuffer so the linker's input poll proceeds. Registry status IN_PROGRESS; MON_ID_313B added.
  4. SSPAR (Sspar.c): was "string span reverse" (loaded operand2 as a SET descriptor -> PV at
     instr 16719); MANUAL (opcode 176664B, sect 14.20) says "String Set PARity": operand2 = mode
     VALUE 0-3 (0 clear/1 set/2 even/3 odd), sets bit7 of each byte, K=1. Rewrote correctly.
RESULT: linker runs full startup and exits cleanly (MON 0B LEAVE, instr 58264) driven by just
"EXIT". No PV, no unimplemented MON. Regression: +1 test fail (Sspar_BY_ReverseSpan) = BOGUS,
encodes the old wrong instruction (same-author C# has the same SSPAR bug) -> regenerate/remove in
Phase 5, alongside the 11 SCOMP ByteDiff. Suite now 39791/12 (all 12 fails are stale-baseline).
C# SYNC (both real CPU bugs, same author): fix SCOPA (compare-with-pad, 3rd op=pad byte) and SSPAR
(set-parity, 2nd op=mode 0-3) in RetroCore; regenerate their test cases.
NEXT: drive a real LINK - "OPEN-DOMAIN ""T"";;LOAD T;;CLOSE N,N;;EXIT;;" with a known-good NRF
(test-real.nrf) staged as T:NRF; command grammar from nd500-c-compile-and-link.md sect 4/7.
