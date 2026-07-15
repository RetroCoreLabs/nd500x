# MON-call completeness matrix - NC compiler + ND LINKER vs carved L07

**Full path:** `/home/ronny/repos/nd500x/docs/MON_COMPLETENESS_MATRIX.md`
Date: 2026-07-14

## Purpose

Every MON call the ND-500 C compiler `nc-a06.dom` and the ND LINKER `linker-b01.dom`
invoke, with implementation + carve-correctness status. Companion C# sync doc:
`/home/ronny/repos/nd500x/docs/MON_CSHARP_SYNC_HANDOFF.md`.

## Sources (ground truth = carve + ND manuals ONLY; C# RetroCore is NOT an oracle)

- NC required calls: captured with `/home/ronny/repos/nd500x/test/diag_monlog.c` (mon_log INFO), `COMPILE T,T,T`.
- LINKER required calls: static analysis `/mnt/d/ND/500/nd-linker/linker-b01.dom.moncalls.md`
  (95 call sites, 56 distinct numbers). Binary `/mnt/d/ND/500/nd-linker/linker-b01.dom`.
- Carve oracle tree: `/mnt/e/Dev/Ronny/NDInsight/tools/sintran-segment-carver/versions/L-VSX-500/re/mon-analysis/<octal>B-<Name>/`
  (README.md + `<Name>.ASM` + `<Name>.pseudo.c`). Index: `.../re/MON-CALL-INDEX.md`.
- NC-oracle contracts: `/mnt/e/Dev/Ronny/NDInsight/SINTRAN/ND500/mon-oracle-for-NC/*.md`.
- Our handlers: `/home/ronny/repos/nd500x/src/libmon/handlers/mon_<N>B_*.c`; registry
  `/home/ronny/repos/nd500x/src/libmon/mon_registry.c`; dispatch `/home/ronny/repos/nd500x/src/libmon/mon_dispatch.c`.

## KEY FINDING

The linker needs a **much larger** working MON surface than NC. Many linker-required
handler FILES exist but are **auto-generated stubs** (`mon_set_error(ctx,-1); return MON_ERROR;`,
registry `MON_STATUS_NOT_IMPLEMENTED`) - they were never implemented. Five calls the linker
needs have **no handler at all** but ARE in the carve (they were mislabelled "undocumented" in
the older linker analysis):
`045B-DefineBreakpoint`, `320B-UELogin`, `511B-DVIO`, `512B-XMSGCallA`, `513B-XMSGCallB`.

NC's own path is in good shape after this session's fixes; the linker is ~20 handlers short.

## Legend

- MATCH = contract matches carve on all byte-proven points
- PARTIAL = works but a detail diverges or is manual-inferred
- STUB = handler file exists but returns error -1 (not implemented)
- MISSING = no handler + no registry entry
- FIXED = corrected this session (working tree)

## Table (union of NC + LINKER required calls)

