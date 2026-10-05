# Handoff: the ND-5000 macro trap path - THA, ENTT retryability, abort flag

Date: 2026-10-05. Branch `fix-5000` in nd500x (local only, nothing pushed),
plus one commit on nd100x. Lane: MACRO only - plain `Nd5000 attach`, no
`--microword`, no `--accp`.

Read `docs/CURRENT-PLAN.md` first; this document is the narrative for the
four plan sections added on 2026-10-05.

## Goal

Get DOM programs running under real SINTRAN over the octobus - CPU-STAT and
NC. Not reached. The run now gets roughly twice as far as it did and dies in a
different place.

## What was fixed, with the evidence

### 1. THA was zero for a whole run (segment-12 "bounds" fault)

Not a bounds fault at all. `THA` (trap handler address) is a DIT-resident
field at offset 182, indexed by CED, and nothing in nd500x ever sourced it -
it stayed 0. The swapper's trap-handler install loop then wrote handler
addresses to `0 + trapno*4`; the `n=12` store landed on the process's own THA
copy at offset 0x30 and the next store went to 0x0800481A.

Oracle: the ND-5000 symbolic control-store listing `MICRO-5800-B30.LIST`
(location indexed in `docs/EXTERNAL-ARTIFACTS.md`) - `012210 LOAD_THA` and
`012233 STOR_THA` both route through `012035 CED_TO_DIT`, which builds a DIT
address from `SRF14` (CED). So THA is read from memory on every access, not
held in a register.

Fixes:
- `src/cpu/cpu.c`: `nd500_dit_read_tha`, `nd500_dit_read_ith`,
  `nd500_dit_write_ith`, `nd500_is_in_trap_handler`,
  `nd500_set_in_trap_handler`. The DIT is the single holder of ITH when
  configured; the C struct field is only the fallback.
- nd100x `src/machine/mfbus_bridge.c`: the THA read belongs in
  `mfbus_declare_capability_table`, NOT in `mfbus_load_context`. Placed in
  `mfbus_load_context` it runs before DITBASE is set and reads the PREVIOUS
  process's PCB - which shows up as crossed PS/DIT pairs in the log.

Verified after the fix: the install loop's counter at logical 0x34 climbs
9 -> 41 and stops, segment 13 demand-pages through all 63 pages, and the
handler for trap 38 is 0x08004924 = 0x080047C2 + 29 steps, 12 bytes apart,
each an `entt`.

### 2. ENTT was not retryable - 503 RETT refusals

`RETT ... Not in trap handler` fired 503 times in one run. Root cause: ENTT
set `B` before writing its 47-word frame. A page fault partway through the
frame restarted the instruction with `B` already changed, and the restarted
ENTT took its instruction-sequence-error arm.

Proof: the per-step trace shows `B=0x00000004` then `B=0x08001728` at the
same PC.

Fix, aligned with the C# (`Entt.c`): pre-validate the whole frame span
(180 + 40 bytes) with `nd500_mmu_peek_space` before any mutation; on a hole,
report the page fault against `cpu->trap_saved_PC` - the ORIGINAL trapping
instruction, not `fi->address`. `Emulated.Tests.ND500`'s
`EnttFaultRestartTests.cs` states that restart-at-ENTT "can never be the
design", because every process continue runs 011370B and the restarted ENTT
takes its ISE arm. `B`, `TOS` and `L` are set at the END, behind a terminal
abort guard.

Measured: refusals 503 -> 0, parks 70 -> 131.

### 3. The abort flag, where the C# unwinds instead

The C# throws `Nd500InstructionAbortException` and catches it in
`CpuND500.Execute.cs:716` with `UnwindOnAbortedAccess` default true, so it is
abort-safe by construction. nd500x has no unwind, so every access site needs
`if (nd500_trap_occurred() || cpu->instr_aborted) return;`.

