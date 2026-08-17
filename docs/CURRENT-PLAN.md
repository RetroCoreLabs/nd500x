# CURRENT PLAN

One page that answers "what are we doing and what is the plan" without
having to ask a session that may have compacted it away. UPDATE THIS AT
EVERY MILESTONE: goal changed, phase finished, investigation opened or
closed, dead end proven. Keep it short; details belong in the referenced
docs.

This is deliberately a living status file inside `docs/` (an exception to
the "working notes live in `$NDIX/notes/`" rule) so any session finds it
cold.

Last updated: 2026-08-17.

## Standing goal

Run the real ND-500 vendor toolchain under nd500x: C source -> NC ->
CAT-500 -> :NRF -> ND Linker -> :DOM that runs and exits cleanly. This
end-to-end pipeline was first achieved 2026-07-20 and is the regression
bar: it must keep working.

## Current work (as of the last commits on fix/deabf-i1-success-and-load-investigation)

- Instruction conformance: the corpus runner learned ST2 and exact
  trap-condition bits; instruction_validation was brought from 261
  failures down to 2, then the last two trap gaps were closed (see
  `git log` around 8d9c511, 04f18b5, 21be7ab).
- Linker `LOAD`: CLOSED 2026-08-09. Every blocker in
  `docs/LINKER-LOAD-ERROR52-INVESTIGATION.md` is fixed. `OPEN-DOMAIN` + `LOAD`
  + `CLOSE` links a `.DOM` that runs and gives the right answer. Bisected: the
  last fix was `a3047b1` (LOOPI index data type) on 2026-07-20, so the document
  described a dead blocker for three weeks. No open investigation replaces it.
- File formats, 2026-08-11 to 2026-08-17: CLOSED. `DESCRIPTION-FILE:DESC` is
  fully decoded - both the segment entry and the domain entry, from the ND-500
  Loader/Debug Monitor's own print code - and the size rule `PLB+PSIZE+1 = .pseg`
  holds over 48 checks on 13 vendor floppies with no mismatches. The `:LINK`
  format is decoded as NLL's loader table, 32-byte cells in ascending value
  order. Specs live in NDInsight `SINTRAN/File-Formats/`; the carve chains are in
  `SINTRAN/ND500/nd-500-mon/CARVE-ANSWER-*.md`. Two of the manual's domain-entry
  field placements are wrong - see the answer document before trusting it.
- MON write-open semantics, 2026-08-17: SETTLED by carving the SINTRAN L file
  system segment. Open does NOT truncate; CLOSE writes the session length back.
  Implemented and unit-tested (ndmonlib `2cddec3`, nd500x `ccaff3c`); ledger line
  in `docs/SYNC-BACKLOG.md`, C# note is item 19 of the shared rolling file.

## Next steps

Small and known, in the order they are worth doing:

1. Teach `pcc-nd500`'s `desc_utils.c` and `nd500-dump` the fields now proven in
   its `desc.h` - the domain entry past DNAME, and the COMSEGNO-bounded arrays.
   The header knows more than the tools decode.
2. A pass over `docs/CONVERT_DOMAIN_FORMAT_AND_USAGE.md` and the DESC docs in
   this repo, which still describe a smaller verified set than what is proven.
3. `cpu-stat` reports the wrong CPU type and instruction set (0 / Nord-10 /
   ND-100) for an ND-500.

Open questions that are NOT tasks, because they need material not on hand: the
L-era `:LINK` string regions need an L-series NLL binary; `DLINKDATE`,
`ABSFIXAD`, `LOWLOGFIX`, `PLOLOGFIX` and `PUPLOGFIX` have proven offsets but
unknown meanings and read zero in all 26 real segment entries, so they need a
sample that sets one; the segment-entry flags word at byte 60, the Process Entry
internals and the Symbol Entry layout are undecoded. SINTRAN's ABORT path was
never carved, which is the one caveat on the close-time truncation above.

## Ruled out - PROVEN, do not re-investigate

Read this BEFORE starting any loop iteration on a related symptom. Each
entry was expensive to settle at least once; several were re-derived after
compactions before this list existed.

- Linker prompt clipping ("NDL(ADV)" -> "ND", "Domain name" -> "Do") is
  the LINKER's OWN escape stream - guest output, not a transport, MON,
  NUL, or terminal bug. Proven in pyte with the TDV log.
- The 2026-07-26 "general domain-header regression" never existed: it was
  a stale hand-linked `diag_linkdrive` binary. The diag_* harnesses are
  CMake targets now (`make diag`); never trust a result from a binary
  `make` did not just build. This bit again on 2026-08-09 in a different
  shape - a bisect script that fell back to the previous commit's binary
  when the CMake target did not exist yet, and produced a confident wrong
  answer that agreed with the document it was checking. Delete the binary,
  do not just rebuild over it.
- The NC heap crash root cause is SETTLED: the caller domain's MMU
  translation (g_pst/g_pcb_table) was lost across nested UECOM runs;
  fixed with nd500_mmu_state_save/restore in shell_execute_command. It
  was NOT missing monitor heap setup and NOT a missing trap-27 handler.
- The linker HELP crash was an ignorable Address-Zero trap, not a fatal
  PV; fixed in both emulators.
- `docs/LINKER-LOAD-ERROR52-INVESTIGATION.md` has its own dead-end
  section (seven+ disproven theories, including the section-11.5 memtrace
  histogram) - read it before theorizing on error 52.
- A `:DOM` found at 0 bytes is NOT a bug and NOT a regression of the MON 0B
  LEAVE write-back fix. Traced 2026-08-17: a quoted destination is created
  EMPTY at open and filled only by segment write-back at close, so any run
  interrupted between those two points leaves exactly 0 bytes - and the next
  attempt then trips 076B "file already exists", which reads like a different
  fault. Two theories were eliminated before this one: the write-back (intact,
  observed firing) and fopen `"wb"` truncating at open (the linker never uses
  access code 0 - it uses 1, 2 and 3, and `OPEN-DOMAIN` uses 2).
- The DESC size fields are not file sizes and never were. They store the LAST
  BYTE INDEX, which is why byte scans for the file size found nothing for
  years. Do not restart that scan.

## How to work here (hard-won)

- Order of authority and where out-of-repo truth lives:
  `docs/EXTERNAL-ARTIFACTS.md`.
- SINTRAN behavior facts that keep biting: `docs/SINTRAN-CONVENTIONS.md`.
- Every nd500x fix that touches CPU/MON behavior gets a line in
  `docs/SYNC-BACKLOG.md` for the C# side, at the time of the fix.
