# HANDOFF TO THE C# EMULATOR (RetroCore) - SINTRAN file-table semantics

**Status:** nd500x side IMPLEMENTED, VERIFIED, COMMITTED.
**Action required on the C# side:** port both items below.
**Written:** 2026-07-17 (retro-documented - see "Why this doc is late").

**This document:** `/home/ronny/repos/nd500x/docs/HANDOFF_CSHARP_FILE_TABLE_SINTRAN_SEMANTICS.md`

Both changes live in `/home/ronny/repos/nd500x/src/libmon/mon_file_table.c`.

| Item | nd500x commit | Date |
|------|---------------|------|
| 1. SINTRAN abbreviated file-name resolution on OPEN | `badb3c4` | 2026-07-17 |
| 2. Device-0 command buffer is CR-terminated | `19f7354` | 2026-07-16 |

### Why this doc is late

An audit on 2026-07-17 found these two behaviour changes had **no C# handoff**,
while everything else in the tree did. They are recorded here in full so the C#
side is not left behind. Rule going forward: the handoff doc is written *at the
time of the change*, not batched. See
`/home/ronny/repos/nd500x/docs/HANDOFF_CSHARP_STRING_WRITE_MMU.md` for the
pattern.

---

# ITEM 1 - SINTRAN abbreviated file-name resolution on OPEN (`badb3c4`)

## The problem

The ND linker opens `UE-ERMSG--C:ERR`. No such file exists on disk; the real one
is `UE-ERMSG-EN-C06:ERR`. SINTRAN resolves the abbreviation. A literal
open/`fopen` fails, `MON 50B OPEN` returns -46, and the linker never finds its
error-message file.

This is **not** a convenience feature - the linker depends on it. It also
resolves `DDBTABLES-G` -> `DDBTABLES-G06:VTM`, which is how the linker loads its
DDB tables at all.

## Source of truth

Carved byte-for-byte from **L-VSX-500 segment 006-S3FS**:
- comparator **`COMPS` @041552**
- scanner decision **`GOBJI` @056326** (terminal codes at 056576-056607)

Carve doc:
`/mnt/e/Dev/Ronny/NDInsight/tools/sintran-segment-carver/versions/L-VSX-500/re/segments-ref/006-S3FS/CARVE-ANSWER-FLPAR-MDEAB-FOR-IMPLEMENTER.md`

## The comparator (COMPS)

Compares a **whole supplied name** against a **whole stored name** in ONE pass,
returning 3 states. The `-` handling yields per-subpart behaviour implicitly -
there is no separate subpart splitter.

```
NO_MATCH = 0, PREFIX_MATCH = 1, EXACT_MATCH = 2

sintran_comps(a = supplied, b = stored):
  loop:
    ca = a[i]; cb = b[j]
    if ca == cb:
        if ca == terminator: return EXACT_MATCH      // 041603 / 041612
        i++; j++; continue                           // 041606
    if ca == '*':  i++; j++; continue                // 041600 / 041661  (per-char wildcard)
    if ca == terminator: return PREFIX_MATCH         // 041616-041620  (supplied is a prefix)
    if ca == '-':                                    // 041621-041706
        // positional/empty subpart: skip B to its next '-' boundary,
        // so this slot matches ANY value
        while b[j] != '-' and b[j] != terminator: j++
        if b[j] == terminator: return NO_MATCH
        i++; j++; continue
    return NO_MATCH                                  // 041621->041623->041614
```

Key semantics, each cited to its octal address in the source:
- **Supplied-is-prefix-of-stored matches.** This is what makes `DDBTABLES-G`
  match `DDBTABLES-G06`.
- **`-` (including the empty middle of `--`) skips the stored name to its next
  `-` boundary**, so that slot matches any value. This is what makes
  `UE-ERMSG--C` match `UE-ERMSG-EN-C06` (the empty slot swallows `EN`).
- **`*` is a per-character wildcard.**

**Terminator note:** on the guest side both strings end with `047` (`'`). By the
time a name reaches this code the descriptor reader has already converted that to
a C NUL, so NUL is the terminator in our implementation. **The C# side must apply
the same rule at whatever point its descriptor reader normalises the `'`.**

## The scanner decision (GOBJI)

```
scan the directory:
  exact match  -> WINS OUTRIGHT, stop the scan immediately, unique
  else count prefix matches:
      0 hits  -> error 056 (no such file)   -> we return -46
      >1 hits -> error 057 (ambiguous)      -> we return -47
      1 hit   -> unique, resolve to it
```

**Name and type are matched as SEPARATE strings**, per the `NAME:TYPE` framing
split (`SEPOB`/`SEPFS`). Do not match the joined string.

## Two corrections to earlier assumptions (important - these were WRONG in this tree)

