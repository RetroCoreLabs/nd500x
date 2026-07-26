# ~/run_500.sh config reconstruction (2026-07-24)

## What was found

`~/run_500.sh` launches:
```
/home/ronny/repos/nd500x/build/bin/nd500x --monitor \
    --config /home/ronny/repos/nd500x/build/test_sintran/nd500x.ini
```
(or `--telnet <port> --config ...` if a port arg is given).

`/home/ronny/repos/nd500x/build/test_sintran/nd500x.ini` was **missing**. It
was never committed to git (it lived only under the gitignored `build/`
tree), and no copy or historical trace of it was found:
- `git log --all --oneline -- '*.ini'` — no hits, repo-wide.
- No other `.ini` file anywhere under `/home/ronny/repos/nd500x`.

Nothing recreates `build/test_sintran/` on a fresh build or `make clean` —
unlike `build/nc_sandbox`, which has a CTest fixture
(`test/nc_sandbox_setup.cmake`) that reseeds it before each `ctest` run, there
is no equivalent fixture for `test_sintran/` or `build/link_sandbox/`. All
three are ordinary directories under `build/`, so any clean/rebuild silently
destroys hand-set-up sandbox state.

## What was reconstructed

The ini format itself is documented in source, not lost: `load_config()` in
`/home/ronny/repos/nd500x/src/frontend/nd500x/nd500x.c` (around line 86)
parses simple `key = value` lines and recognizes exactly five keys:
`sintran-root`, `user`, `terminal-type`, `telnet-port`, `monitor`.

Rebuilt file, `/home/ronny/repos/nd500x/build/test_sintran/nd500x.ini`:
```
sintran-root = /home/ronny/repos/nd500x
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
$ ls -la /home/ronny/repos/nd500x/SYSTEM
total 8
drwxr-xr-x  2 ronny ronny 4096 Jul 20 01:31 .
drwxr-xr-x 19 ronny ronny 4096 Jul 23 17:27 ..
```

The documented ND LINKER workflow (`docs/` via the `nd500-linker` skill)
needs `SYSTEM/LINKER.DOM` + `DDBTABLES` + `LINKER.INIT` under `sintran-root`
for the SINTRAN shell's `@lin` abbreviation to resolve. Those files are not
at the repo root, and the sandbox that used to have them —
`/home/ronny/repos/nd500x/build/link_sandbox` — **does not currently exist**
(confirmed via `ls`, no such directory). It was presumably lost the same way
as `test_sintran/` — never committed, wiped by a clean/rebuild.

**This is left open, by user decision (2026-07-24 conversation):** keep
`sintran-root` pointed at the repo root and leave `SYSTEM/` empty for now.
`~/run_500.sh` works for GUEST-level SINTRAN shell work today; `@lin` will
not work until `SYSTEM/` is populated or `build/link_sandbox` is recreated
and the ini repointed at it.

## Verified working

- `/home/ronny/repos/nd500x/build/bin/nd500x` exists (already built).
- `~/run_500.sh` (no args) now loads the ini and starts `--monitor` mode
  rooted at the repo root as user GUEST.

## Related

- `/home/ronny/repos/nd500x/README.md`, "SINTRAN Shell (Monitor Mode)" section
  — has the general `--monitor`/`--config` usage docs, plus a "Convenience
  launcher (`~/run_500.sh`)" subsection added 2026-07-24 with this exact
  ini and the same repo-root-vs-link_sandbox caveat.
- Skill: `nd500-linker` (`~/.claude/skills/nd500-linker/SKILL.md`) — updated
  with a 2026-07-24 note on the missing `link_sandbox` and ini reconstruction.
- Memory: `nd500x-ini-reconstructed` and `linker-run-from-link-sandbox`
  (`~/.claude/projects/-home-ronny-repos-nd500x/memory/`) — updated the same
  day; the latter marked STALE pending `link_sandbox` recreation.
