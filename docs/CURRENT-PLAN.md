# CURRENT PLAN

One page that answers "what are we doing and what is the plan" without
having to ask a session that may have compacted it away. UPDATE THIS AT
EVERY MILESTONE: goal changed, phase finished, investigation opened or
closed, dead end proven. Keep it short; details belong in the referenced
docs.

This is deliberately a living status file inside `docs/` (an exception to
the "working notes live in `$NDIX/notes/`" rule) so any session finds it
cold.

Last updated: 2026-10-03.

## Standing goal

Run the real ND-500 vendor toolchain under nd500x: C source -> NC ->
CAT-500 -> :NRF -> ND Linker -> :DOM that runs and exits cleanly. This
end-to-end pipeline was first achieved 2026-07-20 and is the regression
bar: it must keep working.

## Current work: the ND-5000 over the octobus

THE GOAL OF THIS PHASE, in one sentence: get SINTRAN's ND-500/5000 MONITOR J04
through `START-SWAPPER` on the nd100x machine with an ND-5000 station attached,
and then run DOM programs on it.

`START-SWAPPER` REACHED 2026-10-03. It prints `> Loading Control Store`,
`> Loading Swapper`, `> Allocating memory - 7342B pages` and returns to the
prompt with no trap and no fatal error. `PLACE-DOMAIN CPU-STAT` does the same,
and `VERSION` matches the working RetroCore machine line for line. The phase is
NOT finished: `CPU-STAT` itself still produces no output, which is the live
blocker below.

The octobus carries no data (ND-05.020.01 ch. 5.3). It carries commands; the
DATA and the whole mailbox live in the MPM-5 shared pool. That one fact decides
the shape of everything below.

SCOPE, set by Ronny: port OCTOBUS and its dependencies. The ND-500 machine
interface (`NDBusND500IF`, the PCB 3022, `LMAR5`/`LCON5`/`MICFU`) is NOT ported
and nd100x has no 3022. Consequences that are correct and not gaps: the servicer
carries only the octobus transport arm, so links resolve by identity against a
pool-relative byte offset rather than the 3022's word `<< 1`.

Where the boot now reaches, measured 2026-09-30 on a live run:

- `DEFINE-MEMORY-CONFIGURATION` with base page 04100B and one 10000B part
  succeeds, and `MEMORY-CONFIGURATION` matches the RetroCore PART table field
  for field. `PAGES FOR SWAPPING` went 3266B -> 13245B.
- `LOAD-CONTROL-STORE` completes: 128 LOCSM pulses fill a real 16384-microword
  store, the DUCS read-back checksum agrees, and the monitor verifies four
  control-store spots instead of dying at the first.
- The monitor then runs `DISKICK -> STOPMIC -> STARTMIC(0x36) -> ENKICK(0x31)`
  and STOPS TALKING ON THE OCTOBUS. Past that point it polls the mailbox.
- `START-SWAPPER` reaches `> Loading Control Store` and `> Loading Swapper`.
  Three earlier failures are gone, in this order, each removed by a named port:
  `ND-500(0) timeout` (the mailbox servicer plus the nd100x-side poll),
  `NOT KNOWN TRAP At program address: 14 400004000B` (answering 12B CACHE), and a
  blank swapper version (the copy family, 13B/14B/30B/31B/35B/10B/11B).
- `VERSION` now prints `Swapper.......: REV.-K01`, which is exactly what the same
  command prints on the working RetroCore machine. It was blank before the copy
  family was ported, because SINTRAN reads that string out of the swapper image
  with a copy-family transfer.
- `START-SWAPPER` now COMPLETES, 2026-10-03: `> Allocating memory - 7342B pages`
  and back to the prompt. Five further failures were removed after the three
  above, each by a named port with a test that fails when reverted:
  `86c77be` the five data-returning ACCP commands; nd100x `90307b0` octobus
  frames PARKED for an Ack=10 busy retry instead of dropped on a full 16-word
  FIFO (that drop truncated RECO's reply and the monitor issued an emergency
  244B TERMINATE ACCP); `9a46727` ALIVE answered from the microprogram
  flip-flop; `a9406bc` BMOVE no longer aborting on the host's run_flag, which
  had made the swapper's own copy write nothing; `ba28998` one physical segment
  table instead of two.
- THE LAST TWO, 2026-10-03, both on the start path and both measured on a
  `START-SWAPPER` run with nothing else in it: nd500x `7b38055` and nd100x
  `1eb0f63`. 3MONCO and 3TRACO resume the loaded process; 3START never does.
  A monitor-call stop leaves the ND-500 through `CALL_MON -> SET_IDLE`, which
  marks "no current process", so the B30 IDLE loop at 0o24724-25 skips
  CNTXTSAVE and NEWCNTXT/CNTXTLOAD reads the block SINTRAN has just filled -
  the entry point 0x08000004, not the parked MON return address 0x08008255.
  Our resume arm had no test on the micro-function and swallowed the second
  3START; correcting that exposed a second defect, a context load refused
  because the parked runner sat at STOPPED rather than the IDLE that a load
  requires. Before these two the monitor printed `ADDRESS OUTSIDE PROGRAM
  SEGMENT / NOT KNOWN TRAP / At program address: 0 1B` with no trap reported
  from the ND-500 side at all.
- WHERE IT STOPS NOW: `CPU-STAT` prints nothing. Under investigation; no cause
  stated yet.
- TPE CONFIGURATION D05 passes `NO ERRORS DETECTED`; TPE OCTOBUS tests 1-3 pass.

## Next steps

In the order they are worth doing. Number 1 is the live blocker.

1. WHY `CPU-STAT` PRODUCES NO OUTPUT. The swapper now runs, so this starts from
   a working swapper rather than a broken one - which is why the earlier reading
   of it was wrong. Open, no cause stated. The oracle's measured sequence is the
   thing to compare against, `Nd500MicrocodeServicer.cs`: from PLACE-DOMAIN
   onward TWO processes are live, the swapper and the domain, each with its own
   message block, and it records `MICFU=0013 X5CPU=1` as the 3START of the
   DOMAIN followed by a trap-stop on the DOMAIN's block. Our two blocks are
   0x00C130 for X5CPU 0 and 0x008E30 for X5CPU 1.
2. WHATEVER ARRIVES NEXT. Every unported code answers 5ERANSWER(4), is counted
   in `NdbusServicer.micfu_counts` and is logged once per code, so the next one
   to port is a measurement. Do not port ahead of it.
4. THE ND-5000 SELFTEST still fails its `0B...BUS test` and `1B...MIR test`
   during `DEFINE-MEMORY-CONFIGURATION`. `RMIR` sits in the `default:` arm of the
   ACCP command dispatch. Separate from the timeout and reached earlier.
5. TPE OCTOBUS tests 4-6 abort "No Domino controllers are present". The gate is
   inside TPE at `cmd_select_octobus_station @ ram:7be2` and no document decodes
   it. nd500x's presence reporting is provably correct (station 56 NotPresent
   clear, the other 60 report 0o130), so this needs the TPE code disassembled,
   not more emulator changes.
