# Carve analysis: MON 60B / N500M gateway + the CAT-500 handshake levers

Full path: /home/ronny/repos/nd500x/docs/CARVE_ANALYSIS_MON60_AND_HANDSHAKE.md

Source of truth analysed: the byte-verified SINTRAN III L-VSX-500 (L07) segment carve at
`/mnt/e/Dev/Ronny/NDInsight/tools/sintran-segment-carver/versions/L-VSX-500/`
(the `re/mon-analysis/` and `re/ND500-SYSTEM-MONITOR/` trees).

This document records (b) what I learned going deeper into the MON 60B / N500M gateway,
(a) exactly what the CAT-500 prompt-hang experiment needs, and why the gateway is NOT on
the path for the experiment.

---

## 1. MON 60B / N500M is the ND-100 -> ND-500 gateway, NOT NC's MON path

`re/mon-analysis/60B-N500M/README.md` (dispatch + all 47 subfunctions byte-verified):

- MON 60B `N500M` is the call by which an **ND-100 host program** controls the ND-500
  coprocessor: register/memory access, process control, control-store (microcode) load,
  memory config, domain management. `A` -> param list; `params[0]` = subfunction code
  (0B..177B), dispatched through `5IFUNC` (worker/param-prep side, segment `050-S3I5PIT`)
  and `FUNCS` (server side, segment `030-S3SM5`).
- The full mechanism is carved end to end: caller thunk -> gateway -> MON 60 -> N500M
  `5IFUNC[code]` (param prep) -> `5NOPAR` -> `FPT2ENTRY`/`5FP2E` -> `FUNCS[code]` ->
  ND-500 op; plus the 3022 IOX bus register map, the control-store gate (`RSTA5` bit 9
  `5CLOST`), the 5MPM mailbox message + `ACT50` activation, and the level-12 answer ISR
  (`5STDR -> CHN5S -> DECOM -> MCHAN`, dispatch on MICFU: `SAT 24`=monitor-call,
  `SAT 25`=trace), living in the RESIDENT interrupt segment `026-S3IMPIT`.

**KEY CONCLUSION for nd500x:** this whole path is the *two-processor split* (ND-100 CPU
driving the ND-500 CPU over the 3022 bus). nd500x emulates ONLY the ND-500 side and
services ND-500 MON calls directly (intercepting CALLG into segment 31). So the MON 60B
gateway / 5MPM handshake / IOX bus is **background architecture, not on the path** for NC
or CAT-500 running under nd500x. It becomes relevant only if we ever emulate the ND-100
host half. It does NOT gate the CAT-500 experiment.

## 2. How NC's / CAT-500's own MON calls really dispatch (the ND-500 side)

From `re/mon-analysis/503B-InputString/README.md`: an ND-500 process moncall traps to
**level-12** (MOCALL) -> `MCHANDEL` reads `MCNO` -> the resident **level-12 GOSW**
(`5CMNO`/`L12MIN`) -> handler (e.g. slot 3 = `NINSTR` for 503B). This is a DIFFERENT
dispatch from the ND-100 `GOTAB`/`MCTAB` path. In nd500x we replace this entire trap+GOSW
with our own C handler table, so the only thing that matters is that each handler produces
the correct *observable contract*. The carve gives those contracts (below).

## 3. The two byte-verified contracts that drive the experiment

### 3a. 143B RSIO - execution mode (the prompt-hang lever)
`re/mon-analysis/143B-ExecutionInfo/` (dispatch + control flow byte-verified; field
meanings inferred from manual ND-860228.2):
- Returns execution mode `0=interactive, 1=batch, 2=mode job, 3=RT`, plus command-input
  device, command-output device, owner dir+user index.
- nd500x `src/libmon/handlers/mon_143B_ExecutionInfo.c:33` HARDCODES
  `DEFAULT_EXEC_MODE = 0` (interactive), input_dev=0, output_dev=1.
- OBSERVED (from booting cat-cat5-b06.dom under nd500x earlier this session): CAT-500's
  init sequence issues `143B RSIO`. So the mode nd500x returns is live-relevant: returning
  interactive is the suspected reason CAT-500 then prints `Cat-500: ` and blocks on 503B.

