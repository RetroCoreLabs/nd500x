# SINTRAN conventions that keep biting

Behavioral facts about SINTRAN III and the ND-500 vendor toolchain that
are NOT written down anywhere else findable, and that were each supplied
by hand (some repeatedly) during past sessions. Each entry says how it is
known. If an entry is wrong, fix it HERE, not just in the code.

## C source fed to NC must be K&R 1978 style

NC (nc-a06.dom) is a 1980s compiler. Test inputs must be old-style C:

- No ANSI prototypes; parameter types are declared between the argument
  list and the body:

      int add(a, b)
      int a, b;
      {
          return a + b;
      }

- No `void`, no `const`, no modern keywords; no `//` comments.
- ASCII only. The whole toolchain predates Unicode.

How known: user-stated, three times, after ANSI-style test files failed to
compile. The standing examples live in `$ND500USERS/GUEST/` (HELLO.C,
A.C, B.C).

## String and filename conventions

- `0x27` (the `'` character) is end-of-string in SINTRAN, the way `0x00`
  is in Unix. (User-stated; also recorded in CLAUDE.md.)
- A filename in double quotes means CREATE-IF-MISSING: writing to
  `A:SYMB` fails if the file does not exist; writing to `"A:SYMB"`
  creates it first. (User-stated; needed to make NC/linker output files
  appear.)
- File type separator: SINTRAN uses `:` (e.g. `HELLO:NRF`); on the host
  side this maps to `.` transparently (`HELLO.NRF`). Names are upshifted
  to uppercase by the path layer (`src/libmon/` mon_path.c). (Implemented
  and carve-verified in the filename matcher work, 2026-07-17.)
- `--` in a filename is a wildcard matching anything in between
  (`UE-ERMSG--C:ERR` matches `UE-ERMSG-EN-C06:ERR`). There is NO `*`
  wildcard; "no type given" means all types. (User-stated during the
  UE-ERMSG episode; abbreviation matching itself is carve-verified -
  COMPS/GOBJI, ambiguous = error 057.)
- Command matching uses shortest-unique-prefix: `NC-A` resolves to
  `NC-A06` if unambiguous. Documented in the SINTRAN Users Guide
  (ND-60.050.06).

## Terminal types

- The valid terminal-type list comes from the VTM tables file
  (DDBTABLES-*.VTM), NEVER from a hardcoded table. Format:
  `docs/VTM-FILE-FORMAT.md`.
- SINTRAN's `SET-TERMINAL-TYPE` sets the type; a program needing VTM with
  terminal type 0 will prompt for it. This sandbox uses type 53 in
  nd500x.ini. (User-stated.)

## Open/lookup semantics

- Unqualified filenames resolve own-directory first, then `(SYSTEM)`.
  Carve-verified (GFILI); implemented as `mon_translate_path_lookup`,
  wired into 50B OPEN and 256B DEABF.
- MON calls on ND-500 take 32-bit word (`W`) INTEGER parameters
  (ND-100: 16-bit halfwords). Assembly declares them `W BLOCK`.

## Toolchain shape (why "NC alone" cannot produce code)

- NC is only the front-end. Code generation happens in CAT-500
  (cat-cat5-b06.dom, the Vienna Development Method back-end), which NC
  invokes through MON 317B UECOM with the CAT intermediate in a scratch
  file. Linking is a third program (linker-b01.dom). A stubbed UECOM
  therefore looks exactly like "NC silently produces an empty NRF".
  (Established 2026-07; vendor-confirmed via the user's contact.)
- The linker invokes `(SYSTEM)LINKER-AUTO-<lang>.JOB` at CLOSE; the C job
  LOADs `(SYSTEM)NC-LIB` and `(SYSTEM)CAT-LIB` (read from the job file
  bytes). Missing link-time libraries surface as
  `No such file name (0000:56)`, which looks like an emulator bug and is
  not one.
- CAT-500 stamps language byte = 1 (FORTRAN) on every NRF it emits,
  including the vendor C stdlib; the LINKER-AUTO-FORT:JOB warning that
  follows is expected, not a defect.

## Debugging priority order

When a real vendor binary misbehaves under nd500x, assume OUR CPU/MON
emulation is wrong until the carve or a manual proves otherwise. Every
"the guest program has a bug" theory so far (NC codegen loop, linker
input handling, heap crash) has resolved to an emulator or MON defect -
with exactly one settled exception, the linker's own prompt-clipping
escape stream. Never patch in an empirically-guessed constant (a `;1`
suffix, a terminal table, a default) when the carve can be asked for the
real algorithm.
