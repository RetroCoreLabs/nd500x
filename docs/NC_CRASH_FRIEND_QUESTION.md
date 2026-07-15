# Question for a working NC: is one specific heap free legitimate?

**Full path of this document:** `/home/ronny/repos/nd500x/docs/NC_CRASH_FRIEND_QUESTION.md`
Date: 2026-07-14

## TL;DR — the one thing I need from a working NC

When NC (the ND-500 C front end, `nc-a06.dom`, "Norsk Data C - Version: A06 - 1989-01-10")
compiles a trivial program, my emulator hits a **double-free** of one heap cell and then a heap
overlap. I have traced it to a single decision I cannot judge without a reference:

> **Does a *working* NC free the heap cell that its "reader" routine captured from the scratch
> mailbox, while the reader is still holding a pointer to it?**

If a working NC does **not** free that cell at that point, then something upstream in my emulator
is triggering a free that should not happen (my bug). If a working NC **does** free it there and
still compiles fine, then the real machine simply never re-used the freed memory in time, and the
fix belongs in my allocator, not the free decision.

Everything below is the evidence, the exact addresses, and the concrete checkpoints so a working
NC run can be matched instruction-for-instruction. All code addresses are in the shared
`nc-a06.dom` image, so PCs line up on any emulator running the same binary.

---

## The setup (so your run matches mine)

- Binary: `nc-a06.dom` (the same A06 image).
- Command: `COMPILE B,B,B` where `B:C` (host `B.C`) is:

  ```c
  #define VALUE 42
  int x;
  main()
  {
   x = VALUE;
  }
  ```

- Pristine working directory: only `B.C` (and `A.C`) present in `GUEST`; empty `SCRATCH`. No
  leftover `B.CAT`/`B.LIST`/`B.NRF` from a previous run (leftover output files change the run).
- Clock pinned deterministic. My run reaches the fault at ~1.487 million instructions.

The compile does **not** succeed on my emulator: it aborts before writing real `:NRF` object code.

---

## The failure, end to end (all verified on my emulator)

1. **A heap cell `0x1802A1B0` is allocated, used as a small record, and freed legitimately** at
   instruction **1,326,719** (pushed onto the 8-byte free list, FLOG1).

2. **A "reader" routine at `0x08005024` had captured a pointer to that cell.** The reader reads
   the scratch mailbox global `0x08000200` into a local (at `0x0800503E`), then refreshes the
   mailbox to a fresh cell (at `0x08005106`). So it holds an *old* copy of the pointer in its
   local `b.14` and keeps running for ~84,000 instructions.
   - Note: the mailbox `0x08000200` itself is updated correctly (it moves off `0x1802A1B0` to
     `0x18003000` at instruction 1,323,358, *before* the free). The stale pointer is the reader's
     **captured local**, not the mailbox.

3. **The producer path frees `0x1802A1B0` (step 1) while the reader still holds it.** This is the
   decision in question.

4. **~84,000 instructions later the reader consumes its stale local** and passes it to a
   list-merge/concat routine at `0x08024829`, which **frees `0x1802A1B0` a second time** at
   instruction **1,407,063** — a **double free**.

5. The second free reads the block's size from the block's own first halfword. But the cell was
   already free, so its first word now holds a free-list link (`0x1802A1B8`); the high half
   `0x1802` (= 6146) is read as the size → `ceil(log2(6146)) = 13` → the 8-byte cell is pushed
   onto the **32 KB** free list (FLOG13).

6. The buddy allocator (`GETB`) then hands out a **32 KB block at `0x1802A1B0`** that **overlaps
   eight smaller still-free blocks** (FLOG1..FLOG12). The overlapping block is written as a record;
   a pointer field ends up as garbage `0xA1B8A1A8` (two heap half-addresses spliced together).

7. Dereferencing `0xA1B8A1A8` traps (protect violation) at PC `0x08023EA1`/`0x08023EA4`. NC's own
   trap handler (`THA[36] = 0x0802D817`) then runs and exits via `MON 0B` — a clean give-up, not a
   compile.

So the whole catastrophe grows from the **double free in step 3/4**, which grows from the
**producer freeing a cell the reader still references in step 3**.

---

## The exact call sites (shared binary — match these PCs)

| What | Address |
|---|---|
| Reader routine (captures the pointer, refreshes mailbox) | `0x08005024` (reads mailbox at `0x0800503E`, refreshes at `0x08005106`) |
| Scratch mailbox global | `0x08000200` |
| List-merge / concat (frees its input records) | `0x08024829`; free calls at `0x080248D7` (input A) and `0x080248E5` (input B) |
| Free wrapper (size-bucketed push) | `0x0802CEB3`; the actual free-list push store is `0x0802CEFE` |
| Free-list head table (FLOG0..) | `0x0801D544` (= heap-vars/TOS `0x0801D538` + 12) |
| Buddy allocator instruction wrapper | `0x0802CF08` (the `GETB` at `0x0802CF60`) |
| The two frees of cell `0x1802A1B0` | instr **1,326,719** (legit, FLOG1) and instr **1,407,063** (double free, FLOG13) |
| Fatal deref | `0x08023EA1` `by test r1.0`, r1 = `0xA1B8A1A8` |