6. The nd500x debugger cannot read the shared window - `cpu_mms.c:1489-1492` and
   `:1406` in nd100x carry the same missing bank check the MMU fix corrected.
7. nd500x Tier A gate unfinished on `cleanup/step2-warnings`: warning sites in
   `src/`, tests unlinked, CI jobs never added.
8. `SYNC-BACKLOG.md` now carries the six 2026-10-03 rows (`10b8bf6`); the
   earlier octobus commits still have none.

Absent from the octobus closure, by count of C# lines, if any of it turns out to
be needed: `AccpOctobusStation.cs`, `Nd5000AccpAttachment.cs`,
`NucleusStructures`+`NucleusClient`, `OctobusScsiDiocStation.cs`+`Bdio*`.

## Open, and NOT to be invented

- What the header words at pool bytes `0x8820` and `0x8822` are. SINTRAN spins on
  those two plus X5PRO at `0x890C` after ENKICK. Neither is in this repo's
  mailbox layout and neither was found in RetroCore. UNKNOWN.
- X5SEM'S FREE VALUE IS NOT WHAT THIS REPO DOCUMENTS. `ndbus_mailbox.h` states,
  from XMSINIT, that X5SEM is free at 0 while the three per-CPU cells are -1.
  MEASURED 2026-09-30 on a live boot: at answer time the cell at pool byte
  0x8800 holds 0xFFFF, consistently, on all 20 answers of a run. A test-and-set
  that succeeds only on 0 therefore never takes it, and every answer is written
  unlocked. What is NOT known is which of two things that means: SINTRAN uses -1
  as X5SEM's free value too, or the ND-100 genuinely holds the semaphore across
  the whole sequence. RetroCore cannot arbitrate it - its `TryTakeSemaphore` also
  requires 0, so it fails the same way and logs it only in a debug build. DO NOT
  change the free-value convention on the strength of the reading alone; find the
  writer of that cell first.
- Whether answering 3RMICV alone releases the post-ENKICK spin. PARTLY ANSWERED:
  the timeout is gone with 3RMICV and 12B CACHE answered and no X5PRO claim, so
  a claim on X5PRO is not required to clear it. Whether the `0x8820`/`0x8822`
  spin is fully satisfied or merely no longer reached is unverified.
- Whether the MPM-5 window is recorded as KMPM5 (0x04) or KMECCR (0x08).
  `MBMEMARRAY` at ND-100 word `0xA94` reads `0x0006`, which is a selector and not
  the array address.

## Earlier work, still the regression bar (toolchain phase)

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

### Toolchain leftovers, lower priority than the octobus phase

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

### ND-5000 / octobus phase

- THE DEFINE-MEMORY-CONFIGURATION PARAMETERS WERE NEVER WRONG. `4100` /
  `10000B` / `YES YES YES` is exactly right, proven against Ronny's working
  RetroCore `MEM-CONF` output. Six combinations were tried and all six failed
  identically, and the failure was claimed to be a parameter fault three times
  before the real cause was found. Do not re-test parameters.
- `GIVE-ND-500-PAGES` is not the missing step and does not exist in MONITOR J04 -
  `HELP GIVE-ND-500-PAGES` prints nothing. The error is raised by
  `DEFINE-MEMORY-CONFIGURATION` itself.
- The real cause of `No memory available for ND-500(0) buffers` was in nd100x's
  MMU: the out-of-range test compared the physical address against local RAM size
  instead of asking the memory-bank table, so every MMU-translated access into the
  MPM-5 window trapped MOR. Fixed by asking `mms_memory_bank_lookup()`, which is
  what RetroCore's `SystemBus.IsAddressMapped` does.
- The login regression that fix exposed was a SECOND bug in the same path:
  nd100x's mfbus bridge took the MSB of a half-word write from `value >> 8` while
  every caller puts it in the LOW 8 bits, so every even-byte write into the shared
  window stored 0. Its unit test had encoded the same bug and asserted `0xEE00`.
- The "contiguity" theory for the 128 KB hole at bank 32 from base page 04100B is
  DISPROVEN: RetroCore has the identical hole and works.
- `SSPTM` is NOT unimplemented in nd100x. It is `STS_PAGE_TABLE_MODE` in
  `cpu_types.h:244`. That claim came from grepping the mnemonic instead of the
  concept.
- The byte-packing/checksum hypothesis for the control store was wrong because
  there was no control-store code at all to be wrong: LOCSM (0x13) and DUCS (0x15)
  fell into the `default:` arm and acked without moving a byte, so SINTRAN summed
  a stale parameter field.
