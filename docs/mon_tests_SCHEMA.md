# mon_tests.json - shared MON test spec (Phase 2)

**STATUS: DRAFT - NOT FROZEN.** The C side has explicitly asked that this
spec not be finalized yet (`NC_TOOLCHAIN_MON_PLAN.md` Phase 2 division of
labor). Shape and values here may still change before either runner treats
this file as a locked contract. See "Open items" below for what is
specifically unsettled.

Location: `~/repos/nd500x/test/mon_tests.json` (`test/mon_tests.json`
on the WSL box this was generated on). This doc lives at
`~/repos/nd500x/docs/mon_tests_SCHEMA.md`, next to `NC_TOOLCHAIN_MON_PLAN.md`.

Originally generated mechanically by a Python script (not committed - see
"Regeneration" below) from:

- the 33 YAML files under `NDInsight/Developer/MON/calls/` for the 34 NC MON
  calls (321B UEADM has no YAML - its description comes from
  `$ND500_TESTDATA/FraTor/nc/mon-calls-described.md`'s "NOT FOUND" section,
  which itself quotes manual text),
- the ND-860228.2 EN manual OCR, Appendix A error-code table
  (`NDInsight/Developer/MON/Monitor Calls.md`, "APPENDIX A: ERROR MESSAGES"
  starting line 25974; the background-program octal/decimal error table
  specifically starts at line 26076),
- `NC_TOOLCHAIN_MON_PLAN.md` for the few conventions it states as
  authoritative (MOINF `0xF8000000+n`, queue-ordering/non-blocking InByte).

**No emulator was run to produce or revise this spec.** Every value is
either quoted from a source document or is an explicitly-tagged spec
convention; corrections were made by re-reading the manual, not by running
either emulator.

## Version history (see `header.changelog` for the machine-readable form)

- **v1.0.0** - original shape: cases at the `MonContext` boundary
  (`monNumber`, `argAddresses`, `initial`/`final` `{regs, ram}`, `kFlag`,
  `provenance`, `source`, `notes`). No CALLG bytes, no MMU state.
- **v2.0.0** - lifted cases to drive a real CALLG instruction through the
  indirect-segment-31 capability + program MMU decode path: added per-case
  `mmu` and `callg` blocks plus `initial.regs.pc` / `final.regs.pc` /
  `final.regs.l`. This shape was **rejected by the C side** after review.
- **v3.0.0 (current)** - REVERTED to the v1.0.0 `MonContext` boundary shape
  (see "Case boundary" below for the full reasoning) and corrected several
  SINTRAN background-program error codes against Appendix A (some v2.0.0
  codes had been read from the *wrong* Appendix A table, or invented
  outright with no source - see "Error-code corrections" below). Status set
  to DRAFT / NOT FROZEN per explicit C-side request.

## Case boundary (v3.0.0): why `MonContext`, not real CALLG

The C side settled this and it is now binding for this spec: cases target
the **`MonContext`** boundary - the interface nd500x's
`src/cpu/nd500_indirect.c` hands to `mon_dispatch()` after intercepting a
segment-31 CALLG and resolving the operand effective addresses. **ALL** MON
semantics - parameter layout, byte counts, error codes, 32-bit INTEGER
width, file effects - live at or below that boundary.

The two things the `MonContext` boundary does **not** exercise are already
covered by other test suites, so lifting this shared spec to real CALLG
bytes + MMU capability-table state (as v2.0.0 did) bought zero additional
parity coverage for MON semantics while forcing emulator-specific MMU state
into language-neutral JSON:

- **CALLG operand decoding** (opcode/operand-mode encoding, effective-address
  computation for CONSTANT/CONSTANT_SHORT/ABSOLUTE argument operands) - is
  covered by `nd500_tests.json`, the general CPU instruction corpus.
- **MMU translation** (program/data MMU enable, capability tables,
  page-index tables, page-status modes) - is covered by
  `test_mmu_translation`.

The v2.0.0 CALLG-encoding investigation was not wasted: it did surface one
real C/C# disagreement (CALLG argument operands in CONSTANT/CONSTANT_SHORT
mode - C zeroes the address, C# treats the constant as the address). That
disagreement is real but orthogonal to MON semantics and is tracked
separately, not in this spec.

## Two contract points (pinned so both sides build the same `MonContext`)

Both harnesses build the `MonContext` directly - there is no CPU/CALLG step
and no MMU translation layer engaged when replaying a case. Two points must
match exactly on both sides or cases will silently mis-decode:

1. **Flat/identity RAM, MMU OFF.** `argAddresses` in every case are DIRECT
   offsets into that case's `ram` array. A runner MUST NOT apply any address
   translation to `argAddresses` or to any address embedded in `ram`.
