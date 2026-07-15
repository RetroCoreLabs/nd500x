# C# / RetroCore sync handoff - MON-call contracts (NC + LINKER)

> **STATUS: HELD - DO NOT SEND TO C# YET.** This is a working changelog, not a released
> handoff. It is released only after Phase 5 of `/home/ronny/repos/nd500x/docs/MON_TO_BINARY_PLAN.md`,
> i.e. after we can actually compile a C source to an object AND link it to a runnable binary
> under nd500x, with every change in the Verification Tracker marked VERIFIED. Until then, treat
> everything here as unverified/provisional.

**Full path:** `/home/ronny/repos/nd500x/docs/MON_CSHARP_SYNC_HANDOFF.md`
Date: 2026-07-14
Audience: the LLM maintaining the C# RetroCore ND-500 emulator
(`/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500/` + its MON layer).

## Read me first

- RetroCore is NOT an oracle and nd500x is NOT an oracle. The ONLY ground truth is the carved
  SINTRAN L07 (`/mnt/e/Dev/Ronny/NDInsight/tools/sintran-segment-carver/versions/L-VSX-500/re/mon-analysis/`)
  plus the ND manuals. This handoff records what nd500x changed and why, sourced from the carve,
  so the two emulators converge on the carve - not on each other.
- Full per-call status matrix: `/home/ronny/repos/nd500x/docs/MON_COMPLETENESS_MATRIX.md`.
- ND-500 MON INTEGER params are 32-bit words (W). "INTEGER2" labels in specs are ND-100 carryover.
- Every "UNVERIFIED"/"INFERRED" tag below means: not byte-proven in the carve; needs a live
  single-step trace to confirm. Flag it identically in C#; do not harden it into fact.

## PART A - CPU fixes already landed in nd500x (apply to RetroCore CPU)

These are committed in nd500x (`git log` on branch main, 2026-07-14). They gate whether NC parses.

1. **SCOMP byte-difference S flag was INVERTED.** Per ND-500 Ref Manual sect 14.10 (p254): a
   SMALLER byte in source-1 (source1 < source2) sets S=1; a GREATER byte sets S=0 - matching the
   COMP sign convention that the shared conditional branches (`if>=go` tests S=0, `if<go` tests
   S=1) rely on. RetroCore had/has the same inversion pattern - fix `Scomp` so S is set when
   source1 < source2. This unblocked NC's keyword binary search (type keywords now parse).
   **Cost:** 11 SCOMP ByteDiff test cases encode the OLD wrong convention and must be regenerated
   in `/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.Tests.ND500/Validation/`.

2. **COMP2 float (F/D) compare** must decode operands to IEEE and compare as floats, not integer-
   subtract the raw ND float bit patterns. Set Z on equal, S on op1<op2, C on op1>=op2.

3. **Non-ignorable traps (PV/ISE/THM/PGF, bits 32-41)** must be delivered to the running program
   via its THA vector, not treated as a fatal halt. Halt only when THA==0 / slot==0 (Trap Handler
   Missing) or when already inside a handler (double fault). NC installs THA[36]=PV handler and
   relies on this.

## PART B - MON handler contract fixes landed in nd500x (mirror in RetroCore MON layer)

Files under `/home/ronny/repos/nd500x/src/libmon/handlers/`.

### 117B RFILE (ReadFromFile) - prior session, still relevant
Error 3 (EOF) is a RANGE check raised only when the requested block is ENTIRELY beyond the file's
data. A within-bounds SHORT read returns SUCCESS (K clear) and writes the ACTUAL bytes-read count
back to the caller's NoOfBytes arg. Only bytes_read==0 returns error 3. (Carve 006-S3FS worker
102130B; success path 102433-102470 writes recomputed count.)