- `0xBE30` - the value SINTRAN writes into X5BEX - is a real QUEUE HEAD NODE, not
  a free-list entry and not the message. Its N5STA is 0, which is why reading it
  as the message makes the walk look broken; its LINK points at the actual
  message. Settled from `$RETROCORE` `IServicerHost.ResolveMailboxLink` ("Verified
  octobus 2026-07-21") and the `WalkQueue` diagnostic, which names the head node
  and the real message separately. A chain walk that refuses N5STA != 1 and then
  follows the link handles it correctly.
- X5FIF IS A BYTE OFFSET, not a word address, and so are X5BEX and every LINK.
  A word address puts every ring slot at half its true offset, which lands back
  inside the mailbox structure and corrupts rather than faults.
- X5ACT's re-armed value is 1, NOT -1. XMSINIT writes -1 once; every re-arm after
  that writes 1. Any doorbell sniff with a repeat threshold above 1 can therefore
  never latch.
- RETROCORE HAS NOT SOLVED THE ANSWER SIGNAL EITHER, and its code says so: its
  GIVEINT frame computes to destination station 0 on its own configuration, its
  fabric drops station 0 silently, and its own note reads "737 answers sent, 0
  delivered" with SINTRAN then finding each answer by timing out at a fixed
  16.243M ND-100 instructions. Both of its workarounds are environment-gated and
  labelled "THIS IS NOT A FIX AND MUST NOT BECOME ONE", so neither was ported.
  The arithmetic was ported; the workarounds were not. Do not treat a green
  RetroCore run as evidence that this path works there.
- A guard definition that is PRIVATE to one CMake target does not reach another.
  `ND100X_WITH_ND500` was defined only for nd100x's `machine` library, so the
  mailbox poll added to the `devices` library compiled to nothing and the machine
  would have timed out exactly as if the poll had never been written. Check the
  generated `flags.make`, not the CMake source.

### Toolchain phase

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

## ND-5000 octobus lane: where the two emulators part company (2026-10-04)

A differential run of the reference emulator's
`ShortBringup_Octobus_NoStartSwapper_PlaceAndRun_Capture` against the same
bring-up on this lane. Both reach the SAME instruction before diverging, so
the divergence is narrow and named.

### CORRECTION 2026-10-04: the inline buffer is NOT the string corruption

The section below says the missing inline user buffer "closes the
string-corruption hunt". It does not, and the claim is withdrawn.

MEASURED after the fix landed: a full PLACE-DOMAIN + RUN +
LIST-ACTIVE-PROCESSES run performs ZERO MON 504B, 511B or 512B calls, and the
process name still prints as "(YSTSMS S S S S S)TERMINAL-1". So the inline
copy cannot be what corrupts it - LIST-ACTIVE-PROCESSES is an ND-100 SINTRAN
command and no ND-500 output monitor call is involved in printing that line.

What the inline-buffer work IS: a correct port of a piece this emulator
genuinely lacked. The reference performs 39 of those copies for CPU-STAT, and
the mechanism and both guards are read out of the microcode. It is also, as of
this date, NEVER EXERCISED LIVE on this lane - only by its unit tests - because
no ND-500 program has yet reached its own output calls here. Treat it as
unverified against a real run.

The ruled-out list below stands; it was established independently of this
claim. The string corruption itself is OPEN again with no candidate.

### SETTLED - the output monitor calls carry their buffer inline

SINTRAN never asks for an output call's user buffer. MP-P2-N500.NPL:140656
tests MIFLAG bit WSMC: clear means it sends 3RMED (MICFU 10B) and fetches
the buffer, set means it reads the buffer out of the message through ABUFA.
A whole-run MICFU tally on the reference lane shows 10B ABSENT, so only the
inline arm is ever taken - and with nothing written there SINTRAN prints
whatever stale bytes sat at ABUFA. That is the "structured garbage" in a
process listing and in a CPU-STAT report while the program's own buffer held
the right text.

Implemented for {504B, 511B, 512B} - the microcode's CALL_5XX set. Details,
the microcode line numbers for both guards and the word-vs-byte ABUFA
measurement are in the commit and in `src/ndbus/ndbus_servicer.h`.

This also closes the string-corruption hunt. PROVEN RULED OUT along the way,
do not re-investigate:
- 26B / 3WMONCO as the carrier - its count was 0 in the corrupting run.
- The copy family's main loop - parity-safe at any alignment.
- A blanket endian fault - `TERMINAL-1` came through the same path correct.
- The window byte path - `write_msb` to 2N, `write_lsb` to 2N+1, verified.

### OPEN - segment 13 demand paging stops after the first page

The two lanes agree exactly up to here. Both fault at the SAME PC with the
same cause:

    this lane:  trap 46B at P=0x0800467F fault=0x4 psn=13 mms=0xA000000D
    reference:  psn=13 where=0xD (PFZPST, no PST entry) @0x00000004
                pc=0x0800467F

After that the reference MARCHES: `where` walks 0xD -> 0x3 -> 0xF and the
faults land on 62 DISTINCT addresses, 0x800, 0x1000, 0x1800 ... with
`worstRepeat=1` - every fault is a new address, so every answer made
progress. This lane re-faults at 0x4 forever and the monitor ends up
spinning on MICFU 1B.

The counts say the same thing. Reference PLACE-DOMAIN:
`restarts=8/8 swpfu[LNEWSWAP:8 LSWPAGE:1]`, finished in 12.7s, and its RUN
then does `LNEWSWAP:195 LSWPAGE:17` with 142 page-fault traps. So a
REPEATING LNEWSWAP IS NORMAL - one per page backed. What is not normal is
repeating it for the same address.

So the question is narrow: after SINTRAN answers LNEWSWAP for psn 13, why
does the retry still find no PST entry. Two things to read first, both
measured on this lane and neither yet explained:
- the connect record comes back all zero - `SWPST=0 SPFLA=0 r36=0x0000
  STATE=0x0 growable=0` - and STATE's grow-permit set is {13,14,15}, so a
  zero there makes the segment non-growable. Either the record is being read
  from the wrong place or SINTRAN never wrote one.
- `PST entry 13 is ZERO` was already observed directly in an earlier run.

### THE FAULT-PROFILE COMPARISON, the measure to work against 2026-10-04

Both lanes' page-fault censuses, counted by physical segment, side, and the
MMS where-nibble (0xD = no PST entry, 0x3 = write violation, 0xF = ordinary
demand page). Every entry has worstRepeat=1 on the reference, meaning each
fault is a NEW address and every answer made progress.

    segment/side     reference 0xD/0x3/0xF     this lane
    psn 11 inst          1 / 1 / 8              1 / 1 / 0
    psn 12 data          1 /  -  / 3            1 /  -  / 2
    psn 13 data          1 / 1 / 62             1 / 1 / 62   EXACT
    psn 14 data          1 / 1 / 62             not reached

Segment 13 is now byte-for-byte the reference's behaviour - one no-PST-entry
fault, one write violation, then 62 ordinary demand pages over 62 distinct
addresses - where before the fix it faulted at offset 4 for ever with
where=0xD. 64 distinct fault addresses on that segment, 69 traps in the run
against the reference's 142.

THE NEXT FAILURE IS SPECIFIC: segment 12's THIRD ordinary demand page. We take
two and then stop, and the monitor prints

    ADDRESS OUTSIDE DATA SEGMENT / PAGE FAULT
    At program address: 1 43611B   Logical address: 1 44032B
    Physical segment: 12D

That is P=0x08004789 touching 0x0800481A with where=0xF. "Outside the data
segment" is a BOUNDS verdict, so the question is the segment's declared length
rather than its paging. Segment 11's eight instruction-side demand pages and
the whole of segment 14 are behind it.

### WHERE THE SEGMENT-13 WORK ENDED UP, 2026-10-04

The cause was found and fixed, and it was in this emulator: the restart's
answer slots are a union whose arm follows the message kind, and this side
decoded the monitor-call arm over a trap record. See the commit "Read the
restart's answer slots only when they hold an answer".

MEASURED after the fix, same bring-up:
- the new gate fires 65 times in one run, so the mis-decode was frequent;
- PST completions go from 2 to 3 - one more segment now completes;
- PLACE-DOMAIN CPU-STAT now finishes cleanly: control store, swapper and
  "Allocating memory - 7342B pages", where before the swapper never completed;
- RUN now reports a well-formed page fault instead of dying:
      ADDRESS OUTSIDE DATA SEGMENT / PAGE FAULT
      At program address: 1 43611B   Logical address: 1 44032B
      Physical segment: 12D   MMS: 24000000017B
  which is the NEXT problem, not the old one.

So the 201B swapper fatal and the segment-13 stall are gone. The remaining
failure is a page fault in segment 12 during RUN, and it is a fresh
investigation rather than a continuation of this one.

### Segment 13: the stall is localised to one branch, and the cause sits
### one step EARLIER than the PST

MEASURED 2026-10-04 with a 965,638-instruction ND-500 trace. Read this before
touching the PST or the swapper - it replaces every earlier guess here.

HOW A PST ENTRY IS BUILT. The ND-500 swapper writes its own PST, reaching it
through virtual 0x28010800 (segment 5) which translates to physical 0x3A000.
Three steps, two routines:

    PST[11]  0x00000FF9  PC=0x08000473   step 1: raw frame
    PST[11]  0x40000FF9  PC=0x080037B3   step 2: mode bit
    PST[11]  0x40000FF8  PC=0x0800388D   step 3: final
    PST[12]  0x40000FF6  PC=0x0800388D   step 3: final
    PST[13]  0x00000FF4  PC=0x08000473   step 1, and nothing more

Step 1 writes the frame that belongs in the INDEX PAGE'S slot 0; step 3
corrects it to the segment's own frame, one lower. Both final values match
the reference exactly, so steps 1-3 are right.

THE BRANCH. The index-page builder at 0x08003632 runs exactly three times,
with R holding the index page's physical address: 0x007FC000 (frame 0xFF8,
segment 11), 0x007FB000 (0xFF6, segment 12) and 0x007FA000 (0xFF4,
segment 13). The routine 0x08003632..0x0800363F is a SHARED SUBROUTINE whose
last instruction is a return, and the saved L proves it has two callers:

    L=0x08003723  twice   -> caller then calls the completer at 0x08003724
    L=0x0800264A  once    -> caller never calls the completer

