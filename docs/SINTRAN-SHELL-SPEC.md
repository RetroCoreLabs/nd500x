# SINTRAN III shell for nd500x - Phase 0 spec (sourced from the real manuals)

Full path of this document: `/home/ronny/repos/nd500x/docs/SINTRAN-SHELL-SPEC.md`

Written 2026-07-20. This is a SPEC ONLY - no code has been written. Every
behaviour below is cited to a real ND manual or to a byte-level reverse
engineering; anything we could not source is called out as **UNVERIFIED / GAP**.

Goal: a faithful in-emulator SINTRAN shell - the `@`-prompt you log in to, run
programs from, and log out of - so you can drop copies of `:PROG`/`:DOM`
programs under a user directory and run them by name, with `HELP`.

## Sources (all real, on disk)

- **RefMan** = `/mnt/e/Dev/Ronny/NDInsight/Reference-Manuals/ND-60.128.5 EN SINTRAN III Reference Manual.md` (the definitive per-command reference)
- **Batch Guide** = `/mnt/e/Dev/Ronny/NDInsight/Reference-Manuals/ND-60.132.03 SINTRAN III Timesharing Batch Guide.md`
- **User's Guide** = `/mnt/e/Dev/Ronny/NDInsight/Reference-Manuals/ND-60.050.06 SINTRAN III Users Guide.md` (file-system chapter 3.2/3.3, error codes D.2.1)
- **Password RE** = `/mnt/e/Dev/Ronny/NDInsight/tools/sintran-segment-carver/versions/L-VSX-500/re/PASSWORD-ALGORITHM.md` (byte-proven login fold)

## THE decision that reshapes the request: two shells, not one

SINTRAN has a top-level command shell (ND-100 side). Running an **ND-500 domain
(`:DOM`)** is NOT done there - it is done in a **separate "ND-500 Monitor"**
sub-shell, whose commands are `RECOVER-DOMAIN <name>` (place + start a domain),
`PLACE-DOMAIN`, `RUN`, and `HELP` (RefMan 16232 / 16047 / 16598 / 14739).

nd500x **is** an ND-500 emulator. So the authentic home for "run my `:DOM` by
name" is the **ND-500 Monitor shell**, not the SINTRAN top level. Your original
`login`/`run`/`logout` idea maps most naturally onto:

- a thin **SINTRAN top-level** for the login/session feel (ESCAPE login, `@`
  prompt, `LIST-FILES`, `LOGOUT`), which then
- enters the **ND-500 Monitor** where `RECOVER-DOMAIN <name>` runs your domains.

Recommendation: build the **ND-500 Monitor shell first** (that is where DOMs run
and where `HELP` lives), and add the SINTRAN top-level login wrapper around it.
Open decision for you at the end.

## Session: login is the ESCAPE key, not a "login" command

This is the biggest correction to the original request. Verbatim, Batch Guide
934: after the terminal is online you **press ESCAPE**; SINTRAN prints the time
and asks **"User Name"**; you type it + CR; it prompts for a **password**
(optional, **not echoed**, any characters + CR); if accounting is on it also asks
a **project password**; then the **`@`** prompt appears.

- `@` is the prompt the system prints when ready for a command (RefMan 158).
- **`@LOGOUT`** ends the session (RefMan 6420): prints time/date and `-- EXIT --`.
  Parameters: none.
- `@ENTER <user>,<password>,<project>,<maxtime>` is **NOT interactive login** - it
  identifies the owner of a **batch job** and must be the first line of a batch
  file (RefMan 4611). Example: `@ENTER GUEST,,,5`.
- ESCAPE also **aborts a running program** -> `USER BREAK AT <addr>` and closes
  open files (Batch Guide 1602). Line editing: CTRL+A delete char, CTRL+W delete
  word, CTRL+Q re-edit previous line, CTRL+H backspace (RefMan 210-234).

So the faithful flow is: **press ESC -> type user name -> type (hidden) password
-> `@`**; leave with **`@LOGOUT`**. (We can offer a `login <user>` convenience
alias, clearly marked non-authentic, but the real thing is ESC-driven.)

### Password mechanism (byte-proven, Password RE)