2. **All ND-500 INTEGER parameters are 32-bit words.** Every MON parameter
   typed `INTEGER` in its YAML is a full 32-bit word, read/written via the
   equivalent of `mon_read_param_word` / `mon_write_param_word` - **never**
   a 16-bit halfword. A runner that treats INTEGER parameters as halfwords
   will mis-decode every case with more than one INTEGER parameter (the
   second parameter's address would be off by a factor of 2 from what
   `argAddresses` expects).

Both points are duplicated verbatim in `header.contractPoints` in the JSON.

## Top-level shape

```json
{
  "header": { ... },
  "calls": [ { "octal": "1B", "monNumber": 1, "name": "InByte",
               "shortName": "INBT", "parameters": [...], "cases": [...] },
             ... ]
}
```

## `header`

- `schemaVersion` - **`3.0.0`**.
- `status` - DRAFT/NOT-FROZEN marker (see top of this doc).
- `changelog` - v1.0.0 -> v2.0.0 -> v3.0.0 summaries (machine-readable form
  of "Version history" above).
- `caseBoundary` - the `MonContext`-boundary decision and its rationale (see
  "Case boundary" above), plus `notExercisedHere` naming the two other test
  suites that cover CALLG decoding and MMU translation.
- `contractPoints` - `flatIdentityRamMmuOff` and `integerParamsAre32Bit`
  (see "Two contract points" above).
- `sourceOfTruth` - the YAML docs, the Appendix A manual OCR, and the plan;
  plus a v3.0.0 addendum noting the error-code corrections were re-derived
  from the **background-program** octal/decimal table (line 26076+), not
  the earlier **RT-program** decimal-only table (line ~25988) that some
  v2.0.0 cases had mistakenly cited (see "Error-code corrections" below).
- `provenanceDefinitions` - the three tags used on every case (see below).
- `callgConvention` - retained from v1.0.0/v2.0.0 for background: how
  CALLG's argument-address convention and the W1/K-flag error mechanism
  work in general. **Not** the case-execution mechanism any more (see
  `caseBoundary`) - kept here purely as reference/rationale for why
  `argAddresses` and `i1`/K-flag exist in the case shape.
- `determinismConventions` - fixed-clock, terminal-device-number,
  MOINF-fake-address, queue-ordering, string-terminator, and memory-layout
  conventions pinned for reproducibility. See `openItemsNotSettled` for the
  clock-value gap.
- `errorCodeTable` - **new in v3.0.0**. A reference copy of the Monitor
  Calls.md Appendix A background-program error table (decimal 0-114, the
  range touched by any case in this corpus), as `{decimal, octal, meaning,
  line, doc}` entries, provenance `manual`. Use this table - not emulator
  memory, not the RT-program table at line ~25988 - to check or extend any
  case's error code.
- `errorCodeCorrections` - **new in v3.0.0**. Every error-code change made
  in this revision: call, case name, old value, new value, status, meaning,
  and the Appendix A line. See "Error-code corrections" below for the full
  narrative.
- `openItemsNotSettled` - **new in v3.0.0**. The explicitly-not-settled
  items called out during this revision (RFILE short-block EOF inference,
  1B InByte's disputed 57, 317B's open mapping, and the pre-existing
  113B/114B/262B/422B/30B gaps). See "Open items" below.
- `coveredCalls` - the 34 NC calls actually generated in this pass.
- `phase3TodoLinkerCalls` - the ~25-30 additional linker-only MON numbers
  from the plan's item 4 (`162B OUTST`, the disputed `513B`, etc.). **Not**
  generated in this pass - listed only as a TODO manifest.

## `calls[].cases[]`

Each case (v3.0.0 shape - back to the v1.0.0 `MonContext` boundary; no
`mmu`/`callg` blocks, no `pc`/`l` register fields):

```json
{
  "name": "InByte_Console_EmptyQueue_Error57",
  "provenance": "convention",
  "source": "<citation: YAML file + page/line, or Appendix A line, or plan quote>",
  "monNumber": 1,
  "argAddresses": [8192, 8196],
  "initial": { "regs": {"i1":0,"i2":0,"i3":0,"i4":0,"st":0}, "ram": [[addr,byte],...], "console": {"queued": "X"} },
  "final":   { "regs": {"i1":57}, "ram": [[addr,byte],...] },
  "kFlag": true,
  "status": "OPEN",
  "notes": "optional free-text caveat, may include an OPEN-item explanation"
}
```

- `argAddresses` - direct RAM offsets, one per CALLG argument slot (see
  contract point 1 above). CALLG arguments are always *addresses* of the
  parameter cells (ND-05.009.4 EN section 13.7), never values.
