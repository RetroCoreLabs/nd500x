# NDIX working scripts

Boot drivers, guest-image builders and measurement harnesses written while
debugging NDIX on nd500x. They lived in a session scratch directory, which
meant the reproduction steps referenced from `docs/` pointed at a temporary
path that would vanish. They are kept here so the repros survive.

These are working tools, not a polished suite: each was written for one
investigation. They are here because re-deriving them costs more than keeping
them.

## Environment

No script contains a machine-specific path. Set these before use:

| Variable | What it points at |
|---|---|
| `NDIX` | NDIX-C checkout - disk images and `kernel/MASTER/` |
| `NDIX_B` | NDIX-B checkout - the authentic 1988 headers and sources |
| `PCC_ND500` | pcc-nd500 checkout - the cross toolchain, its `bin/` |
| `ND5000UC` | ND-5000 microcode tree (referenced by the docs only) |
| `RETROCORE` | RetroCore checkout (referenced by the docs only) |

The nd500x repo root is derived from each script's own location, never
configured.

## The ones worth knowing about

| Script | What it does |
|---|---|
| `exp4.py` | Two-terminal telnet login churn. **The** reproduction for the open `resume()` / unmapped u-area crash - see `docs/HANDOFF_NDIX_RESUME_UAREA_2026-07-31.md`. Takes a telnet port. |
| `exp3.py`, `churn.py` | Earlier, simpler churn drivers. |
| `verify.sh`, `verify_auto.sh` | Boot-and-check gates: run a boot and assert on what the log contains. |
| `hitrate.sh`, `tnrate.sh`, `zrep.sh` | Repeat a boot N times and report how often a fault reproduces. Built for the ~4-in-6 crash rate measurement. |
| `mkimage.sh`, `stage.sh`, `stage.py`, `stage2.py` | Build a populated guest root filesystem image from the NDIX-B/NDIX-C trees. |
| `mkar.py` | Build an ND-500 `a.out` archive with a ranlib TOC, matching the 1988 `ar`/`ranlib` layout. |
| `mkobj.sh`, `mkprog.sh`, `build1.sh` | Compile/assemble/link single guest objects and programs through the cross toolchain. |
| `ptyboot*.py`, `ptyrun.py`, `ptyndix.py` | Boot nd500x under a pty so a scripted driver can type at the guest console. |
| `telcli.py`, `tclient.py` | Minimal telnet clients for the guest terminal server. |
| `fscheck.py`, `fswalk.py`, `sbdump.py` | Inspect a guest FFS image - superblock, inodes, directory walk. |
| `devcheck.py`, `genproto.py`, `knr_yacc.sh`, `linkclose*.sh` | One-off investigation helpers, kept for reference. |

## Caveat

Most of these were run once or twice against a specific tree state. Treat them
as a starting point and read before running - several write files, and the
image builders create multi-megabyte images.
