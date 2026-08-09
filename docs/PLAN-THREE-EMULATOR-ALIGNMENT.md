# Plan: get all three ND-500 CPUs agreeing, with corpus tests that prove it

**Written 2026-08-09.** Concrete, ordered, with an owner per step. The worked
example is SOLO/TUTTI/DT/DE because that is where the disagreement is live, but
phase 5 turns the mechanism into something reusable for the next instruction.

## The three CPUs

| # | CPU | Where | Role |
|---|---|---|---|
| 1 | nd500x | this repo, `src/cpu/` | C functional emulator |
| 2 | `CpuND500` | `RetroCore/Emulated.HW/ND/CPU/ND500/` | C# functional emulator |
| 3 | `CpuND5000` | `RetroCore/Nuget/HackerCorpLabs.Emulation.CPU.ND5000/` | executes `MICRO-5800-B30.DATA`. **When it runs, it is the machine** |

Ownership matters for scheduling: **1 is ours, 2 and 3 and the corpus
generators are the RetroCore session's tree on a shared branch.** Anything
marked RETROCORE below cannot be done from here.

## Definition of done - falsifiable, not a feeling

1. `CpuND5000` executes `FE 00` and `FE 01` without throwing.
2. For the four SOLO/TUTTI seed cases, CPUs 1, 2 and 3 produce the same PSD,
   the same trap set, and the same PC.
3. The corpus contains 18 positive + 8 negative SOLO/TUTTI cases, all passing on
   both runners.
4. `conformance_quarantine[]` in `test/test_conformance.c` is EMPTY.
5. `test_solo_traps` still 19/19 and ctest still green.

If any of 1-5 is not true, the answer to "are they aligned" is no.

## Phase 0 - unblock the oracle (RETROCORE, blocks everything)

Nothing downstream can be settled by evidence until this is done.

**0.1 Model `SPEC,MOD` in the microcode engine.**
`Nuget/HackerCorpLabs.Emulation.CPU.ND5000/src/OperandRouter.cs` currently
throws on the whole SPEC group by policy (line 25: "Unimplemented selects (MMS,
SPEC, IDU, AAP results) throw ... so real gaps surface"). Add a plain 32-bit
`Registers.SpecMod` with an A-side read (select 157 octal / `A,SPEC,MOD`) and a
D-side write (select 300 octal / `D,SPEC,MOD`). Storage and readback only - the
sole bit SOLO/TUTTI touch is BM25 = bit 21.
*Verify:* SOLO no longer throws at 004526 and TUTTI no longer throws at 004534.

**0.2 Expose `Psd` in the oracle state.**
`tests/MacroOracleState.cs` has `Zro Sgn Cry Ovfl Pia` but no `Psd`. Add it as
ST1 bit 4, seeded into both engines and compared on the way out, exactly as
`Pia` is done. Optionally expose `SpecMod` bit 21 too - it is the arm bit and
ST1 cannot see it.
*Verify:* a `MacroInstructionOracle.RunBoth` case on `FE 00` reports a PSD
difference when the two engines disagree.

## Phase 1 - let the hardware answer (RETROCORE)

Run these four through `RunBoth`. The one-instruction boundary is fine: seeding
`Psd=1` IS "a region is already open", so no two-SOLO sequence is needed.

| Seed | Code | Question |
|---|---|---|
| `Psd=0, Pia=0` | `FE 00` | SOLO sets PSD |
| **`Psd=1, Pia=0`** | `FE 00` | **does the control store raise DT on a nested SOLO?** |
| `Psd=1, Pia=0` | `FE 01` | TUTTI clears PSD |
| `Psd=0, Pia=0` | `FE 01` | does TUTTI raise IIC? |

**Read a null result correctly.** DT is set NOWHERE in the microcode - `BM36`
appears with no `D,MIC,...` or `D,SPEC,...` destination in the entire listing -
so the timeout is hardware watching the modus bit. The oracle settles what the
control store DOES; it cannot settle what the hardware counter does afterwards.
A "no DT" result therefore disproves an immediate trap, which is the specific
claim in `Solo.cs`, and does NOT by itself prove the timer keeps running.

**If the oracle contradicts nd500x, nd500x changes.** That is the whole point of
running it.

## Phase 2 - make the three agree

**2.1 (RETROCORE) Delete the nested-SOLO trap block**, `Instructions/CONTROL/Solo.cs:52-58`.
Contradicted by phase 1 and by the microcode never setting DT.

**2.2 (RETROCORE) Implement the three missing rules** in `CpuND500`, mirroring
nd500x `8a46aaf` + `712ba00`:
- the 256-cycle DT timeout - `ProcessSwitchTimeoutCounter` is written in
  `Solo.cs:66` / `Tutti.cs:54` and read nowhere;