The two emulators agree on the model: the flag is set in exactly one place
(C# `CpuND500.Trap.cs:940`, C `src/cpu/cpu.c:1578`), for non-ignorable traps
only, and cleared per step at the top (C# `Execute.cs:628`, C `cpu.c:252`).

Real defects found by the systematic CALL/ENT/RET comparison, not by
single-file reasoning:
- `instruction_helpers.c`: `nd500_load_string_descriptor` returned `true`
  unconditionally, making 27 STRING-class checks dead code.
- `CALL/Call.c:121` and `CALL/Callg.c:244`: out-of-bounds WRITES - the
  argument loops ran to `ND500_MAX_OPERANDS` (258) against a
  `pending_call_arg_addresses[256]` array. Rebounded to `TRAP_SEQ_MAXARG`.
- `CALL/Entm.c`: `TOS` committed between the guards; moved below the terminal
  guard beside `B`/`L`.
- `BRANCH/{Loop,Loopd,Loopi}.c`: a faulted index write-back could still
  commit the branch PC.
- `cpu_instr.c` `mmu_read8/16/32`: no abort check after the translate.

## The open failure - where the next session starts

With `952441d` the run ends in:

```
FATAL * 21B:77B * ND-500(0) Monitor Internal
       Fatal internal system error - ND-500(0) CPU locked
       Reason..............ND-5000 lock timeout
```

preceded by about sixty IDENTICAL parks:

```
parked on trap 46B at P=0x8004924 fault=0x8001800 psn=11
```

That is the ENTT frame write faulting, correctly refusing to half-apply, and
SINTRAN never satisfying the page-in - so it re-faults until the monitor gives
up. Distinct park PCs for the whole run: 0x8000004, 0x8000012, 0x800453B,
0x800467F, 0x8004751, 0x8004924.

Two things to measure rather than assume:
- the same fault was reported `psn=12` in earlier runs and `psn=11` now. The
  ENTT frame at `THA+256 = 0x08001728` spans into 0x08001800, so WHICH
  physical segment that lands in is itself a measurement.
- nd500x lacks the C#'s one-fault-per-instruction suppression
  (`CpuND500.MMU.cs:1646`), so one instruction can re-fault many times. That
  may be what the monitor is seeing.

## Recorded, NOT changed - each needs its own evidence first

- Flags committed after a faulted destination store. Systemic, eight classes.
- Raw translate-then-access sites outside `src/cpu/instructions/`:
  `nd500_fecall.c:868-881`, `nd500_mon_sintran.c:72/91/143/156/169/186`,
  `nd500_ndix_boot.c:95`, the trap-vector fetch at `cpu.c:1918`, and the
  debugger paths.
- `SYSTEM/Swap.c`'s non-retryable half-done swap. Shared gap; the C# relies on
  its unwind.
- The string corruption `(YSTSMS S S S S S)TERMINAL-1` - no candidate.
- `perform_block_copy` odd-trailing-byte parity defect.
- Segment 11's missing instruction-side pages; segment 14 never reached.

## Traps that cost time in THIS session - do not repeat them

1. **A pool snapshot only decodes correctly in its OWN run.** Physical
   placement is per-run: `pa = 0x74800 + (logical & 0xFFFFFF)` in one
   snapshot, `0x7F7800` in the next. A branch was reported as the divergence
   from a disassembly of the wrong run's snapshot, and retracted.
   `ND500X_VWATCH=<hex>[:<len>]` (added in `src/cpu/nd500_settings.c/.h`, reported by
   `nd500_ptewatch_wr` in `src/machine/io.c`) exists because of this and logs
   PC, vaddr AND paddr together.
2. **`MFBUS_PCDUMP` is a stop-time PC sampler, not an execution tracer.** It
   reporting nothing for an address is NOT evidence the address never
   executed.
3. **Watch the address the instruction computes, not the one in the source
   text.** `b.0x18` with `B=0x1C` is address 0x34.
4. **The trap sink is what RECORDS a raise.** Calling only
   `nd500_stop_on_trap` without offering to the sink first regressed the run
   from 319 parks to 68. Do both, in `raise_trap`'s order.
5. **The sink always returns 0 by design** - "declined" is not "absent".
6. The bridge's ITH carry overwrote the DIT with a stale copy
   (`dit_ITH=1` -> `0`); carry REMOVED, DIT is the holder. The
   `dispatch_pending[]` shadow was measured `:= 0` at every save and REVERTED.
   nd100x `src/machine/mfbus_bridge.c` is `48f06b4` plus the THA move only.

## Commits, in order

nd500x, branch `fix-5000`:
`777924c` VWATCH / `3fb4c6a` nd500_dit_read_tha / `0987e16` plan: segment-12
traced to THA / `5cad152` non-faulting handler probe + A-segment in copy log /
`6cf8b0b` plan: trap-path chain / `196c49a` plan: ENTT interlock /
`46d827f` plan: interlock carry ruled out / `952441d` CALL/ENT/RET family
aligned with the C# / `53d9008` abort flag honoured where the reference
unwinds.

nd100x: `440ca49` THA load / `48f06b4` THA read placement.

Nothing pushed. Both branches stay local.

## How to reproduce a run

The pty-driven SINTRAN sessions used here were written to the session
scratchpad, not to the repo (they would carry root-anchored paths). The
sequence each one drives is: ESC / SYSTEM / ND-500 / DEFINE-SWAP-FILE /
PLACE-DOMAIN CPU-STAT / RUN / LIST-ACTIVE-PROCESSES, capturing stderr and the
console separately. `docs/INVESTIGATION-TRAPS.md` has the three-command
bring-up for this lane.