### 73B SMAX + 43B CLOSE - truncation deferral (this session)
SMAX only RECORDS the logical max-byte length; it must NOT ftruncate immediately (that shortens a
scratch under the program's feet). The physical truncation is applied at CLOSE. nd500x added a
`max_bytes_set` flag on the open-file entry, set by 73B, and CLOSE ftruncates to `bytes_in_file`
only when that flag is set and the file is non-scratch. RetroCore should mirror: SMAX records,
CLOSE applies. (Carve does NOT byte-prove the SMAX->CLOSE link; it is the consistent model.)

### 312B MOINF (CheckMonCall) - prior session
For 321B UEADM, return MCTAB[321B] = octal 065453 (carve-verified). NC only takes its "UEADM
available" path when this is non-zero.

### 123B RELES (ReleaseResource) - this session
Releasing a VALID-but-unreserved device/file is an idempotent SUCCESS no-op (return code 0,
K clear). Do NOT return error 5 "Device not reserved" - error 5 belongs to the I/O operations
that REQUIRE a reservation, never to RELES. Only a genuinely invalid device number / bad io_flag
errors (nd500x uses 174B). Carve: BRELEASE primitive BRELE=010610B branches on DF.RESLI=0 straight
to register-restore/return with no error loaded.

### 64B ERMSG (WarningMessage) - this session
The carved ERMSG worker (dual entry ERMSG/QERMS @16714B, closes at 17020B EXIT) has NO error/K
return path - it looks up the message text, writes it, and always continues. Do NOT fabricate an
error return for ErrCode==0 ("0 is illegal" is a caller-side manual note, not a status this call
returns). nd500x removed that branch. (Message-text lookup itself is still a stub on both sides -
real fix = appendix-A code->text table routed to terminal/error device.)

### 256B DEABF (FullFileName) - this session, matters for NC create-if-missing
- Accept the ND-500 **2-argument** form (AbrevName, FullName). The default-file-type param 3 is
  ND-100-only and IGNORED on ND-500. Requiring 3 args rejects NC/linker's real 2-arg CALLG.
- Resolve the abbreviated name to a real file. On SUCCESS write the expanded name (apostrophe-
  0x27-terminated) to the FullName buffer and return success. On an UNRESOLVED name set the K flag
  and return error **46 (056B NO SUCH FILE NAME)** - the exact code NC tests to decide whether to
  CREATE the file. A pass-through echo that always succeeds silently breaks NC.
  (VERIFIED vs carve 006-S3FS DEABF + NC-oracle tier3; ND-500 body does not cleanly decode so field
  assembly follows the manual.)

### 50B OPEN - prior session
Added missing `break` so the -52 illegal-parameter case does not fall through to no-such-file.

## PART C - Contract corrections nd500x has NOT yet applied (both emulators should)

Low/medium priority; audited this session, fixes described, not yet coded:

- **11B TIME:** return is a DOUBLE WORD (LONGINT) in A/D, not a single 32-bit word. Also route
  through the pinned clock for consistency with 113B/114B.
- **12B SETCM:** cap the stored command buffer at 32 chars (documented capacity).
- **114B TUSED:** optional byte-proven entry arg-check that returns error selector 147 on mismatch.
- **162B OUTST:** OUTPUT must stop at the 0x27 (`'`) terminator (primary), byte-count secondary;
  accept a 2-arg (device+string) call; `$`(0x24) emits `$`+0x0A. Carve 025-S3IRPIT worker breaks
  on 047 octal. A count-only implementation mis-handles the linker's terminator-delimited 2-arg call.
- **221B CreateFile:** omitted NoOfPages should default to 0 (indexed, no prealloc), not 1.
- **412B FSCNT:** return the assigned SegmentNo through the OUTPUT arg slot (like 422B GSWSP's
  `mon_write_param_word`), not the deprecated W1/error-code channel; and actually connect the file
  as an MMU/PST segment (currently bookkeeping-only). AccessType may be 0..3 (widen the >2 reject).

## PART D - STUB handlers the LINKER needs implemented (both emulators lack these)

All are `mon_set_error(-1)` stubs in nd500x today. Implement per carve:

- **71B DESCF / 72B EESCF:** currently RETURN ERROR (bug). Must be SUCCESS (disable/enable escape
  flag or no-op). Both emulators.
- **104B HOLD / 514B 5TMOUT:** no scheduler -> validate (TimeUnit/UnitType in 1..4, on bad ->
  error 124 dec = 174B octal; count 0 = immediate) then SUCCESS no-op (no real wait). 514B also
  writes RestartReason=0 to its param 2.
- **254B GERDV:** carve byte-verified TWO outputs [ErrorDevice, RTProgram]; RTProgram=0 when
  unreserved. Order matters.
- **257B FOPEN:** resolve open file -> FileNo/AccessCode(0/1/2)/DevNo; not-found = SINTRAN error
  octal 122 family (not -1); on error path FileNo carries the LDN for peripheral files.
- **66B ISIZE:** return bytes currently in the input buffer (device 1 = own terminal) in W1.
- **214B GUSNA / 217B GUIOI / 273B MGFIL:** user/file index+name lookups from the open-file/user
  tables; note the ND-100 packed dir|user word vs the ND-500 separate-INTEGER outputs.
- **263B GDEVT:** DevType 0..7 + 32-bit DevAttr from the device table (codes inferred, confirm
  vs manual/yaml).
- **322B GSGNO:** input is a 6-char segment NAME (3 words), NOT one word; look up in the segment
  table, return the segment number.
- **423B CAPCOP:** 6 args SrcSeg, SrcType(0=data/1=prog), DstSeg(0=first-unused), DstType,
  AccCode(0=unchanged/1=RO/2=RW), RetSeg[O]. Needs a capability-copy over the per-domain PCB.
  No carve body exists - error codes UNKNOWN, flag inferred.
- **505B GERRCOD:** needs a per-process last-trap-error field set at trap dispatch; the call reads
  it and CLEARS it (byte-verified read-then-clear), returns 1 INTEGER (NUMPA=1). Interim: return 0.
- **336B IOMTY / 144B MAGTP / 244B GDIEN / 53B RSEGM:** blocked or low-prio (uncarved dispatch or
  need a backing store). Implement the safe skeleton + benign no-op for unknown sub-functions; do
  not fabricate device/error semantics.

## PART E - The 5 MISSING calls (no handler on either side) - implement

The older linker analysis called these "undocumented"; they are actually in the carve.

- **45B DefineBreakpoint** (dec 37, symbol F1631): benign no-op SUCCESS, do NOT set K. 4 args,
  semantics UNVERIFIED. Linker calls it 5x at startup - it cannot be dispatch-missing.
- **320B UELOG/UELogin** (dec 208): benign no-op SUCCESS. 1 arg = pointer to {W code 0x24, byte-ptr
  name buffer}. Manual name-only; contract inferred.
- **511B DVIO/DeviceInputOutput** (dec 329, worker 141027B): the FUSED form of 504B DVOUTS then
  503B DVINST (write a prompt, then read a line). 16 args; byte-proven facts: 16 args with the two
  STRING buffers at slots 2 (out prompt) and 3 (in buffer), and count>2048 -> error 174B (EC174).
  Implement by composing the existing DVOUTS + DVINST cores. Remaining 12-arg semantic order is
  inferred by fusing the two (DevNo, OutByteCount, OutBuffer@, InBuffer@, MaxNo, NoOfBytesRet,
  BreakStrat, EchoStrat, BreakT1-4, EchoT1-4).
- **512B A5XMSG / 513B B5XMSG** (dec 330 / 331): the ND-500->ND-100 XMSG gateway. *** ONE shared
  handler body (A5XMS=B5XMS=142253B, byte-proven - the ROM does NOT branch on the MON number). ***
  Register BOTH numbers pointing at the same core. The core: read arg0 as a code word, mask with
  X5MASK=077B (6 bits) to select an LFxxx subfunction, GOSW-dispatch, forward to MON 200B (XMSG),
  copy selected result words back.
  - Variadic CALLG (1-6 args): arg0 = code word (`subfunction | modifier`), args 1..N = XMSG msg
    params. LFREA (code 6) passes an INDIRECT buffer pointer - deref, don't copy.
  - **Return convention (critical):** SUCCESS = **W1 == 1** (NOT 0). The caller maps
    `error = (W1==1) ? 0 : 0x4200 - W1`. A 0-for-success handler makes the linker read 0x4200 as a
    fatal file error. Carve XMSG-fail error code = -36 octal / -30 decimal.
  - Subfunction table (OCTAL) authoritative in
    `/mnt/e/Dev/Ronny/NDInsight/Developer/MON/calls/513B_XMSGCallB.yaml`. Linker uses 14:
    0 LFDUM, 2 LFGET, 3 LFREL, 4 LFRHD, 6 LFREA, 10 LFSCM, 11 LFMST, 12 LFOPN, 13 LFCLS, 14 LFSND,
    15 LFRCV, 17 LFGST, 36 LFPRV, 45 LFDMM. Control/status subfns can no-op with W1=1; the
    transport subfns (LFOPN/CLS/GET/REL/SND/RCV/REA/RHD) need a minimal in-emulator XMSG mailbox
    (port table + message buffers) or the linker reads garbage. Message field offsets UNVERIFIED.

## PART F - registry decimal numbers for the missing calls

`mon_register_ex(dec, "octalB", short, long, desc, params, handler, status, count)`:
- 45B = 37, "F1631"/"DefineBreakpoint", 4 args, NOT_IMPLEMENTED (benign no-op)
- 320B = 208, "UELOG"/"UELogin", 1 arg
- 511B = 329, "DVIO"/"DeviceInputOutput", 16 args
- 512B = 330, "A5XMSG"/"XMSGCallA", 5 args (shares handler)
- 513B = 331, "B5XMSG"/"XMSGCallB", 6 args max (variadic 1-6, shares handler)

## Change log (nd500x working tree / commits, 2026-07-14)

Committed: SCOMP S-flag; COMP2 F/D; trap->THA dispatch; 117B RFILE; 73B SMAX; 50B OPEN; 312B MOINF.
This session (working tree, uncommitted): 123B RELES no-op; 64B ERMSG code-0; 256B DEABF 2-arg+err46;
43B CLOSE + 73B `max_bytes_set` truncation-at-close.