- DE alongside the provoking trap - proven by `DEL_TRAP` 012542, which ORs bit
  31 when PSD is set and then falls through to `TRAP` either way;
- ignorable traps suppressed while PSD is set - `.PSD` is never read on the trap
  path.

**2.3 (RETROCORE) Fix the `Tutti.cs` doc comment** - it claims "Privileged
instruction" and "Trap conditions: IIC". The code is right; only the comment is
wrong.

**2.4 (OURS) Re-verify nd500x against whatever phase 1 returns.** If the oracle
disagrees with `Solo.c`, change `Solo.c` - not the test.

## Phase 3 - the corpus (RETROCORE generates, OURS runs)

**3.1 Fix the generator defect first.** `CreateGenericSystemScenario`,
`Emulated.Tests.ND500/Validation/Generators/ComprehensiveSystemGenerator.cs:805`,
stamps `ExpectedTrap = IllegalInstruction` on EVERY SYSTEM-class instruction it
falls through to. That is where the wrong `tutti_Default` comes from. Drive
privilege from the manual's explicit "Privileged instruction" line, not from the
instruction class, and re-check the rest of that list: `pmof dmof rpgu cpgu zpgu
rwip cwip zwip rphs wphs freeb ddirt int`.

**3.2 Generate 18 positive + 8 negative cases** - full matrix with exact `st`
values in `docs/SPEC-CORPUS-SOLO-TUTTI-DT-DE.md`. The negatives use the corpus's
own encoding (`isNegativeTest`, `negativeTestType`, `expectedValidationFailure`),
which already carries 3664 cases. N2 is the important one: SOLO with
`st = 0x00000010` and a deliberately wrong claimed final of `0x40000010`, which
passes only if the emulator does NOT raise DT on a nested SOLO.

**3.3 (OURS) Regenerate, re-run, delete the quarantine.** Copy the corpus in,
`make`, `ctest`. Then remove the `tutti_Default` entry from
`conformance_quarantine[]` - the runner already FAILS if a quarantined case
starts passing, so this step is forced, not optional.

## Phase 4 - the sequence tier (OURS)

T1-T11 in the spec cannot be expressed as single-instruction corpus cases
(timeouts, DE-on-fault, suppression-then-release, reset behaviour). `test/test_solo_traps.c`
already covers them at 19/19. Keep them there; do NOT contort them into the
corpus, and do NOT let a corpus case pre-load an emulator-internal cycle marker -
that tests the implementation, not the architecture.

**4.1** Ask the RetroCore side for the equivalent C# sequence test so the tier is
covered on both sides. This is the one part with no shared artefact, so it is
duplicated by design.

## Phase 5 - make it reusable, so the next instruction is cheap

**5.1 (RETROCORE) A standing three-way gate.** Once `SPEC,MOD` is modelled, any
instruction whose microroutine touches only modelled selects can be run through
`RunBoth`. Add SOLO/TUTTI to `ControlManualCoverageTests` - and note that file's
line 49-50 records "there is NO CONTROL_tutti corpus file ... tutti is simply
absent", so TUTTI needs adding to that corpus too.

**5.2 (OURS) Widen the audit with the oracle.** `docs/SYNC-BACKLOG.md` marks 141
rows "present in C# (name-level)" - identifier matched, behaviour NOT compared.
The one row read line by line was wrong in both directions. Re-verify them
against the control store, cheapest first.

**5.3 (OURS) The privilege sweep.** The `tutti_Default` defect and nd500x's wrong
TUTTI guard were the same assumption from opposite directions. Check every
nd500x instruction calling `nd500_require_privilege()` against the manual's
explicit "Privileged instruction" line and the microcode.

## What can proceed in parallel, with nobody blocked

- OURS, no dependency: phase 4, 5.2, 5.3.
- RETROCORE, no dependency: 0.1, 0.2, 2.3, 3.1.
- Blocked on phase 0: phase 1, and therefore 2.1, 2.4.
- Blocked on phase 3.2: 3.3.

## Rules for this work, learned the expensive way

1. **Never fix an emulator to match the corpus without checking the corpus.**
   `tutti_Default` was wrong. The handoff doc warns about exactly this and it
   still happened.
2. **Grepping one identifier is not reading the code.** The first version of the
   SOLO finding was wrong in BOTH directions because it grepped
   `ProcessSwitchTimeoutCounter` and never opened `Solo.cs`.
3. **Cross-emulator agreement proves nothing** - shared lineage, shared bugs.
   Only the manual, the control store, and real hardware are oracles.
4. **Label inference as inference.** The repeat-SOLO timing is still inferred and
   must stay marked so until hardware or the ND-5000 hardware description says
   otherwise.
5. **A dead end is a result** - record it. The microcode SOURCE is not on the
   `211276D` floppy; it is a separate product, ND-no. 250291D, which we do not
   have. Do not search that media again.