Segments 11 and 12 come from the first caller and complete. Segment 13 comes
from the second and does not. The completer 0x08003724 is entered exactly
TWICE in the whole run.

So nothing is broken in the PST write path. The swapper CHOOSES a different
path for segment 13.

WHAT THE COUNT ACTUALLY IS - ARITHMETIC-VERIFIED ON ALL FOUR CASES,
2026-10-04. It is the PAGE INDEX OF THE FAULTING ADDRESS, and the branch is
"page > 0". Nothing about it is wrong.

The producer is `080044A4: w4 =: b.0x38` (logged one instruction late as
0x080044A6), and the computation is:

    08004495: r := b.0x14          % r = 0x080240BC, the SAME in all four passes
    08004497: w4 := r.0x1E         % word at 0x080240DA
    0800449A: w4 and $0x7FFFFFF    % clear the 5-bit segment number
    080044A0: w sha  r4,$0x35      % shift right 11 = divide by the 0x800 page
    080044A4: w4 =:  b.0x38        % -> the count

PC=0x080082FF writes three halfwords at 0x080240D8/DA/DC, and the word the
swapper reads is their tail. Worked through:

    halfword@240DC   word read     AND 0x07FFFFFF   >>11   count
    0x0004           0x08000004    4                0      0   seg 11, 1st pass
    0x453A           0x0800453A    0x453A           8      8   seg 11, 2nd pass
    0x0E28           0x08000E28    0xE28            1      1   seg 12
    0x0004           0x08000004    4                0      0   seg 13

Those words are THE PAGE-FAULT ADDRESSES from the trap reports of the same
run - addr=0x0800453A psn=11, addr=0x08000E28 psn=12, addr=0x00000004 psn=13.
So the swapper is being told where the fault was, and it computes which page of
the segment that is. Segment 13's first fault is at offset 4, which is page 0,
so the branch skips - CORRECTLY.

SO THE SWAPPER AND THE BRANCH ARE BOTH RIGHT, and the question moves to the
page-0 case. The reference's own fault census for segment 13 is:

    where=0xD (PFZPST, no PST entry)  psn=13  faults=1   @0x00000004
    where=0x3 (write violation)       psn=13  faults=1   @0x00000800
    where=0xF (ordinary demand page)  psn=13  faults=62  62 distinct addresses

It faults ONCE at offset 4 with no PST entry, that fault is satisfied by
something, and every later fault is at page 1 or beyond - where this same
branch passes. This lane re-faults at offset 4 for ever.

THE OPEN QUESTION, and it is the last one in this chain: what satisfies the
FIRST fault on a segment - the where=0xD, page-0, no-PST-entry case. It is not
the path chased through this whole section, because that path is gated on
page > 0 by design.

A NOTE ON METHOD, because this section cost three retractions. Every time a
conclusion was drawn from the LISTING it was wrong - the MON 377B at
0x08004869 never executes, and neither does the call at 0x08004635; both sit
between the real jump source and its target in the printed order only. Every
conclusion drawn from the TRACE, or from addresses and arithmetic, has held.
Read what executed, then read the code it executed.

ALSO CORRECTED: the PC a write watch logs is NOT uniformly the instruction
performing the write. `08000DE2: h2 =: r.0x12` is exact, while the block move
at 0x08004884 and the store at 0x080044A4 are both logged one instruction
late. Identify the writer by the ADDRESS it touched and the registers in
force, not by the logged PC alone.

RETRACTED SAME DAY - THE COUNT IS NOT SINTRAN'S ANSWER. The section below
claims the count comes from the MON 377B at 0x08004869. That instruction
NEVER EXECUTES: the bridge logs a resume address on every monitor call, and
`resume=0x8004879` - the address after that 16-byte call - appears zero times
in a full run. The three 377B sites that do run resume at 0x8001797,
0x8002771 and 0x8008255.

The error was reading the LINEAR DISASSEMBLY as the execution path. 0x08004887
is reached by a JUMP from 0x08004649, not by falling through the code printed
above it, so the MON call that sits between them in the listing is not on the
path at all. Adjacency in a listing is not control flow - the same mistake as
reading two labels as a dispatch.

THE ACTUAL PATH, from the trace (0x08004649 + 0x239 = 0x08004882):

    0800461C: r := b.0x18
    0800461E: w comp2 r.0x56,$0x8014D20
    08004630: w stz   r.0x56
    08004635: call    $0x8002BE4          % result is the candidate source
    08004643: r := b.0x18
    08004645: w comp2 b.0x38,@b.0x18
    08004649: if << go $0x239             -> 0x08004882
    08004882: r := b.0x8
    08004884: w move  b.0x38,r.0x14
    08004887: h wconv b.0x50,r2           % stores the count

No monitor call is on it. The count comes from a register set before the jump,
and the only thing on the path that could set it is the call to 0x08002BE4 at
0x08004635. THAT is the next thing to read - not asserted, just the one
candidate the path allows.

PC ATTRIBUTION IN THE WATCHES IS CORRECT, checked rather than assumed: the
store `08000DE2: h2 =: r.0x12` is logged against PC=0x08000DE2 and its bytes
land at exactly slot+0x12. So the logged PC is the instruction performing the
write, not its successor, and the earlier attributions in this document stand.
In this syntax `=:` is a store and `:=` is a load.

SUPERSEDED REASONING BELOW - the correlation of count values with passes is
sound and still holds; only the claim about where the value comes from is
withdrawn.

THE COUNT IS SINTRAN'S ANSWER - the stall is at the seam, 2026-10-04.

PC=0x08004887 writes the count, and it does so immediately before every pass
through the deciding branch. Correlated by line number in one run:

    WRITE 0  by 0x08004887  ->  pass PST[11]  count 0  skip
    WRITE 8  by 0x08004887  ->  pass PST[11]  count 8  COMPLETE
    WRITE 1  by 0x08004887  ->  pass PST[12]  count 1  COMPLETE
    WRITE 0  by 0x08004887  ->  pass PST[13]  count 0  skip

