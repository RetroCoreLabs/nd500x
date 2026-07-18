# Carve question: does SINTRAN III fall back from the current user to (SYSTEM) on an unqualified OPEN?

> **ANSWERED + IMPLEMENTED 2026-07-18.** Verdict: **option (B)** - own-directory
> first, then an automatic `(SYSTEM)` fallback, performed **inside SINTRAN's
> resolver (`GFILI`), not the linker**. Triggered on error 56; **suppressed when a
> `(USER)` is named** (`GFILI` zeroes its `,B40` gate when the spec's first char
> is `(`). `GFIAC` (friend/access) is confirmed NOT part of `GFILI` - it is a
> separate post-resolution permission stage, so it is not modelled in the lookup.
> Carve evidence: `006-S3FS.asm` GFILI@057173B -> GOBJI@056326B (own scan 057263,
> SYSTEM scan 057443 via GSYSI@055540B); fallback gate 057414B on `,B40 == -1`.
> Confirmed by ND-60.050.06 Users Guide L1720-1724. Full carves:
> `.../L-VSX-500/re/segments-ref/006-S3FS/CARVE-ANSWER-UNQUALIFIED-OPEN-USER-SYSTEM-FALLBACK.md`
> and `GFILI-COMPLETE-CARVE.md`.
> Implemented as `mon_translate_path_lookup()` in
> `/home/ronny/repos/nd500x/src/libmon/mon_path.c`, wired into `50B OPEN`
> (lookup, not create) and `256B DEABF`. SYSTEM modelled as a fixed `(SYSTEM)`
> directory. Verified 5/5 (own-dir hit, SYSTEM fallback for CAT-LIB/NC-LIB,
> named-user suppression, not-found -> own dir) with no linker-startup regression.
> Remaining carve gap (not blocking): the exact GSYSI->GMUSI SYSTEM-name literal
> (our fixed "SYSTEM" rests on the symbol name + manual, not a decoded constant).

---



Full path of this file: `/home/ronny/repos/nd500x/docs/CARVE-QUESTION-USER-SYSTEM-FILE-LOOKUP.md`
Date: 2026-07-18
Target carve: SINTRAN III L-VSX-500 monitor (segment 006-S3FS), file-name
resolver family (`GFILI` unquoted lookup @057173B, `GCFIL`/`CROBJ`
@064670B/@063726B, abbreviation matcher `COMPS`/`GOBJI` @056326B).

## THE QUESTION (single, precise)

When a user program issues **MON 50B OPEN** (or its internal callees `GFILI` /
`GCFIL`) with an **unqualified file name** - a name that carries **no explicit
`(user)` prefix** - does the SINTRAN III file-name resolver:

- **(A)** search **only the caller's own / default user directory**, and return
  `056B` "No such file name" if the file is not there; or
- **(B)** search the caller's own directory **first**, and on a miss **fall back
  to the `(SYSTEM)` user directory** (i.e. a two-stage search: `(ME)` then
  `(SYSTEM)`); or
- **(C)** something else (e.g. an explicit ordered "directory search list", a
  `friend`/`public`-access gate, or a default-directory index that is set per
  process and is not simply the login user)?

If **(B)** or **(C)**: **what exactly triggers the SYSTEM/public stage** - is it
unconditional for any unqualified name, or only when the file's owner has granted
public read access, or only for particular file types / subsystem files? And is
the fallback performed **inside** the OPEN/`GFILI` path, or does the **caller**
(e.g. the ND linker) retry with an explicit `(SYSTEM)name` after the first miss?

## WHY THIS MATTERS (concrete, from a live trace)

The ND-500 linker (`linker-b01.dom`), driven under nd500x, now runs correctly
from `build/link_sandbox`: it renders its banner, processes `LINKER:INIT`, reaches
the command loop, and `OPEN-DOMAIN "A-TEST"` creates the domain. The next real
step is `LOAD <object>`, which pulls in **library** object files.

Our sandbox splits files by SINTRAN user into host directories:

- `build/link_sandbox/GUEST/`  - the user's own files (e.g. `B.NRF`, the domain
  `A-TEST.DOM`, `DDBTABLES-G06.VTM`, `LINKER.INIT/HELP`, error-message files).
- `build/link_sandbox/SYSTEM/` - the **libraries**: `CAT-LIB.NRF`, `NC-LIB.NRF`
  (C/CAT runtime libraries the linked program needs).

nd500x's current path translator
(`/home/ronny/repos/nd500x/src/libmon/mon_path.c`) maps an unqualified name to
**exactly one** directory: `<root>/<CURRENT_USER>/<name>.<ext>`, where
`CURRENT_USER` defaults to `GUEST`
(`/home/ronny/repos/nd500x/src/libmon/mon_config.c:15`). There is **no SYSTEM
fallback**. So an unqualified open of `CAT-LIB:NRF` becomes
`./GUEST/CAT-LIB.NRF` and will **fail -46**, even though the library exists at
`./SYSTEM/CAT-LIB.NRF`.

We need the carve-verified rule so we implement the **real** behaviour, not a
guess:

- If SINTRAN answer is **(A)**: then on a real system the libraries physically
  live in the caller's own directory (or the linker always qualifies them as
  `(SYSTEM)...`), and nd500x should NOT invent a fallback - instead the library
  files belong under `GUEST/` (or the linker's `LOAD`/`INCLUDE` command emits an
  explicit `(SYSTEM)` prefix we should honour).
- If **(B)/(C)**: nd500x's translator (or the OPEN handler) must implement the
  same current-user -> SYSTEM/public search, and we need the exact trigger so we
  don't over-open (security: a private file in `(SYSTEM)` must not leak to a
  different user's unqualified open).

## SPECIFIC EVIDENCE REQUESTS FROM THE CARVE

1. In `GFILI` (@057173B) and/or the `GCFIL`/`GetFileName` path: after the first
   directory lookup misses, is there a **second lookup against a different
   directory/user index** (SYSTEM), or does it return the "not found" error
   immediately? Cite the branch (octal PC) and the directory-index source.

2. How is the "current directory / default user" chosen for the lookup - a fixed
   login user, or a per-process **default-directory index** register/field? (This
   bears on whether `(SYSTEM)` is just "user index 0/1" reached by the same code
   with a different index.)

3. Is there a documented **friend / public-access** check (owner grants) that
   gates whether an unqualified open can see another user's (or SYSTEM's) file?
   If so, where, and what does the ND-500 linker's `LOAD` rely on?

4. Does the **ND linker itself** qualify library names with `(SYSTEM)` before the
   OPEN (i.e. the fallback is the *linker's*, not SINTRAN's)? If the carve of the
   linker's `LOAD`/library-search shows an explicit `(SYSTEM)` or a search-list,
   that answers it without changing SINTRAN OPEN semantics.

## WHAT WE WILL DO WITH EACH ANSWER

- **(A) + linker qualifies:** honour an explicit `(SYSTEM)` prefix in
  `mon_path.c` (map `(SYSTEM)x` -> `./SYSTEM/x`); no implicit fallback.
- **(B):** add a two-stage resolve in the OPEN lookup: try `<current_user>` then
  `SYSTEM`, gated exactly as the carve specifies.
- **(C):** implement the specific mechanism (search list / default-dir index /
  friend gate) as carved.
