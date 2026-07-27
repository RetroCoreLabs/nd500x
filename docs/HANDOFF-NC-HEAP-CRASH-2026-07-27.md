# Handoff: NC-A06 post-compile heap-exhaustion crash (cosmetic)

**Date:** 2026-07-27 (REVISED same day - root cause corrected, see section 2b)
**Status:** ON HOLD. No open code changes. The original "verified root cause"
(section 2) was DISPROVEN by a trace re-analysis later the same day; read
section 2b before acting on anything in section 2 or the old plan in section 4.
**Repo:** `~/repos/nd500x` (WSL), branch `fix/deabf-i1-success-and-load-investigation`.
**Submodule:** `external/ndmonlib` on branch `ndix-mon600`.

---

## 1. The big picture (what already works)

The real Norsk Data NC C compiler + LINKER run end-to-end inside the `nd500x`
emulator's SINTRAN shell (`--monitor` mode):

```
HELLO.C  --NC-A06-->  HELLO.NRF  --LINKER-B01-->  HELLO.DOM  --@HELLO-->  "Hello world"
```

This is committed and pushed (main `4bde3b1`, submodule `b6fe237`). The key
enabler was implementing MON 317B UECOM (nested command execution). Full history:
`docs/NC_CRASH_0x08023EA4_ROOTCAUSE.md` UPDATES 70-77.

## 2. The ONE remaining item = this handoff

After a successful C **compile**, `NC-A06` (a vendor `.DOM`) exits with:

```
NC: [TRAP] No trap handler at THA[27] (THA=0x0802357C, ptr=0x080235E8)
[STOP] stack overflow at PC=0x0801E100 data=0x00000000
```

It is **cosmetic**: the `.NRF` is written *before* the trap and the process exits,
so compile/link/run all work. The task is to make NC exit cleanly instead.

### Original root-cause claim (DISPROVEN - kept for the record, see 2b)
- `NC-A06` installs trap handlers only for traps 32-41, **NOT trap 27 (STO)**.
- On a heavy `GETB` (buddy-heap allocate) it finds an **unestablished heap**
  (`STAH==ENDH==0`, all freelists 0, zero `FREEB` in the whole run) and raises
  `TRAP_STO` (bit 27) so a handler can grow the heap. With no trap-27 handler the
  STO is fatal.
- A vendor DOM expects the **real SINTRAN ND-500 monitor** to establish its heap
  at domain start. `nd500x`'s DOM loader does not do this - that is the gap.
- Contrast: our linked `HELLO.DOM` hits the *identical* empty-heap STO but
  **survives** - it was linked with the C runtime (USLIB3/NC-LIB/CAT-LIB) whose
  trap-27 handler seeds/grows the heap and retries.

## 2b. CORRECTED root cause (2026-07-27 re-analysis, from full HEAPDBG traces)

Re-ran `MODE COMPILE-HELLO` with `ND500X_HEAPDBG=1` (and once more with
`ND500X_MONLOG=1` to interleave UECOM nesting markers). The compile was HEALTHY
at re-analysis time (fresh 933-byte `HELLO.NRF`), so these traces are trustworthy.

### What the traces prove (contradicting section 2)

1. **The vendor runtime establishes its heap LAZILY, ITSELF.** At startup of
   NC-A06 AND of every nested instance (nest depths 1-3), the first GETB
   (PC=0x0802CF60, TOS=0x0801D538) finds an empty heap, raises STO, and a
   recovery path runs: it calls **MON 422B GSWSP** (from PC=0x0802CD75) to get a
   fresh segment (VA = seg<<27, e.g. 0x18000000), sets STAH/ENDH, seeds the
   freelists, and the retried GETB succeeds. This worked 5 separate times in one
   compile run. So "no pre-established heap" is NOT the failure - it is the
   designed, working steady state. This matches the ND-500 Reference Manual
   p.35: "The heap variables must be initialized by the user program" (see
   `/home/ronny/repos/nd500x/docs/ANALYSIS_CHAPTER_3_HEAP.md` - note that doc's
   own "SINTRAN should do it" reading is a misreading of that quote).