(The 0x0B/0x0C/0x0D writes to the same address AFTER each pass are segment
numbers 11/12/13 stored by a later routine reusing the frame. Several routines
use that frame slot, so the raw address watch is noisy and only the
correlation by position is meaningful.)

The code around it says where the value comes from:

    08004857: r := b.0x8
    08004859: w stz   r.0x14                 % zero the field
    0800485B: call    $0x800008D
    08004869: call    MON 377B, $0x8012A34, $0x8023D80
    ...
    08004884: w move  b.0x38, r.0x14
    08004887: h wconv b.0x50, r2             % pass the answer on as the count
    0800488D: call    $0x8003727             % the routine holding the branch

The swapper zeroes the field, asks SINTRAN with MON 377B, and passes back what
it got. So the count is not something the swapper computes - it is SINTRAN'S
ANSWER, and a zero answer makes it skip the segment. That puts the segment-13
stall in the ND-100/ND-500 monitor-call seam rather than in the swapper.

NOT YET ESTABLISHED, and not to be guessed: WHICH 377B instance supplies it.
The write-backs logged either side of both the count-8 and the count-0 write
are identical (mask 0x6, values 0x0A and 0x8E30 into 0x080240B0/0x080240B4),
so the supplying call is a different instance whose answer is not yet
isolated. The segment-13 case does carry one answer the other does not -
mask 0x4, one parameter, logical 0x08000E04 = 0x00000000 - but nothing yet
ties that to b.0x50, and a frame-slot coincidence is exactly the kind of link
that has already been wrong three times here.

NEXT: instrument the 377B whose arguments are $0x8012A34 and $0x8023D80
specifically - the call at 0x08004869 - and compare its answer for segment 11
against segment 13.

THE DECISION RULE, READ OFF THE MACHINE 2026-10-04. Both operands of the
branch below, captured at the instruction that makes it (MFBUS_PCDUMP in
nd100x). The table at logical 0x08023D40 is CONSTANT in every pass -
00000000 000003FF 0000FFFF 00000000 - so the only varying side is the local:

    R=0x28010828  PST[10]   B+0x14 = 0              branch taken, not completed
    R=0x2801082C  PST[11]   B+0x14 = 0, then 8, 8   falls through, COMPLETED
    R=0x28010830  PST[12]   B+0x14 = 1, 1           falls through, COMPLETED
    R=0x28010834  PST[13]   B+0x14 = 0              branch taken, not completed

The rule is exactly: B+0x14 > 0 completes the entry, 0 skips it. Note that
SEGMENT 11 ALSO STARTS AT ZERO and only completes once the value has grown to
8 - so a zero here is a normal transient state, not an error. Segment 13's
value never grows. It is a per-segment count, and segment 13's stays zero even
though the first step already allocated frame 0xFF4 for it.

7 of 8 dumps used, so the capture is complete and not truncated.

WHERE THE COUNT COMES FROM - the open thread. There is NO store to b.0x14
anywhere in the swapper's 36 KB of code: the whole image disassembles with 2616
assignments and not one of them targets that local. So it is not a local the
routine writes, it is an INCOMING CALL PARAMETER written by the caller through
a different base register. The next step is the caller of the frame whose B is
0x08024744.

TWO FACTS WORTH KEEPING FOR ANY FUTURE READ OF THIS CODE:
- The swapper's program image is CONTIGUOUS in the pool, so no page walk is
  needed to read it: pa = 0x74800 + (logical AND 0xFFFFFF). Verified at four
  widely spaced points (0x08000473, 0x08002E38, 0x08003770, 0x08008115) and
  identical in three independent pool snapshots.
- That makes the whole swapper disassemblable in one command, which is how the
  "no store to b.0x14" negative was established rather than guessed:
      dd if=<snapshot> bs=1 skip=$((16 + 0x74800)) count=$((0x9000)) of=sw.bin
      nd500-dis -a -noansi -b 0x08000000 sw.bin
  The snapshot file carries a 16-byte header before the pool image.

THE DECIDING INSTRUCTION, FOUND 2026-10-04. One conditional decides whether
a segment's PST entry is completed, and it is reached with identical code and
identical registers in both cases.

METHOD, because it is the one that worked after several that did not. Take the
monitor-call resume that handles segment 11 and the one that handles segment
13, and diff their instruction streams from the resume point. They are
IDENTICAL for 337 instructions and diverge at the 338th:

    seg 11:  ... 0x08003769  0x08003770 -> 0x08003773  (completes)
    seg 13:  ... 0x08003769  0x08003770 -> 0x08003890  (does not)

Registers at 0x08003770 are identical except R, which is the PST entry being
processed - 0x2801082C for segment 11, 0x28010834 for segment 13. B is
0x08024744 in both.

The code at 0x0800375D, read out of three pool snapshots that all agree, and
disassembled:

    w1 := $0x8023D5C
    w1 * $0x3
    w2 := b.0x24
    w2 + r1
    w comp2 b.0x14,$0x8023D40+
    if <<= go $0x120

So the swapper compares a local at b+0x14 against an entry in its own table at
ND-500 logical 0x08023D40 and branches AWAY from the completion path when the
comparison is less-or-equal. A limit check, decided from data - not a fault and
not a trap. Segment 13 fails it; segments 11 and 12 pass it.

THE NEXT QUESTION, and it is now a small one: what are the two compared values,
and which of them does this emulator get wrong. The table is at logical
0x08023D40 and the local is at b+0x14 = 0x08024758 for the frame measured.

Everything below about which caller enters the shared subroutine is a
CONSEQUENCE of this branch, not a separate fact. The routine at 0x08002E38 and
the two callers 0x08003723 / 0x0800264A are downstream of it.

WRONG ANSWER, RETRACTED 2026-10-04 - THE DESCRIPTOR TABLE IS NOT THE CAUSE.
What follows below was recorded as the cause and is not. Read the retraction
first; the section is kept because the measurements in it are sound and only
the conclusion drawn from them was not.

The claim was that segment 13's descriptor is never populated. It IS
populated, by the routine at 0x08000DDD, which writes the halfword 0x0001
into the record with the stores at 0x08000DE2 and 0x08000DE8, and which runs
once for EACH of slots 10, 11, 12 and 13. The timeline, from one run:

    slot 10 field written  line 892880
    slot 11 field written  line 956648    branch 1  line 964843  completes
    slot 12 field written  line 957159    branch 2  line 966018  completes
    slot 13 field written  line 967965    branch 3  line 968852  does NOT

Slot 13's field is written BEFORE the branch that then takes the
non-completing path. So the descriptor is populated in time and the swapper
still routes differently. The field is not the discriminator and neither is
the table.

HOW THE WRONG ANSWER WAS REACHED, because the mechanism will repeat. Every
`seg-desc` dump available was taken at log lines 467 through 966839 - all of
them BEFORE line 967965 - so slot 13 read as thirty-two zero bytes in every
one of them. A snapshot earlier than the write it is being used to rule out
cannot rule it out. The reference's own table dump was taken at a different
point in its run, so the two were never comparable in the first place.

