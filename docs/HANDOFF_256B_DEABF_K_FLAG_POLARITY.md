# HANDOFF: MON 256B DEABF returns K CLEAR on success (K polarity was inverted)

Date: 2026-07-20
File changed: `/home/ronny/repos/nd500x/src/libmon/handlers/mon_256B_FullFileName.c`
Cross-emulator target: RetroCore `Emulated.HW/ND/CPU/ND500` MON 256B handler

## Symptom

In the ND Linker B01, `OPEN-DOMAIN A-TEST` failed with:

```
 *** ERROR - SINTRAN                                                  (0000:00)
```

Every MON call in the command logged `-> SUCCESS`. Nothing in the MON trace was
failing, yet the linker took an error path. `LOAD` then always failed with
`Command not valid when no current domain or segment exists. (0054:67)`, because
no domain had been opened.

## Root cause

`mon_256B_FullFileName` explicitly set the K flag to 1 on the success path:

```c
mon_set_success(ctx);          /* clears K */
...
if (ctx->set_k_flag) {
    ctx->set_k_flag(ctx->cpu, 1);   /* then set it back to 1 */
}
```

For MON 256B, **K set means ERROR, not success.** The K=1 was inverted.

## Byte-level evidence

All addresses from `/mnt/d/ND/500/nd-linker/linker-b01.dom.asm`.

### 1. The OPEN-DOMAIN call site tests K directly

```
B0000A6D: C3 F8 00 00 AE 03 4E 50 52   call  $0xFFFFFFFFF80000AE,$0x3   ; MON 256B DEABF
B0000A76: D0 03                        if k go   $0x3                   ; -> B0000A79
B0000A78: 80                           ret                              ; K CLEAR = success
B0000A79: 20 43                        w1 =:   b.0xC                    ; K SET = error:
B0000A7B: 0C 43                        w1 :=   b.0xC                    ;   stash code in b.0xC
B0000A7D: 81                           retk                             ;   propagate error
```

`0xF80000AE`: `0xAE` = 256 octal = DEABF. Runtime confirmation with the K-watch
harness, on `OPEN-DOMAIN A-TEST`:

```
[KSET] PC=B0000A76 (raised at B0000A6D) I1=00000000 instr=133011 B=B0001DD4
```

K was set on return from DEABF, so the linker took `B0000A79` and aborted the
command. That is the `SINTRAN (0000:00)` message.

### 2. The evidence that motivated the old K=1 was misread

The previous revision justified K=1 with: "the linker resolves file names with
`callg <256B>; if k go <success>` at B004DA08/B004DA13". Both halves are wrong.

**(a) `B004DA08` is not DEABF-specific.** `B004D9E6`-`B004DA5B` is the linker's
generic indirect MON trampoline: one `callg b.0x40,$N,...` per arity 1..6, with
the gate loaded from a frame local. Every 3-argument MON call goes through
`B004DA08`. It carries no information about DEABF's private convention.

```
B004D9E6: callg b.0x40,$0x1,@b.0x18          ; arity 1
B004D9F6: callg b.0x40,$0x2,@b.0x18,@b.0x1C  ; arity 2
B004DA08: callg b.0x40,$0x3,...              ; arity 3  <- the cited site
B004DA1C: callg b.0x40,$0x4,...              ; arity 4
```

**(b) In that trampoline the if-k target is the ERROR arm.** Following
`B004DA13: if k go $0x1DE` to `B004DBF1`, against the fall-through:

```
B004DBED: 4A 43        w stz   b.0xC     ; K CLEAR arm -> error slot ZEROED  = success
B004DBEF: C0 05        go      $0x5
B004DBF1: 1A 51 43     w move  b.0x44,b.0xC  ; K SET arm -> returned W1 into error slot
B004DBF4: 18 40        r:=     b.0x0
B004DBF6: 0C 43        w1 :=   b.0xC
B004DBF8: 20 83        w1 =:   r.0xC
B004DBFA: 80           ret
```

The wrapper at `B004D8DE` treats a NONZERO `b.0xC` as an error code. So K set =
error at the trampoline too. Both call sites agree.

### 3. Why the bug stayed invisible at the LOAD site

At the trampoline, the K=1 error arm copies the handler's returned W1 into
`b.0xC`. The companion fix in the same handler sets `I1 = 0` on success, so the
error arm wrote 0 into `b.0xC` — which the wrapper reads as "no error". The
inverted K was masked there. `B0000A76` tests K directly with no such masking,
so that is where it surfaced.

## Fix

Delete the `set_k_flag(ctx->cpu, 1)`. `mon_set_success()` already leaves K clear,
which is the correct success signal. The `I1 = 0` success value is UNCHANGED and
still required (the trampoline copies I1 into the caller's result slot).

## Verification

Sandbox: `/home/ronny/repos/nd500x/build/link_sandbox`
(the linker MUST run from here — it needs `GUEST/LINKER.INIT`, `GUEST/LINKER.HELP`,
`GUEST/DDBTABLES-G06.VTM`, `SYSTEM/`; running from the repo root produces a
spurious `*** WARNING - "LINKER:HHHHH:HHHHH:H" / No such file name (0000:56)`).

```
cd /home/ronny/repos/nd500x/build/link_sandbox
../bin/diag_linkdrive /mnt/d/ND/500/nd-linker/linker-b01.dom "OPEN-DOMAIN A-TEST;;EXIT" 40000000
```

Before: `*** ERROR - SINTRAN (0000:00)`, no domain opened.
After: no error, linker advances to the next command prompt.

Consequence: `LOAD` no longer reports `(0054:67) no current domain`. It now
resolves the object name through DEABF, OPENs the `.NRF`, and reads it — the
next failure is a genuine NRF content complaint,
`*** ERROR - "4" in module  is illegal control byte. (0054:16)`, which is a
separate open issue.

## What the C# side must mirror

- MON 256B DEABF: on success leave **K CLEAR** and set **W1/I1 = 0**.
- On failure (name does not resolve): set K and return error 46 (056B NO SUCH
  FILE NAME) — unchanged.
- Do not infer any MON call's K convention from `B004D9E6`-`B004DA5B`; that range
  is a shared arity-dispatched trampoline, and in it K set is the error arm.