### 3b. 317B UECOM - synchronous execute-a-command-and-return
`re/mon-analysis/317B-ExecuteCommand/` (byte-verified end to end):
- UECOM/COMSB/UELOG share one body; a mode code at frame `,B -175` distinguishes them
  (COMSB=1, UECOM=2/4, UELOG=3). The body upper-cases the command IN PLACE and calls the
  standard SINTRAN command decoder (`JPL I 41`), then **returns synchronously after the
  command has run**. So "CAT-CAT5-B" is run exactly as a typed command; UECOM must actually
  load+run the subsystem and return. Confirms Route B is mandatory.
- Divergences to note for both emulators: (i) real UECOM upper-cases the guest command
  buffer in place - nd500x reads into a local copy; (ii) two UECOM variants (mode 2 vs 4,
  a range test on `T`) whose meaning is NOT byte-proven - do not guess.

## 4. What experiment (a) needs - checklist

Goal: determine empirically whether CAT-500's prompt-hang is JUST the 143B mode flag, i.e.
whether returning batch mode makes CAT-500 skip the `Cat-500:` prompt and instead read its
command non-interactively (from the command channel / scratch).

Needs:
1. The CAT-500 binary: `/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06.dom`.
2. nd500x built with a way to make 143B RSIO return mode=1 (batch). Simplest: an env-gated
   override in `mon_143B_ExecutionInfo.c` (e.g. `ND500X_EXEC_MODE`), so the experiment does
   NOT hardcode a behavioural change before we know it is correct. (ASSUME NOTHING: prove
   the mode gates the prompt before committing the change.)
3. A sandbox run harness that loads the dom and single-steps/runs, logging: the 143B call
   and its returned mode, whether the `Cat-500:` prompt (504B DVOUTS) still fires, and where
   it stops (STOP_WAIT_INPUT on 503B = still prompting; anything else = mode changed the path).
   Reuse the existing diag harness pattern (build/nc_sandbox, ND500X_PIN_CLOCK=1) and MON log.
4. Compare mode=0 (current) vs mode=1 (override) runs: does the 503B suspend disappear?

## 4a. EXPERIMENT RESULT (2026-07-15) - hypothesis DISPROVEN

Ran cat-cat5-b06.dom standalone under build/bin/diag_domload in build/nc_sandbox
(SCRATCH64.DATA present), comparing mode=0 (default) vs ND500X_EXEC_MODE=1 (batch),
via an env-gated override temporarily added to mon_143B_ExecutionInfo.c.

**Outcome: the 143B RSIO execution mode does NOT gate CAT-500's prompt.** In BOTH modes
CAT-500 prints the banner + `Cat-500:` prompt (504B DVOUTS @0x0801F0C8) and then reads a
command via 503B DVINST @0x0801F0F1. Identical control flow; mode 0 vs 1 changed nothing.
The env lever was REVERTED (its premise is false - committing a batch-mode change would be
a false fix). This is exactly what running the experiment before implementing was for.

**What the run DID reveal (all byte-observed, not assumed):**
- CAT-500 init issues, in order: 11B TIME, 114B TUSED, 143B RSIO, 422B GSWSP x2, a 2nd
  143B RSIO @0x0801EF73, then 41B ROBJE @0x0801EC43 (open object), 76B SETBS, 62B RMAX,
  **117B RFILE @0x0801EE8F (reads its input file)**, 73B SMAX, 504B DVOUTS (banner+prompt),
  2B OUTBT. So the object open + read path already works under nd500x.
- Feeding console "EXIT\r" -> CAT-500 runs a short summary (32B MSG x several, 114B TUSED,
  11B TIME) and exits cleanly via **0B LEAVE** @0x0801E4EA. So the blocking-read + console
  feed mechanism functions, and "the command stream ended" is a real, reachable clean exit.
- NO run ever reached the MAIN code-gen driver 0x0800D4E2 (the routine that maps the CAT
  input, runs the codegen loop, and writes the :NRF via 120B WFILE @0x0801EED7). Feeding
  guessed words ("SCRATCH","COMPILE") just blocked again on the next 503B read.