2. **The fatal event is a DOUBLE STO, not a missing handler per se.** Clean
   stderr ordering (HEAPDBG-only run) at NC's EXIT phase:
   - GETB at PC=0x0801E100 with TOS=0x08022B8C reads a heap-vars block that is
     ENTIRELY ZERO - **including MAXL=0**. That same block held MAXL=23,
     STAH=0x18000000, ENDH=0x1801FFFF earlier in the SAME run (established
     during codegen). MAXL=0 means TOS points at zeroed memory, not at any
     DOM-initialized heap-vars block.
   - The STO recovery is entered, and ITS allocation (same PC=0x0802CF60 path,
     TOS=0x0801D538) ALSO finds all-zero heap vars, raising a second STO while
     already inside the recovery context, whose THA (0x0802357C) has no entry
     27 - plausibly BY DESIGN to prevent handler recursion. That second STO is
     the fatal "No trap handler at THA[27]".
   - Near-identical NON-fatal precedent in the same log: the same max_log=0
     zero-vars STO occurred earlier in the run and was SURVIVED. Fatality
     requires BOTH heap-vars blocks to be zero simultaneously.

### The real open question

Why does TOS point at all-zero memory at NC's exit, when that block was
populated earlier in the same run? Two live suspects, both emulator-side:

- **Stale/wrong TOS or context state across the UECOM nested-run
  snapshot/restore.** The zero block sightings bracket nested UECOM calls, and
  `shell_execute_command()` (full-RAM + CPU + segment-allocator snapshot/restore
  in `/home/ronny/repos/nd500x/src/frontend/nd500x/nd500x_shell.c`) is exactly
  the machinery operating at those boundaries. A wrong TOS after restore reads
  zeros from an innocent address and produces precisely the MAXL=0 signature.
- **Trap-enable (OTE) state divergence.** The benign zero-vars STOs recover
  inline while the fatal one dispatches through THA; if the enable state of
  trap 27 is mishandled across context switches (ENTT/RETT/LCNTXT), the same
  program state flips from recoverable to fatal.

A third possibility (vendor code voluntarily zeroes its heap vars at teardown
and lazily re-establishes) would make the zeroing NORMAL - in that case only
the double-trap dispatch state is wrong. Not yet discriminated.

## 2c. EXPERIMENT RESULT (2026-07-27, later same day): ROOT CAUSE PROVEN

The discriminating experiment below was RUN (instrumentation now in the tree,
env-gated `ND500X_STODBG`: STO-raise logging in `trap_stack_overflow()` in
`/home/ronny/repos/nd500x/src/cpu/cpu.c`, UECOM boundary + fixed-address probes
in `shell_execute_command()` in
`/home/ronny/repos/nd500x/src/frontend/nd500x/nd500x_shell.c`). Verdict:
**hypothesis 1 (UECOM restore) - but it is the MMU TRANSLATION that is lost,
not the RAM contents.**

Decisive probe (same VA, same domain CED=3 = CAT-CAT5-B, across its nested
`NC-A` UECOM call):

```
pre-snapshot : va 0x08022B8C -> pa 0x00042B8C val 0x00000017  (MAXL, valid)
               va 0x080235E8 -> pa 0x000435E8 val 0x0801E851  (THA[27], valid)
post-restore : va 0x08022B8C -> pa 0x00052B8C val 0x00000000  (WRONG PAGE)
               va 0x080235E8 -> pa 0x000535E8 val 0x00000000  (WRONG PAGE)
```

The caller's memory is intact and the RAM restore works; the caller domain's
virtual-to-physical translation CHANGED across the nested run (0x0004xxxx ->
0x0005xxxx, which is where OTHER domains map that VA). Every "reads as zero"
symptom (heap vars MAXL=0, THA[27]=0, LL=HL=0) is this one defect: reads going
through the wrong mapping. Also corrected attributions: the fatal victim is
**CAT-CAT5-B (CED=3, nest depth 2)** immediately after its nested NC-A child
returns, and it DID have a valid trap-27 handler installed before the child ran.

