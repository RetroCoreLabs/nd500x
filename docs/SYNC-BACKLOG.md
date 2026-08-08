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
| (pre-2026-08-08) | various | Float arithmetic fixes | docs/SYNC-FLOAT-ARITHMETIC-FIXES.md | unaudited |
| (pre-2026-08-08) | various | Float native bias-256 rebase | docs/SYNC-FLOAT-NATIVE-REBASE.md | unaudited |
| (pre-2026-08-08) | various | MON 257B FOPEN present in SINTRAN L | docs/SYNC-MON-257B-FOPEN-PRESENT-IN-SINTRAN-L.md | unaudited |
| (pre-2026-08-08) | various | STRING wrong-instruction fixes | docs/SYNC-STRING-WRONG-INSTRUCTION-FIXES.md | unaudited |
| 2026-08-08 | ndmonlib `97a2a22` + pointer bump | MON 113B CLOCK returned `tm_year % 100`; now writes the full year. **INFERRED, not proven** - the manual states no width and the case rests on one NPL line. Read item 13 before mirroring | shared-file item 13 | open |

## Backlog state as of 2026-08-08

NOT AUDITED. This ledger was created 2026-08-08; everything before that
date lives only in the SYNC-*.md docs above, the shared rolling file, and
git history. The known standing fact (recorded 2026-07) is that the C#
side is far behind on CPU/MON fixes. An audit pass - walking nd500x git
history against RetroCore's - is needed to turn "unaudited" rows into
real `open`/`done` states; until then, treat every nd500x behavioral fix
since early July as potentially unmirrored.
