<!-- Paths in this file are repo-relative, or use the same environment
     variables as tools/ndix/*.sh: $NDIX = NDIX-C checkout,
     $NDIX_B = NDIX-B checkout. No machine-specific paths. -->

# MON 600 generics 8 (SIINTR) and 2 (TAPE) - implementation spec

> **CORRECTION 2026-07-30 - read this before the SIINTR section below.**
> The SIINTR section assumes both requests are unimplemented. That is WRONG.
> Reading `src/cpu/nd500_fecall.c` shows both already
> work through the generic paths:
>   - `FE_IDEV` gen 8 has no special case, so it falls through to the default at
>     line 230 and returns `completion = 0`. `siattach` already succeeds - which
>     is why only `xgattach` ever complains at boot, never `siintr`.
>   - `FE_DCTL` async gen 8 (lines 938-962) already writes `completion = 0` and
>     queues the completion interrupt via `fe_int_pending`/`fe_int_gen`/
>     `fe_int_sub`/`fe_int_rpk`.
>
> The real remaining gap is the INTERRUPT PRIORITY: `fe_deliver` is called with a
> hardcoded `FE_IPL_DK = 4` for every queued completion (lines 722-725), but
> SIINTR's priority is `IPL_SI = 3` - the value `siattach` puts in
> `Idev_cpk.ipl`. The emulator should carry the per-device IPL captured at
> `FE_IDEV` time instead of assuming disk priority. (The clock already passes its
> `IPL_CL = 3` explicitly at line 731, so the mechanism exists.)
>
> Also NOT VERIFIED: nothing has actually driven a netisr, so "SIINTR works" is
> inference from code reading, not measurement. See the task entry for how to
> test it and the `SOFTINT` panic hazard.


Ready-to-apply plan. NOT applied yet: nd500x must not be rebuilt while the
userland/toolchain agent is booting `build/bin/nd500x`.

Everything below is read directly from the NDIX kernel source (READ-ONLY tree),
file:line given for each claim. Nothing here is inferred.

Target file for the implementation:
`src/cpu/nd500_fecall.c`

---

## 1. SIINTR - generic 8 - BSD software network interrupt

Not a device. It is how NDIX asks the ND-100 front end to deliver the 4.3BSD
`netisr` software interrupt back to the ND-500. Two requests only.

### 1a. FE_IDEV, generic SIINTR<<16, QF_SYNC

`$NDIX/kernel/MASTER/if/si.c:38-42`

    Idev_cpk(*si_pkt).ipl = IPL_SI;
    fecall(SIINTR<<16, FE_IDEV|QF_SYNC<<16,
           &Idev_rpk(*si_pkt), &Idev_cpk(*si_pkt));
    if (Idev_rpk(*si_pkt).completion != 0) { nderror(...); return; }

Required of the emulator: set response `completion = 0`. The command packet
carries `ipl = IPL_SI`; record it, nothing else is read back.

On success the driver itself sets (`if/si.c:45-56`) `fep->alive++`,
`fep->state = GD_RUN`, and `sitab[0]` = {sd_s3sub 0, sd_s3gen SIINTR,
sd_state SD_RUN, sd_apkt si_pkt, sd_spkt si_pkt+1, sd_fd fep}. The emulator
touches none of that.

NOTE: the comment at `if/si.c:29-30` says siattach "sleeps at PZERO to await
interrupt". The CODE does not sleep - the fecall is QF_SYNC and it returns
immediately. So no async completion is needed for attach.

### 1b. FE_DCTL, generic SIINTR<<16, QF_ASYNC - the actual interrupt request

`$NDIX/kernel/MASTER/machine/basic.c:38-51`

    setsoftnet(anisr)          /* == schednetisr(), net/netisr.h:31 */
    {
        s = spl3();
        netisr |= 1 << anisr;
        if (!SoftIntr) {
            SoftIntr++;
            fecall(SIINTR<<16, FE_DCTL|QF_ASYNC<<16,
                   &Dctl_rpk(*crpk_ptr), &Dctl_cpk_si(*crpk_ptr));
        }
        splx(s);
    }

`crpk_ptr = sitab[0].sd_apkt` = `si_pkt`.

Required of the emulator:
1. set `Dctl_rpk.completion = 0` in the response packet, and
2. deliver an INTERRUPT on generic 8 (sub 0). The existing async-completion +
   interrupt-injection path already proven for DISK is the mechanism to reuse.

The kernel then dispatches to `siintr(sub)` from `machine/trap.c` dispatch
(`if/si.c:79`), which:
- panics `"SOFTINT"` if `SoftIntr == 0` (`if/si.c:86-87`) -> DO NOT inject a
  generic-8 interrupt that was not requested by an FE_DCTL, or the kernel panics.
