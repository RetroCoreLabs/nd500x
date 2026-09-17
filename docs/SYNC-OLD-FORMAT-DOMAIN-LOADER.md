# Old-format domains (:PSEG + :DSEG + DESCRIPTION-FILE:DESC) run directly in the monitor shell

Date: 2026-09-15. nd500x change; C# (RetroCore) has no equivalent yet.

## What changed

Typing a domain name at the nd500x monitor shell (`nd500x --monitor`, the
`run_500.sh` path) used to resolve ONLY `<user>/<NAME>.DOM` then
`SYSTEM/<NAME>.DOM`. A domain shipped in the OLD format - `<NAME>:PSEG`,
`<NAME>:DSEG`, `<NAME>:LINK` plus an entry in the owning user's
`DESCRIPTION-FILE:DESC` - gave `NO SUCH COMMAND OR DOMAIN`, and the only route
was to convert it first with the vendor CONVERT-DOM-A03 program
(`docs/CONVERT_DOMAIN_FORMAT_AND_USAGE.md`).

Now `stage_domain()` in `src/frontend/nd500x/nd500x_shell.c` falls back to the
old format: it looks the typed name up (exact match, case-insensitive) in the
current user's `DESCRIPTION-FILE.DESC`, then in `SYSTEM/DESCRIPTION-FILE.DESC`,
and `ndlib_load_old_domain()` in `src/ndlib/ndlib_dom.c` stages the pair into
the same in-memory image the :DOM reader produces, so
`ndlib_dom_load_to_machine()` places it with no change. Both shell load paths
use it: the `@` prompt / `RECOVER-DOMAIN`, and the nested MON 317B UECOM runner.

The DESC byte layout is `include/nd500_desc.h`, a verbatim copy of
`src/include/nd500/desc.h` from the pcc-nd500 tree (its offsets were read out
of the ND-500 Loader/Debug Monitor's own print code and checked on 13 vendor
floppies).

## The mapping, and the evidence for every value

The staged image reproduces what CONVERT-DOM-A03 writes for the same files.
Two conversions made under nd500x were dumped (header bytes 0xD8-0x104 and the
segment table at 0x254) and compared with their DESC entries:

| Field | DESC entry (LINKAGE-LOAD-H02) | Converted `$ND500USERS/FLOPPY-USER/LINKAGE-LOAD-H02.DOM` |
|---|---|---|
| start address | STADR `0xB0000DD1` | STADDR@0xD8 `0xB0000DD1` |
| THA | `0xB0215310` | THA@0xE0 `0xB0215310` |
| segment number | PSEG use `0x00400000` = bit 22, DSEG use same | segment table slot 22 |
| PROG image | `.pseg` 123989 bytes (PLB 0 + PSIZE 123988 + 1) | seg 22 PROG SZ 123989, FLA 0, ATT `0x10002000` |
| DATA image | `.dseg` 2184977 bytes (DLB 75834 + DSIZE 2109142 + 1) | seg 22 DATA SZ 2184977, FLA 0, ATT `0xE1002000` |
| trap enable | ENABLEINT `0x0FFE00AC` | OTE1@0xF0 `0xFC015800`, OTE2@0xEC `0x0000001F` |
| TEMM | (not in DESC) | TEMM1@0x100 `0xFFFFFA00`, TEMM2@0xFC `0x0000001F` |

| Field | DESC entry (LED-B03) | Converted `$ND500USERS/SYSTEM/LED-NEW.DOM` |
|---|---|---|
| start address | `0x08000004` | `0x08000004` |
| THA | `0x0806011C` | `0x0806011C` |
| segment number | PSEG/DSEG use `0x00000002` = bit 1 | slot 1 |
| PROG / DATA | `.pseg` 223695, `.dseg` 394525 | SZ 223695 / 394525, FLA 0, same ATT values |
| trap enable | ENABLEINT 0 | OTE1 = OTE2 = 0 |
| TEMM | - | same constant as above |

