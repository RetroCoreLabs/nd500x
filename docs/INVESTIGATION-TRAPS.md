# Investigation traps on the ND-5000 / octobus lane

Measured failure modes of INVESTIGATIONS on this lane, with the controls that
catch each one. This is not a status document and it records no open items;
`docs/CURRENT-PLAN.md` owns status. Nothing here goes stale when the code moves.

It exists because the same handful of mistakes have been made repeatedly across
sessions, each time costing hours, and each time looking like a defect in the
machine rather than a defect in the measurement. The counts below come from
reading the session transcripts for this project: 62,998 recorded messages, of
which 70 touch an octal/decimal confusion, 31 a measurement whose silence was
read as evidence, 27 a stated assumption that was later refuted, and 16 a read
of the wrong tree or file.

Order of authority for the facts themselves is in `docs/EXTERNAL-ARTIFACTS.md`.
This file is about METHOD.

---

## 1. Radix. Three number systems share these files and these logs

The most expensive trap by count, and it has produced wrong conclusions that
then survived several rounds of analysis.

Three notations are in play at once:

| Notation | Example | Where it appears |
|---|---|---|
| Hex | `0x8E30` | this emulator's logs and source |
| C octal | `0144` | C source literals - **a leading zero is octal** |
| ND octal | `46B`, `102B` | SINTRAN and monitor output, ND manuals, carve notes |

Two measured incidents:

- A per-segment descriptor stride was written `0100u` in C - octal, 64 decimal -
  on the line directly below a comment that said "100 bytes". The dump then read
  every descriptor at the wrong address and reported them as absent. The carve's
  own anchor settled it: the table's stride is 100 DECIMAL, because
  `0x08038000 + 14*100` is the documented address of the PSN 14 entry.
- `LIST-SEGMENT-TABLE-ENTRY ALL` prints physical segment numbers in ND octal.
  Its `15B` entry was read as decimal 15 and the real subject of the
  investigation - decimal 13 - was believed to be absent from the table. It was
  present, in use, and 102B pages in size. A whole line of analysis
  ("a fresh unbacked segment that must grow") was built on the misread.

**Controls.**

- A leading zero in a C literal is octal. Prefer a decimal literal with the
  octal value in the comment, never the reverse.
- SINTRAN reads NUMERIC PARAMETERS as octal and echoes them with a `B`. It will
  answer fully and confidently about the object you did not ask for; the `B` on
  the echo is the only signal. Prefer a command that identifies its subject by
  NAME - `LIST-SEGMENT-TABLE-ENTRY ALL` over a numbered query.
- When a log line carries an ND quantity, print both radices:
  `psn=13 (15B)`, `trap=38 (46B)`. A single radix in a log is a future misread.

---

## 2. A measurement that cannot observe the positive is not a measurement

31 recorded instances. The shape is always the same: an instrument reports
nothing, and the nothing is read as a fact about the machine.

Three distinct causes, all measured on this lane:

- **A spent budget.** A `static unsigned n; if (n++ < 40)` diagnostic exhausted
  its budget 95,000 log lines before the process under investigation started.
  Its silence was read as "this code path never executes".
- **The wrong base.** A descriptor field was read at an offset that was correct
  within a structure, from a base that was the wrong structure. It returned
  `0x0000` - a confident, plausible, wrong number. This is the worst variant,
  because unlike the others it does not look like a failure.
- **The wrong moment.** A table dump gated on "the first two monitor calls"
  sampled before the table was populated. Every entry read zero, including the
  entries that demonstrably work.

**Controls.**

- **Run the positive control first.** Point the instrument at a case that is
  known to work. If it is silent there, the instrument is broken and a silent
  run on the real subject proves nothing. In the descriptor case the control was
  decisive: `word0` read 1, 11 and 4 for three segments whose sizes the monitor
  independently printed as `1B`, `13B` and `4B`, which is what made a zero for a
  fourth segment a real absence rather than a bad address.
- **A bounded diagnostic should say when it stops.** A budget that expires
  silently converts itself into a false negative. Print one line on exhaustion.
- Gate on the ADDRESS or the SUBJECT, never on a global count, when the
  interesting event is by definition the one a count stops printing.

---