THREE FILTERING ERRORS IN ONE INVESTIGATION, all the same shape: a search
whose pattern cannot match the thing being looked for returns a confident
empty set.
  - A byte address was grepped for (0x5F519) where the machine does a WORD
    store at 0x5F518. Reported as "the flag byte is never written"; it is
    written, as 0xEE080000 at +4 by 0x08002CF6.
  - An exact-address match was used where a store has WIDTH, so a 32-bit
    store two bytes earlier was missed.
  - Only `[PTEWR]` lines were parsed, which are the CPU-store-level watch.
    The write that settles this is a `[PTEWATCH] w8` line from the BUS-level
    watch with no PTEWR partner, because it came from a halfword store.
Check that a pattern can match a known-present case before trusting its
absence.

WHAT STILL STANDS. Everything above this retraction, plus: the branch is
decided by WHICH CALLER enters the shared subroutine, and that choice is made
before 0x08003632 runs. The open question is now what routes the swapper to
the 0x0800264A caller for segment 13 and to 0x08003723 for 11 and 12.

SUPERSEDED - the text below is the retracted reasoning, kept for its
measurements only. Segment 13's descriptor in
the swapper's own table is EMPTY on this lane and populated on the
reference's. That table is at ND-500 logical 0x08038000, stride 100 decimal
bytes, indexed by segment; the reference calls it TABLE-A and reads a flag
byte at +5 and a halfword at +0o14:

    slot 10   0001/40    identical on both lanes
    slot 11   0001/80    identical on both lanes
    slot 12   0001/00    identical on both lanes
    slot 13   0001/08    on the reference; ALL ZERO here
    slot 14   0001/08    on the reference; ALL ZERO here

Slots 10-12 agree byte for byte, so the table is being built correctly up to
12. Note the flag: 10, 11 and 12 carry 0x40, 0x80 and 0x00, while 13 and 14
carry 0x08 - a DIFFERENT KIND of segment, which is consistent with the
swapper taking a different code path for them. Segment 13 is the fresh,
empty scratch segment that PLACE-DOMAIN connects for the domain. The swapper
does reach that slot: the branch trail ends with R=0x08038514, which is
0x08038000 + 13*100 exactly.

THE OPEN QUESTION, in one sentence: what populates TABLE-A slots 13 and 14
with 0001/08 on the reference, and why does nothing populate them here.

TWO THINGS RULED OUT, with the measurement:
- The ND-100 does not write the PST. A range watch on PST[10..15] caught 48
  writes in four complete passes with the 400-write budget barely touched -
  three zeroing passes (P=060720B level 0, P=052661B level 1, P=075707B
  level 11) and one pass of twelve words that are not PST entries at all. All
  of them are at log lines 29-77, long before the swapper's stores at 101584
  onward. Initialisation, not a stomp.
- The restart ROUTING is correct. After the swapper's LNEWSWAP the next
  restart goes to X5CPU 1, the domain, which then faults on the next page.
  The reference does exactly this 74 times for its segment 14, so repeatedly
  asking LNEWSWAP for one segment is normal - each call grows it by a page.

AND ONE INSTRUMENT TRAP, which nearly produced a wrong answer. The
completer's absence after PST[13] step 1 was first read off a trace that had
33 lines of budget left, where absence and "never ran" are indistinguishable.
Re-run with MFBUS_RESUME_TRACE=2000000, a 66-fold larger budget, and the
count after that store is 33 AGAIN - so the ND-500 really does stop there,
and only the second run could say so.

### The pack difference does NOT explain the segment-13 stall

Written here because the previous section said it might, and reading the
reference's own PST scan settles it the other way.

A PST entry has two modes. Mode 0 is a direct one-page entry and is what a
CAPABILITY TABLE uses; mode 1 sets bit 30 and the entry points at an INDEX
PAGE whose words are the segment's page frame numbers. On the reference:

    PSN  3: mode=0 pfn=0x0000E8  (phys 0x00074000)   capability table
    PSN 10: mode=0 pfn=0x0001B2  (phys 0x000D9000)   capability table
    PSN 11: mode=1 pfn=0x000FF8 -> index page lists 0FF9 0FB2 0F6A ...
    PSN 12: mode=1 pfn=0x000FF6 -> index page lists 0FB1 0FF5 0FB3 ...
    PSN 13: mode=1 pfn=0x000FF3 -> index page lists 0FF4 0FF2 0FF1 ...

PST[11] and PST[12] are 0x40000FF8 and 0x40000FF6 on BOTH lanes - the same
frames, bit for bit - even though the two packs report different swap sizes
(13245B against 11246B). So the segment allocator is not perturbed by the
pack at all; the only entry the pack moves is PST[10], a mode-0 capability
table page that comes from a different pool region (0x118 here against
0x1B2 there).

The two lanes therefore allocate identically through PSN 12 and diverge only
at 13. That makes this emulation, not the pack, and the next instrument is
the one that names the writer: nothing writes the PST by message on either
lane - all PHYSWR traffic goes to the capability and trap-config blocks at
0x074096..0x0740C4 and 0x08C096..0x08C0C4 - so the PST is written by the
ND-100 straight into the shared pool through its bank window, which no MICFU
trace can see. Watch the pool range holding PST[10..15] from the ND-100 side,
which reports the writing P and level.

### Packs differ - but only PST[10] moves with them

The reference's own pack and this lane's are both 78,643,200 bytes but are
NOT the same image (md5 `a35ee154...` against `813435ed...`). A content
difference between the two lanes can therefore be the pack rather than the
emulation. Copy this lane's pack and point the reference at it with the
`RETROCORE_ND5000_PACK` environment variable before attributing anything.

## The segment-12 fault, traced to THA (2026-10-04)

The symptom was `ADDRESS OUTSIDE DATA SEGMENT` / `PAGE FAULT` at program address
`1 43611B` = `0x08004789`, logical address `1 44032B` = `0x0800481A`, physical
segment 12, on `RUN` after `PLACE-DOMAIN CPU-STAT`. The bounds were never the
problem.

**The routine.** Disassembled from a pool snapshot taken IN THE SAME RUN as the
trace, which is the only way to read it: the logical-to-physical delta is decided
per run (`0x74800` in one snapshot, `0x7F7800` in the next), and a snapshot from
another run decodes the wrong bytes into plausible-looking code. Alignment
confirmed two ways - the traced bytes `20 E5` at `0x08004789`, and the third byte
`0x14` matching the `B+0x14` the frame dump shows.

```
08004743: ents $0x2C
0800474E: tha =: b.0x14          ; copy THA into the frame
08004761: w stz b.0x24           ; offset = 0
08004763: w1 := $0xC  =: b.0x28  ; step = 12
08004767: w1 := $0x9  =: b.0x18  ; trap number 9 = loop counter
08004772: bi test $0x800111C+    ; is this trap number wanted?
0800477D: w test @b.0x14+        ; slot already filled?
08004789: w1 =: @b.0x14+         ; INSTALL handler at [THA + n*4]   <- faults
080047A2: by comp2 b.0x1B,$0x29  ; loop until trap number 41
```

