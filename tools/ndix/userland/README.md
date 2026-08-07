# Rebuilding the NDIX userland

Cross-builds programs from the preserved 1988 NDIX-C sources and puts them into
a bootable disk image. Written while adding the 59 programs that had source in
`baseline/` but no binary in the shipped image.

Everything here writes to `$ND500X_WORK` (default `nd500x/work/`). No script
modifies a preserved source tree, and none writes to a disk image in use — the
rebuild produces a new file, to be booted and fsck'd before it replaces
anything.

## Environment

| Variable | Points at |
|---|---|
| `NDIX` | NDIX-C checkout — sources, disk images, `kernel/MASTER/` |
| `NDIX_B` | NDIX-B checkout — the 1988 userland headers |
| `PCC_ND500` | pcc-nd500 cross toolchain (its `bin/`) |
| `ND500X_WORK` | scratch tree (default: `nd500x/work`) |

## Order

    mkinc.sh                 # merged include tree -> work/init/inc
    mklibc.sh                # libc from source + symbol index -> work/libx
    prog.sh <name> <src...>  # one program -> work/bin
    batch.sh                 # the whole missing-program list
    batch2.sh                # the ones needing yacc, extra headers or a second .c

    enum.sh                  # list the image's real contents (boots the guest)
    genproto2.py             # proto from that listing + work/bin
    rebuild.sh               # mkfs + mkproto + splice -> work/stage/rootfs_new.img
    verify2.sh               # diff old vs new file lists, both directions

## Things that cost time to find

**The include tree is three trees plus the kernel's.** No single one is a
complete `/usr/include`: NDIX-B has the userland headers but no `signal.h` and
no `sys/`; the kernel tree *is* `<sys/*>`, `<machine/*>`, `<net/*>`,
`<netinet/*>` and `<if/*>`, exactly as `/usr/include/sys` is a link into `/sys`
on a real BSD. `nd500-cpp` has `/usr/include` compiled in as a fallback, so a
header missing from all of them silently resolves to the HOST's glibc — every
`features.h: undefined control` in a build log is that, 2020s C reaching a K&R
front end.

**`mkinc.sh` adds include guards.** The 1988 headers have none, so reaching one
twice redeclares a `static char sccsid[]` and the compile fails. Guards go on
copies in the work tree.

**`.s` files must be preprocessed.** They are C-preprocessed assembly:
`sys/sbrk.s` has `#define incr 20` and writes `b.incr` as a stack-frame offset.
Assembling directly leaves those as undefined externals.

**`-DMAXCAR=32`.** `sys/syscall.s` ends with `.set FU1, 20+MAXCAR*4` but
includes only `<machine/param.h>`, which does not define it. `machine/frame.h`,
`machine/locore.h` and `h/varargs.h` all say 32.

**`.set` must precede its use.** `nd500-as` cannot resolve a forward `.set`, and
does not say so — it exits 0 and writes no object file. `mklibc.sh` hoists them
and checks the object is non-empty.

**Commons are definitions.** `errno` is `int errno;` at file scope in
`gen/perror.c`, so it appears as `UNDF|EXT` with a non-zero size. Indexing only
non-UNDF symbols left `_errno` unresolvable — the single most requested missing
symbol across the batch.

**Use `byacc`, not `yacc`.** On this box `yacc` is a symlink to `bison.yacc`,
whose output is ANSI C. `tools/ndix/knr_yacc.sh` rewrites byacc's skeleton into
K&R. The grammars themselves also use the pre-POSIX `= { action }` form.

**Do not trust `stage24.proto`.** It describes an older filesystem than the
shipped image: rebuilding from it silently drops `fsck`, `mount`, `dump`,
`cron`, `syslogd` and all of `/lib`. `enum.sh` + `genproto2.py` build the
prototype from the image itself instead.

## The archive's `usr.include` is a year older than its own libc

This is the single most productive thing to know. Three headers in the
preserved include set all carry the same date — `netdb.h 2.5 87/05/13`,
`inet.h 2.5 87/05/13`, `telnet.h 2.5 87/05/13` — while the library sources
beside them are Release 3, `88/05/11`. The snapshot is stale, and the mismatch
is silent until something fails to compile:

| Header says | Library actually does | Broke |
|---|---|---|
| `char *h_addr;` | `gethostnamadr.c` assigns `host.h_addr_list` | `rcmd.c`, and so `finger` `rshd` `rlogind` |
| `struct in_addr inet_addr();` | `inet_addr.c` returns `u_long` | `gethostnamadr.c` |
| no `TELOPT_TTYPE` | — | `telnet`, `telnetd` |

`resolv.h`, `arpa/nameser.h`, `ttyent.h`, `curses.h` and all of `protocols/`
were not preserved at all. Since NDIX-C is 4.3BSD-derived, the fix in every
case is the real 4.3 header — see `bsd43/`. Fixing them took the build from 32
programs to 43, including the entire resolver family.

**A missing header does not announce itself.** `nd500-cpp` has `/usr/include`
compiled in as a fallback, so `<protocols/rwhod.h>` silently resolved to the
*host's* copy and produced a nonsense error about `out_time`. Any error naming
a path outside the work tree means this happened.

## What still does not build

* `nstat`, `systat` — need a `libcurses.a`; only the header was restored, and
  no curses source exists in either tree.
* `kanalyze`, `pstat`, `systat` — `nd500-cpp` gives up with "too much
  defining" on the driver headers. A limit in the cross-compiler, not in the
  sources; raising the macro table in pcc-nd500 would settle it.
* `conf`, `config` — `config.y` uses constructs the K&R rewrite of byacc's
  output does not survive.
* `as` — needs `lex` output (`_yyless`); `ftpd` needs its own yacc grammar.
* `graph` needs the plot library; `panalyze` needs `_pr_user`; `timed`,
  `tftpsubs` unresolved.