Not hashed - the password is folded to a single 16-bit word and compared:
```
acc = 0
for each char until CR:
    c   = uppercase(char)          # letters only
    acc = (rotate_left_16(acc,3) + c) & 0xFFFF
compare acc against the stored 16-bit word in the user entry
```
Echo is turned off during entry (MON 3 ECHOM). Case-insensitive, unsalted (two
users with the same password get the same word). We reproduce this faithfully.
nd500x already tracks a current user (`mon_config_set_current_user`, which
uppercases like SINTRAN); login just needs to fold+compare then set it.

## Running programs

- **SINTRAN top level:** `@RECOVER <name>` or just `@<name>` loads and starts a
  `:PROG` (default type `:PROG`). Search order (RefMan 7296): reentrant
  subsystems -> current user's directory -> user SYSTEM's directory -> if a user
  is named, only that directory. Bare-name only works if it does not clash with a
  command; `@RECOVER` forces the file search.
- **ND-500 Monitor:** `RECOVER-DOMAIN <domain>` places + starts a domain;
  `PLACE-DOMAIN <domain>` prepares it; `RUN` starts an already-placed domain
  (RefMan 16232/16047/16598). Domain lookup order mirrors the file rule (basic
  commands -> standard domains -> current user -> SYSTEM -> macros); else
  `NO SUCH COMMAND OR DOMAIN`.

This is exactly the run-by-name behaviour you wanted - and it already matches
nd500x's own file lookup (`mon_path.c`, own-user then SYSTEM).

## Filenames (User's Guide 3.2)

