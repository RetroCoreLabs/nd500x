# CURRENT PLAN

One page that answers "what are we doing and what is the plan" without
having to ask a session that may have compacted it away. UPDATE THIS AT
EVERY MILESTONE: goal changed, phase finished, investigation opened or
closed, dead end proven. Keep it short; details belong in the referenced
docs.

This is deliberately a living status file inside `docs/` (an exception to
the "working notes live in `$NDIX/notes/`" rule) so any session finds it
cold.

Last updated: 2026-08-08.

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
- Open investigation: linker `LOAD` error 52 at B0040D75 - full byte-level
  state in `docs/LINKER-LOAD-ERROR52-INVESTIGATION.md`.

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
  `make` did not just build.
- The NC heap crash root cause is SETTLED: the caller domain's MMU
  translation (g_pst/g_pcb_table) was lost across nested UECOM runs;
  fixed with nd500_mmu_state_save/restore in shell_execute_command. It
  was NOT missing monitor heap setup and NOT a missing trap-27 handler.
- The linker HELP crash was an ignorable Address-Zero trap, not a fatal
  PV; fixed in both emulators.
- `docs/LINKER-LOAD-ERROR52-INVESTIGATION.md` has its own dead-end
  section (seven+ disproven theories, including the section-11.5 memtrace
  histogram) - read it before theorizing on error 52.

## How to work here (hard-won)

- Order of authority and where out-of-repo truth lives:
  `docs/EXTERNAL-ARTIFACTS.md`.
- SINTRAN behavior facts that keep biting: `docs/SINTRAN-CONVENTIONS.md`.
- Every nd500x fix that touches CPU/MON behavior gets a line in
  `docs/SYNC-BACKLOG.md` for the C# side, at the time of the fix.
