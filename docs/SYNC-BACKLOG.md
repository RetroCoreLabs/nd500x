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
| (pre-2026-08-08) | various | Float arithmetic fixes | docs/SYNC-FLOAT-ARITHMETIC-FIXES.md | **done in C#, checked 2026-08-17** - the rebased codec (`ReadOperandAsIeeeFloat`/`WriteOperandFromIeeeFloat`) is used by 13 ND-500 files and every ARITHMETIC file is committed; no longer a name-level guess |
| (pre-2026-08-08) | various | Float native bias-256 rebase | docs/SYNC-FLOAT-NATIVE-REBASE.md | **done in C#, checked 2026-08-17** - same codec, same files; the doc's "not yet committed pending regen" note is stale |
| (pre-2026-08-08) | various | MON 257B FOPEN present in SINTRAN L | docs/SYNC-MON-257B-FOPEN-PRESENT-IN-SINTRAN-L.md | **done in C#, checked 2026-08-17** - `MON_257_FOPEN.cs` really searches file numbers 64..127 and writes FileNo/AccessCode/DevNo. Both emulators have since replaced the status-based MOINF with the SAME carved L07 MCTAB table, so the doc's step 1 (flip a status flag so MOINF reports 257B present) is superseded on BOTH sides |
| (pre-2026-08-08) | various | STRING wrong-instruction fixes | docs/SYNC-STRING-WRONG-INSTRUCTION-FIXES.md | **done in C#, checked 2026-08-17** - Sscan/Sspan/Smatch/Scopt committed in `a72d1f33f` and Scpuno in `1defdb121`; that later commit also shares one descriptor-address helper, which settles the doc's OPEN VERIFICATION ITEM (effective_address vs ReadOperandValue). The doc's "DONE (subagent), uncommitted" note is stale |
| 2026-08-08 | ndmonlib `97a2a22` + pointer bump | MON 113B CLOCK returned `tm_year % 100`; now writes the full year. **INFERRED, not proven** | shared-file item 13 | **done** - `MON_113_CLOCK.cs:100` writes `now.Year` and carries the same "INFERRED, not proven" note |
| 2026-08-06 | `8a46aaf`, `712ba00` | SOLO/TUTTI: the 256-cycle DT timeout, DE on a non-ignorable trap inside PSD, and ignorable traps suppressed inside PSD | this file, SOLO/TUTTI section below | **done in C#** - RetroCore `13e9c0ef7`: all four rules ported (repeat-SOLO no longer traps immediately and does not restart the clock; the 256-cycle DT timeout now exists at all - `ProcessSwitchTimeoutCounter` was written in two places and read in none; DE alongside a non-ignorable trap; ignorable traps suppressed while PSD). Timeout check runs BEFORE the ignorable check, mirroring cpu.c:768. Builds clean, full ND500 suite 2090 passed / 15 failed, all 15 pre-existing Windows-absolute-path lookups (`E:\`, `D:\`) that cannot resolve from WSL |
| 2026-08-09 | `ea4ed54` | **nd500x bugs found by using the ND-5000 microcode as the oracle:** a repeated SOLO restarted the timeout; TUTTI wrongly required privilege while SOLO did not; two blocks of invented prose in `Solo.c`. All fixed, `test_solo_traps` 12 -> 19 cases | `docs/SPEC-CORPUS-SOLO-TUTTI-DT-DE.md` | **done (nd500x)** |
| 2026-08-11 | ndmonlib 300B/301B | MON 300B EUSEL and 301B DUSEL implemented against ND-860228-2 p.442/p.490, and moved off the same `NOT_IMPLEMENTED` hard stop. The body pages settle an architecture split the overview tables get backwards: 300B/301B are the ND-100 form, 405B USTBRK the ND-500 form of one facility, over one per-terminal state | shared-file item 18 | **done in C#** - RetroCore `33359efb8`: both implemented against the same per-terminal state 405B uses, registered with parameter lists and status InProgress (the async ESCAPE->transfer is still undelivered on both sides). 405B's own status corrected Validated -> InProgress in the same commit. Builds clean |
| 2026-08-11 | ndmonlib `34f034d` + registry status | MON 405B USTRK implemented, and its registry entry moved off `MON_STATUS_NOT_IMPLEMENTED`, which `mon_dispatch.c:173` enforces BEFORE the handler runs - the working handler was unreachable and 405B hard-stopped. Verified with `ND500X_MONLOG=1`: SUCCESS, and `Nll:` commands complete | shared-file item 17 | **done** - C# handler already matched; `SintranEmulation.Definitions.cs:313` corrected `Validated` -> `InProgress`. Open question in item 17: C# gates only on `Handler == null` and never consults the status flag, so the two dispatchers enforce different rules |
| 2026-08-17 | ndmonlib `2cddec3` + nd500x `ccaff3c` | MON 50B/43B: a sequential-write (access 0) open resets this session's max byte pointer and CLOSE truncates the host file to it, carved from the SINTRAN L file system segment (`SOFT@066123B` sets it to -1 for access 0 alone; `FCL2@070132B` stores it into object entry word 62B). Open still does NOT truncate - mode is `"r+b"`, never `"wb"`. Also: three write handlers (2B, 24B, 504B) did not maintain the length, and the exit close-all path applied no length at all | shared-file item 19 | **done and VERIFIED in C#** - RetroCore `80d107f12` on `ethernet-ii-controller-fixes`: builds with 0 errors, ND500 Sintran tests 152 passed / 0 failed. C# DID have the data-loss bug: `OpenFileTable.GetFileOpenParams` mapped access 0 to `FileMode.Create`. Now `OpenOrCreate/ReadWrite` plus a `SeqWriteLength` flag applied by the close-time `SetLength` that already served SMAX; `WriteByte` (which 2B/24B/504B all funnel through) and 120B WFILE both call the new `OpenFileTable.NoteWrite`. `CloseAllForExit` already routed through `entry.Close()`, so C# needed no separate exit-path fix. Two WSL traps found on the way, worth knowing for any future C# sync from this side: `dotnet test` cannot restore in WSL because the repo's NuGet configuration lists a Windows-only fallback folder (`C:\Program Files (x86)\Microsoft Visual Studio\Shared\NuGetPackages`), so run `dotnet vstest` against the built assembly instead; and an Access-denied writing into `obj/` is transient locking by a Windows-side build, not a permission problem - a retry cleared it |
| 2026-08-17 | ndmonlib `7cdaff1` + nd500x `5c0d1ec` | **Regression tests** for the three bugs found today: MON 50B reporting 076B (not 056B) for a quoted create over an existing file; 2B OUTBT writes surviving the close-time truncation (a write path that forgets `mon_file_note_write` cuts its output to zero); and `mon_console_take_pushback` handing back the byte the ESCAPE poll peeked. Each proven to FAIL without its fix | this file | **both sides now** - nd500x tests run green (ctest 39/39); the C# equivalents are RetroCore `44b74908f` + `22890c6ca` and they COMPILE AND PASS from WSL - 43/43, and the truncation case was checked by putting the bug back. C# coverage: all four SOLO/TUTTI rules, the access-0 open/close truncation semantics including the segment-mapped exemption, and MON 300B/301B with the stored-address-survives-OFF assertion |
| 2026-08-09 | n/a - corpus defect | `tutti_Default` expects IIC. TUTTI is not privileged; the expectation comes from a SYSTEM-class catch-all at `ComprehensiveSystemGenerator.cs:805` and contradicts RetroCore's own `Tutti.cs` and `TrapConditionSpec.cs:621`. Quarantined in `test_conformance.c` | `docs/SPEC-CORPUS-SOLO-TUTTI-DT-DE.md` | **fixed in C#, COMPILES** (RetroCore `e5d0bd3b5`, verified in `365959a74`): the SYSTEM catch-all now consults an explicit unprivileged set (TUTTI only - instructions.json has no privilege flag, so anything else would be a guess). **CLOSED 2026-08-17**: corpus regenerated from WSL and installed (nd500x `e7a684e`), and the set was completed to all four affected instructions - `dcc`, `pcc` and `ddirt` had the same catch-all expectation, with manual/control-store evidence that nd500x's quarantine already carried (RetroCore `672b89e0a`). `conformance_quarantine[]` is now EMPTY; test_conformance reports 40,082 passed / 0 failed / 0 quarantined |
| 2026-09-15 | (this change, uncommitted at writing) | Monitor shell runs OLD-FORMAT domains (`:PSEG`+`:DSEG` via `DESCRIPTION-FILE:DESC`) directly: `ndlib_load_old_domain` stages them as a :DOM image; values reproduce CONVERT-DOM-A03 output byte for byte on two conversions; ENABLEINT->OTE is `<<9` / `>>23` from ONE non-zero witness | docs/SYNC-OLD-FORMAT-DOMAIN-LOADER.md | open - C# reads :DOM only |
| 2026-09-18 | `22e855d` | Data status bits an instruction does not name are now reset (manual 6.5.1): the Z/S helpers clear C and O; "/", ABS, IXI, MUL4, DIV4, PWCONV, WPCONV, TSET, STZ, CLEBI, SETBI, PCOMP given the missing reset; SHR keeps K; float TEST clears C. Found by the corpus flag-preset twins (RetroCore `7c0170554`) | shared-file item 22 | open |
| 2026-09-18 | `654d6d7` `a23301c` `486b367` `053ef24` | Microcode sweep findings, ruled on the manual and the B30 listing: DIV4 BY/H remainder zero-filled; F/D divide rounds by 7.2.7; float -0 store Z from the bit pattern, TEST -0 is Z only; SFILLN/SMOVN typed elements and Z=1 on m done, SFILL/SFILLN F/D fill from A/E:A, DR trap (new) on a start outside the string; string descriptors keep all 32 count bits. Corpus from RetroCore `d21cd2bef` | shared-file item 23 | open |
| 2026-09-18 | `cb233b5` `c594c1b` | Decimal instructions (chapter 17) rewritten from the manual: descriptor SGN 26-24 / SC 23-16 signed / FW 15-0 (was SC 23-18, FW 17-13), exact 31-digit arithmetic, scaling, rounding, BO keeping signed low digits, IVO on bad digits/signs and the scaling-difference restriction, DR on FW=0; 374 manual-derived vectors (RetroCore `0bad65b87`) | shared-file item 24 | open |
| 2026-09-18 | `63ea9b4` | FREEB takes the element's address (LADDR), not the word at it; register/constant element is IOS | shared-file item 25 | open |
| 2026-09-18 | `4fa9f67` `5192061` `233c4cb` `ad812c8` `29f4cf4` | Found by the new manual-derived cases: CLR resets C/S/O; F INT/INTR read A (were reading I) and compute exactly; AXI 0**-n is IOV with the largest number (was IVO, 0); PSUM C is the adder carry (was set for every negative result); SMVWH Z=1 on source empty / dest full; SLOCA resets C/O | shared-file item 26 | open |
| 2026-09-18 | `ed40151` | Special register stores/loads (CED CAD PS TOS THA OTE MTE CTE TEMM P =:, A1..A4 :=/=:) reset C and O; P=: stores the address of the P=: instruction | shared-file item 27 | open |
| 2026-09-19 | `14a9000` | F/D ADD SUB MUL DIV MULAD (all operand forms) exact with manual 7.2.7 rounding (were host IEEE); FO stores the largest value, FU a signed zero; exponent-0 divisor is DZ; MULAD two rounded steps; F COMP and F/D COMP2 are float compares (COMPF/COMP2F/COMP2D): Z/S from the exact difference, C=0, FU/FO cleared | shared-file item 28 | open |

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
  agrees.** That was the state of this audit on 2026-08-09: rows read
  "present in C# (name-level)", meaning the identifier existed in RetroCore
  and nothing more.
  **Superseded 2026-08-17 - there are no name-level rows left.** The four
  that carried that marker were opened and checked: the float rows against
  the actual codec calls and their commits, MON 257B against the handler
  body (and both MOINF implementations, which have since converged on the
  same carved L07 MCTAB), and the STRING row against five committed files
  plus the later refactor that settled its open verification item. Two of
  those docs still say the C# side is "uncommitted" or "pending regen";
  both notes are stale, and the ledger rows now say so rather than the docs.
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