## 3. Two processes share one CPU structure. Say which one

From `PLACE-DOMAIN` onward the swapper and the domain are both live, each with
its own message block, and both run through a single CPU state structure in the
bridge. A trace that prints a PC and nothing else cannot say whose instruction
stream it is.

Measured: the swapper's entry sequence `0x04 -> 0x11 -> 0x14 -> 0x16` was read
as the domain's. It is the swapper's own correct stream - a 13-byte `init`, then
`move`, `stz`, `comp2` - and reading it as the domain's produced an elaborate
theory about a CALL instruction decoding wrongly. No such defect exists. The
decoder was confirmed correct by both disassemblers and by executing the
instruction offline.

**Control.** Every per-process log line carries the process number. Adding `PS`
alone is not enough, because two processes can share a `CED`; the process number
is what disambiguates.

---

## 4. The two write paths into the shared pool

A watch on one path cannot see the other, and the difference decides who wrote a
cell.

| Path | Function family | Who uses it |
|---|---|---|
| The ND-500 CPU's own stores | `nd500_bus_write8` into the machine's memory, which IS the pool | ND-500 code: the swapper, a domain |
| The setters | `ndbus_pool_write8/16/32`, `ndbus_pool_write_bytes` | the ND-100 window, the mailbox, the servicer copy engine, the lock, the context |

`NDBUS_POOL_WWATCH_BYTE` watches the SETTERS only. Measured consequence: a watch
on a segment descriptor recorded two writes, both zeroing, while the descriptor
ended up populated - because the populating writes were the ND-500 CPU's own
stores and the watch never saw them. Read correctly, that is a finding: the
swapper builds its own segment descriptors, and the ND-100 does not write them.

**Control.** Before concluding "nothing writes this cell", confirm which of the
two paths the watch covers. A cell that changes with no write logged is the
signature of the other path, not of a missing writer.

---

## 5. A program that stops is not necessarily a program that failed

A monitor call, a trap and a stop look alike from outside, and the monitor's own
console text is not a verdict.

- The ND-500 side reports nothing for some failures. The monitor can print a
  trap report composed from fields nobody set: `NOT KNOWN TRAP` at
  `program address: 0 0B` is the signature of an unfilled record, not of a trap.
- Conversely a correct-looking fault report can be about a fault the reporting
  process never took. A page fault was delivered twice - once from the machine's
  stop fields and once from the still-pending trap state on the next step - and
  the second delivery landed on the wrong process's message, which SINTRAN
  correctly read as a fatal error in the swapper.
- A monitor call that returns to the next instruction was answered INLINE. One
  that parks the process was forwarded. Those are different lanes and only the
  second is the real path; see `docs/SINTRAN-CONVENTIONS.md` and section 6.

**Controls for "did this program actually run".** Each program's `userguide.md`
in `$NDINSIGHT/SINTRAN/ND500-APPS/<NAME>/` states the exact output it produces.
Check three things, never the presence of some recognisable text:

1. `MON 0B ExitFromProgram` reached. `MON 00B` is a different and undocumented
   thing and is not an exit.
2. The console text matches the guide's field list IN FULL. A banner is not a
   run; two labels and a blank value is a failed run.
3. The process segment is the domain's, not the swapper's.

---

## 6. On this lane the attached machine owns every monitor call

`ndmonlib` is the SIMULATED SINTRAN, for a build of this emulator with no real
machine beside it. When an ND-5000 runs next to an nd100x booting the genuine
SINTRAN, that SINTRAN performs every monitor call over the mailbox and the local
emulation must answer none of them. A locally answered call produces a plausible
reply that no machine ever produced, and the program then proceeds on state that
cannot be reproduced or trusted.

The host hook in `src/cpu/nd500_indirect.c` takes precedence over the local seam
and a decline does not fall through to it. Both gates test for an attached host,
so a standalone build is unaffected.

**Control.** Count the forwarded calls. A run whose console output is produced
with ZERO monitor-call round trips was answered locally and does not count as
evidence about the real path.

---

## 7. Read the artifact, not the memory of it

16 recorded instances of reading the wrong tree or the wrong file.