Mechanism: the DOM loader
(`/home/ronny/repos/nd500x/src/ndlib/ndlib_dom_loader.c`, calls to
`nd500_mmu_set_pst_entry` / `nd500_mmu_set_data_capability` /
`nd500_mmu_set_program_capability`) writes the C-side MMU tables `g_pst`
(GLOBAL physical-segment table, shared across domains) and `g_pcb_table`
(per-domain capabilities) in
`/home/ronny/repos/nd500x/src/cpu/nd500_mmu.c` (statics at the top). A nested
UECOM load overwrites PST entries the caller's domain still references. The
UECOM snapshot covers CPU struct + physical RAM + segment-allocator state -
**not these MMU tables**. Same bug class as the segment-allocator table (which
is why that snapshot already exists); this is the remaining unsnapshotted
layer.

**FIX IMPLEMENTED AND VERIFIED (2026-07-27):** added
`nd500_mmu_state_save()`/`_restore()` (snapshot of `g_pst` + `g_pcb_table`,
mirroring `nd500_segment_alloc_state_save`/`_restore`) at the end of
`/home/ronny/repos/nd500x/src/cpu/nd500_mmu.c` (+ prototypes in
`/home/ronny/repos/nd500x/src/cpu/nd500_mmu.h`), wired into
`shell_execute_command()` in
`/home/ronny/repos/nd500x/src/frontend/nd500x/nd500x_shell.c` alongside the
existing RAM/CPU/segment-allocator snapshots (save + both restore paths).

Verification results:
- Probe now shows CED=3's translation PRESERVED across the nested run
  (va 0x08022B8C -> pa 0x00042B8C val 0x17 both before AND after).
- Zero occurrences of "No trap handler at THA[27]" / "[STOP] stack overflow"
  in a full MODE COMPILE-HELLO run. CAT-CAT5-B now prints its proper vendor
  epilogue ("programCAT_COMPILER terminated / execution time 0:00:01") which
  never appeared before.
- The predicted PC=0 follow-on gap (section 3 finding 3) did NOT materialize -
  it was another artifact of the broken translation, not a separate bug.
- `HELLO.NRF` stays 933 bytes; `MODE LINK-HELLO` -> 6,313,424-byte `HELLO.DOM`;
  `@HELLO` prints "Hello world" + clean exit (16,579 instrs).
- `ctest`: `stack_overflow` green; 20/23 pass. The 3 failures
  (`ote_instructions`, `mon_calls`, `instruction_validation` - one PUTBF
  S-flag case) are the concurrent teams' pre-existing/unrelated breakage
  (this fix touches no instruction semantics: two memcpy-snapshot functions
  plus env-gated `ND500X_STODBG` logging).

### Discriminating experiment (RUN - see 2c above for the result)

Log TOS, THA, and the trap-enable mask at every STO raise (in
`trap_stack_overflow`) and at every UECOM snapshot/restore boundary (in
`shell_execute_command`). One compile run then shows directly whether
TOS=0x08022B8C's contents were populated at snapshot time and zero after
restore (restore bug), whether TOS itself changed to a bogus value
(context-restore bug), or whether the vendor code zeroed them voluntarily
(dispatch-state bug only).

### Consequence for the old plan (section 4)

Section 4's option (A) "seed the heap FREELISTS at DOM load for handler-less
DOMs" is built on the disproven premise. The DOMs are NOT handler-less (their
lazy-establish STO path works), and load-time seeding would only mask the first
STO - the MAXL=0 anomaly means seeded-at-load vars would ALSO read zero at the
fatal point if the memory/TOS is wrong. What REMAINS valid from the earlier
work: GETB must stay pure (section 3 / manual section 3.3 - that lesson
stands), and the PC=0 follow-on gap (section 3 finding 3) is still real.

Trace files from the re-analysis (session scratchpad, may be gone later; the
repro commands in section 6 regenerate them):
`/tmp/claude-1000/-home-ronny-repos-nd500x/c79ab8e4-280a-4a44-a1dc-de912b970903/scratchpad/heapdbg_mode.log`
(HEAPDBG only, clean stderr ordering) and `.../heapdbg_mon.log` (HEAPDBG +
MONLOG=1, UECOM markers; stdout/stderr interleaving NOT time-accurate).