1. **The ambiguous-file code is 057, and no-such is 056. NOT 0111.** The
   `0111`/`0113` codes belong to a **separate access/type classifier** that runs
   *after* a unique resolve. If the C# side has 0111 wired as "ambiguous", it is
   wrong.
2. The "A=0 unique / A=-2" contract that was previously believed is really the
   **SPUSH/SPOP skip-count**, not a result code.

## Where it hooks

**Only after the literal open fails, and only on a READ open.** SINTRAN does not
permit abbreviation when creating a file:

```c
if (!fp && !allows_write) {
    char resolved[256];
    int rc = sintran_resolve_abbrev(host_path, resolved, sizeof(resolved));
    if (rc == 0) {
        fp = fopen(resolved, fmode);
        ...
    } else if (rc == -47) {
        return -47;  /* Error 57B: Ambiguous file name */
    }
}
```

## Known UNVERIFIED edge (carried in the source comment)

When the **stored** name runs out of subparts before the **supplied** name (i.e.
the supplied name has MORE subparts than the stored one), the carve did not trace
the branch. We treat it as `NO_MATCH`. The verified common path is unaffected.

A second consequence worth knowing: because **EXACT short-circuits the scan**,
`A--C` could take `A-XX-C` without ever noticing `A-YY-C` also matches. That is
what the carved code does; it is not a bug in the port.

## Verified

```
MON OPEN: './GUEST/UE-ERMSG--C.ERR' resolved to './GUEST/UE-ERMSG-EN-C06.ERR'   -> opened as file 101
MON OPEN: './GUEST/DDBTABLES-G.VTM' resolved to './GUEST/DDBTABLES-G06.VTM'     -> SUCCESS
```
Unit test 9/9. ctest unchanged at 16/18 (both failures pre-date the change,
proven via `git stash` + rebuild).

---

# ITEM 2 - Device 0 (command buffer) is CR-terminated, not raw EOF (`19f7354`)

## The rule

**Device 0 = the SINTRAN command buffer = the invocation command line, and it is
ALWAYS terminated by CR (`015B` / `0x0D`).**

Byte-proven in the L07 command processor by a carve trace: at the `47B` source
marker it substitutes CR (`050773: SAA 15 -> SBYT`) and resets the byte pointer.
So **`47B` is an internal SOURCE marker that never reaches a device-0 reader** -
the buffer a program reads via `1B INBT` ends in CR, never `47B`, never raw EOF.

**A program launched with no arguments still reads a lone CR.**

## The bug

`mon_read_command_buffer_char()` returned raw -1 (EOF) once the stored bytes ran
out. The ND linker then busy-spun: **20,088 consecutive `1B INBT` retries measured
at PC=0xB004E759.**

## The fix

```c
int mon_read_command_buffer_char(void) {
    int len = (int)strlen(g_command_buffer);

    if (g_command_buffer_pos < len)
        return (unsigned char)g_command_buffer[g_command_buffer_pos++];

    /* Deliver exactly ONE synthetic CR if the line did not already end in one,
     * THEN signal end-of-line. */
    if (g_command_buffer_pos == len && (len == 0 || g_command_buffer[len - 1] != '\r')) {
        g_command_buffer_pos++;   /* consume the synthetic terminating CR */
        return '\r';
    }
    return -1;  /* end of line (CR already delivered) */
}
```

Behaviours the C# side must match:
- line **without** CR -> bytes, then one CR, then end-of-line
- line **already** CR-terminated -> **no double CR**
- **empty / no args** -> a lone CR (NOT wait, NOT EOF)
- read **past** the CR -> end-of-line (our caller treats -1 as "suspend")

## Verified

With this change, no harness workaround and no fed input, the linker's first
device-0 read returns `0x0D`, and it proceeds to its banner and startup dialogue -
it reads the CR once and never reads device 0 again. This is the read-side
confirmation of the carver's write-side CR proof, and answers "what a device-0
read returns" = **args + CR**.

Tests: 5 assertions in `/home/ronny/repos/nd500x/test/test_mon_calls.c`.
NC unaffected (it reads device 1).

## Still UNVERIFIED

What a real device-0 read returns **after** the CR (past end-of-line) is unproven -
the linker never does it. Settling it would need a live nd100x trace against
`SMD0-L.IMG`. We treat it as suspend.

---

# Device model summary (worth having on both sides)

| Device | Meaning |
|--------|---------|
| **0** | SINTRAN command buffer = the invocation command line (args + CR) |
| **1** | the program's own terminal |

The ND linker uses a **DUAL input source**: it reads the command line once via
`503B DVINST` on the terminal, and characters via `1B INBT` on device 0. Feeding
only one of the two channels sends it down an unreachable path and produces a
completely convincing FALSE blocker - this cost days on the nd500x side. Any C#
test harness driving the linker must feed BOTH.