It is a trap-handler install loop. `0xE5` is LOCAL_IND_PI (`cpu_instr.c:426`):
the destination is the pointer at `B+disp` plus `I*4`.

**The cause.** THA is zero. `ND500X_VWATCH=0x30:4` caught the copy being made and
it writes `0x00000000`; the per-step trace then showed THA = 0 for all 1,017,829
steps of a whole run. With THA = 0 the installs land at `0 + n*4`, inside the
frame; the one at `n = 12` lands on `0x30`, the THA copy itself, overwriting it
with `0x080047E6`, and the next goes to `0x080047E6 + 13*4 = 0x0800481A`. The
run's own log said it from the start and it was read past: `trap 46B raised at
P=0x08004789 addr=0x0800481A (no local handler; THA=0x00000000)`.

**Not the swapper's job to set it.** Its 256 KB program image holds five `tha=:`
reads (`0x08001571`, `0x08004125`, `0x08004131`, `0x08004153`, `0x0800474E`) and
ZERO `tha:=` writes - a byte scan for `FD CA` against `FD CB`.

**What was fixed.** `nd500_dit_read_tha` (nd500x `3fb4c6a`) plus the context load
calling it (nd100x `440ca49`). THA is now non-zero for 56,902 steps where it was
zero for all of them. The fault is UNCHANGED, because the two processes derive
different DIT bases from their own PS:

| context load | PS | DIT | THA |
|---|---|---|---|
| `P=0x8008255` | 3 | `0x8C000` | `0x08001628` (70 loads) |
| `P=0x8004751`, `0x800467F`, `0x8000004` | 0xA | `0x74000` | 0 |

The process that runs the install loop is the `PS=0xA` one.

**Who fills a THA.** Three write paths watched into each PCB page. The ND-100
only ZEROES those cells (from `P=060720B` level 0, the same fill routine that
clears pool word 0). No ND-500 CPU store touches them. The real value arrives
through the servicer's mailbox block copy, and the census of all 41 copies in a
run shows two PCBs served through two mailbox buffers:

```
copy #4  : 4 bytes 0x00CC00 -> 0x0740B6, value 0x00000000   <- PS=0xA THA slot
copy #17 : 4 bytes 0x00D400 -> 0x08C0B6, value 0x08001628   <- PS=3   THA slot
copy #30 : 4 bytes 0x08C0B6 -> 0x00D400, value 0x08001628   <- read back
```

So SINTRAN sends a real THA, into the `0x8C000` PCB, and only zeros into
`0x74000`. The `0x74000` page gets capability writes at +0x44 from the ND-500 CPU
at `PC=0x08000753/5A/61/68` through logical `0x68000044` (segment 13), which is
`+64 + segment*2` in the documented PCB layout - so `0x74000` IS a PCB, and the
process translating through it resolves its pages, which is evidence it is the
faulting process's own.

**What the microcode says, and the open question.** `tha:=` / `tha=:` are not
plain register moves: `012210 LOAD_THA` and `012233 STOR_THA` both go through
`012035 CED_TO_DIT`, which builds a DIT address from `SRF14` (CED) by repeated
`A+B` doubling. So THA is a DIT-RESIDENT FIELD indexed by CED, read from memory
on every access - not a register the way this emulator holds it.

That makes the DIT BASE the crux, and ours is not told to us: the bridge's own
comment says it "does NOT hand a DIT base over - the one it tracks is a
diagnostic learned from the trap-config writes", while
`nd500_mmu_declare_process_segment` sets `DITBASE` from `PST[PS]`. Two sources
for one value, one of them learned. UNKNOWN, and not to be invented: which
register the microcode uses as the DIT base, and therefore whether the faulting
process should be reading `0x74000` (where its THA is genuinely zero) or
`0x8C000` (where SINTRAN put one).

**Method notes earned here.** A pool snapshot only decodes correctly in its own
run. A PC-match instrument that samples at stop time cannot prove an instruction
never executed - `MFBUS_PCDUMP` reporting nothing for `0x08004781` was read that
way and was wrong. And the ND-100 bank-write watch had produced zero lines in
four consecutive runs; a whole-pool positive control gave 3000 lines, which is
what turned its silence on a range into evidence.


## After THA: what the trap path then showed (2026-10-04/05)

Sourcing THA from the DIT (`3fb4c6a`, `48f06b4`) removed the segment-12 fault and
let the swapper's install loop work - the counter at logical 0x34 climbs 9..41
and stops, so handlers 9 through 41 really are installed. Each step after that
uncovered one more defect of the SAME shape: a diagnostic or check that RAISES a
fault while handling a fault, or state the context switch does not carry.

**1. The handler-slot probe faulted.** `raise_trap`'s non-ignorable path read
THA[tn] with `nd500_mmu_translate`. Fixed in `5cad152` with
`nd500_mmu_peek_space`. Measured before the fix: the entry-point fetch at
0x08000004 faulted, the probe read THA[38] as DATA, that read faulted, and the
station reported `P=0x8000004 fault=0x8000004 psn=12` - the instruction address
wearing the data segment - 192 times. Unreachable while THA was 0.

**2. The ENTT verification faulted, and an absent handler page was called a bad
handler.** Same file, one level deeper. The handler for trap 38 is 0x08004924 -
`0x080047C2 + 29` steps, and the handlers really are `entt` instructions 12 bytes
apart from 0x080047C2 - so the address was right and its PAGE was absent. The
check read 0x00, said "No ENTT instruction", cleared the trap bit and returned;
the instruction retried, faulted identically, and the runaway guard halted the
CPU after 500 repeats. Now it peeks and reports a page fault ON THE HANDLER
ADDRESS. With that, the handler is paged in and RUNS.

**3. Reporting a trap takes TWO steps on this lane.** `mfbus_trap_sink` returns 0
BY DESIGN - it records the raise so the host knows what to report - and the STOP
is what the bridge turns into a park. A first version of the fix called only the
stop, and the run went back to stalling after 68 parks instead of reaching
0x080008F6: the CPU stopped and nobody said why. `nd500_offer_trap_to_sink`
treats 0 as "declined" and CLEARS the trap state, so it cannot be used alone
either. Both, in raise_trap's order.

**4. A silent translation failure.** At `PC=0x080008F6` the stop line reported
`paddr == vaddr` with `in_trap=0`: `nd500_mmu_translate` returned the virtual
address WITHOUT raising, so the guard at `cpu.c:371` - which exists for exactly
this and asks the trap state rather than whether the PC moved - never fired.
`nd500_mmu.c` has 19 `return virtual_addr;` sites and not all of them raise
first. WHICH ONE is still open. `ND500X_MMULOG=1` names the reason, and in the
run that reached the same region it printed
`PS_ASI page not valid! vaddr=0x08000931 pte_addr=0x007FC004` - which DOES raise
- so the silent case may be a different site or a different address.

**5. The inside-trap-handler flag, and what it is NOT.** The handler runs, faults
on its own data at 0x08001800, parks, is restarted, reaches its `RETT` at
0x08004A28 - and the CPU refuses it 503 times: `Not in trap handler`.
`Rett.c:212` tests nothing but `cpu->in_trap_handler`, and `Entt.c` tests the
same flag at the handler's FIRST instruction, which passed. So it was true on
entry and false on return.

ND-05.009.4 Table 6 puts that flag in the DIT at 273B = 187, so it was carried
there (`nd500_dit_read_ith`/`_write_ith`, read beside THA and written in
`mfbus_save_context`). **The carry is MEASURED WORKING** - `ITH=1` saved to
0x8C000+187, the swapper correctly loading 0 from its own 0x74000, and process 1
loading `ITH=1` back - and the RETT is STILL refused. So the flag is lost to a
CLEAR, not to a missing save.

`lregbl` is RULED OUT: it is one of only two in-flight clears and a run that did
reach the refusal logged ZERO of them. The other is the successful RETT, which
never happens. So on the evidence nothing clears it, which leaves one untested
possibility: the refused RETT may be executing in the SWAPPER's context rather
than process 1's. Both processes run the same program image at 0x0800xxxx and
both have CED=0, so that confusion is possible and would make the refusal
CORRECT. The refusal now prints CED, PS, B, THA, DITBASE and the DIT's own ITH
byte to settle it. OPEN.

**Where the run gets to now.** 70 parks, segment 13 paged through all 63 pages,
the reference's own fault at `P=0x8004751` reading 0x08001060, the handler
entered and faulting on 0x08001800 - then the RETT refusals and SINTRAN polling
`MICFU=1B` with nothing to run. CPU-STAT still prints nothing.


## The RETT refusals are an ENTT interlock lost to a park (2026-10-05)

Watching the inside-trap-handler byte itself (pool 0x08C0BB = PCB 0x8C000 + 187)
on all three write paths ended the guessing. The tail of a run reads:

```
CONTEXT SWITCH X5CPU=0 -> 1 (P=0x8004924 PS=0xA ... B=0x8001728)
MICFU 24B restart of a process parked on a TRAP
raise_trap: trapBit=0x800000000 trapPC=0x08004924 dataAddr=0  instr_count=1019614
  [PTEWATCH] w8 phys=0x0008C0BB val=0x01      <- this trap's own dispatch sets ITH
  [PTEWATCH] w8 phys=0x0008C0BB val=0x00      <- and it is cleared again
