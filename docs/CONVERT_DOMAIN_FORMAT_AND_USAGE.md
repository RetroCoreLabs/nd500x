# CONVERT-DOMAIN: Old vs New ND-500 Domain Format, and How to Convert PSEG/DSEG to :DOM

Scope: how the Norsk Data ND-500 CONVERT-DOMAIN (A03) program converts an
"old format" domain (a `:PSEG` / `:DSEG` / `:LINK` triple plus a
`DESCRIPTION-FILE:DESC` entry) into a "new format" runnable `:DOM` file, with
everything verified against on-disk documentation and the actual program's
own help/init text and the example LED editor files.

RULE FOLLOWED IN THIS DOCUMENT: Only statements read directly from a source
file are given as fact, each with the FULL ABSOLUTE PATH of the source.
Anything not directly read is labelled INFERRED or UNVERIFIED, with the exact
check that would confirm it.

Primary sources actually read for this report (full absolute paths):

- `$ND500_TESTDATA/CONVERT-DOMAIN/convert-dom-a03.help`  (the program's own HELP text, NUL-stripped)
- `$ND500_TESTDATA/CONVERT-DOMAIN/convert-dom-a03.init`  (the program's startup banner text, NUL-stripped)
- `docs/ND-860289-2-EN ND Linker User Guide and Reference Manual.md`  (Appendix E "The New Domain Format", Appendix F "The CONVERT-DOMAIN Program", chapter 3.1)
- `$NDINSIGHT/Operations/SINTRAN/ND-30.003.007 EN SINTRAN III System Supervisor.md`  (old domain format / description file, pages ~120-122)
- `$NDINSIGHT/Developer/Workflow/CONVERT-DOMAIN-PSEG-DSEG-TO-DOM.md`  (an existing workflow note in the doc repo)
- The LED example file set in `$ND500_TESTDATA/LED/x/`

---

## A. What CONVERT-DOMAIN does (verified)

From the program's own startup banner,
`$ND500_TESTDATA/CONVERT-DOMAIN/convert-dom-a03.init` (NUL-stripped) reads
verbatim:

```
% This program converts domains and segments from :PSEG/:DSEG/:LINK
% format to :DOM/:SEG format. If you need help, press the help key.
```

From the program's HELP text,
`$ND500_TESTDATA/CONVERT-DOMAIN/convert-dom-a03.help` (NUL-stripped), the top
banner and the HELP topic read verbatim:

```
Convert-Domain is a program for converting domains in the old domain
format to new domains in the new domain format.

The main difference between the formats is that the old domain format
has a description file, while the new format does not.
```

Corroborated by `docs/ND-860289-2-EN ND Linker User
Guide and Reference Manual.md`, Appendix F body, which states verbatim:

> "The introduction of the new domain format has necessitated the development
> of a program for converting files from the old domain format to the new
> domain format. This system will be available during the transition period
> and will later be phased out."

So CONVERT-DOMAIN is a one-way migration tool. It takes an already-built,
already-loaded old-format domain and re-packages its existing segment contents
into a single self-contained `:DOM` file (plus any `:SEG` free-segment files).
It does NOT compile and does NOT re-link from object code (see section F).

---

## B. Old domain format vs New domain format (verified)

### Old format (what the source files are)

From `$ND500_TESTDATA/CONVERT-DOMAIN/convert-dom-a03.help`, topic
OLD-DOMAIN-FORMAT, verbatim:

```
The old domain format has a description file that stores bookkeeping
information about each domain and each segment for the user who owns
the description file.

The contents  of each segment is stored in three files with file types
:PSEG, :DSEG and :LINK.
```

From `$NDINSIGHT/Operations/SINTRAN/ND-30.003.007 EN SINTRAN
III System Supervisor.md`, verbatim (lines ~4536-4568):

> "A domain can consist of up to 32 program segments and 32 data segments.
> Each program or data segment is stored on a separate file of file type
> `:PSEG` or `:DSEG`, respectively."
>
> "Information about how these segments are linked together is stored on the
> file with name `<domain name>:LINK`. There is one file for each domain."
>
> "The information about where the different files associated with a domain
> are stored, is found in the `DESCRIPTION-FILE:DESC`. There is one such file
> for each user owning domains. All domains owned by the user are described in
> this file."
>
> "A domain's files should never be deleted by the SINTRAN command
> `@DELETE-FILE`. If one of these files is deleted, it is no longer possible
> to run the domain, and it has to be reloaded ... If the DESCRIPTION-FILE for
> a user is deleted, none of the user's domains can be run."

So the OLD format of one domain is, per user area:

```
<domain-name>:PSEG     one file per program segment (pure ND-500 code)
<domain-name>:DSEG     one file per data segment
<domain-name>:LINK     one link-information file for the whole domain
DESCRIPTION-FILE:DESC   ONE per user; lists WHERE every domain's files are
                        and the per-domain / per-segment bookkeeping
```

KEY POINT (verified): CONVERT-DOMAIN and the Linkage-Loader address a domain
BY NAME, not by file. The `DESCRIPTION-FILE:DESC` is the index that maps a
domain name to its actual `:PSEG` / `:DSEG` / `:LINK` files. That is why the
old format needs a description file and the new format does not.

Standard/system domains are the exception: from the same System Supervisor
doc, verbatim: "Standard domains have the same type of information as found in
the description file stored on a system-included segment on SEGFILE0 (segment
number 20B)."

### New format (what CONVERT-DOMAIN produces)

From `$ND500_TESTDATA/CONVERT-DOMAIN/convert-dom-a03.help`, topic
NEW-DOMAIN-FORMAT, verbatim:

```
The new domain format has no description file, instead each domain is
stored on a separate file with file type :DOM and the bookkeeping
information about it is stored in a header at the beginning of the
:DOM file. Each segment that is private for this domain is called a
slave segment and is stored in the domain file.

Thus a simple domain may be copied by using @COPY-FILE only.

If a segment is shared with other domains or for any other reason
cannot be considered a slave segment, it is called a free segment.
These are stored in separate :SEG files and the bookkeeping
information about them is stored at the beginning of the file where it
is stored.
```

From `docs/ND-860289-2-EN ND Linker User Guide and
Reference Manual.md`, Appendix E, verbatim:

> "Whereas previously the domain consisted of at least three files (file types
> PSEG, DSEG, and LINK), and details of all domains in one user area were
> stored in a DESCRIPTION FILE (file type DESC), the new domain format means
> that each domain consists of one file (of type DOM) optionally linked to
> free segment files (of type SEG). All information concerning a domain is
> stored in the domain file itself, meaning that a description file is no
> longer necessary."

New format on disk:

```
<domain-name>:DOM      one self-contained file: header + debug info +
                       link info + slave (program/data) segments
<free-seg-name>:SEG     optional, one per shared/forced free segment
```

### The :DOM header / where entry point and segments are described (verified)

Same Linker manual, chapter 3.1 and Appendix E, verbatim highlights:

- "Each domain file contains a domain header ... The header occupies the first
  two pages of the file, but the next two pages are reserved ... After the
  domain header comes space for debug information, and link information."
- The default `:DOM` layout table (Appendix E, "Domain/Segment File Layout")
  gives octal file displacements: DOMAIN HEADER at byte 0, DEBUG INFO at
  0002000 (2 MB), LINK INFO at 01002000, then PROGRAM/DATA slave segments.
- The Domain Header contents list (Appendix E, "Summary of Domain and Segment
  Headers") includes verbatim fields: "Identification", "Privileges",
  "Mother domain", "16 child domains", "Debug info boundaries",
  "Link info boundaries", "Start address (Restart address)",
  "Trap block - THA, MTE, OTE, CTE, TEMM", "32 indirect segment defs",
  "Source code language mask", "Id message",
  "64 segment defs - slave or linked segments", "Name pool".
- The header's per-segment table is indexed by `SEGTABDISP`: verbatim
  "Displacements within domain header (i.e. within :DOM file) to the array
  defining the segments of the domain", tabulated for program and data
  segments 0..31 (e.g. segment 0: program 1124B, data 1160B).
- Segment attribute bit 14 "Linked segment (*)" with the note: "If set,
  segment is linked to. Then LB contains indexes to the file name in the name
  pool, while SZ contains LINKKEY to the segment file." Bit 19 is
  "Start vector on segment".

So in the NEW format the entry point ("Start address / Restart address") and
the trap block live in the `:DOM` header, and each of the up-to-64
program/data segment slots is described by a segment-def record in that same
header. In the OLD format that same bookkeeping is split between the
`:LINK` file and the `DESCRIPTION-FILE:DESC` entry.

What the description file holds (verified summary): per-user index of every
domain the user owns, the on-disk location of each domain's `:PSEG`/`:DSEG`/
`:LINK` files, and per-domain / per-segment bookkeeping. Exact byte layout of
`:DESC` is UNVERIFIED here (no `:DESC` format spec was found in the repos read;
to verify, one would reverse the `DESCRIPTION-FILE:DESC` bytes against a known
domain, or find a `:DESC` layout appendix).

---

## C. Exact CONVERT-DOMAIN command syntax and every prompt (verified)

Read in full from `$ND500_TESTDATA/CONVERT-DOMAIN/convert-dom-a03.help`, topic
CONVERT-DOMAIN, verbatim command shape:

```
CONVERT-DOMAIN <Destination domain>
               <Source domain>
               <Include linked segments (Yes,No)>
               <Display progress information (Yes,No)>
               <Force free segment number(s)>...
```

The help text states verbatim: "The CONVERT-DOMAIN command has 5 parameters"
and "The dots indicate that the last parameter is repeated until you give no
value to it." "<Destination> and <Source> are mandatory, while the other
parameters are optional. <Destination> will accept an empty string (i.e. just
CR)."

Parameter-by-parameter, quoting the help file:

1. Destination domain (mandatory, empty string allowed).
   "the name of the destination domain that is in the new domain format. If
   the :DOM file does not exist, it will be created. If you want to prevent an
   existing :DOM file from being overwritten, you should enclose the name in
   double quotes. If the file already exists, you will get an error message
   and no conversion will take place. Occurrences of '$' in the destination
   name will be substituted with the source domain name. An empty string is
   equivalent to one '$'."
   (Linker manual Appendix F adds: default = same name as source; to take the
   default press space, type two commas `,,`, or type `$`; `new-$` for source
   ACCOUNTS-DOMAIN yields NEW-ACCOUNTS-DOMAIN. Destination may name another
   user with the SINTRAN `(directory-name:user-name)file-name` syntax.)

2. Source domain (mandatory, NO default).
   "the name of the source domain. This domain must be in the old domain
   format. There is no default name."
   (Linker manual Appendix F, verbatim: "You should not specify file type for
   source domain." i.e. give `LED-B03`, never `LED-B03:PSEG`.)

3. Include linked Segments (Yes/No, default NO, optional so it is not
   prompted unless you ask).
   "If you respond with YES, all :SEG files needed to run the destination
   domain will be copied to the same user as the destination domain, and the
   destination domain will be linked to these segments. This is useful if you
   want to copy the domain to a floppy diskette ... If you respond with NO
   (default), the destination domain will get links to :SEG files that may
   reside in other user areas ..."

4. Progress Info? (Yes/No, default YES).
   "YES (default) means that it will give information about what it will do
   next. For example ... you will see the following message:
   >> Converting debug part for segment 3 <<
   NO means that this information will not be displayed."

5. Force free segment number(s)? (repeatable, optional).
   "Convert domain tries to do a 'clever' guess if the segment should be free
   or not. However, you may want to force a segment to be free even when
   Convert domain puts it on a :DOM file ... CONVERT-DOMAIN $ NOTIS-WP,,,0:31
   Each segment will be put on a :SEG file ... Alternatively, you could have
   written 0-31 or 0..31 (all of them work)."
   (Ranges: `3-6`, `3..6`, `3:6` all accepted, per Linker manual Appendix F.)

Other commands the program accepts (help topics list, verbatim):

- `EXIT` -- verbatim: "This command leaves the CONVERT-DOMAIN command
  processor."
- `HELP` -- command and default topic; pattern matching with `-`, `+`, `*`.
- `%` -- shell comment (topic COMMENT).
- `@` -- run a SINTRAN III command from inside the program, e.g.
  `@DELETE-FILE destination:DOM`.
- Topics only (not commands): NEW-DOMAIN-FORMAT, OLD-DOMAIN-FORMAT, SHELL,
  SIBAS, LIMITATIONS.

Two ways to invoke (verified from help topics CONVERT-DOMAIN and SHELL, and
Linker manual Appendix F):

- Batch / one line -- no shell prompt: put the command and ALL parameters on
  the single `@ND ...` line, e.g. `@ND CONV-DOM DEST-DOM SOURCE-DOM`. Help
  verbatim: "if you write parameters on the command line, the SHELL will not
  be used."
- Interactive -- `@ND CONVERT-DOMAIN` with no parameters enters the ND-SHELL
  command processor at prompt `CONV:`; you then type `CONVERT-DOMAIN` and are
  prompted for each parameter in turn. SHIFT+HELP lists commands; a command
  name + HELP describes it.

LIMITATIONS the program itself warns about (help topic LIMITATIONS, verbatim
"Do not attempt to convert"): Sibas version F or older; Notis-DS version D or
older; Notis-ID version B or older; ND-500 Basic version B or older; and two
programs that have only `:PSEG`/`:DSEG` and NO description file and therefore
"cannot be converted": the ND-500/5000 Swapper and the Symbolic Debugger.

---

## D. Exact input files needed, and outputs produced

Inputs CONVERT-DOMAIN consumes for one old domain (verified from the format
descriptions in sections B/C):

- The complete segment contents: `<domain>:PSEG` and `<domain>:DSEG`.
- The link information file `<domain>:LINK`.
- The `DESCRIPTION-FILE:DESC` on the source user -- REQUIRED, because the
  program is given a domain NAME (parameter 2) and must look that name up in
  the description file to find the actual segment files and their bookkeeping.
  This is a direct consequence of the verified statement (System Supervisor
  doc): "The information about where the different files associated with a
  domain are stored, is found in the DESCRIPTION-FILE:DESC." Without a `:DESC`
  entry for the named domain, there is no documented way for the program to
  locate the segments.
  INFERENCE LABEL: the exact failure mode when the `:DESC` entry is missing is
  INFERRED (not read from any error-message list). To verify: run the program
  against a domain name absent from `DESCRIPTION-FILE:DESC` and capture the
  error.
- For any segment the domain is LINKED to, the corresponding free segment file
  on its user area (already `:SEG`, or its old `:PSEG`/`:DSEG`/`:LINK` to be
  converted) -- only relevant when the domain has external links.

Outputs produced (verified from help topics NEW-DOMAIN-FORMAT and
CONVERT-DOMAIN, and Linker manual Appendix F "Segment handling"):

- One `<destination>:DOM` file containing the header + debug info + link info +
  all slave (private) segments.
- Zero or more `<free-seg-name>:SEG` files for segments that are shared,
  linked-to, or force-freed (parameter 5). Verbatim (Linker manual):
  "Segments that belong to the source domain, are converted to slave segment
  in the destination domain. Segments linked to, are converted to free
  segments."
- The original old-format files are NOT modified by the conversion. (Stated in
  the NDInsight workflow note; INFERRED-consistent with the tool being a
  copy/convert. To verify: checksum the `:PSEG`/`:DSEG`/`:LINK` before and
  after a run.)

Conversion is ONE-WAY: no command exists to turn a `:DOM` back into
`:PSEG`/`:DSEG`/`:LINK` (no such command appears in the help topic list).

---

## E. The LED example at $ND500_TESTDATA/LED/x/

Directory listing actually read (sizes in bytes):

```
$ND500_TESTDATA/LED/x/description-file.desc     22528
$ND500_TESTDATA/LED/x/led-b03.pseg             223695
$ND500_TESTDATA/LED/x/led-b03.dseg             394525
$ND500_TESTDATA/LED/x/led-b03.link                  0   (empty)
$ND500_TESTDATA/LED/x/scratch-seg-01.pseg           5   (5 NUL bytes)
$ND500_TESTDATA/LED/x/scratch-seg-01.dseg        1029
$ND500_TESTDATA/LED/x/scratch-seg-01.link           0   (empty)
$ND500_TESTDATA/LED/x/upk-if.defs                4770
```

Printable strings actually read out of
`$ND500_TESTDATA/LED/x/description-file.desc`:

```
SCRATCH-DOMAIN
LED-B03
(211160B03-XX-01D:FLOPPY-USER)
(211160B03-XX-01D:FLOPPY-USER)SCRATCH-SEG-01
(211160B03-XX-01D:FLOPPY-USER)LED-B03
```

What this tells us (facts):

- The description file describes TWO domains for this user: `LED-B03` and
  `SCRATCH-DOMAIN`, and it references segment files `LED-B03` and
  `SCRATCH-SEG-01` located on user `(211160B03-XX-01D:FLOPPY-USER)`.
- The on-disk host filenames use a dot (`led-b03.pseg`); on SINTRAN the same
  file is `LED-B03:PSEG`. The dot/colon difference is only the host
  representation.
- `led-b03.link` is 0 bytes. This is the domain's link-information file
  present but empty. INFERRED meaning: a fully loaded domain with no
  unresolved external references has an empty link area (consistent with the
  Linker-manual statement that link info "contains the entries in the symbol
  table that are defined with values in the slave segments"). The file EXISTS,
  so it is not the "deleted file" failure case. Whether CONVERT-DOMAIN A03
  accepts a zero-byte `:LINK` is UNVERIFIED -- verify by actually running the
  conversion under nd500x and checking for an error.
- `scratch-seg-01.pseg` is 5 NUL bytes and `scratch-seg-01.dseg` is tiny; this
  is essentially an empty scratch segment belonging to `SCRATCH-DOMAIN`, not
  to `LED-B03`.

Sufficiency verdict for converting the LED-B03 editor:

- Present and needed for LED-B03: `led-b03.pseg`, `led-b03.dseg`,
  `led-b03.link`, and the `description-file.desc` entry for `LED-B03`. That is
  the complete documented old-format file set. So, on paper, the set IS
  sufficient to attempt a conversion.
- CAVEAT (important, verified from the desc bytes): the description file names
  the segment files under user `(211160B03-XX-01D:FLOPPY-USER)`. For
  CONVERT-DOMAIN to resolve the domain, the `:PSEG`/`:DSEG`/`:LINK` files must
  be reachable at the location the `:DESC` records, i.e. the files must sit on
  a SINTRAN user whose description file points at them. Simply having the eight
  host files in one host directory is NOT the same as having them registered
  under the right SINTRAN user with a matching `DESCRIPTION-FILE:DESC`. To run
  the conversion you must first stage them into the emulated SINTRAN filesystem
  as user FLOPPY-USER (or edit/rebuild the description file to match wherever
  you actually place them).
- `upk-if.defs` is not part of the domain file set (it is a PLANC/PACK
  interface definitions source, INFERRED from the `.defs` extension and the
  `upk-if` name); it is not consumed by CONVERT-DOMAIN.

Exact interactive command sequence to convert LED-B03 (assuming the files are
staged as SINTRAN user FLOPPY-USER with its `DESCRIPTION-FILE:DESC` intact,
logged in as that user, wanting a fresh `LED-B03:DOM` on the same user):

```
@ND CONVERT-DOMAIN
CONV: CONVERT-DOMAIN
  <Destination domain>:  "LED-B03"
  <Source domain>:       LED-B03
  <Include linked segments (Yes,No)>:      (CR = No)
  <Display progress information (Yes,No)>: (CR = Yes)
  <Force free segment number(s)>:          (CR = none, ends the command)
CONV: EXIT
```

Equivalent single-line (non-interactive) form:

```
@ND CONVERT-DOMAIN "LED-B03" LED-B03
```

Notes on that sequence (all verified from section C):
- The double quotes around `LED-B03` make it a NEW file and protect any
  existing `LED-B03:DOM` from being silently overwritten (you get an error
  instead). Drop the quotes only if you intend to allow creation without that
  guard.
- The source `LED-B03` is given with NO file type.
- LED-B03 has no external library links in this file set (its `:LINK` is
  empty), so parameters 3 and 5 are not needed; UNVERIFIED until a real run
  shows whether any free `:SEG` is emitted.

Post-conversion verification (from the Linker, per the manual): `@LINKER` then
`LIST-STATUS LED-B03` to see the segments / start address / trap block, then
test-run with `@ND LED-B03`.

~~UNVERIFIED end-to-end~~ **NOW VERIFIED (2026-08-10), on a different domain**
(`LINKAGE-LOAD-H02`, the NLL H02 installer floppy's own old-format domain, staged
under `~/ND500USERS/FLOPPY-USER/` -
`E:\Dev\Ronny\NDInsight\SINTRAN\ND500-APPS\CONVERT-DOM-A03\userguide.md` has the full
transcript). Confirms both open risk factors this doc flagged:

1. **An empty `:LINK` file IS accepted** - `LINKAGE-LOAD-H02.LINK` is 0 bytes, same as
   `led-b03.link`, and the conversion completed with no error.
2. **Exact `(directory:user)` resolution is NOT required** - the real
   `description-file.desc` names the segment files under
   `(210319H02:FLOPPY-USER)LINKAGE-LOAD-H02`, but the files were staged flat under a
   single-level SINTRAN user directory named just `FLOPPY-USER` (no `210319H02`
   sub-level), logged in AS that user, and CONVERT-DOMAIN resolved the bare source name
   `LINKAGE-LOAD-H02` correctly anyway. This means being logged in as the domain's owning
   user with that user's own `DESCRIPTION-FILE:DESC` present is what matters - the
   `(directory:user)` string embedded in the `:DESC`'s SNAME field appears to be
   informational/provenance, not a hard resolution requirement for a domain the CURRENT
   user owns.

Output was the four expected `>> Converting ... part for segment 22` progress lines (no
`:SEG` was produced - consistent with no linked segments) followed by `>> Finished`, and
a `LINKAGE-LOAD-H02.DOM` (2,316,049 bytes) was written. This is strong, though not
identical-domain, evidence that the same recipe will work for `LED-B03` too.

---

## F. PSEG/DSEG -> DOM: CONVERT-DOMAIN vs the ND LINKER

Both can end up producing a `:DOM`, but they start from DIFFERENT inputs and
do different work. This is verified, not assumed:

- The ND LINKER builds a `:DOM` from NRF object modules (a fresh link). From
  `docs/ND-860289-2-EN ND Linker User Guide and
  Reference Manual.md`, verbatim: "The main purpose of the ND Linker is to
  collect ND-500(0) program modules existing in NRF format and convert them
  into an executable program." The Linker reads `:NRF`, resolves symbols, and
  writes `:DOM` (+ `:SEG`). It does NOT read `:PSEG`/`:DSEG`.

- CONVERT-DOMAIN does NOT link and does NOT read NRF. It takes an
  already-built OLD-format domain (`:PSEG`/`:DSEG`/`:LINK` + `:DESC`) and
  re-packages the SAME already-loaded segment bytes into a `:DOM`. From
  `$ND500_TESTDATA/CONVERT-DOMAIN/convert-dom-a03.init`, verbatim: "This program
  converts domains and segments from :PSEG/:DSEG/:LINK format to :DOM/:SEG
  format."

So:

- If you have the ORIGINAL `:NRF` object files, the LINKER can produce a `:DOM`
  directly (recommended path; a fresh relink). The old CONVERT-DOMAIN help
  itself says, verbatim (topic OLD-DOMAIN-FORMAT): you may convert "by using
  this convert domain program or by relinking/loading using ND's LINKER that
  is released together with this program."
- If you only have an already-built old domain (`:PSEG`/`:DSEG`/`:LINK` +
  `:DESC`) and no NRF, CONVERT-DOMAIN is the tool -- it is the ONLY documented
  path that turns `:PSEG`/`:DSEG` bytes into a `:DOM` without the object code.

For the LED example: there are no `.nrf` files in `$ND500_TESTDATA/LED/x/`, only
the old-format triple + desc, so CONVERT-DOMAIN (not the Linker) is the
applicable tool for that specific file set.

---

## Bottom line for the nd500x goal

To make `LED-B03:PSEG` + `LED-B03:DSEG` (+ empty `:LINK`) runnable as
`LED-B03:DOM` under nd500x, the documented route is CONVERT-DOMAIN, driven as
`@ND CONVERT-DOMAIN "LED-B03" LED-B03` while logged in as the user whose
`DESCRIPTION-FILE:DESC` registers domain `LED-B03`. The one real risk factors,
both to confirm by an actual run: (1) the empty `led-b03.link` being accepted,
and (2) the description file's `(211160B03-XX-01D:FLOPPY-USER)` references
resolving to wherever the segment files are actually staged in the emulated
filesystem.