- There is more than one checkout of the C# emulator on some machines. Only the
  one named by `$RETROCORE` is current. A stale sibling tree has been read and
  quoted as the oracle.
- **Status headings in the sibling trees and in older notes lie.** A documented
  audit there found eleven items still headed OPEN that were already fixed. The
  pattern is structural: a fix gets written up where the work happened, never in
  the list that sends the next reader there. Before investigating anything
  marked open, grep the code and the tests for it.
- Material that answers questions on this lane and is easy to miss:

| What | Where |
|---|---|
| Per-program requirements, invocation and EXPECTED OUTPUT | `$NDINSIGHT/SINTRAN/ND500-APPS/<NAME>/userguide.md` |
| MON call parameter and return contracts, per call | `$NDINSIGHT/Developer/MON/calls/*.yaml` |
| MON oracle write-ups, including `422B GSWSP` | `$NDINSIGHT/SINTRAN/ND500/mon-oracle-for-NC/` |
| Swapper message field dossier, per `SWPFU` function | `$NDINSIGHT/SINTRAN/ND500/N5SWAP-SWMSG-FIELD-DOSSIER-RELAY-2026-08-17.md` |
| SINTRAN's own source for the swapper decode | `$NDINSIGHT/SINTRAN/NPL-SOURCE/NPL/MP-P2-N500.NPL` |

The carve and the manuals outrank both emulators. `docs/EXTERNAL-ARTIFACTS.md`
has the full index and the order of authority.

---

## 8. The bring-up is three commands

Typing more than the documented sequence has made things worse, measurably.

```
DEFINE-SWAP-FILE SWAP-FILE:DATA     the only required step; lost on a cold start
PLACE-DOMAIN <name>                 loads the control store, loads the swapper,
                                    allocates memory - all by itself
RUN
```

`START-SWAPPER` is not part of it. On this lane it has been recorded as hanging
the monitor outright, and the long bring-up has been recorded as breaking a
program that runs under the short one. When the monitor misbehaves, the first
thing to try is not typing the command.

A SINTRAN command with a missing parameter PROMPTS, and the prompt eats the next
scripted line. A driver that sends `LIST-FILES x,` has its following command
swallowed and gets an answer about nothing. Give prompting commands their
parameters, or do not send them.

---

## 9. Every observation on this lane costs a full boot

One session produced 132 driver scripts and 251 logs, because each measurement
required booting SINTRAN to the monitor prompt first - roughly eight minutes per
answer. That cost is what turns a two-minute question into an hour, and it is
what makes the traps above expensive rather than merely annoying.

Two consequences worth planning around:

- **Put every instrument you might need into one run.** The marginal cost of an
  extra log line is nothing; the marginal cost of another run is eight minutes.
  A run that answers one question and raises two is a bad trade.
- **Capture the pool instead of re-running the boot.** The expensive part is the
  boot, not the subject, and everything this lane argues about is in the shared
  pool: the mailbox, the message blocks, the physical segment table, the Domain
  Information Tables and the swapper's segment descriptors.

  Capture, from the nd100x checkout which owns the bridge:

  ```
  MFBUS_SNAPSHOT_PATH=<file> MFBUS_SNAPSHOT_AT_MON=<n> ./build/bin/nd100x ...
  ```

  `MFBUS_SNAPSHOT_AT_MON` is the monitor call to capture at, counted per CPU.
  The run logs the file it wrote, or logs that the write failed - it does not
  fail silently.

  Read it with `make diag && ./build/bin/diag_pool_snapshot <file> [desc_pa
  [first_psn [msg_byte [pstp_pa]]]]`, which decodes the segment descriptors, a
  message block and the physical segment table. A snapshot from a pool of a
  different size is REFUSED rather than misread.

  MEASURED: a capture at monitor call 15 of a `PLACE-DOMAIN CPU-STAT` run
  reproduced the descriptors and the PST exactly - psn 11 holding 11 pages,
  psn 12 holding 4 with STATE 7, psn 13 empty, `PST[12]=0x40000FF6`,
  `PST[13]=0` - in 7 milliseconds. The live run that first produced those
  numbers took eight minutes.

  It snapshots the POOL only. The ND-100's own memory, registers and disk are
  not in it, so a question about SINTRAN's side still needs a boot.