Grammar (User's Guide 1525-1529):
```
file-ref := [ "(" [ directory ":" ] owner ")" ] name [ ":" type ] [ ";" version ]
name/owner/directory: 1..16 chars   type: 1..4 chars   version: 1..256 (1 = newest)
"-" divides abbreviatable subparts;  "..." quotes create-if-new
```
Standard types (User's Guide 1434-1441): `:SYMB` source, `:BRF` binary
relocatable, `:PROG` runnable program, `:CORE` core image, `:BIN` binary
absolute, `:DATA` data. Default type is chosen per command context (e.g. run =
`:PROG`, create = `:DATA`); a type after `:` always overrides.

Resolution (User's Guide 1720-1724): own directory first, then SYSTEM; if a user
is named, only that directory. **nd500x `mon_path.c` already implements this
faithfully.** Divergences to fix if we want full fidelity: the `(dir:owner)`
inner-colon split, and `;version` - both currently unsupported.

## Errors and messages (NOT the linker's `(nnnn:nn)` style)

The interactive shell prints **plain text** messages, e.g. `NO SUCH FILE NAME`,
`AMBIGUOUS COMMAND`, `NO SUCH COMMAND OR DOMAIN` (Batch Guide 1204/1258, RefMan
16232). Real example (Batch Guide 1204):
```
@L1=F
NO SUCH FILE NAME
@
```
Runtime/kernel errors use `ERROR nn AT <addr>; <text>` (Batch Guide 1246). The
`*** ERROR ... (nnnn:nn)` format is the **linker's own**, not SINTRAN's - do not
copy it into the shell.

File-system error numbers are **octal**, returned in the A register (User's Guide
D.2.1). Key ones: `056 No such file name`, `057 Ambiguous file name`, `046 No
such user name`, `060 Wrong password`, `074 No such file version`, `076 File
already exists`, `025 Not authorized`. Command-level: `AMBIGUOUS COMMAND`
(octal 341).

## Abbreviation (Batch Guide 1044-1052)

Commands abbreviate to any **unique prefix per hyphen-word**: `LIST-FILES` may be
`L-FIL` or `LI-FI`, but `LI-F` gives `AMBIGUOUS COMMAND` (clashes with
`LIST-FRIENDS`). Same for file names. On create, the name must be given in full.

## HELP (RefMan 14739 - the ND-500 Monitor entry)

`HELP <command name>` lists all commands matching the (possibly abbreviated) name
with their parameters; default lists all. Optional parameters are shown in `[]`.
This is the authentic `help` you wanted. Per-program **examples** are not a SINTRAN
feature - we add them as an nd500x extension via an optional sidecar file (e.g.
`NAME.DOM.meta`) surfaced by `HELP NAME`, clearly marked non-authentic.

## Tier-1 command set (what the MVP implements), all sourced

| Command | Shell | Purpose | Source |
|---|---|---|---|
| (ESCAPE) then User Name + password | SINTRAN | interactive login | Batch Guide 934 |
| LOGOUT | SINTRAN | end session, `-- EXIT --` | RefMan 6420 |
| RECOVER `<name>` / `@<name>` | SINTRAN | run a `:PROG` by name | RefMan 7296 |
| LIST-FILES `<name>,<out>` | SINTRAN | list files (object no. + full name) | RefMan 5735 |
| RECOVER-DOMAIN `<domain>` | ND-500 Mon | place + start a `:DOM` | RefMan 16232 |
| PLACE-DOMAIN `<domain>` / RUN | ND-500 Mon | prepare / start a domain | RefMan 16047/16598 |
| HELP `[<cmd>]` | ND-500 Mon | list commands + parameters | RefMan 14739 |
| CHANGE-PASSWORD | SINTRAN | change password (echo off) | RefMan 1436 |

Tier-2 (file maintenance, sourced, later): CREATE-FILE (RefMan 2132),
DELETE-FILE (2787), RENAME-FILE (7595), COPY-FILE (2031), SET-FILE-ACCESS (8844),
CREATE-USER (2246), LIST-USERS (6318).

## What nd500x already has (so this is mostly wiring)

- Current user + FS root config with defaults: `src/libmon/mon_config.c`
  (uppercases user names like SINTRAN; default user is **`GUEST`** - see GAP 1).
- Own-user -> SYSTEM filename resolution: `src/libmon/mon_path.c`
  (`mon_translate_path_lookup`), faithful to User's Guide 1720-1724.
- Pluggable console with **blocking reads**: `ConsoleIO` + `STOP_WAIT_INPUT`
  (INBT suspends when no input) - the bridge a live shell needs.
- DOM load + run: `--dom` / `loaddom` + the run loop.
- Scripted input feeding: `test/diag_linkdrive.c` mechanism (reuse for batch).

Missing = the shell REPL itself: the ESC-login flow, the command parser with
abbreviation, run-by-name wired to `mon_path`, `LIST-FILES`, `HELP`, and the
host-terminal <-> `ConsoleIO` bridge for interactive runs.

## GAPS / UNVERIFIED (do not invent - confirm before relying on these)

1. **`GUEST` is an nd500x invention.** Real SINTRAN has no GUEST; the fallback/
   system user is **SYSTEM**, and the current user should be whoever logged in.
2. **LIST-FILES exact column widths** - manual states content ("object number and
   full name") but shows no literal header row. NOT FOUND.
3. **Top-level `@HELP` full entry** (Format/Rules/Example) - only a summary line;
   the detailed HELP entry we have is the ND-500 Monitor's. NOT FOUND.
4. **Default numeric value of the ESCAPE character** - definable, stored in CESC;
   the default value itself NOT FOUND in the sections read.
5. **Password length/complexity rules** - none stated ("any set of characters").
6. **How you enter the ND-500 Monitor** from the SINTRAN top level - the commands
   are documented under an "ND-500 Monitor" section but the entry command was not
   located. AMBIGUOUS - confirm.
7. **`(dir:owner)` inner-colon split and `;version`** - real syntax, not yet
   parsed by `mon_path.c`.

## Transport: local terminal AND telnet (TDV/VT) - added 2026-07-20

Requirement: the shell must be reachable both from the local console and over a
**TCP/telnet** server, so a real/emulated **TDV terminal** (or a VT emulator) can
connect. Plus a **`SET-TERMINAL-TYPE`** command to pick the terminal (e.g. VT100
vs TDV 2200/9), so output is encoded with the right escape sequences.

This fits the existing design because the console is a swappable `ConsoleIO`
(read_char / write_char / char_available / wait_for_input / user_break):

- **Local transport (exists):** `mon_install_stdio_console()` in
  `src/libmon/mon_file_table.c` already puts the host terminal into raw mode and
  bridges STDIN/STDOUT to `ConsoleIO`. Reuse as-is.
- **Telnet transport (new):** a `ConsoleIO` backed by a listening TCP socket -
  `accept()` a client, `read_char`/`wait_for_input` = socket recv, `write_char` =
  socket send, with minimal telnet IAC negotiation (suppress echo/go-ahead, put
  the client in character-at-a-time mode). Selected by `--telnet <port>` on the
  command line or a `[telnet] port=...` line in the ini file. One client at a
  time is fine for v1.
- **Config/ini:** a small ini (default `./nd500x.ini` or `--config <file>`) can
  set: sintran root, default user, transport (local|telnet) + port, and default
  terminal type. Command-line flags override the ini.

### SET-TERMINAL-TYPE (SOURCED, ND-60.128.5)

Command (RefMan 9309-9334): `@SET-TERMINAL-TYPE <terminal number>,<terminal type>`
- `<terminal number>` = a terminal (decimal; default = user's own terminal).
- `<terminal type>` = decimal -32768..32767, default 0.
- Only user SYSTEM may set terminals other than its own. Companion:
  `@GET-TERMINAL-TYPE` (RefMan 5030). Manual example: `@SET-TERMINAL-TYPE ,,53`.

MON calls (already stubbed in nd500x, to be filled): **MSTTY = MON 17B** (set) and
**MGTTY = MON 16B / 116** (get). Today `mon_17B_SetTerminalType.c` accepts-and-
ignores and `16B` answers 0. NOTE: `mon_52B_TerminalMode.c` is a DIFFERENT call
(TERMO page-stop/uppercase), NOT terminal type - do not use it.

**The terminal-type number is BIT-ENCODED, 16-bit** (RefMan 17597-17608), not a
plain model id:

| Bits | Meaning |
|---|---|
| 14 | terminal is a VDU (not hard copy) |
| 13 | handles ASCII backspace (BS) |
| 12 | ASCII form-feed (FF) clears screen / new page |
| 11 | VDU has cursor positioning |
| 10 | uses ASCII ESC within input sequences |
| 7-0 | terminal MODEL number (table below) |

Model numbers relevant to you (RefMan 17632-17689):

- **6 = DEC-VT100** (80-col) - your guess CONFIRMED.
- **53 = TANDBERG TDV-2200/9-ND NOTIS** - the base ND-standard TDV 2200/9, and the
  value the manual's own `SET-TERMINAL-TYPE` example uses (`,,53`).
- **93 = TANDBERG TDV-2200/9S-ND NOTIS** - real, but the "**S**" variant, not the
  plain 2200/9. (Also 55 = 2200/9-ND-NET, 83 = 2200/9-V2, 7 = older TDV-2000.)

So for "TDV 2200/9", **53** is the likely intended value; 93 is the 2200/9S.

**Important scope note:** the manuals give NO per-model escape-sequence/cursor
table - terminal behaviour is defined ONLY by the 5 capability bits (10-14).
Faithful v1 = store the 16-bit type, expose those capability bits, and drive
output by them (does FF clear the screen? does the VDU have cursor addressing?).
Fine escape-code differences between specific models live in each terminal's own
manual / the VTM-compound utility (ND-60.151), not here - out of scope for v1.

## Proposed build phases (after you approve this spec)

- **Phase 1 (MVP):** `nd500x --monitor` starts the ND-500 Monitor shell:
  `RECOVER-DOMAIN`/`PLACE-DOMAIN`/`RUN`/`HELP`, run-by-name via `mon_path`,
  host-terminal bridge for interactive DOM runs, `--script <file>` for batch,
  sidecar `NAME.DOM.meta` help. FS root config (default `./sintran`, auto-create
  `SYSTEM/`).
- **Phase 2:** SINTRAN top-level wrapper: ESC login + User Name + hidden password
  (the byte-proven fold), `@` prompt, `LIST-FILES`, `LOGOUT`, abbreviation engine,
  plain-text error messages, `CHANGE-PASSWORD`. Switch default user off `GUEST`.
- **Phase 3:** file maintenance (CREATE/DELETE/RENAME/COPY-FILE, users), version
  and `(dir:owner)` syntax, access (friend/public) enforcement.

## Decisions made 2026-07-20 (drive Phase 1)

1. **Front door = BOTH layers together** - a SINTRAN top-level login that drops
   the user into the ND-500 Monitor prompt where DOMs run.
2. **Login = simple `login <user>` for now** - no password fold yet; clearly
   marked as an nd500x convenience, not the authentic ESC-driven flow. (The
   byte-proven password fold is kept in this spec for a later fidelity pass.)
3. **Default user = SYSTEM** - drop `GUEST`; current user becomes whoever
   `login`s; SYSTEM is the fallback/system directory.

Build order given these: one `nd500x --monitor` (or `--shell`) mode that opens at
a SINTRAN-ish prompt, supports `login <user>` / `logout` / `help`, and gives
access to the ND-500 Monitor verbs (`recover-domain <name>` / bare `<name>` to
run a DOM, `list-files`) wired to `mon_path` + the existing DOM load/run and the
`ConsoleIO` host-terminal bridge; `--script <file>` for batch input.