raise_trap: trapBit=0x800000000 trapPC=0x08004A28 ...   (then forever)
```

So the first thing that happens after the restart is a trap AT 0x08004924 - the
handler's own first instruction, its ENTT - before any RETT runs. Trap bit 35 is
what `trap_instruction_sequence_error` raises, and `Entt.c:261` raises exactly
that when `cpu->trap_dispatch_pending` is 0:

> Verify a trap dispatch is awaiting its ENTT (set by invoke_trap_handler).
> Deliberately NOT in_trap_handler: with nested traps allowed, a deeper
> handler's ENTT runs while the outer handler is still active.

**The park sits BETWEEN the dispatch and the handler's first instruction.** The
handler is entered, its ENTT faults while building its frame (the fault on
0x08001800 at P=0x08004924), the process parks, SINTRAN pages the data in and
restarts it at P=0x08004924 - and the ENTT is re-executed with
`trap_dispatch_pending` gone, because that flag is a C field no save or load
carries. The ISE then dispatches a handler of its own, which sets and clears
ITH, and the RETT at 0x08004A28 is reached with nothing pending: 503 refusals.

So the ITH work was necessary but not sufficient, and the ITH readings along the
way were all explained by it: `dit_ITH=1` while the C copy was stale, then
`dit_ITH=0` once the bridge's save wrote the stale copy over the DIT, then
`dit_ITH=0` again because the ISE dispatch's own clear is the last write before
the RETT.

**What is settled.** The flag belongs in the DIT (ND-05.009.4 Table 6, offset
273B = 187) and is read there now; nothing in the bridge copies it either way;
`lregbl` is not involved (zero clears in runs that reach the refusal); the
refused RETT is process 1's own context (PS=0xA), not the swapper's. SINTRAN
also writes that byte itself - a 1-byte mailbox copy to 0x08C0BB and a 256-byte
PCB init over it - so it is shared state, not ours alone.

**What is NOT settled, and must not be invented.** Where the dispatch-to-ENTT
interlock lives on the real machine. The context block has microcode scratch
slots (frame arg27-30 and arg39-40, block SC1 at 0x6C and SC2 at 0x70) and the
microcode plainly keeps scratch across a context switch, but WHICH slot carries
this interlock is unknown, and writing our own meaning into a block field the
machine reads would be an invention. A host-side shadow in the bridge, keyed by
X5CPU, is the honest alternative: it is bookkeeping about a host flag, not a
claim about hardware layout.


## The dispatch interlock is already consumed at every save (2026-10-05)

A host-side shadow of `trap_dispatch_pending`, keyed by X5CPU, was tried in the
bridge and changed nothing - still 503 instruction-sequence traps. Logging both
sides said why, and it rules the idea out rather than leaving it in doubt:

```
160 save: dispatch_pending[0] := 0
145 save: dispatch_pending[1] := 0
 71 switch: dispatch_pending[0] -> 0
 71 switch: dispatch_pending[1] -> 0
```

**Every save records 0**, process 1 included, 145 times. So the flag is not lost
by the park - it is already consumed before the park happens. Carrying it cannot
help, and the shadow was REVERTED rather than left in as unjustified state.

Only three places touch the flag: `cpu.c:2022` sets it on dispatch,
`Entt.c:481` clears it at the END of a successful ENTT (after every frame
write), and the reset paths. A 0 at save time therefore means the ENTT RAN TO
COMPLETION. But the park reports `P=0x8004924`, the handler entry, and the ISE
that follows the restart is raised at that same address - so either the reported
P is not the instruction that faulted, or the handler is re-entered at its entry
with the interlock legitimately spent.

Those two cannot be separated by reading. The instrument is
`MFBUS_RESUME_TRACE`, which prints P and the bytes per step: the sequence around
0x08004924 says whether the ENTT completed and the body faulted, or the ENTT
faulted and a nested dispatch re-entered the same handler. UNKNOWN until then -
and the project's own comment that "our single-level saved-trap state cannot
nest" is the thing to check against, not to assume.


## How to work here (hard-won)

- Order of authority and where out-of-repo truth lives:
  `docs/EXTERNAL-ARTIFACTS.md`.
- SINTRAN behavior facts that keep biting: `docs/SINTRAN-CONVENTIONS.md`.
- Every nd500x fix that touches CPU/MON behavior gets a line in
  `docs/SYNC-BACKLOG.md` for the C# side, at the time of the fix.
