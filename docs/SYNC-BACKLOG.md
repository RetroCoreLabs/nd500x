# C# (RetroCore) sync backlog

The single ledger of nd500x changes not yet mirrored to the RetroCore C#
emulator. The per-fix detail docs are `docs/SYNC-*.md` in this repo and the
numbered items in the shared rolling file
`$ND500_TESTDATA/retrocore-mon-fixes.md` (C#-bound MON/CPU notes go THERE,
never as new docs inside the RetroCore repo).

Rules:
- APPEND one line here at the time of every nd500x commit that changes
  CPU, MMU, trap, or MON behavior. Status starts as `open`.
- Mark `done` (with the RetroCore commit) when the C# side has the fix;
  do not delete lines.
- This file is the answer to "how far behind is C#" - if it cannot answer
  that, it has not been maintained.

Format: `| date | nd500x commit | what changed | detail doc / shared-file item | status |`

## Ledger

| Date | nd500x commit | Change | Detail | Status |
|---|---|---|---|---|
| (pre-2026-08-08) | various | Float arithmetic fixes | docs/SYNC-FLOAT-ARITHMETIC-FIXES.md | present in C# (name-level) |
| (pre-2026-08-08) | various | Float native bias-256 rebase | docs/SYNC-FLOAT-NATIVE-REBASE.md | present in C# (name-level) |
| (pre-2026-08-08) | various | MON 257B FOPEN present in SINTRAN L | docs/SYNC-MON-257B-FOPEN-PRESENT-IN-SINTRAN-L.md | present in C# (name-level) |
| (pre-2026-08-08) | various | STRING wrong-instruction fixes | docs/SYNC-STRING-WRONG-INSTRUCTION-FIXES.md | present in C# (name-level) |
| 2026-08-08 | ndmonlib `97a2a22` + pointer bump | MON 113B CLOCK returned `tm_year % 100`; now writes the full year. **INFERRED, not proven** | shared-file item 13 | **done** - `MON_113_CLOCK.cs:100` writes `now.Year` and carries the same "INFERRED, not proven" note |
| 2026-08-06 | `8a46aaf`, `712ba00` | SOLO/TUTTI process-switch disable, DT and DE traps, and the reset that stops a stale SOLO region trapping | this file, "Confirmed gap" below | **open - the only confirmed gap** |

## Backlog state as of 2026-08-09 - AUDITED

**The old "C# is far behind on CPU/MON fixes" claim is WRONG and has been
withdrawn.** It was recorded in 2026-07 and was never re-checked. RetroCore
has been tracking nd500x closely and has its own explicit porting commits,
for example "MON port: add the 27 handlers nd500x has and C# did not, plus
the 46-vs-24 sweep" and "ND-500 SINTRAN: port the monitor calls nd500x
implements, and CHAIN's IOV" (both 2026-07-30/08-03).

### What the audit did

Took the 203 nd500x commits since 2026-07-01 touching `src/cpu/instructions`,
`nd500_mmu.c`, `cpu.c`, `cpu_instr.c`, `nd500_domain.c`, `src/libmon` or
`external/ndmonlib`. Dropped the ones with no distinctive identifier and the
NDIX/Windows/tty/debug-only ones, leaving 142. For each, checked whether its
distinctive tokens (instruction mnemonics, MON numbers) appear anywhere in the
RetroCore log since 2026-06-25. **124 of 142 matched.** The 18 that did not
were then grepped against the RetroCore ND-500 source directly.

### Confirmed gap - SOLO/TUTTI DT/DE traps

The only one that survived. C# declares
`CpuND500.ProcessSwitchTimeoutCounter` (`CpuND500.cs:438`) and resets it to 0
in `Instructions/CONTROL/Solo.cs:66` and `Instructions/SYSTEM/Tutti.cs:54`.
Those are the ONLY three references in the whole solution: nothing ever
increments it, and nothing raises DT or DE from it. The `DT` and `DE` register
flags exist in `Registers.cs` and are cleared on reset, but are never set by
the timeout. nd500x implemented the mechanism in `8a46aaf` and fixed the
reset in `712ba00`. Mirror those two.

### Spot-checked and genuinely mirrored

Of the other 17 no-token-match commits, these were read in the C# source and
are present, several citing the C fix by date:
`IFKGO` (`Instructions/BRANCH/Ifkgo.cs` branches from `fi.StartAddress` and
its comment names the 2026-07-09 C fix), `ENTB`
(`Instructionset.BuddySystem.cs`), MON 71B/72B `DESCF`/`EESCF`, MON 144B
`MAGTP`, MON 263B `GDEVT`, MON 321B `UEADM`, and MON 113B (above).

### Limits of this audit - read before trusting a row

- **A name match proves the subject was touched, not that the behavior
  agrees.** Every row above marked "present in C# (name-level)" means the
  identifier exists in RetroCore, nothing more. Only MON 113B, IFKGO and
  SOLO/TUTTI were read line by line.
- Not covered, because C# has no equivalent subsystem and should not: the
  NDIX MON 600 front-end (`fecall`, `FE_READ`, `RETK`), nd500x debug knobs
  (`ND500X_NO_INVALID00`, `UDATADBG`) and nd500x-only performance work
  (the `TSB` translation cache, decoder indexing).
- Three MMU/CPU items were neither token-matched nor read, and are genuinely
  unknown: `94a603c` demand-grow in PS_ADI mode, `41cc67f` scntxt/lcntxt
  accessing the context block physically, and `43104be` the translation cache
  (which RetroCore has only noted as a "TLB aliasing hazard").
- RetroCore was audited at `ed00a037c` on branch `ethernet-ii-controller-fixes`.