---

## An EASY check first (added 2026-07-14): does your NC even parse a trivial file?

Before the internal free trace, there is a much simpler, user-visible symptom. On my emulator
**every** program fails the same way (10+ K&R samples tried; none produce a non-empty `:NRF`), and
the listing shows NC **rejecting even a trivial valid declaration**:

```
Norsk Data C - Version: A06 - 1989-01-10
*FILE*
    1.  nt x; main(){ x = 1; }        <-- source on disk is "int x; ..."; NC's buffer shows "nt x"
           ^
*ERROR*   at m.1    error in SIMPLE_DECLARATOR
*MESSAGE*           IDENTIFIER      deleted
***   1 error  detected  ***
```

Two things to check on your **working** NC with the same one-liner `int x; main(){ x = 1; }`:

- **Q0a.** Does your NC compile it cleanly and write a non-empty `T:NRF`, or does it also report
  `error in SIMPLE_DECLARATOR` / `IDENTIFIER deleted`?
- **Q0b.** In your listing, is the source line intact (`int x; ...`), or is the first character
  altered/dropped as in mine (`nt x`)? A corrupted source buffer would point at the file read /
  preprocess path rather than the parser.

If your working NC also rejects `int x;`, then the syntax/invocation is the issue (and the heap
crash is just the error-recovery path blowing up). If your NC compiles it fine, then my emulator
is corrupting NC's source/symbol handling early - which is consistent with the heap double-free
below reaching NC's tables. Either answer narrows it hugely.

Instruction counts to fault scale with program size on my side (1.48M for `int x;` up to 3.23M
for a K&R function with args), so NC does real, increasing work per program before dying - it is
not failing instantly.

## The precise questions (in priority order)

**Q1 (the decisive one).** On a working NC compiling the same `B:C`:
   - Does the routine reached via the producer path (the concat `0x08024829`, or whatever frees
     that record on your run) **free the cell that the reader `0x08005024` captured from the
     mailbox**, at the equivalent point (~instr 1,326,719 on mine)?
   - Or does your NC **not** free it there (leaving the reader's captured pointer valid)?

**Q2.** Does your NC's reader `0x08005024` ever **consume the same pointer twice** — i.e. is the
   node it captured from the mailbox ever freed once and then referenced/freed again? Or is each
   node freed exactly once on a correct run?

**Q3.** If your NC *does* free it once and never double-frees, what is different upstream? Two
   concrete candidates I would check:
   - the value the reader captures from mailbox `0x08000200` at `0x0800503E` (does yours capture
     `0x1802A1B0` there, or a different/newer node?);
   - the size the free wrapper `0x0802CEB3` computes for the block (does yours ever bucket a small
     cell onto a large free list because it read the size from an already-freed cell?).

**Q4 (sanity).** Does your working NC actually reach code generation and write a non-empty `:NRF`
   for this program, or does it also stop at the front end? (This tells me whether `nc-a06.dom`
   alone is expected to produce object code, or whether a separate back-end/driver is required.)

---

## What each answer tells me

- **Working NC does NOT free that cell there (Q1)** → my emulator is producing an upstream value
  that makes NC take a free-then-reuse path it should not. I then bisect what differs before instr
  1,326,719 (most likely a wrong instruction result or a wrong SINTRAN service value feeding NC's
  list bookkeeping). This is the outcome I expect and can act on.

- **Working NC DOES free it and still compiles (Q1 + Q4 yes)** → the free is legitimate and real
  hardware simply never re-used the freed memory before the reader was done. The fix is then in my
  allocator/free-timing (e.g. do not re-hand-out or re-bucket a cell while a live reference remains
  within the same logical operation), not in the free decision.

- **Working NC also stops at the front end (Q4 no)** → then `COMPILE` alone is not the right
  invocation and the "failure" is partly expected; I need the correct two-phase or driver recipe.

---

## Not useful (already ruled out on my side)

- Comparing against my *other* emulator is worthless here — both are the same codebase kept in
  sync, so they share this exact behaviour. Only a genuinely working NC (real hardware or a setup
  that actually emits `:NRF`) can answer Q1.
- The scratch mailbox `0x08000200` is managed correctly; the stale pointer is a captured local.
- The heap segments (from `MON 422B GSWSP`) are mapped correctly and do not overlap.
- The `GETB` buddy-allocator instruction and the free-list structure are shared (`TOS+12` ==
  `0x0801D544`); it is not a two-heaps problem.
