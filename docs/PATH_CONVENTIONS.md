# Path conventions

Nothing tracked in this repository names a path on a particular machine. A repo
that carries `/home/<someone>/...` or `/mnt/e/...` only works on the machine it
was written on.

Two forms are used, and only these two:

1. **Inside this repo** - paths are relative to the repo root, e.g.
   `src/cpu/nd500_fecall.c`, `build/bin/nd500x`. Scripts derive the root from
   their own location:

       REPO_ROOT=$(cd "$(dirname "$0")/../.." && pwd)

2. **Outside this repo** - an environment variable. Set the ones you need; a
   script that requires one says so and fails early rather than guessing.

| Variable | Tree |
|---|---|
| `NDIX` | NDIX-C checkout: disk images, `kernel/MASTER/`, and `notes/` |
| `NDIX_B` | NDIX-B checkout: authentic 1988 headers and sources |
| `PCC_ND500` | pcc-nd500 checkout - the cross toolchain, its `bin/` |
| `RETROCORE` | RetroCore checkout - the C# ND-500 CPU |
| `ND500_TESTDATA` | ND-500 test material: `.dom` images, NRF files, disk images. CMake probes this for the DOM smoke test |
| `ND500USERS` | SINTRAN user-area tree |
| `NDINSIGHT` | NDInsight tree - the MON call corpus |
| `ND5000UC` | ND-5000 microcode and manual transcriptions |
| `RETROTERM` | RetroTerm checkout |
| `NDMONLIB` | ndmonlib checkout, when used outside `external/` |
| `ND100X` | nd100x checkout |
| `LIBDAP` | libdap / mcp-dap-server checkout |
| `ND500_FS` | SINTRAN filesystem root used by the JSON machine config |
| `ND500X_WORK` | Scratch/work area. Defaults to `$REPO_ROOT/work`, which is gitignored |
| `ND500X_TAPE` | SIMH `.tap` image for the tape device (generic 2) |
| `ND500X_DISK` | Root disk image |

## Working notes

Notes, handoffs, session write-ups and investigation logs are **not** repository
content. They live in `$NDIX/notes/`. Only material that has become real
reference documentation belongs in `docs/`.

## Build and test tooling

Anything on the build or test path is shell, not Python, so a checkout needs no
interpreter beyond a C compiler and CMake. `test/mktape.sh` is the model.
Python remains fine for ad-hoc debugging, but those scripts live outside the
repo.
