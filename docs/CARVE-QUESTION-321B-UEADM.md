# Carve question: MON 321B UEADM sub-function semantics

Full path of this document: `/home/ronny/repos/nd500x/docs/CARVE-QUESTION-321B-UEADM.md`

Written 2026-07-20. Blocks: implementing `/home/ronny/repos/nd500x/src/libmon/handlers/mon_321B_UEAdministrator.c`.

## Why this matters now

MON 321B UEADM is currently a stub in nd500x. It does no work, logs
`Deprecated MON call - returning error`, and returns error 124 decimal (174B, Illegal
parameter) with the K flag set.

That stub is the **root cause of two ND Linker crashes**: the `HELP` command, and any
abbreviated command form such as `REFER` (the full `REFER-ENTRY` works). Both print a full
ND-500 register block and `*** ND LINKER abortion ***`, trap 7644B PROTECT VIOLATION at
address 26000664565B = `0xB0036975`.

## Evidence chain (all reproduced 2026-07-20 on a freshly relinked harness)

Run from `/home/ronny/repos/nd500x/build/link_sandbox`:

```
../bin/diag_linkdrive /mnt/d/ND/500/nd-linker/linker-b01.dom \
  'OPEN-DOMAIN "RTEST";;HELP;;EXIT' 300000000
```

1. **The failing path calls 321B; the working path never does.**
   `HELP` and `REFER` both emit `CALL 321B UEADM (3 args) at PC=0xB004D37A` followed by
   `EXIT 321B UEADM -> ERROR`, then trap. The control run (`REFER-ENTRY`) makes **zero**
   321B calls.

2. **The error propagates into a null string descriptor.** Breaking at the faulting
   instruction `B0036971: 2D E4 14 D1  by comp2 IND(b.20)(r1),W2` with `B=0xB0001E58`:

   | run | `b.20` | length (I4) | ST1 |
   |---|---|---|---|
   | working `REFER-ENTRY` | `B0 03 12 B8` (valid pointer) | 3 | `0x02` |
   | failing `REFER` | `00 00 00 00` (**null**) | 0 | `0x42` |

   The linker's own guard two instructions earlier is `w3 comp W4` / `if>go $19`, i.e. it
   only skips the compare on GREATER. With both lengths zero it falls through and
   dereferences the null pointer. The linker is not defensive here; its caller is simply
   never supposed to hand it an empty descriptor.

3. **The linker's UEADM wrapper.** Disassembled from `linker-b01.dom` (decode the bytes -
   the `.asm` dump in `/mnt/d/ND/500/nd-linker/linker-b01.dom.asm` is not reliable):

```
B004D366: 4A 4C                 w stz     b.48              ; result slot := 0
B004D368: 0D 48                 w2 :=     b.32
B004D36A: FE 2A E5 1C           h3 laddr  IND(b.28)(r2)
B004D36E: 22 4E                 w3 =:     b.56
B004D370: 0F 49                 w4 :=     b.36
B004D374: 57 01                 w4 +      $1
B004D376: 6F 02                 w4 *      $2
B004D378: 23 4D                 w4 =:     b.52               ; arg[2] = (b.36 + 1) * 2
B004D37A: C3 F8 00 00 D1 03 45 46 4D
                                call      $0xF80000D1,$3,b.20,b.24,b.52
B004D383: D2 06                 if-kgo    $6                 ; K set -> jump to the ret
B004D385: 20 4C                 w1 =:     b.48               ; success: keep returned w1
B004D387: FE 03                 clrk
B004D389: 0C 4C                 w1 :=     b.48
B004D38B: 80                    ret
```

`0xF80000D1` = segment 31 + 0xD1 = 209 decimal = 0o321, the standard nd500x MON trampoline.

Note the error path: on K set it branches straight to `ret` **without** reloading `w1`, so
the caller receives whatever the MON call left in `w1` - for us, the injected error code 124 -
and treats it as a legitimate result. That is how the garbage reaches the string descriptor.

4. **Observed argument values** (`ND500X_BREAK_PC=0xB004D37A`, dump at `0xB0059350`):

```
arg[0] @ B0059350 = 00 00 00 01     -> 1
arg[1] @ B0059354 = 00 00 00 00     -> 0
arg[2] @ B0059370 = 00 00 00 30     -> 48 = (23 + 1) * 2
```

`arg[0] = 1` is consistent with the carve's finding that UEADM decodes a sub-function
selector range-checked to `[1..8]`.

## What the carve already proves

From `/mnt/e/Dev/Ronny/NDInsight/tools/sintran-segment-carver/versions/L-VSX-500/re/mon-analysis/321B-UEAdministrator/README.md`
(status: byte-verified, dispatch + worker body):

- `MCTAB[321B] = 065453B = UEADM`; worker carved in `003-S3CP` at `065453B-065642B`.
- Prologue saves `X := B` at `,B -166` and the link at `,B -167`.
- Sub-function selector read from `param[12]`, range-checked `[1..8]`; out of range stores
  error `124B` into `param[12]`.
- Physical per-user-table read via `LDT I 73 / LDATX`.
- Two bit-fields extracted from `param[17]` (`SHR 7 / AND 103` and `SHR 13 / AND 77`).
- Computed jump table at `065600B-065612B` with a pointer table at `065615B-065640B`.

The README states plainly what is **not** proven: "the meaning of each of the 8
sub-functions, the exact layout of the user table it reads, and the manual's claim that 321B
is 'no longer supported'".

## The questions

1. **What is sub-function 1?** That is the only selector the ND Linker uses in the observed
   traces. Decode the first entry of the jump table at `065615B` and its target.
2. **What does sub-function 1 return in the A register** (which becomes the ND-500 `w1`)?
   The linker stores it as its own return value and, on the evidence above, a wrong value
   here produces an empty string descriptor further downstream.
3. **What are the second and third parameters** for sub-function 1? Observed as 0 and 48
   respectively, with 48 computed as `(n + 1) * 2` - which looks like a byte count for
   `n = 23` 16-bit words. Confirm whether the third parameter is a buffer length, and if so
   where the buffer itself is passed.
4. **Does sub-function 1 read the `UE-ERMSG-*` files?** The sandbox holds
   `/home/ronny/repos/nd500x/build/link_sandbox/GUEST/UE-ERMSG--C.ERR` and
   `/home/ronny/repos/nd500x/build/link_sandbox/GUEST/UE-ERMSG-EN-C06.ERR`, which is what
   made a message/help-text role plausible - but that is an inference, not evidence.

## Correction to feed back into the YAML

`/mnt/e/Dev/Ronny/NDInsight/Developer/MON/calls/321B_UEAdministrator.yaml` says under
`emulation.unverified`: *"Nothing observed actually CALLS 321B - only 312B probes for it."*

**That is now false.** The ND Linker (`linker-b01.dom`) calls 321B directly with 3 arguments
from `PC=0xB004D37A`. Its `observed_calls` section should record this caller.

## Do not disturb

`/home/ronny/repos/nd500x/src/libmon/handlers/mon_312B_CheckMonCall.c` carries a deliberate
hard-coded exception, above the registry lookup, that reports 321B as PRESENT
(`MCTAB[321B] = 065453B`) because the NC C front-end requires it. Changing 321B's own
registration must not change that answer.
