# External artifact index

Where the truth for this project lives when it is NOT in this repository.
Every location is given through an environment variable (see
`docs/PATH_CONVENTIONS.md` for the variable table); nothing here names a
drive or a home directory.

This file exists because these locations were previously supplied from
memory, piecemeal, one correction at a time - and once with the wrong tree.
When a location changes, change it HERE.

## Order of authority

1. The ND-500 / SINTRAN reference manuals and the carved SINTRAN code.
2. The vendor binaries themselves (bytes on disk).
3. This emulator and the RetroCore C# emulator - both are implementations
   with bugs. Neither is a reference for the other; they are ports of the
   same design and cross-diffing them cannot find shared defects.

## The SINTRAN L07 carve (MON call ground truth)

| What | Where |
|---|---|
| Full working tree - AUTHORITATIVE, ~158 MON-call folders | `$NDINSIGHT/tools/sintran-segment-carver/versions/L-VSX-500/re/mon-analysis/` |
| `MON-CALL-INDEX.md` (current, regenerated) | same tree as above |
| Delivery mirror - frozen SNAPSHOT SUBSET, may be stale | `$ND500_TESTDATA/t/` (unverified subpath; the point is: prefer the `$NDINSIGHT` tree) |

If the carve does not answer a question, do not guess: write a carve request
for the carver session and ask for more carving.

## MON call documentation

| What | Where |
|---|---|
| Per-call YAML corpus (263 calls) | `$NDINSIGHT/Developer/MON/calls/` |
| Scanned reference manuals (the FULL manuals hold the device/function tables the YAMLs defer to - check them before asking the carver) | `$NDINSIGHT/Reference-Manuals/` |
| Installation procedures (e.g. LED install, ND-211160) | `$NDINSIGHT/Installation/Installation-Description/` |

## Vendor programs and libraries (the real 1980s binaries)

All under `$ND500_TESTDATA`:

| Artifact | Where |
|---|---|
| NC compiler front-end + its asm listing + MON call notes | `$ND500_TESTDATA/FraTor/nc/` (`nc-a06.dom`, `nc-a06.asm`, `mon-calls-described.md`) |
| CAT-500 code generator (VDM back-end, invoked by NC via MON 317B UECOM) | `$ND500_TESTDATA/CAT5-CAT/cat-cat5-b06.dom` |
| ND Linker + help/init/auto-jobs + linker docs | `$ND500_TESTDATA/nd-linker/` (`linker-b01.dom`, `.help`, `.init`, `linker-auto*.job`) |
| C runtime libraries (:NRF) | `$ND500_TESTDATA/c-libs/` |
| LED editor (ships as PSEG+DSEG pair, not a .DOM) | `$ND500_TESTDATA/LED/` |
| ND-500 Symbolic Debugger | `$ND500_TESTDATA/ND-500 Symbolic Debugger/` |
| CONVERT-DOMAIN | `$ND500_TESTDATA/CONVERT-DOMAIN/` |
| cpu-stat | `$ND500_TESTDATA/FraTor/cpu-stat/` |

A vendor file that no filename search finds is usually INSIDE a disk image
(`.img` floppy/HD images under `$ND500_TESTDATA` and `$NDIX`); extract it,
do not substitute a different version under a renamed filename (standing
rule: ask for the missing file by exact name).

## Cross-emulator (RetroCore C#) coordination

| What | Where |
|---|---|
| RetroCore checkout (C# ND-500 CPU + test generators) | `$RETROCORE` |
| Shared rolling fix list for the C# side - C#-bound MON/CPU porting notes go HERE as numbered items, never as new docs in the RetroCore repo | `$ND500_TESTDATA/retrocore-mon-fixes.md` |
| Per-fix sync docs in this repo | `docs/SYNC-*.md`, ledger in `docs/SYNC-BACKLOG.md` |

## Working notes and handoffs

Session write-ups, investigations and handoffs live in `$NDIX/notes/`, not
in `docs/` (see `docs/PATH_CONVENTIONS.md`). Every handoff document carries
full absolute paths and states its audience on the first line ("Audience:
the C# session" / "the carver session" / "a future nd500x session").

## The SINTRAN user sandbox

`$ND500USERS` (GUEST / SCRATCH / SYSTEM + `populate-system.sh` +
`nd500x.ini`). Its own README documents the layout and the standing test
artifacts.

**Run the LINKER from `$ND500USERS`.** Its `SYSTEM/` holds everything the
linker opens at startup: `LINKER-B01.DOM/.HELP/.INIT`, the `DDBTABLES-*.VTM`
terminal tables, `UE-ERMSG-EN-C06.ERR`, and the libraries `CAT-LIB.NRF` and
`NC-LIB.NRF` under the un-revisioned names an unqualified open asks for.
`populate-system.sh` builds it, uppercasing every name on the way in because
`src/libmon/mon_path.c` upshifts and the tree sits on a case-sensitive
filesystem.

`build/nc_sandbox` is the COMPILER's, built by
`test/nc_sandbox_setup.cmake` from tracked fixtures in `test/nc_fixtures/`.
Do not run the linker from it - it starts and fails in a way that reads like
a real defect.

Older documents say to run the linker from `build/link_sandbox`. That was a
hand-assembled directory under gitignored `build/`; it did not survive a
`make clean` and is not present now. `$ND500USERS` replaced it. Where an
investigation record cites `build/link_sandbox`, read it as the sandbox of
its day, not as an instruction.