| MON | Name | Needed by | Status | Note |
|-----|------|-----------|--------|------|
| 0B | LEAVE/ExitFromProgram | NC+LNK | MATCH | terminate; no params/output |
| 1B | INBT | LNK | PARTIAL/OK | byte in W1+param; dev0=cmd buf |
| 2B | OUTBT | LNK | MATCH | |
| 3B | ECHOM | LNK | CANNOT-VERIFY | carve body unresolved; manual-based OK |
| 4B | BRKM | LNK | PARTIAL/OK | strategy table matches manual |
| 11B | TIME | NC+LNK | PARTIAL | carve return is DOUBLE WORD; we write single 32-bit. ignores PIN_CLOCK |
| 12B | SETCM | NC | PARTIAL | cap stored cmd at 32 chars (we accept 256) |
| 13B | CIBUF | LNK | CANNOT-VERIFY | no carve folder; success no-op OK |
| 16B | MGTTY | LNK | PARTIAL/OK | returns const type 0 |
| 17B | MSTTY | LNK | PARTIAL/OK | accept+ignore |
| 30B | GETRT | NC | PARTIAL | W1 return correct; RT descriptor fabricated (domain fields=0) |
| 32B | MSG/OutMessage | NC | PARTIAL/inferred | writes <=512 str; contract inferred |
| 41B | ROBJE | NC+LNK | PARTIAL | page-count field correct; middle fields byte-shifted (DEFERRED, NC only reads pages) |
| 43B | CLOSE | NC+LNK | FIXED | now applies deferred SMAX truncation at close (guarded) |
| 45B | DefineBreakpoint | LNK | MISSING | benign no-op success (4 args, semantics unverified); linker startup |
| 50B | OPEN | NC+LNK | FIXED (prior) | -52 break added |
| 53B | RSEGM | LNK | STUB | ND-100 call, carve worker uncarved; low prio |
| 54B | MDLFI/DeleteFile | NC | MATCH | empty->124dec, valid+nomatch->46dec |
| 62B | RMAX | NC+LNK | (prior audit) | bytes-in-file |
| 64B | ERMSG | NC+LNK | FIXED | dropped fabricated code-0 error; msg text still octal-only (TODO) |
| 66B | ISIZE | LNK | STUB | return input-buffer byte count in W1 |
| 71B | DESCF | LNK | STUB/BUG | returns error; must be success (escape-disable flag/no-op) |
| 72B | EESCF | LNK | STUB/BUG | returns error; must be success (inverse of 71B) |
| 73B | SMAX | NC+LNK | FIXED (prior+this) | records length; sets max_bytes_set for CLOSE |
| 74B | SETBT | LNK | PASS | byte ptr read as 32-bit INTEGER4 (carve-verified) |
| 76B | SETBS | NC+LNK | MATCH (prior) | |
| 104B | HOLD | LNK | STUB/BUG | returns error; no scheduler -> validate then success no-op |
| 113B | CLOCK | NC+LNK | CANNOT-VERIFY | 7xINT layout manual-sourced; carve silent |
| 114B | TUSED | NC | PARTIAL | value path OK; optional 147 err-path missing |
| 117B | RFILE | NC+LNK | FIXED (prior) | short read=success+count |
| 120B | WFILE | NC+LNK | MATCH (prior) | |
| 123B | RELES | NC | FIXED | unreserved release = success no-op (was err 5) |
| 143B | RSIO | NC+LNK | (prior audit) | |
| 144B | MAGTP | LNK | STUB | not safely implementable from carve; skeleton + no-op subfns |
| 162B | OUTST | LNK | PARTIAL/WRONG | must stop at 0x27 terminator + 2-arg min; we're count-only 3-arg |
| 214B | GUSNA | LNK | STUB | user-name lookup; RemoteFlag=0 |
| 217B | GUIOI | LNK | STUB | 3 separate indexes from open-file table |
| 221B | CreateFile | NC | PARTIAL | omitted NoOfPages default should be 0 (indexed) |
| 244B | GDIEN | LNK | STUB | needs Appendix C dir-entry layout + dir store; blocked |
| 254B | GERDV | LNK | STUB | carve byte-verified: 2 outputs [ErrorDevice, RTProgram=0] |
| 256B | DEABF/FullFileName | NC+LNK | FIXED | 2-arg; returns 46 on unresolved (NC create-if-missing) |
| 257B | FOPEN | LNK | STUB | open-file info; not-found err octal 122 family |
| 262B | CPUST | NC | (prior audit) | fills 24-byte buffer |
| 263B | GDEVT | LNK | STUB | DevType 0..7 + DevAttr 32-bit |
| 273B | MGFIL | LNK | STUB | index->filename; carve err literal octal 113 |
| 312B | MOINF | NC+LNK | FIXED (prior) | MCTAB[321B]=065453B |
| 317B | UECOM | NC+LNK | (prior audit) | |
| 320B | UELOG/UELogin | LNK | MISSING | benign no-op success (1 arg); manual name-only |
| 321B | UEADM | NC | (prior audit) | |
| 322B | GSGNO | LNK | STUB | read 6-char seg NAME (3 words), lookup, return seg no |
| 336B | IOMTY | LNK | STUB | complex variadic; low prio; func0->terminal_state |
| 412B | FSCNT/FileAsSegment | LNK | PARTIAL | wrong return channel + no real segment connection; 14x in linker |
| 422B | GSWSP | NC+LNK | MATCH (prior) | segment mapping clean |
| 423B | CAPCOP | LNK | STUB | 6-arg contract right; needs capability-copy impl; no carve body |
| 503B | DVINST/InputString | NC+LNK | MATCH | break-byte inclusive, 14-arg order |
| 504B | DVOUTS/OutputString | NC+LNK | MATCH | count-delimited, max 2048B->174B |
| 505B | GERRCOD | LNK | STUB | needs per-process trap-code state; read-then-clear |
| 511B | DVIO | LNK | MISSING | fused DVOUTS+DVINST; 16 args; compose 503B+504B |
| 512B | A5XMSG/XMSGCallA | LNK | MISSING | XMSG gateway; SHARES body with 513B |
| 513B | B5XMSG/XMSGCallB | LNK | MISSING | XMSG gateway; variadic; success=W1==1; 14 subfns |
| 514B | 5TMOUT | LNK | STUB | validate TimeUnit 1..4 (err 124dec), num0=immediate, no-op wait |

## Work remaining for the LINKER to run

**Implement (stubs -> real):** 71B, 72B (bug: currently error), 104B, 66B, 214B, 217B, 254B,
257B, 263B, 273B, 322B, 336B, 423B, 505B, 514B; **fix** 162B (terminator), 412B (return
channel + real segment). 53B, 144B, 244B are blocked/low-prio (carve-uncarved or need a
backing store).
**Add missing:** 45B, 320B (benign no-op success); 511B (compose 503B+504B); 512B+513B (one
shared XMSG handler keyed on arg0 & 077B).

## Carve-honesty caveats (carried across all agents)

- Many workers dispatch through an uncarved MFELL/CALLPROC bridge; the `MON N -> worker` link is
  often INFERRED, not a followed pointer. Register/field layouts are frequently manual-sourced.
- ND-500 MON INTEGER params are 32-bit W (registry "INTEGER2" labels are misleading but the
  handlers correctly use the word helpers).
- A definitive contract for the unverified calls needs a live single-step trace (break at the
  worker entry on a real MON, confirm P lands there) - noted per-call in the C# handoff.