- `nderror()`s and returns if `Dctl_rpk.completion != 0` (`if/si.c:89-92`).
- clears the NETISR_RAW / NETISR_IP / NETISR_NS bit and calls
  `rawintr()` / `ipintr()` / `nsintr()` (`if/si.c:98-119`).
- prints `siintr(): bad netisr 0x%x` if no bit was set.

Callers of schednetisr today: `net/raw_usrreq.c:75` (NETISR_RAW),
`net/if_loop.c:93` and `if/if_et.c:736` (NETISR_IP). So loopback and raw
sockets exercise this even with no ethernet.

Constant: `GTRAP = 0x6`, `SIINTR = 8` (`machine/if.h`). IPL_SI from
`machine/fevar.h`/`icb.h` - read the exact value before hardcoding.

**Ordering constraint (the one real hazard):** the interrupt must be delivered
AFTER `setsoftnet` returns and `SoftIntr` is 1 - it already is, since
`SoftIntr++` precedes the fecall. But it must NOT be delivered synchronously
inside the fecall while the kernel is still at `spl3()` if the dispatcher
would re-enter `setsoftnet`. Deliver it as a queued/async interrupt, as the
disk path does.

---

## 2. TAPE - generic 2 - magtape (io/mt.c, 763 lines)

`$NDIX/kernel/MASTER/io/mt.c`. Six request sites:

| mt.c line | request | queue | device word |
|---|---|---|---|
| 99  | FE_IDEV | QF_SYNC  | `TAPE<<16` |
| 183 | FE_OPEN | QF_SYNC  | `TAPE<<16 \| mtunit` |
| 274 | FE_CLOS | QF_SYNC  | `TAPE<<16 \| mtunit` |
| 438 | FE_DCTL | QF_ASYNC | `TAPE<<16 \| mtunit` |
| 454 | FE_READ | QF_ASYNC | `TAPE<<16 \| sd->sd_s3sub` |
| 463 | FE_WRIT | QF_ASYNC | `TAPE<<16 \| mtunit` |

`MAXMT = MAXTAPE` (mt.c:44). Debug output is available in-kernel via
`PRDEBUG(MTVER, ...)` (mt.c:185) - useful for verification without touching
the frozen source.

### Packet layouts - READ, not guessed

All from `$NDIX/kernel/MASTER/machine/if.h`. `naddr_t` is the
ND physical address produced by `dton()`.

    _open_rpk_tape   (if.h:191)  short completion; short status;
    _read_cpk_tape   (if.h:223)  long maxbytes; naddr_t physaddr;
    _writ_cpk_tape   (if.h:274)  long nbytes;   naddr_t physaddr;
    _dctl_cpk_tape   (if.h:323)  short operation; short parameter;
    _dctl_rpk_tape   (if.h:386)  short completion; short status; short ops;

`mt.c` uses the generic `Read_rpk` / `Writ_rpk`, i.e. `_read_rpk_xxxx`
(if.h:236): `short completion; short status; long nbytes;` - so the ACTUAL
record length read is returned in `nbytes`.

DCTL operations (`if.h:324-332`, all marked `/* XXX */` in the original):

    DCTL_FSF 1  forward space file      DCTL_REW     5  rewind
    DCTL_BSF 2  backward space file     DCTL_REW_UNL 6  rewind + unload
    DCTL_FSR 3  forward space record    DCTL_STAT    7  status
    DCTL_BSR 4  backward space record   DCTL_WEOF    8  write end-of-file

`mt.c:434-435` fills `operation = bp->b_command`, `parameter = bp->b_repcnt`
(the repeat count for the space operations).

IPL for tape is `IPL_TP = 4` (`machine/icb.h:82`); `IPL_SI = 3` (icb.h:87).

### Tape image format - RECOMMENDATION, not an open question

The interface is RECORD-structured, not block-structured: reads are
"give me up to `maxbytes`, tell me how many bytes this record actually was",
writes are "write this many bytes as one record", and the control ops are
space-record / space-file / write-filemark. A flat raw-block image CANNOT
represent variable-length records or filemarks, so it cannot back this driver.

SIMH `.tap` maps onto it one-to-one: each record is a 4-byte little-endian
length, the data (padded to even), then the same 4-byte length again; a length
of 0 is a tape mark (filemark); 0xFFFFFFFF is end-of-medium. That gives FSR/BSR
(step one record forward/back via the length prefixes/suffixes), FSF/BSF (scan
to the next/previous 0-length marker), WEOF (write a 0), and REW (seek 0)
directly. Recommend SIMH `.tap` unless the user wants otherwise.

## Order of work

SIINTR first: two requests, no backing store, no user decision needed, and it
unblocks the loopback/raw-socket paths. TAPE second, and only after the packet
layouts are read out of `fevar.h`/`icb.h`/`mtreg.h` and the tape-image format
is chosen.
