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
| 2026-08-06 | `8a46aaf`, `712ba00` | SOLO/TUTTI: the 256-cycle DT timeout, DE on a non-ignorable trap inside PSD, and ignorable traps suppressed inside PSD | this file, SOLO/TUTTI section below | **open** - C# has PSD and the repeat-SOLO DT but none of these three |
| 2026-08-09 | `ea4ed54` | **nd500x bugs found by using the ND-5000 microcode as the oracle:** a repeated SOLO restarted the timeout; TUTTI wrongly required privilege while SOLO did not; two blocks of invented prose in `Solo.c`. All fixed, `test_solo_traps` 12 -> 19 cases | `docs/SPEC-CORPUS-SOLO-TUTTI-DT-DE.md` | **done (nd500x)** |
| 2026-08-11 | ndmonlib 300B/301B | MON 300B EUSEL and 301B DUSEL implemented against ND-860228-2 p.442/p.490, and moved off the same `NOT_IMPLEMENTED` hard stop. The body pages settle an architecture split the overview tables get backwards: 300B/301B are the ND-100 form, 405B USTBRK the ND-500 form of one facility, over one per-terminal state | shared-file item 18 | **open** - C# has both as stubs (`UnimplementedMon`) |
| 2026-08-11 | ndmonlib `34f034d` + registry status | MON 405B USTRK implemented, and its registry entry moved off `MON_STATUS_NOT_IMPLEMENTED`, which `mon_dispatch.c:173` enforces BEFORE the handler runs - the working handler was unreachable and 405B hard-stopped. Verified with `ND500X_MONLOG=1`: SUCCESS, and `Nll:` commands complete | shared-file item 17 | **done** - C# handler already matched; `SintranEmulation.Definitions.cs:313` corrected `Validated` -> `InProgress`. Open question in item 17: C# gates only on `Handler == null` and never consults the status flag, so the two dispatchers enforce different rules |
| 2026-08-17 | ndmonlib `2cddec3` + nd500x `ccaff3c` | MON 50B/43B: a sequential-write (access 0) open resets this session's max byte pointer and CLOSE truncates the host file to it, carved from the SINTRAN L file system segment (`SOFT@066123B` sets it to -1 for access 0 alone; `FCL2@070132B` stores it into object entry word 62B). Open still does NOT truncate - mode is `"r+b"`, never `"wb"`. Also: three write handlers (2B, 24B, 504B) did not maintain the length, and the exit close-all path applied no length at all | shared-file item 19 | **open** - C# not yet checked; if its access 0 maps to a truncating mode, that is a data-loss bug |
| 2026-08-09 | n/a - corpus defect | `tutti_Default` expects IIC. TUTTI is not privileged; the expectation comes from a SYSTEM-class catch-all at `ComprehensiveSystemGenerator.cs:805` and contradicts RetroCore's own `Tutti.cs` and `TrapConditionSpec.cs:621`. Quarantined in `test_conformance.c` | `docs/SPEC-CORPUS-SOLO-TUTTI-DT-DE.md` | **open against the generator** |

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

### SOLO/TUTTI - read line by line against the manual, and it goes BOTH ways

An earlier version of this section claimed C# "only resets a counter" and had
no PSD at all. **That was wrong** - it came from grepping for
`ProcessSwitchTimeoutCounter` and missing `regs.ST.PSD = true`. Corrected
2026-08-09 by reading `Solo.cs`, `Tutti.cs`, every `.PSD` and every `DT`/`DE`
write in the solution, and checking each rule against
`docs/ND-05.009.4 EN ND-500 Reference Manual.md` ch.16.1-16.2 and ch.6.5.4
rather than against either emulator.

Both sides set PSD in SOLO and clear it in TUTTI. The differences:

| Manual rule (ch.16.1 / 6.5.4) | nd500x | C# |
|---|---|---|
| ">256 cycles with PSD set -> DT", privileged unlimited | yes, `check_solo_timeout()` `cpu.c:1397`, counts macroinstruction cycles per ch.16.1's ND-5000 wording | **no** - `ProcessSwitchTimeoutCounter` is set to 0 in `Solo.cs:66` / `Tutti.cs:54` and read nowhere; nothing increments it |
| "Non-ignorable and fatal traps cause a disable process switch error trap" (DE) | yes, `cpu.c:1266`, raised alongside the provoking trap | **no** - the only `TrapCondition.DE` in the solution is a test setting the flag |
| "Ignorable trap conditions are ignored in SOLO-TUTTI sequences regardless of enabling" | yes, `check_pending_traps()` returns early on PSD, `cpu.c:1462` | **no** - PSD is never read on the trap path |
| "Disable process switch timeout occurs if unprivileged users attempt to repeat SOLO's" | fixed `ea4ed54` - the timer is no longer restarted, so the repeat trips the original deadline | **wrong** - `Solo.cs:53-58` raises DT *immediately* on a nested SOLO |
| TUTTI is not privileged | fixed `ea4ed54` - the guard is gone | correct - `Tutti.cs` never enforced it |

**Settled by the ND-5000 control store, 2026-08-09**, after this section first
got the repeat-SOLO rule wrong in both directions. `SOLO_0` (004524) arms the
region by OR-ing a modus-register bit (004531/004532) and `TUTTI_0` clears it
unconditionally (004536). An OR is a level set and there is NO counter reset
anywhere in the SOLO path, so the timer runs from the FIRST SOLO of a region -
ch.16.1's repeat sentence is that consequence, not a separate check. So:

- **C# is wrong to trap immediately** on the nested SOLO: that fires two cycles
  into a region the manual allows to run 256. Replace it with "do not restart
  the timer", which needs the timeout from row 1 to exist first.
- **nd500x was wrong the other way** and re-stamped its start marker on every
  SOLO, so unprivileged code could hold the process switch disabled forever.
  Fixed.
- Neither instruction is privileged (manual marks privileged ones with an
  explicit "Privileged instruction" line; 16.1 and 16.2 have none, and TUTTI_0
  has no PIA test). nd500x wrongly guarded TUTTI while leaving SOLO open -
  fixed. C#'s code is right here; only its `Tutti.cs` comment is wrong.

Full derivation, the microwords, and the corpus cases:
`docs/SPEC-CORPUS-SOLO-TUTTI-DT-DE.md`.

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
- **Grepping for one identifier is not reading the code.** The first version
  of the SOLO/TUTTI finding was wrong in both directions - it claimed C# had
  no PSD (it does) and missed that C# implements a rule nd500x lacks. The
  error came from searching for `ProcessSwitchTimeoutCounter` and treating
  three hits as the whole story. Open the file.
- Not covered, because C# has no equivalent subsystem and should not: the
  NDIX MON 600 front-end (`fecall`, `FE_READ`, `RETK`), nd500x debug knobs
  (`ND500X_NO_INVALID00`, `UDATADBG`) and nd500x-only performance work
  (the `TSB` translation cache, decoder indexing).
- Three MMU/CPU items were neither token-matched nor read, and are genuinely
  unknown: `94a603c` demand-grow in PS_ADI mode, `41cc67f` scntxt/lcntxt
  accessing the context block physically, and `43104be` the translation cache
  (which RetroCore has only noted as a "TLB aliasing hazard").
- RetroCore was audited at `ed00a037c` on branch `ethernet-ii-controller-fixes`.