- `regs` use `i1`..`i4` (`I[0..3]`) + `st` (status word) only - no `pc`/`l`
  (those belonged to the removed v2.0.0 real-CALLG execution model).
  `i1` doubles as `W1`, the error/return-value register, per every YAML's
  `assembly_500` example (`ERROR: W1 =: ErrCode`).
- `ram` entries are `[address, byteValue]` pairs - identical shape to the
  existing `nd500_tests.json` CPU corpus (byte-level, not word-level), so
  the same JSON-parsing code can read both files. Always flat/identity
  addresses (contract point 1).
- `kFlag` - top-level boolean, the expected K (skip/error) flag after
  `mon_dispatch()` returns.
- `console.queued` (initial) / `console.output` (final) are only present on
  cases that exercise console I/O (MON 1B, 2B, 32B, 503B, 504B).
- `status` - **new in v3.0.0**, optional. Present (`"OPEN"`) only on cases
  whose asserted value is explicitly disputed or ungrounded; absent means
  "settled" (not "untested" - see `notes` for caveats either way).
- A case with no `final.ram`/`final.regs` beyond `i1`/`kFlag` means "no
  other documented effect" - not "untested"; if the manual doesn't state a
  value, this spec does not invent one (see "Open items" below).

## `provenance` tag rules

| Tag | Meaning |
|---|---|
| `manual` | Value/behavior is quoted or directly derivable from a manual page/line cited in the YAML, or from the Appendix A **background-program** error table. |
| `convention` | An agreed cross-emulator convention not stated verbatim in the manual for *this* call - e.g. the MOINF fake-address formula, the error-in-W1/K-flag mechanism itself, an Appendix A error code applied to a specific call by *reasonable but non-exact-name* inference, or a spec-only test-fixture choice (memory layout, terminal device number for the fixture). |
| `yaml-unvalidated` | Taken from a YAML explicitly marked `validated: false` with no manual confirmation found for the specific value asserted, **or** (new in v3.0.0) a case downgraded to K-flag-only because no source grounds a specific numeric value - see `status: "OPEN"` on that case. |

Every YAML file in this batch is `validated: false` (OCR-extraction stage),
so in practice almost every case's *parameter layout* rests on unvalidated
OCR; the tag reflects the provenance of the **case's expected value**, not
the YAML's own validation flag.