## 3. What I tried (2026-07-27) and why it is the WRONG layer - DO NOT REPEAT

I built a gated fallback INSIDE `nd500_heap_alloc_block()` (GETB) in
`src/cpu/instruction_helpers.c`: for a handler-less DOM (`THA[27]==0`) with an
unestablished heap, allocate a 2 MB MMU-backed segment via
`nd500_mon_allocate_segment()`, set `MAXL/STAH/ENDH`, and bump-allocate
(break-pointer carve) from `STAH..ENDH`.

**It removed the STO crash and the gate correctly protected the codegen program**
(CAT-CAT5-B, which HAS its own handler `THA[27]=0x0802CF71`). BUT it is wrong:

1. **It violates the ND-500 architecture.** `test/test_stack_overflow` Test 2
   asserts *"GETB exhaustion sets STO, no allocation ... freelist NOT re-seeded
   from STAH/ENDH (section 3.3)"*. GETB carving from STAH/ENDH is exactly what
   section 3.3 reserves for the TRAP HANDLER. My carve broke that test; reverting
   restored it. **GETB must stay pure.**
2. The correct discriminator is **`THA[27]==0` (handler-less)**, NOT UECOM nest
   depth. The crashing GETB runs at nest depth 2, so an earlier "depth==0" idea
   fails. `THA[27]==0` is also what keeps the fallback off the codegen program
   (carving from a handler-owned STAH..ENDH is the corruption that regressed NRF
   933->0 in a prior attempt - see UPDATE 76c).
3. **A heap alone does not give a clean exit.** With STO papered over, NC then
   jumps to **PC=0** (`Invalid instruction 0x00 at PC=0x00000000`). More missing
   monitor/domain-teardown semantics lie downstream. Expect this as the NEXT
   blocker even after the heap is correct.

I reverted both files; `stack_overflow` passes again.

## 4. The plan (SUPERSEDED by section 2b - kept for reference only)

**Do not implement this section.** It was written against the disproven root
cause. Run section 2b's discriminating experiment first; the fix follows from
its outcome.

Keep GETB pure. Establish the heap the way the monitor/handler would, in one of:

- **(A, preferred) Seed the heap FREELISTS at DOM load** for a handler-less DOM.
  Allocate a heap segment and `FREEB`-style prepend blocks onto `FLOG[k]` so a
  later `GETB` merely UNLINKS a freelist entry (pure, test-safe). Set `MAXL`.
  Do it once, before execution, for the loaded domain - not per-GETB.
- **(B) Install a synthetic trap-27 handler** for handler-less DOMs that performs
  the monitor's heap-grow. Architecturally the "real" actor, but heavier.

Then address blocker #3 (the PC=0 jump).

Heap-vars layout (at register `TOS`, byte addresses; 1 word = 4 bytes):
`MAXL@+0, STAH@+4, ENDH@+8, FLOG[k]@+12+k*4`. See
`src/cpu/instructions/SYSTEM/Freeb.c` and `nd500_heap_alloc_block()` in
`src/cpu/instruction_helpers.c`. Segment N -> VA `N<<27`
(`nd500_segment_alloc.c:557`). `nd500_mon_allocate_segment(cpu, machine, 0xFF, 0,
size, &assigned)` zeroes pages and demand-backs them.

## 5. CRITICAL environment caveats (read before trusting any result)

- **The shared branch is worked on by MULTIPLE concurrent LLM teams.** At one
  point on 2026-07-27 their *uncommitted* work had **regressed the compile
  itself** (`HELLO.NRF` 933 -> 0 bytes) and left `ote_instructions` +
  `mon_calls` unit tests failing. Later the same day (section 2b re-analysis)
  the compile was healthy again (fresh 933-byte NRF). The state CHANGES UNDER
  YOU: always confirm a real 933-byte `HELLO.NRF` yourself, immediately
  before/after your change, and do not trust any earlier byte count.
