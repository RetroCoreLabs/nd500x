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
