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

REMAINING for end-to-end BOUT.NRF (Route B):
1. 317B UECOM must actually RUN the named subsystem nested (load cat-cat5-b06.dom,
   run to MON 0B LEAVE, return to NC) sharing the scratch file/segment.
2. The NC->CAT-500 parameter handshake (which CAT file, which NRF name) still needs
   tracing - CAT-500 gets it via its command record (device 0/1), not from the bare
   "CAT-CAT5-B" name. Under investigation.
Both emulators need the same nested-invocation model; keep bit-aligned.