- **Committed HEAD does NOT build standalone**: `instruction_helpers.c` calls
  `nd500_ptewatch_wr`, defined only in a teammate's uncommitted `src/machine/io.c`.
  A `git worktree` at HEAD will fail to link. You must build in the live working
  tree (with teammates' uncommitted files present).
- **Only touch your own files.** `instruction_helpers.c` currently carries other
  teams' `IPCURDBG`/`PRIVDBG` instrumentation - leave it. Never commit their work.
- Windows/WSL: the user runs Claude on Windows; the emulator is in WSL. Use `wsl
  bash -lc '...'`. The Bash tool is Git Bash, not WSL.
- Commit rules: never mention Claude/AI in commit messages; no `--no-verify`.

## 6. How to build, run, and diagnose

```bash
# build
cd ~/repos/nd500x && cmake --build build -j4

# unit tests (watch stack_overflow especially)
ctest --test-dir build -j4
ctest --test-dir build -R stack_overflow --output-on-failure

# full compile + link + run (the acceptance harness)
bash ~/hello_build.sh          # prints NRF/DOM sizes + run output

# compile only, with heap trace to stderr
cd ~/ND500USERS
printf 'LOGIN GUEST\nCREATE-FILE HELLO:CAT\nCREATE-FILE HELLO:LIST\nCREATE-FILE HELLO:NRF\nNC-A06\nCHECK HELLO,HELLO,HELLO\nGENERATE-CODE HELLO,HELLO\nEXIT\n' \
  | ND500X_HEAPDBG=1 timeout 90 ~/repos/nd500x/build/bin/nd500x --monitor --config ~/ND500USERS/nd500x.ini
```

Diagnostic env vars (env-gated, committed): `ND500X_HEAPDBG` (GETB/FREEB heap
trace: exhaustion, STAH/ENDH, freelists), `ND500X_MONLOG=1..4` (MON-call trace),
`ND500X_CARVE_BOUT` (frame chain at object WFILE).

## 7. Key files

| File | Role |
|---|---|
| `src/cpu/instruction_helpers.c` `nd500_heap_alloc_block()` | GETB buddy allocator - the STO originates here. **Keep pure.** |
| `src/cpu/instructions/SYSTEM/Freeb.c` | FREEB (heap free / seed) - the mechanism a load-time seeder would mirror. |
| `src/cpu/nd500_segment_alloc.c` | `nd500_mon_allocate_segment()` (seg N -> VA N<<27), state save/restore. |
| `src/cpu/cpu.c` ~line 896 | `invoke_trap_handler()` - where "No trap handler at THA[27]" is printed; where a synthetic handler (option B) would hook. |
| `src/frontend/nd500x/nd500x_shell.c` | SINTRAN shell; `shell_execute_command()` = UECOM nested-run + RAM/segment snapshot (the machinery that made a per-GETB fallback unsafe). DOM load happens here and in `run_domain`. |
| `docs/NC_CRASH_0x08023EA4_ROOTCAUSE.md` | Full investigation log, UPDATES 70-77. Read UPDATE 76-77 first. |

## 8. First steps for the next LLM (revised per section 2b)

1. Read section 2b of THIS document, then
   `docs/NC_CRASH_0x08023EA4_ROOTCAUSE.md` UPDATES 76, 76a-c, 77 (historical
   context - their root-cause claims are superseded by 2b).
2. Confirm the tree is healthy: `ctest` green, `bash ~/hello_build.sh` shows
   `HELLO.NRF: 933 bytes`. If not, STOP - concurrent work is mid-flight; ask the
   user before proceeding (you cannot validate anything).
3. Run section 2b's discriminating experiment: log TOS/THA/trap-enable at STO
   raise and at UECOM snapshot/restore boundaries, one `MODE COMPILE-HELLO`
   run, and determine WHICH of the three hypotheses holds (restore bug /
   TOS-context bug / voluntary zeroing + dispatch-state bug).
4. Fix what the experiment implicates. Do NOT implement old section 4 option
   (A) load-time seeding - disproven premise. Keep GETB pure regardless
   (`stack_overflow` test must stay green) and `HELLO.NRF` must stay 933.
5. Then chase the PC=0 follow-on gap (section 3 finding 3) with
   `ND500X_HEAPDBG` + a CPU trace at PC=0x0801E100.
