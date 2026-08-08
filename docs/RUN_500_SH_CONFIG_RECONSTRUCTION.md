# ~/run_500.sh config reconstruction (2026-07-24)

## What was found

`~/run_500.sh` launches:
```
build/bin/nd500x --monitor \
    --config build/test_sintran/nd500x.ini
```
(or `--telnet <port> --config ...` if a port arg is given).

`build/test_sintran/nd500x.ini` was **missing**. It
was never committed to git (it lived only under the gitignored `build/`
tree), and no copy or historical trace of it was found:
- `git log --all --oneline -- '*.ini'` — no hits, repo-wide.
- No other `.ini` file anywhere under `.`.

Nothing recreates `build/test_sintran/` on a fresh build or `make clean` —
unlike `build/nc_sandbox`, which has a CTest fixture
(`test/nc_sandbox_setup.cmake`) that reseeds it before each `ctest` run, there
is no equivalent fixture for `test_sintran/` or `build/link_sandbox/`. All
three are ordinary directories under `build/`, so any clean/rebuild silently
destroys hand-set-up sandbox state.

## What was reconstructed

The ini format itself is documented in source, not lost: `load_config()` in
`src/frontend/nd500x/nd500x.c` (around line 86)
parses simple `key = value` lines and recognizes exactly five keys:
`sintran-root`, `user`, `terminal-type`, `telnet-port`, `monitor`.

Rebuilt file, `build/test_sintran/nd500x.ini`:
```
sintran-root = .
user = GUEST
terminal-type = 53
```

- `sintran-root` = repo root, chosen because the repo root already has live
  `GUEST/`, `SYSTEM/`, `SCRATCH/` directories matching the layout
  `mon_path.c` expects under `sintran-root` (default `.`).
- `user = GUEST` matches the populated `GUEST/` dir (A.C, B.C,
  DDBTABLES-G06.VTM, etc).
- `terminal-type = 53` (TDV-2200/9) matches what the `nd500-linker` skill
  says the original ini set, and for the documented reason: model 53 uses a
  scrolling render path that shows the linker's full `NDL(ADV):` prompt,
  where VT100/ANSI models clip it to `ND`.

## Known gap: repo-root SYSTEM/ is empty

Checked and confirmed empty:
```
$ ls -la SYSTEM
total 8
drwxr-xr-x  2 ronny ronny 4096 Jul 20 01:31 .
drwxr-xr-x 19 ronny ronny 4096 Jul 23 17:27 ..
```

The documented ND LINKER workflow (`docs/` via the `nd500-linker` skill)
needs `SYSTEM/LINKER.DOM` + `DDBTABLES` + `LINKER.INIT` under `sintran-root`
for the SINTRAN shell's `@lin` abbreviation to resolve. Those files are not
at the repo root, and the sandbox that used to have them —
`build/link_sandbox` — no longer exists. It was never committed and did not
survive a clean, the same way `test_sintran/` went.

**Resolved (2026-08-08).** `$ND500USERS` supersedes it. Its `SYSTEM/` holds
`LINKER-B01.DOM/.HELP/.INIT`, six `DDBTABLES-*.VTM`, `UE-ERMSG-EN-C06.ERR`
and the libraries under the un-revisioned names an unqualified open asks for
(`CAT-LIB.NRF`, `NC-LIB.NRF`), all built by its own `populate-system.sh`.
Point `sintran-root` there — the ini accepts a leading `~`
(`src/frontend/nd500x/nd500x.c:241`), so no machine-specific path is needed:

```ini
sintran-root  = ~/ND500USERS
```

The earlier decision to keep `sintran-root` at the repo root with an empty
`SYSTEM/` is superseded; that arrangement could never resolve `@lin`.

## Verified working

- `build/bin/nd500x` exists (already built).
- `~/run_500.sh` (no args) now loads the ini and starts `--monitor` mode
  rooted at the repo root as user GUEST.

## Related

- `README.md`, "SINTRAN Shell (Monitor Mode)" section
  — has the general `--monitor`/`--config` usage docs, plus a "Convenience
  launcher (`~/run_500.sh`)" subsection added 2026-07-24 with this exact
  ini and the same repo-root-vs-link_sandbox caveat.
- Skill: `nd500-linker` (`~/.claude/skills/nd500-linker/SKILL.md`) — updated
  with a 2026-07-24 note on the missing `link_sandbox` and ini reconstruction.
- Memory: `nd500x-ini-reconstructed` and `linker-run-from-link-sandbox`
  (`~/.claude/projects/-home-ronny-repos-nd500x/memory/`) — updated the same
  day; the latter marked STALE pending `link_sandbox` recreation.