**v3.0.0 tag-change rule of thumb**: an error-code case is tagged `manual`
only when the Appendix A **meaning text is an exact-name match** for the
condition under test (e.g. "Bad file number" for a bad-file-number case,
"No file opened with this number" for a not-open case). It stays
`convention` when a documented code is applied to a related-but-not-exactly
-named condition (e.g. reusing "No file opened with this number" for
FSCDNT's "never connected" condition). It becomes `yaml-unvalidated` with
`status: "OPEN"` when no source names any code for the condition at all -
per the project's absolute-honesty rule, an ungrounded numeric value is
never invented; the case keeps its `kFlag` assertion but drops `i1`.

## Error-code corrections (v3.0.0)

Two distinct bugs were found and fixed in this pass, both against
`NDInsight/Developer/MON/Monitor Calls.md`:

**Bug 1 - wrong table.** Monitor Calls.md's Appendix A actually contains
**two different** error-number tables:
- a first table (line ~25988) headed "the following table lists the error
  numbers/messages which may be reported from **RT programs**" - decimal
  numbers only, a **different namespace**;
- the actual table this spec (and both emulators' W1/K-flag convention) is
  built on: "**From background programs**, monitor calls return error
  numbers which are octal numbers" (line 26076), with explicit
  octal/decimal/meaning columns.

Three v2.0.0 cases (64B WarningMessage, 142B ToErrorDevice, and indirectly
321B UEADM via the plan) had cited **decimal 41 "Illegal error code in
ERMON"** from the **first** (RT-program) table. In the correct
(background-program) table, decimal 41 = octal 051 = "User already exists"
- unrelated to either call. Neither call's YAML gives a specific
call-specific code either, so both cases were downgraded to K-flag-only,
`status: "OPEN"`.

**Bug 2 - invented codes.** 321B UEADM's case asserted decimal 52
("deprecated call" convention from the plan). Decimal 52 in the correct
table = octal 064 = "No such friend" - unrelated. `mon-calls-described.md`
(the only source for 321B, which has no YAML) states only "Return with K
flag set", no numeric code. The plan's claim was unfounded; downgraded to
K-flag-only, `status: "OPEN"`, case renamed
`UEADM_Deprecated_KFlagOnly`.

**Corrected but value unchanged (retagged `manual`)** - 9 cases whose
existing numeric value already matched an exact-name Appendix A entry, but
had been tagged `convention` because the *mapping to this specific call*
was an inference: 41B ROBJE (2, "Bad file number"), 43B/62B/73B/76B/412B
(90, "No file opened with this number"), 117B RFILE (86, "Not open for
random read"), 120B WFILE (85, "Not open for random write"), 122B RESRV
(114, "Device cannot be reserved"). These are now tagged `manual` with an
exact Appendix A line citation.

**Already correct, re-cited** - 50B OPEN, 54B MDLFI, 221B CRALF, 256B DEABF
(all "No such file name" / "File already exists" family) were already
tagged `manual` and correct; only the citation was tightened to an exact
line number.

**413B FSCDNT** - value (90) unchanged, citation tightened, tag **stays**
`convention` (reusing "No file opened with this number" for "never
connected via FileAsSegment" is a reasonable but non-exact-name mapping).

**317B UECOM** - was already flagged as the weakest-grounded mapping in the
batch (prior schema doc caveat #5). Confirmed: old value (30 = octal 036
"Bit-file transfer error") is unrelated to an unrecognized command, and no
YAML/manual source names a specific code. Downgraded to K-flag-only,
`status: "OPEN"`, per the plan's own note that this mapping "can graduate
to a concrete code/K-flag assertion once the error-return path is
settled".

**1B INBT (kept, NOT changed)** - the empty-queue case's value (57) was
**not** changed, per explicit instruction to avoid silently changing a
previously cross-emulator-agreed value. It is now marked `status: "OPEN"`
with a note that decimal 57 = octal 071 = "Space not available to expand
file" (unrelated to an empty queue) and the recommended replacement is 3
("End of file"), corroborated by
`NDInsight/Developer/MON/calls/73B_SetMaxBytes.yaml`: "The error code 3 is
returned if you later try to read beyond this size. Error code 3 means end
of file." This requires cross-emulator re-agreement before changing.

## New provenance distribution (after this revision)

| Tag | Count | Change |
|---|---|---|
| `manual` | 27 | +9 (retagged from `convention` once grounded with an exact-name Appendix A citation) |
| `convention` | 8 | -9 (9 promoted to `manual`) |
| `yaml-unvalidated` | 5 | +5 (64B, 142B, 317B, 321B error-code cases downgraded to K-flag-only + OPEN; the pre-existing 30B GetOwnRTAddress case was already `yaml-unvalidated`) |

40 cases total, unchanged. 5 cases now carry `status: "OPEN"`: the four
downgraded-to-K-flag-only cases above, plus 1B InByte's empty-queue case
(value kept, flagged only).

## Open items (do not silently trust these numbers)

1. **RFILE short-final-block EOF signalling** - the C side infers RFILE
   (MON 117B) should signal EOF (decimal 3) on a short final block
   (`0 < bytesRead < numBytes`), delivering the partial data first,
   reasoning RFILE has no residual-count output parameter. This is an
   INFERENCE, not documented in the manual or `117B_ReadFromFile.yaml`.
   **Not** baked into any case in this spec (the existing 117B case tests
   `NotOpen_Error`, a different condition). If a future case touches this,
   it must be marked `status: "OPEN"` with a note describing the
   disagreement, not asserted as fact.
2. **1B InByte empty-queue error code** - see "Error-code corrections"
   above; kept at 57, flagged OPEN, recommended replacement 3, pending
   cross-emulator re-agreement.
3. **317B ExecuteCommand error mapping** - left OPEN (K-flag only); can
   graduate to a concrete code once the C side settles the error-return
   path (Phase 3).
4. **113B GetCurrentTime / 114B GetTimeUsed** - only the buffer *layout* is
   manual-grounded; date/time values remain placeholder zeros pending a
   fixed-clock test-hook decision.
5. **262B GetSystemInfo** - the 24-byte buffer's CPU-type/version/patch
   contents are not asserted (no manual-fixed test values found); only
   "call succeeds" is checked.
6. **422B GetScratchSegment / 30B GetOwnRTAddress** - the concrete
   segment/RT-description-address values are emulator-internal allocation
   state, not manual facts; not asserted.
7. Only 1-3 cases per call were generated (nominal-success + the most
   clearly-documented error/boundary case). Broader coverage is left for a
   follow-up pass.

## Regeneration

This spec was originally produced, and this revision applied, by throwaway
Python scripts (not committed - scratch tooling, not project source, per
the "no standalone test programs, clean up" project rule). The
properly-owned generator still belongs in `Emulated.Tests.ND500` (C#, per
the plan's "Division of labor") and should:

- read the same YAML files directly (no hand-transcribed parameter tables),
- read Appendix A's **background-program** octal/decimal table
  programmatically (not the RT-program table, and not a hand-picked `ERR`
  dict) - this is exactly the bug this revision fixed by hand,
- regenerate `mon_tests.json` byte-for-byte reproducibly from source,
- resolve the `openItemsNotSettled` items above only once both emulator
  teams explicitly re-agree - never bake in a "recommended" value silently.
