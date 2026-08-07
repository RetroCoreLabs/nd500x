# 4.3BSD header overlay

Headers that the preserved NDIX-C sources `#include` but that were never
preserved themselves. NDIX-C is 4.3BSD-derived, so these are taken unmodified
from the real 4.3BSD tree rather than reconstructed.

**Source:** `dspinellis/unix-history-repo`, branch `BSD-4_3`, `usr/include/` —
a git conversion of the CSRG distribution tapes.

| File | SCCS id | Needed by |
|---|---|---|
| `resolv.h` | `5.4 (Berkeley) 2/22/86` | `libc/net/res_*.c`, `net/named/gethostnamadr.c` |
| `arpa/nameser.h` | `5.11 (Berkeley) 2/14/86` | same |
| `ttyent.h` | `5.1 (Berkeley) 5/30/85` | `etc/init`, `getty`-family |
| `curses.h` | `5.1 (Berkeley) 6/7/85` | screen programs (`systat`, `nstat`) |
| `netdb.h` | `5.7 (Berkeley) 5/12/86` | **replaces** the archive's stale copy — see below |

## Why netdb.h is replaced, not merely filled in

The archive's own `netdb.h` is `@(#) netdb.h 2.5 87/05/13`, which still has the
4.2-era

    char *h_addr;          /* address */

But NDIX-C's *own* Release 3 libc — `net/named/gethostnamadr.c 3.2 88/05/11` —
assigns `host.h_addr_list = h_addr_ptrs;` (line 143) and also uses `h_addr`
(line 333). Both only work with the 4.3 header, where

    char **h_addr_list;
    #define h_addr h_addr_list[0]   /* for backward compatibility */

So the shipped header is simply older than the libc that ships beside it. The
4.3 header is a superset: source written against the 2.5 header keeps compiling,
because `h_addr` survives as the compatibility macro.

## Verified before use

* every `_res.*` field touched by the NDIX-C resolver exists in `resolv.h` 5.4
* every `T_*` / `C_*` / `RES_*` macro used resolves from these two headers
* `RES_TIMEOUT` is *not* in 4.3's `resolv.h` and does not need to be —
  `res_init.c` defines it itself under `#ifndef`

Nothing in `baseline/` or `kernel/` is modified; the overlay is applied to
copies in the work tree by `mkinc.sh`.