**Corrected model (from CAT-500's own disassembly, cat-cat5-b06.asm):** the command
dispatcher @0x0801EB04 is NOT a keyword matcher. It reads/advances an *active input-record
pointer* `$0x80232BC`, tests the record, and on empty emits 32B MSG + 0B LEAVE. The two
"open a device" helpers (@0x0801EB63/0x0801EB88, specs `$0x8023304`/`$0x8023318`) feed that
record stream. CAT-500 is therefore driven by a **command-record stream from a device/file**
(device-0 command file / device-1 terminal), the interactive `Cat-500:` 503B prompt being
only the fallback. This is the SAME shape as the linker's in-memory command-record poll
(see memory linker-interactive-input-blocker). Reaching 0x0800D4E2 requires populating that
record stream with the command(s) NC would supply.

**Corrected next investigation (NOT an implementation yet - do not guess):**
1. Determine what device/file the two open-device helpers attach (`$0x8023304`/`$0x8023318`)
   and what NC writes there - the CAT-500 command/control stream format.
2. Trace where in the dispatch after a non-empty record CAT-500 branches into the MAIN driver
   0x0800D4E2 (i.e. what a real command record contains).
Only once a real command record is known can code-gen (120B WFILE -> BOUT.NRF) be driven.

## 4b. BREAKTHROUGH (2026-07-15) - the CAT-500 command is KNOWN, from NC's own control stream

The handshake question is ANSWERED, and not by guessing: NC's own output contains the command.

**/home/ronny/repos/nd500x/build/nc_sandbox/SCRATCH/SCRATCH64.DATA (8192 bytes) is NOT the
program - it is NC's CONTROL/COMMAND stream for CAT-500**, in plain text. Verbatim content:

```
1234567890 / 1 / 0 / 0 / 1 / 1 / 75
options m2  a4  f-  r4  l+  d+  n+  s-  p-  i-  o-  pr- ic+ lm+ t-  a-  lo+
0 / 2 / SSI-CODE 438 / 1 / 37
generate-code,SCRATCH-00001:CAT,B:NRF          <-- THE COMMAND
0 0 0 0 0 1 4 / NC-A / ...
(later, CAT-500's own prompt strings:) "generate-code", "check", "CAT file: ",
"output file: ", "list file: ", "source file: "
```

- **The CAT-500 command is `generate-code,<CAT input>,<NRF output>`** - here
  `generate-code,SCRATCH-00001:CAT,B:NRF`. Earlier guesses ("COMPILE", "SCRATCH") were wrong.
- **/home/ronny/repos/nd500x/build/nc_sandbox/GUEST/B.CAT (2048 bytes)** is the separate CAT-code
  PROGRAM (type dictionary CHARPTR/INT/... then binary records incl. MAIN, PROG, ARGV, C!INIT).
- So CAT-500 needs TWO inputs: the control/command stream (SCRATCH64) + the CAT program.

**Colon->dot filename mapping: ALREADY IMPLEMENTED, verified - nothing to add.**
`mon_parse_sintran_name` (src/libmon/mon_path.c:136-163) splits on ':' and `mon_translate_path`
(mon_path.c:173+) joins host paths with '.'; `mon_build_host_path`
(src/libmon/mon_file_table.c:864) does the same NAME:EXT -> NAME.EXT conversion. Proven live:
```
MON OPEN: Opened './SCRATCH/SCRATCH-00001.CAT' as file number 65   (from "SCRATCH-00001:CAT")
MON OPEN: Opened './GUEST/B.NRF'               as file number 66   (from "B:NRF")
```
(An earlier apparent failure was MY test fixture placed at a literal-colon path, not an emulator
bug. `SCRATCH-` names auto-route to the SCRATCH dir via mon_path.c:198.)

**Feeding the command drives CAT-500 deep into real work (byte-observed):**
```
41B ROBJE  -> File 100(8) -> SCRATCH64.DAT        (scratch object entry)
76B SETBS  -> block size 4000(8) = 2048
62B RMAX   -> 20000(8) = 8192 bytes               (matches the real file)
117B RFILE -> read 2048 bytes                      (FULL, correct read)
50B OPEN   -> ./SCRATCH/SCRATCH-00001.CAT (file 65), ./GUEST/B.NRF (file 66)
412B FSCNT -> file 101(8)=65 connected as segment 4 (bytes=2048)   <- CAT input MAPPED
412B FSCNT -> file 100(8)=64 connected as segment 5 (writable=1, bytes=8192) <- scratch MAPPED
120B WFILE -> reached (NRF write path entered)
```
NOTE: the MON log prints numbers in OCTAL (%o). An apparent "asked 4000 got 2048 short read" was
a misread - 4000(8) = 2048(10), i.e. a full correct read. **No file-I/O or mapping bug exists.**
nd500x's 412B FSCNT is REAL (src/cpu/nd500_segment_alloc.c:296 maps actual file bytes); the
CAT-500 MON-contract doc's "412B is bookkeeping-only / the one hard blocker" note is STALE.

**CURRENT BLOCKER (precisely characterised, not guessed):** a repeating **page fault**
(`trapBit=0x4000000000` = `TRAP_PGF`, cpu_protos.h:113) at the command dispatcher
`0x0801EB1E`/`0x0801EB31`, dereferencing `dataAddr=0x20202020` - i.e. FOUR ASCII SPACES used as a
pointer. The dispatcher walks a record CHAIN (`0801EB06: record := record->next` via `$0x80232BC`);
a chain "next" field contains text, so the walk runs off into unmapped space and page-faults
forever (~198 instructions per retry). Occurs with BOTH the comma form
(`generate-code,SCRATCH-00001:CAT,B:NRF`) and the interactive form (`generate-code` then each
filename at its own prompt). Without any fed command there is NO trap (it just blocks) - so the
fed command IS parsed, but into a corrupt record chain.

**Leading hypothesis to test next (UNVERIFIED - do not assume):** CAT-500 walks its command
records inside the MAPPED scratch segment. Our mapped bytes are the plain TEXT above (the
`options m2  a4 ...` line is full of 0x20 spaces - exactly the faulting value), so either
(i) CAT-500 expects binary records where we present text, or (ii) the file bytes land at the wrong
offset within the mapped segment, so the record walk starts at the wrong place. Next experiment:
dump the mapped segment-4/5 contents at the address the dispatcher reads (`$0x80232BC` and what it
points at) and compare against the on-disk bytes to see whether the offset/base is right.

## 4c. COMMAND PROTOCOL SOLVED; generator now fails with "can't generate code" (2026-07-15)

**CAT-500's interactive protocol is fully established (byte-observed console dialogue):**
```
CAT-500 - Version B06 - 1988-01-05
Cat-500: generate-code
CAT file: SCRATCH-00001:CAT
object file: B:NRF
code generation
*ERROR*   can't generate code
```
Prompts in order: `Cat-500: ` -> `CAT file: ` -> `object file: `. Success message would be
"code generation : ok" (dom file offset 221212); failure string "can't generate code" (offset
148046). Single commands never trap: `EXIT` -> clean 0B LEAVE; `help` prints help;
`generate-code` alone prompts for its args. Both the comma form and the prompt-by-prompt form
are accepted.

**DIAGNOSTIC GOTCHA (cost real time - document for both emulators):** calling
`mon_queue_console_input()` installs a ConsoleIO whose `write_char` captures output into an
internal buffer (`queued_console_write_char`, src/libmon/mon_file_table.c:689), so
`mon_504B_OutputString.c:102` prefers it over `putchar` and **all guest console output becomes
invisible on stdout**. Harnesses must print `mon_get_console_output()` to see banners/prompts.

**Verified CORRECT (so NOT the cause) - each checked, not assumed:**
- CAT input segment mapping is byte-perfect. Probing the VA CAT-500 derives from the returned
  segment number (VA = seg << 27): `seg 4 base 0x20000000` reads
  `"..CHARPTR..UCHARPTR..WORDPTR.."` = exactly the CAT file's bytes.
- `seg 5` (the scratch, mapped `accessType=1`, `writable=1`) reads all zeros - and that is
  CORRECT: accessType 1 = "uninitialized/empty" per the 412B handler's documented contract
  (ND-860228.2), so nd500x deliberately leaves the pages zeroed. CAT-500 maps it to WRITE.
  (Caveat: taken from the handler's comment citing the manual; the manual page itself not read.)
- NC's compile SUCCEEDS: GUEST/B.LIST says "Norsk Data C - Version: A06 - 1989-01-10 ...
  *** no errors detected ***". The CAT is complete (2048 bytes, 0xF0-padded tail, no zero-fill).
- **NC creates the CAT itself**: `221B CRALF FileName='SCRATCH-00001:CAT'` ->
  `./SCRATCH/SCRATCH-00001.CAT`, and it DELETES any pre-existing copy. So hand-substituting
  GUEST/B.CAT is both unnecessary and futile. Running CAT-500 against NC's REAL output gives the
  SAME "can't generate code".
- Ruled out: 143B exec mode; colon->dot mapping; 0x27 vs CR terminator; command case; short
  reads (octal misread); 412B being a stub.
- The "record chain" corruption (0x20202020) is a SYMPTOM: 0x0802xxxx there holds MESSAGE
  STRINGS, so the page-fault loop is CAT-500's error-reporting path after the failure, not the cause.

**REMAINING BLOCKER:** CAT-500's generator itself rejects the input ("can't generate code")
even on genuine NC output. **Leading suspicion (UNVERIFIED - do not act on it as fact):** a
version skew - NC is **A06 (1989-01-10)** but CAT-500 is **B06 (1988-01-05)**, and the CAT
header's byte 1 differs between artifacts (`d6` in NC's SCRATCH-00001.CAT vs `d0` in
GUEST/B.CAT), which may be a CAT format/version field. Only ONE CAT-500 exists on disk
(/mnt/d/ND/500/CAT5-CAT/cat-cat5-b06.dom); no newer back-end is available to test against.

**Next steps (in order):** (1) find the branch that selects "can't generate code" (data VA
~0x0802324E) and identify the exact predicate it tests; (2) determine whether the CAT header
encodes a format version and whether B06 accepts what A06 emits; (3) only then consider
whether a different CAT-500 build is needed. Do NOT assume a nd500x bug: nothing in the file
I/O, mapping, or command path is currently disproven.

## 4d. FAILURE ISOLATED: no MON call is at fault (2026-07-15)

Traced the exact failure window with build/bin/diag_cat_record (PC ring buffer that fires when
"can't generate code" first reaches the console buffer). The sequence, byte-observed:

```
50B OPEN x2            -> ./SCRATCH/SCRATCH-00001.CAT (file 65), ./GUEST/B.NRF (file 66)
504B DVOUTS            -> "code generation "
412B FSCNT (file 101(8)=65, AccessType=0) -> SUCCESS, segment 4, bytes=2048   [CAT input]
412B FSCNT (file 100(8)=64, AccessType=1) -> SUCCESS, segment 5, bytes=8192   [scratch]
504B DVOUTS            -> "\r\n"
504B DVOUTS            -> "*ERROR*"   ... "can't generate code"
```

**There is NO MON call between the successful mappings and the error.** Both FSCNT calls
return SUCCESS with K cleared. So the failure is CAT-500's OWN computation over data that we
have independently verified is correctly mapped. **No MON handler / no file I/O is implicated.**

- `321B UEADM` returning "Deprecated MON call - returning error" is a RED HERRING: it is called
  only AFTER the error, on the cleanup path. (`413B FSCDNT -> ERROR` is likewise on teardown.)
  Both are still worth fixing eventually, but neither causes this.
- Failure-path PCs: the record loop `0801D68F -> 0801D3C0..0801D44C -> 0801C1E4 -> 0801D32B..
  0801D3BD` (repeating), then `0801C517..0801C557` -> `call $0x801D735` (generic message
  emitter) -> `0801F0BD/0801F0C8` (504B DVOUTS). `0801C557` only SELECTS a message; the
  decision is upstream in the record loop.
- Error/message strings live in DSEG. Anchored exactly from two known file offsets:
  "subrange or index out of range" (file 0x4424E) -> VA 0x0802324E and "exception handler
  missing" (file 0x442CE) -> VA 0x080232CE, giving **DSEG VA = file_offset + 0x07FDF000**
  (data base 0x08000000). Verified live: "can't generate code" (file 0x2424E) reads at DSEG
  **0x0800324E**. It has NO direct code reference - it is reached via a descriptor/table.

**DIAGNOSTIC CORRECTION (my own bug, not the emulator's):** the ND-500 is BIG-ENDIAN, so for a
32-bit read at A, byte[A] is the HIGH byte ((w>>24)&0xFF). An earlier dump helper used
(w & 0xFF), i.e. byte[A+3], shifting every dump by 3 and making the DSEG message table look
like binary "records". That is what produced the earlier (wrong) "record chain" reading.
Endianness confirmed: seg 4 word 0xD0020013 == the CAT file's leading bytes d0 02 00 13.

**Remaining candidates for "can't generate code" (ALL UNVERIFIED - do not treat as fact):**
1. **Segment->address convention.** nd500x places file bytes such that segment N is readable at
   VA = N<<27 (seg 4 -> 0x20000000, verified to contain the CAT bytes). But statically, the
   segment number FSCNT returns into `$0x80146A0` is only ever consumed to DISCONNECT later
   (`0800C54F` -> 413B FSCDNT); it is never shifted into a data address in that routine. So how
   CAT-500 actually addresses the mapped file is NOT yet established. If it expects the file at
   some other address (or expects SINTRAN to map it into an existing pointer), it would read
   nothing and fail exactly here. **This is the top suspect and the next thing to establish.**
2. CAT format/version skew: NC is A06 (1989-01-10), CAT-500 is B06 (1988-01-05); the CAT header
   byte differs between artifacts (d6 vs d0). Only one CAT-500 build exists on disk.
3. A CPU-level computation bug in nd500x affecting this code path.

**Next concrete step:** determine empirically whether CAT-500 ever READS VA 0x20000000 (the
mapped CAT). If it never reads there, candidate 1 is confirmed and the fix is in
nd500_mon_connect_file_as_segment / the segment addressing convention. If it does read there
and still rejects the data, candidate 2/3 move up.

## 4e. HARD FACT: CAT-500 never reads the mapped CAT file (2026-07-15)

Enabled `nd500_dbg_set_memtrace(MEMTRACE_READ)` for exactly the code-generation window
(from the "code generation" console output until "can't generate code"). Result over the
**11,102 reads** in that window:

| vaddr region | reads | what it is |
|---|---|---|
| `0x10xxxxxx` | 7107 | stack / frame |
| `0x08xxxxxx` | 3039 | CAT-500's own code+data (PSEG/DSEG base 0x08000000) |
| `0x18xxxxxx` |  889 | **segment 3 = its GSWSP working segment** (internal structures) |
| `0x00xxxxxx` |   67 | low memory |
| **`0x2xxxxxxx`** | **0** | **the FSCNT-mapped CAT input - NEVER TOUCHED** |
| `0x28xxxxxx` |    0 | the FSCNT-mapped scratch - never touched |

No read anywhere in the window returns the CAT header (`0xD60200`/`0xD00200`): **zero hits**.

**So CAT-500 maps both files successfully and then fails WITHOUT EVER READING THE CAT.** It
only churns over its own working segment (422B GSWSP assigned it segments 2 and 3; it reads
`0x18xxxxxx` = segment 3). This kills the "it read our bytes and rejected the format" theory
AND means the earlier "segment->address convention" suspicion cannot be concluded from this
alone - it never gets as far as addressing the file.

**The FSCNT result check is NOT the failure** (byte-verified):
```
0801F08B: MON 412B FSCNT
0801F096: if -k go 0801F0A3      ; K clear (success) -> jump
0801F099: h1 =: $0x80224F0       ; error path: store error code
0801F0A3: h stz $0x80224F0       ; success path: zero the error cell
0801F2D6: h1 := $0x80224F0 ; ret ; the "get status" routine the caller tests
0800C5B4: call $0x801F2D6 ; 0800C5BD: h test b.0x18 ; non-zero -> error
```
nd500x's 412B clears K and returns SUCCESS ("mon_set_success: clearing K flag (K=0)",
"EXIT 412B FSCNT -> SUCCESS"), so the error cell is zeroed and this check PASSES.

**Therefore the failure is a decision CAT-500 makes on its own internal state, after a
successful mapping and before reading the file.** The failure-path loop is
`0801D68F -> 0801D3C0..0801D44C -> 0801C1E4 -> 0801D32B..0801D3BD` (repeating), exiting to
`0801C517..0801C557` -> `call $0x801D735` (message emitter) -> 504B DVOUTS.

**Open question / next step:** identify what state that loop tests. Since the CAT is never
read, the likely culprits are (ALL UNVERIFIED): a setup step CAT-500 expects between mapping
and reading (e.g. it may expect FSCNT to hand back an ADDRESS or to map into an EXISTING
working segment rather than allocating a NEW segment 4/5 - note it passes LogSegmentNo=0
"first free" and only ever uses the returned number to DISCONNECT via 413B FSCDNT); or a
missing/incorrect value in a structure it built earlier. Instrument the loop at 0801D3C0 /
0801D32B and find the predicate. Do NOT assume the nd500x segment convention is wrong until
that predicate is known.

## 5. What (b) tells me I do NOT need for (a)
- The MON 60B gateway, 5IFUNC/FUNCS tables, 3022 IOX bus, 5MPM message, control-store gate,
  level-12 return ISR: all irrelevant to the single-CPU nd500x experiment. Documented here
  only so a future ND-100-host emulation effort has the pointer.

Related: /home/ronny/repos/nd500x/docs/CSHARP_HANDOFF_BLOCKING_READ_SESSION.md ,
memory cat500-route.md , blocking-read-semantics.md .
