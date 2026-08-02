# Bug report: "xgattach: bad completion code 01 from feidev" at every boot

Filed for later. Handoff document - all paths absolute.

## Symptom

Every NDIX boot prints, right after the buffer-cache line:

```
using 382 buffers containing 3129344 bytes of memory
xgattach: bad completion code 01 from feidev
```

## Verdict up front: this is OUR DELIBERATE BYPASS, not a MON 600 defect

The premise "assume MON 600 needs some fix" is half right. Nothing is
malfunctioning: the emulator is *intentionally* failing the call, and the
kernel is reporting that failure correctly. The real work item is that XMSG
(generic 7) is not implemented at all.

### Verified chain

1. `src/frontend/nd500x/nd500x_ndix.c:128`
   ```c
   setenv_default("ND500X_NOXMSG", "1");
   ```
   `--ndix` mode turns the bypass ON by default. The comment at line 122 says
   why: "bypass XMSG (without it proc0 sleeps forever)".

2. `src/cpu/nd500_fecall.c:220-228` - with that
   variable set, `FE_IDEV` on generic 7 is answered with a FAILURE completion.
   The block is explicitly labelled `EXPERIMENT (ND500X_NOXMSG)`.

3. `$NDIX/kernel/MASTER/if/xg.c:128-142` - `xgattach()`
   issues `fecall(XMSG << 16, FE_IDEV | (QF_SYNC << 16), ...)` and, on a
   non-zero completion, calls `nderror(...)` and sets `fep->state = GD_IDLE`.
   So the kernel handles the failure gracefully and carries on.

4. `$NDIX/kernel/MASTER/io/nderror.c:54-60` - the printf that
   produces the exact wording. `0%o` means the code is printed in OCTAL, so
   "01" is completion 1.

Net: boot-time behaviour is correct and non-fatal. The message is cosmetic
*today*, and is the visible marker of a missing subsystem.

## Why the bypass exists (do not simply remove it)

Without `ND500X_NOXMSG=1` the emulator answers `FE_IDEV` on generic 7 with
success, the kernel then proceeds into the real XMSG attach path, and proc0
sleeps forever because none of the follow-on XMSG traffic is implemented.
Turning the bypass off without implementing XMSG replaces a cosmetic warning
with a hung boot. That trade is strictly worse.

## The actual work item

Implement MON 600 generic 7 (XMSG). This is already tracked as the third item
of the SIINTR/TAPE task, and the groundwork notes are in
`docs/NDIX_SIINTR_TAPE_SPEC.md`.

Relevant facts already established:
- XMSG gates ALL networking: `$NDIX/kernel/MASTER/if/if_et.c`
  makes ZERO fecalls of its own and rides on `if/xg.c`. So no ethernet until
  XMSG works.
- The command packet for XMSG DCTL is `_dctl_cpk_xmsg` in
  `$NDIX/kernel/MASTER/machine/if.h`:
  `short request; short param;` with requests
  `DCTL_KICK 1`, `DCTL_WAIT 2`, `DCTL_INUSE 3`, `DCTL_RESTART 4`, `DCTL_STOP 5`.
- `IPL_XM = 3` (`$NDIX/kernel/MASTER/machine/icb.h:86`).
- `xgattach` also passes `Idev_cpk_xmsg.paralist = dton(xpara)` - a parameter
  list pointer the emulator must honour, not just a completion code.
- A C# XMSG stack is being written independently at
  `$NDINSIGHT/SINTRAN/XMSG/SRC/` (`Xmsg.Protocol`, `Xmsg.Node`,
  `Xmsg.Hdlc`, `Xmsg.Live`). OPEN QUESTION for the owner, asked but not yet
  answered: is that stack the reference to mirror in C, or is nd500x meant to
  call into it?

## Cheap interim option (cosmetic only)

If the boot-time noise is the annoyance rather than the missing feature, the
honest fix is NOT to silence the kernel - never edit the frozen tree - but to
make the emulator's deliberate refusal quieter or self-describing on the
emulator side. Any such change must keep the completion code non-zero so the
kernel still takes the `GD_IDLE` path. Recommend leaving it alone: the message
is an accurate statement that networking is absent.

## Constraints

- `$NDIX/kernel/` and `/baseline/` are READ-ONLY historical
  preservation. `if/xg.c` and `io/nderror.c` must not be touched.
- Writable: `<nd500x repo root>`.