Conclusions taken from that, and nothing more:

- The WHOLE `.pseg` is the program segment image from segment offset 0, and the
  WHOLE `.dseg` is the data image from offset 0. DLB is NOT an offset into the
  segment (LINKAGE-LOAD has DLB 75834 and its converted DATA size is still the
  full file).
- Segment number = the single set bit of the domain entry's PSEG/DSEG use
  bitmaps.
- ENABLEINT -> OTE: all 15 set bits of `0x0FFE00AC` land 9 bits higher in the
  64-bit OTE (OTE1 low word, OTE2 high word): bits 2,3,5,7 -> OTE1 11,12,14,16;
  bits 17-22 -> OTE1 26-31; bits 23-27 -> OTE2 0-4. Implemented as
  `OTE1 = ENABLEINT << 9`, `OTE2 = ENABLEINT >> 23`. **One non-zero witness
  only** (plus LED's trivial zero). Exact for it; a second domain with a
  non-zero ENABLEINT would confirm or refute it.
- TEMM is the same constant in both conversions and is copied as a constant.
  Its meaning was not derived.
- Header identification bytes 4-7 = 97, 3, 0xF8, 0 as in both conversions
  (0xF8 carries the is-domain-file flag the reader tests).

## What is refused rather than guessed

- A domain whose DESC segment chain has more than one entry: the segment
  entry's own segment-number field is not decoded, so entries could not be
  assigned to segment slots. All 13 known DESC files have exactly one segment
  per domain with PSEG use == DSEG use.
- A `.pseg`/`.dseg` whose byte count differs from `PLB+PSIZE+1` /
  `DLB+DSIZE+1`: the files do not belong to that entry. Message names the file,
  its size and the expected size.
- A missing segment file: message names the DESC, the segment name and the
  last path tried.
- No abbreviation matching for old-format names (the :DOM path keeps the
  carved COMPS/GOBJI abbreviation rule; the DESC lookup needs the exact name).

## Where the segment files are looked for

The DESC segment name has the form `(directory:user)NAME`. Candidates, in
order, with root = the parent of the DESC's directory (the sintran-root/USER
layout): `<root>/<user>/NAME.PSEG`, then the DESC's own directory, then
`<root>/SYSTEM/NAME.PSEG` (the own-directory-then-(SYSTEM) rule every
unqualified file open already follows). So LED-B03's DESC can live in the
login user's directory while the pair stays under SYSTEM.

## Verified runs (2026-09-15, build from this change)

- `LED-B03` typed as GUEST with `$ND500USERS/GUEST/DESCRIPTION-FILE.DESC` (a
  copy of `$ND500_TESTDATA/LED/x/description-file.desc`) and the pair under
  `$ND500USERS/SYSTEM`: placed in domain 1 at 0x08000004, painted the editor
  screen ("Main" window, `LED:` prompt, "August 24, 1988."), exited at end of
  piped input after 507595 instructions - the SAME instruction count and screen
  as running the converted `LED-NEW.DOM`.
- `LINKAGE-LOAD-H02` typed as DOMAIN-USER from that user's DESC: placed at
  0xB0000DD1, printed the `Nll:` prompt, `EXIT` returned to the shell.
- Truncated `.dseg`: refused with the size message; nothing placed.
- `ctest`: 39 of 39 pass.

`$ND500USERS/SYSTEM/DESCRIPTION-FILE.DESC` is the LINKAGE-LOAD-H02 description
file (2 domains: SCRATCH-DOMAIN, LINKAGE-LOAD-H02); it does not list LED-B03
and was not modified. Merging DESC files is not implemented: the 256-byte page
header is not decoded, so a written DESC would be a guess.

## For the C# side

RetroCore's loader reads :DOM only. To mirror: read the DESC entry with the
offsets in `include/nd500_desc.h`, load both files whole as segment images at
offset 0 into the bitmap's segment slot, copy STADR/THA, derive OTE as above,
and refuse the same cases.
