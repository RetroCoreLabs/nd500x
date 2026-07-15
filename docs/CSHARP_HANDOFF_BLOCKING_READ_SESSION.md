# C# Emulator Handoff - Blocking-Read / Interactive-Input Session (2026-07-15)

Full path: /home/ronny/repos/nd500x/docs/CSHARP_HANDOFF_BLOCKING_READ_SESSION.md

Running log of nd500x changes to mirror on the RetroCore C# side. Newest section last.

## 1. Blocking-read semantics (STOP_WAIT_INPUT) - the systemic model

When a TERMINAL read finds no input, SINTRAN suspends the process ("the program waits if
there is no bytes in the input buffer of the device"). nd500x now models this instead of
returning EOF or spinning.

- New stop reason STOP_WAIT_INPUT.
- A MON handler that finds no input sets a "wait" request (wait_requested + wait_device) and
  returns WITHOUT committing. The MON dispatch layer then: stops the run loop, rewinds the PC
  to the CALLG that invoked the MON, and reports STOP_WAIT_INPUT. On resume the CALLG
  re-executes and the MON call retries - now finding the byte/line.
- Applies to MON 1B INBT (terminal/character device) and MON 503B DVINST (terminal line read).
  On empty terminal input both request the wait rather than returning EOF / an empty line.
- An interactive front-end blocks on real stdin (blocking select) when it sees STOP_WAIT_INPUT
  and then resumes; a scripted/headless driver feeds input and resumes.

C# action: add an equivalent "suspend-and-retry" for terminal INBT/DVINST reads with no input.
The key property is RETRY: do not consume/commit the MON call; re-execute it after input arrives.

## 2. Device-0 (command buffer) vs terminal channel - IMPORTANT distinction

The ND LINKER uses TWO input channels:
- Device 0 = SINTRAN command buffer = the BATCH channel. When empty it must return EOF
  (003B End-of-file), NOT block - so the program falls through to interactive input.
- Device 1 = terminal. Interactive command lines are read via 503B DVINST on device 1; that is
  the channel that BLOCKS when empty.

(Verified from a full MON trace: 503B DVINST reads the command line "LIST-STATUS<CR>" from the
terminal; INBT device 0 is the batch/command-buffer poll.)

C# action: keep command-buffer (device 0) empty = EOF; only the terminal blocks.

## (further sections appended as changes land)

## 3. CAT-500 route RESOLVED - NC -> "CAT-CAT5-B" -> cat-cat5-b06.dom

The reason BOUT.NRF is 0 bytes: NC is only the FRONT-END. It emits CAT-code
intermediate into the scratch file (SINTRAN file 0100 octal = 64 = SCRATCHnn:DATA)
plus GUEST/B.CAT, then invokes the CAT-500 CODE-GENERATOR back-end to convert
CAT -> :NRF. That back-end step is not yet executed, so the NRF stays empty.

Ground truth captured this session:
- NC issues THREE 317B UECOM calls, decoded now as: "NC-A", "CAT-CAT5-B", "NC-A".
  The middle one, "CAT-CAT5-B", is the CAT-500 back-end invocation. (Answers the
  C# question "the exact SINTRAN name NC's CAT-CAT5-B resolves to": it is the
  literal command string "CAT-CAT5-B", 0x27-terminated.)
- Binary: /mnt/d/ND/500/CAT5-CAT/cat-cat5-b06.dom = "CAT-500 - Version B06 - 1988-01-05".
  Analysis + MON contract + asm are in /mnt/d/ND/500/CAT5-CAT/.
- UECOM argument layout NC uses: arg0 is a [Length:4][Pointer:4] descriptor
  {maxlen=0x80, cmd_ptr, ...}; the command text is at *cmd_ptr, 0x27-terminated.
  nd500x fix: 317B handler now uses mon_read_descriptor_string (was reading the
  descriptor struct as raw text and getting ""). C# ACTION: read UECOM's command
  the same way (descriptor {len,ptr}) or you will also get an empty command.

CAT-500 standalone behavior (verified by booting the dom in nd500x):
- init: 11B TIME, 114B TUSED, 143B RSIO, 422B GSWSP (working segment), banner via
  504B DVOUTS "CAT-500 - Version B06 ...", opens scratch (41B ROBJE -> SCRATCH64.DAT).
- Then prints prompt "Cat-500: " (504B DVOUTS) and reads a command via 503B DVINST.
  It is INTERACTIVE (device-0/device-1 model, same as the linker): command
  dispatcher 0x0801EB04 reads a record; on empty/terminal it does 32B MSG + 0B
  LEAVE (clean exit).
- It maps the CAT input via 412B FSCNT (FileAsSegment) and writes :NRF via 120B
  WFILE. IMPORTANT: the CAT-500 MON-contract doc calls 412B "the one hard blocker"
  because it was bookkeeping-only - but nd500x's 412B now connects the real file
  bytes (nd500_mon_connect_file_as_segment). C# ACTION: 412B must actually map the
  scratch/CAT file bytes into the address space, else CAT-500 can't read its input.

## 4. SOLVED: C -> NRF WORKS END TO END. One real bug: 412B FSCNT's OUT parameter

**THE BUG (high priority - the C# side almost certainly has it too):**
`MON 412B FSCNT` takes FOUR arguments: `FileNo, LogSegmentNo, AccessType, @out SegNo`.
The 4th is an INDIRECT OUT parameter that receives the assigned segment number. nd500x was
only leaving the value in W1 and never writing argument 3. **C# ACTION: write the assigned
segment number to the caller's 4th (out) argument.**

CAT-500's wrapper proves the signature (byte-verified):
```
0801F080: ents ; w1 := $0x80234A0
0801F08B: call $0xFFFFFFFFF800010A,$0x4,b.0x14,b.0x18,b.0x1C,@b.0x20   ; 0x10A = 412B
0801F096: if -k go 0801F0A3     ; K clear (success) -> zero the error cell
0801F099: h1 =: $0x80224F0      ; error path: store error code
0801F0A3: h stz $0x80224F0      ; success path
```
Symptoms if you get this wrong (exactly what we saw, all byte-observed):
- the caller keeps the OUT cell's stale value (0), so it addresses **segment 0** and reads
  zeros - we measured 67 reads at vaddr 0x00000000 and **0 reads** of the real mapping at
  0x20000000 across the entire 11,102-read code-generation window;
- CAT-500 then prints `*ERROR*   can't generate code` WITHOUT EVER READING ITS INPUT;
- its later `413B FSCDNT(LogSegmentNo=0)` mismatches the real segment ("File 100 mapped to
  segment 5, not 0").

**With the OUT parameter written, the whole pipeline works:**
```
Cat-500: generate-code
CAT file: SCRATCH-00001:CAT
object file: B:NRF
code generation : ok
programCAT_COMPILER terminated
```
`GUEST/B.NRF` = 513 bytes; header `0a 00 01 70 44 ...` matches the golden reference
`/mnt/d/ND/500/FraTor/test-real/test-real.nrf`, with real NRF symbol records
(`PROG!NAME`, `V!ARGC`, `X`). No regressions (MON 53/3 unchanged; instruction validation
39791/12 unchanged).

### The CAT-500 protocol (so you can drive it identically)
- Prompts, in order: `Cat-500: ` -> `CAT file: ` -> `object file: `.
- Command: **`generate-code`**, then the CAT input, then the NRF output. The comma form
  `generate-code,SCRATCH-00001:CAT,B:NRF` also works. `EXIT` exits cleanly via 0B LEAVE;
  `help` prints help.
- The command is NOT guessed: NC writes it verbatim into its control stream
  (`SCRATCH64.DATA`, plain text: options line + `generate-code,SCRATCH-00001:CAT,B:NRF`).
- **NC creates the CAT itself** via `221B CRALF 'SCRATCH-00001:CAT'` ->
  `./SCRATCH/SCRATCH-00001.CAT`, and DELETES any pre-existing copy. Don't hand-substitute it.
- 503B DVINST reads ONE character per call (MaxNo=1).

### Things that are NOT bugs (we checked; don't "fix" these)
- `143B RSIO` execution mode does NOT gate CAT-500's prompt (mode 0 vs 1 identical).
- `AccessType=1` mapping legitimately leaves the segment zeroed (1 = uninitialized/empty);
  CAT-500 maps the scratch to WRITE it.
- `321B UEADM` "deprecated -> error" and `413B FSCDNT -> ERROR` occur only on the POST-error
  cleanup path - red herrings. (413B's segment-number check is still worth fixing.)
- Colon->dot filename mapping already works (`SCRATCH-00001:CAT` -> `./SCRATCH/SCRATCH-00001.CAT`).
- MON logs print numbers in OCTAL - an "asked 4000 got 2048 short read" is 4000(8) = 2048.

### Diagnostic gotcha that will cost you hours
Queuing console input installs a ConsoleIO whose `write_char` captures guest output into an
internal buffer, so 504B DVOUTS output becomes INVISIBLE on stdout. Print the captured buffer
(`mon_get_console_output()`) or you will not see the banner/prompts at all.

### Still open
1. 317B UECOM does not yet RUN the named subsystem nested (load cat-cat5-b06.dom, run to
   MON 0B LEAVE, return to NC). The carve proves UECOM is a synchronous
   execute-command-and-return, so nested invocation is still required for a fully automatic
   NC->CAT-500 flow. Today the two stages are driven separately.
2. Phase 3 (linker -> :DOM) is unchanged.

Both emulators need the same nested-invocation model; keep bit-aligned.

---

## 5. MON 511B DVIO implemented + 143B RSIO command input moved to the terminal (2026-07-16)

**Port these two TOGETHER. Neither works alone** - RSIO alone just trades a hang for an
unimplemented-MON stop (it was probed and reverted twice for exactly that reason).

nd500x commit: `2d9b00f`. Files:
- `/home/ronny/repos/nd500x/src/libmon/mon_device_io.h`                    (new - shared cores)
- `/home/ronny/repos/nd500x/src/libmon/handlers/mon_511B_DVIO.c`           (implemented)
- `/home/ronny/repos/nd500x/src/libmon/handlers/mon_503B_InputString.c`    (core extracted)
- `/home/ronny/repos/nd500x/src/libmon/handlers/mon_504B_OutputString.c`   (core extracted)
- `/home/ronny/repos/nd500x/src/libmon/handlers/mon_143B_ExecutionInfo.c`  (InputDev 0 -> 1)
- `/home/ronny/repos/nd500x/src/libmon/handlers/mon_1B_InByte.c`           (comment only)
- `/home/ronny/repos/nd500x/test/test_mon_calls.c`                         (8 new assertions)
- `/home/ronny/repos/nd500x/test/diag_linkterm.c`                          (new harness)

### 5.1 Change A - 143B RSIO reports the TERMINAL as command input

`InputDev` 0 -> **1**. Its own documented contract says InputDev is the "Terminal number for
interactive"; we report `ExecutionMode=0` (interactive) and `OutputDev=1` (terminal), so
`InputDev=0` (the command buffer = the BATCH channel) was self-inconsistent.

Consequence, byte-verified: with the old value the linker believed its command input was
device 0, issued `1B INBT` on it, found it empty and blocked forever.

**Regression checked:** NC is unaffected in outcome. It now reads device 1 (verified: it
blocks with `dev=1` and exits cleanly on `EXIT`), still compiles `B.C` to
`*** no errors detected ***`, and reaches a clean `MON 0B LEAVE`.

### 5.2 Change B - MON 511B DVIO (fused prompt-then-read)

**DVIO = DVOUTS(arg0, arg1, @arg2) THEN DVINST(arg0, MaxNo=arg14, @ret=arg15, @buf=arg3, arg4..arg13).**

| arg | meaning |
|---|---|
| 0 | DeviceNo |
| 1 | NoOfBytes to WRITE (output phase) |
| 2 | @output buffer (the prompt) |
| 3 | @input buffer |
| 4 | BreakStrat (observed 7) |
| 5 | EchoStrat (observed -1 = ECHO_STRAT_NONE) |
| 6..9 | see caveat below |
| 10..13 | table words (observed -1, 0, 0, 3) |
| 14 | **MaxNo** to read |
| 15 | **@returned byte count - OUT PARAMETER** |

> **Do NOT derive this from `3 + (14 - 1) = 16`.** That arithmetic gets the COUNT right and
> the ORDER wrong. DVINST's MaxNo and returned-count are relocated to args 14/15 so args 1..2
> can carry the output phase. A mapping built on the arithmetic puts MaxNo at arg 1 and
> clobbers the output byte count. **args 3..13 keep DVINST's own indices** - only MaxNo and
> the returned-count move.

**arg[15] is an OUT parameter** - the MON 412B FSCNT bug shape all over again. Nobody writes
it before the call; the caller reads it immediately after (`B0047539: w move b.0x1A0,b.0xA8`).
If your handler only leaves the count in a register, the caller keeps a stale value.

### 5.3 How the mapping was established (reproduce it if you doubt it)

In `/mnt/d/ND/500/nd-linker/linker-b01.dom.asm` each of 503B/504B/511B has exactly ONE call
site, and each is a bare pass-through thunk - nothing runs between `ents` and the `call`, so
the thunk's `b.0x14..` slots ARE its incoming arguments:

```
B004AC8A: ents $0x4C
B004AC90: call $0xF8000143,$0xE ,b.0x14,b.0x18,b.0x1C,@b.0x20,b.0x24..b.0x48   ; 503B
B004ACA7: ents $0x20
B004ACAD: call $0xF8000144,$0x3 ,b.0x14,b.0x18,@b.0x1C                         ; 504B
B004ACB9: ents $0x54
B004ACBF: call $0xF8000149,$0x10,b.0x14,b.0x18,@b.0x1C,@b.0x20,b.0x24..b.0x50  ; 511B
```

Each thunk has ONE caller, which calls it with ZERO CALLG args (`call $0xB004ACB9,$0x0`)
after building the argument block at the top of its own frame (a fixed +0x150). Diffing the
511B caller (`B00474A1..B0047535`) against the 503B caller (`B004754F..B00475C0`) yields the
table above. Three independent confirmations:

1. 511B's args 0..2 are built by a sequence **byte-identical** to the 504B DVOUTS caller's
   (`w move $0xB0053068,b.0x114` / `by2 laddr @b.0x114+` / `w2 =: b.0x16C`), and all three
   callers take arg0 from the same `b.0x58`.
2. arg4's source is written `w move $0x7,b.0x100` at `B004745C`; the live probe observed
   `arg[4] == 7`.
3. args 10..13 are read from the table at `0xB0052F04`, whose bytes in the loaded image are
   `FF FF FF FF | 00 00 00 00 | 00 00 00 00 | 00 00 00 03`; the probe observed
   `arg[10..13] == -1, 0, 0, 3`.

### 5.4 Runtime evidence that it works

```
CALL 511B DVIO (16 args) at PC=0xB004ACBF
  IN: DeviceNo=1 (1), OutBytes=2, OutBuf=0xB0049430, MaxNo=12, InBuf=0xB0001C2E
  504B DVOUTS: Wrote 2 bytes to console (device 1)
  503B DVINST: Read 12 bytes from console (device 1)
EXIT 511B DVIO -> SUCCESS
```
`MaxNo=12` arrives via arg[14] exactly as mapped, and it consumes a 12-byte line
(`"LIST-STATUS\r"`).

### 5.5 Implement by REUSE, not duplication

503B's and 504B's bodies were extracted into cores (`mon_dvinst_read`, `mon_dvouts_write`)
and 511B delegates to both. Because args 4..13 keep their indices, the DVINST core reads the
strategy/table arguments from the context unchanged for BOTH calls; only DeviceNo, MaxNo, the
returned-count index and the buffer are parameterised. The cores deliberately do NOT set
success - the caller owns the final status, because 511B still has its input phase to run
after the output phase returns.

Suspend semantics carry through: if the device has no input the DVINST core sets
`wait_requested`, the MON is left **uncommitted**, the CPU rewinds to the CALLG and the whole
511B (prompt included) re-runs on resume. Do not commit the returned count on that path.

### 5.6 CORRECTION to section 2 - who spins on device-0 EOF

Section 2 / earlier notes said NC's resident reader busy-spins if an empty device-0 `1B INBT`
returns EOF. **That is wrong.** Re-tested 2026-07-16 with an explicit experiment: it is the
**LINKER** that spins - 20,088 consecutive `1B INBT` at `PC=0xB004E759` within 600k
instructions, no other MON activity. NC now reads device 1 and never touches device 0.
The suspend-on-empty behaviour for device 0 is therefore required by the **linker**. Keep it.

### 5.7 Still open (do NOT guess)

- **The linker uses BOTH channels**: device 1 (terminal) for its startup dialogue via 511B
  DVIO, and device 0 (command buffer) for its command loop via `1B INBT` @ `B004E759`.
  Feeding device 0 forces BATCH mode and it then spins on `71B DESCF` @ `B004D9E6`;
  returning EOF spins; suspending blocks. **The correct way to drive the command loop is
  unresolved** - this is the remaining blocker for producing a `:DOM`.
- **args 6..9 semantics.** The caller fills `b.0x104..` via an `h smove` from a `[len][ptr]`
  descriptor at `0xB0052854` / `0xB005285C`, so they are NOT plainly a 128-bit break table in
  the linker's usage. Passing indices 4..13 through to the DVINST core is what the linker's
  own DVINST call site does with the identical values, so this does not block 511B - but do
  not document them as a break table.
