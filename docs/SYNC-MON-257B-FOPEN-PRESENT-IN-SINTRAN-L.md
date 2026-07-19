# SYNC: MON 257B FOPEN is PRESENT in SINTRAN L - implement it + report it present

Full path: /home/ronny/repos/nd500x/docs/SYNC-MON-257B-FOPEN-PRESENT-IN-SINTRAN-L.md

## Ground truth (carve-verified, not inferred)

The ND Linker (B01) OPEN-DOMAIN command, right after it creates the .DOM file,
calls `312B MOINF` to ask "does MON 257B exist in this SINTRAN?" and BRANCHES on
the answer:

- 257B present  -> the linker CALLS `257B FOPEN` to find the file number of its
  already-open error-message file (UE-ERMSG-EN-C:ERR) before reading a message
  block from it.
- 257B absent   -> a fallback path that re-opens the error file with `50B OPEN`.

Whether real SINTRAN L (VSX-500, the linker's target - `262B CPUST` returns
OS=VSX-500, Version=L) actually HAS 257B was settled by reading the monitor-call
table directly from the carved segment:

- Table: `MCTAB / 9MCTA @ 005620B`, 256 16-bit words indexed by MON#, in segment
  `044-S3IDPIT`. `0` = call not generated into this system; non-zero = present,
  the word IS the dispatch entry. (Mechanism carve:
  /mnt/e/Dev/Ronny/NDInsight/SINTRAN/ND500/mon-oracle-for-NC/312B-MOINF_317B-UECOM.md)
- Segment file:
  /mnt/e/Dev/Ronny/NDInsight/tools/sintran-segment-carver/versions/L-VSX-500/segments/044-S3IDPIT.bin
- dd formula (byte offset of MCTAB[N] = 1824 + N_decimal*2; verified against the
  oracle's own 312B/317B reads):
    * MCTAB[312B] (N=202): `dd bs=1 skip=2228 count=2` -> `35 80` = 032600B = MOINF (matches oracle)
    * MCTAB[257B] (N=175): `dd bs=1 skip=2174 count=2` -> `92 8a` = **0x928A = 0111212B, NON-ZERO**

Therefore **MON 257B FOPEN IS PRESENT in SINTRAN L**. Reporting it absent (which
both emulators did, because 257B was a STUB) forces the linker down a fallback
path and is INCORRECT for the L system we emulate.

(K = MCTAB[257B]=0x7278, M = 0x2499 - both also non-zero; only K,L,M checked.)

## nd500x (C) changes - MIRROR THESE IN C#

1. `src/libmon/mon_registry.c`: MON 257B status
   `MON_STATUS_NOT_IMPLEMENTED` -> `MON_STATUS_IN_PROGRESS`. The MON executor
   short-circuits STUB/NOT_IMPLEMENTED calls to an "unimplemented MON" stop
   BEFORE dispatching to the handler, and `312B MOINF` reports a call present
   iff its status is VALIDATED or IN_PROGRESS. So the status flip is what both
   (a) lets the handler actually run and (b) makes MOINF report 257B present.

2. `src/libmon/handlers/mon_257B_OpenFileInfo.c`: real implementation (was a
   STUB that returned error -1). Contract as the linker issues it (5-arg CALLG):
     [I] FileName   (STRING descriptor [len:4][ptr:4])
     [I] FileType   (STRING descriptor [len:4][ptr:4])
     [O] FileNo     (INTEGER word)  - open file number (64..127) if found
     [O] AccessCode (INTEGER word)  - 0=read, 1=write, 2=read+write
     [O] DevNo      (INTEGER word)  - LDN for peripheral files, else 0
   Behaviour: search the open-file table (file numbers 64..127) for an in-use
   entry whose object_name (+ type, when supplied) matches, case-insensitively,
   the requested name. Found -> write FileNo/AccessCode/DevNo, return SUCCESS
   with K SET. Not open -> return error 056B (NO SUCH FILE NAME) with K CLEAR so
   the linker's if-not-k branch opens the file itself. access_mode -> AccessCode
   map: {0 SEQ_WRITE,5 SEQ_APPEND}->1(write); {1 SEQ_READ,3 RAND_READ,7
   RAND_READ_CTG}->0(read); else ->2(read+write).

3. `src/libmon/mon_log.h`: added `MON_ID_257B` ("257B","FOPEN","OpenFileInfo").

## Validation

- `test_instruction_validation --continue`: 40061/40061 PASS (unchanged).
- NC compile gate `ctest -R dom_nc_compile`: 4/4 PASS (unchanged).
- Linker OPEN-DOMAIN now takes the real "257B present" path and CALLS FOPEN. In
  our sandbox FOPEN returns "not open" (our startup closes the error file that
  real SINTRAN keeps open - a separate, non-blocking emulation gap), so the
  linker re-opens it; console output is byte-identical to before. No regression.

## What this fix does NOT do (important for scope)

It does NOT fix the linker's post-OPEN-DOMAIN blocker. `LOAD B:NRF` after
`OPEN-DOMAIN "A-TEST"` still fails with `*** ERROR - Command not valid when no
current domain or segment exists. (0054:67)`. FOPEN is error-message-file
infrastructure, orthogonal to current-domain state. See the current-domain
findings in /home/ronny/repos/nd500x/docs/LINKER-LOAD-ERROR52-INVESTIGATION.md.
