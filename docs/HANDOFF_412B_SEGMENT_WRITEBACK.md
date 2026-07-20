# HANDOFF: connected file segments are written back to disk (412B/413B/43B)

Date: 2026-07-20
Files changed:
- `/home/ronny/repos/nd500x/src/cpu/nd500_segment_alloc.c`
- `/home/ronny/repos/nd500x/src/cpu/nd500_indirect.c`
- `/home/ronny/repos/nd500x/src/libmon/mon_types.h`
- `/home/ronny/repos/nd500x/src/libmon/handlers/mon_413B_FileNotAsSegment.c`
- `/home/ronny/repos/nd500x/src/libmon/handlers/mon_43B_CloseFile.c`

Cross-emulator target: RetroCore `Emulated.HW/ND/CPU/ND500` MON 412B/413B/43B.

## Symptom

The ND Linker produced a `:DOM` that could not run. nd500x loaded it and stopped
immediately with `Invalid instruction 0x00 at PC=0x00000000`, because the
domain's start-address field at header offset `0xD8` was zero (a good vendor
`:DOM` has e.g. `b0 01 3b 41` there).

The file was **6,312,168 bytes containing exactly 8 non-zero bytes.**

## Root cause

This was already recorded as a known gap in
`/mnt/e/Dev/Ronny/NDInsight/Developer/MON/calls/412B_FileAsSegment.yaml`, under
`unverified`:

> There is no WRITE-BACK from the segment to the file at any point in 412B's
> lifetime, and 413B does not do it either (see 413B). A file mapped writable and
> modified through the segment will not have those changes reach disk.

SINTRAN's "connect a file as a segment" is a **mapping, not a copy**: the program
reads and writes the FILE through ordinary memory accesses to the segment. The ND
Linker builds an entire domain that way - it never issues a single `120B WFILE`
for the domain body. So everything it built lived only in the segment's pages and
was discarded at CLOSE.

The linker itself was fine throughout. Proof: after `SET-START-ADDRESS MAIN` it
reported

```
Redefinition of Main Start Address accepted. Previous = 1000000004B ignored.
```

i.e. it *had* a start address (`1000000004B` octal = `0x08000004`) all along - it
simply never reached the file.

## Fix

`nd500_segment_writeback()` walks the segment's two-level (PS_ADI) page tables and
writes every **mapped** page to its file offset. Untouched pages of a demand-grown
segment stay holes, so the file keeps the sparse shape the program built.

Called from two places, both before the mapping is dropped:

- `413B FSCDNT` - explicit disconnect.
- `43B CLOSE` - closing a file that is still connected. SINTRAN: *"The file is
  disconnected when it is closed."*

`nd500_segment_release()` then frees the registry slot so the logical segment
number can be reused.

### Return contract - read this before copying

`nd500_segment_writeback()` returns **1 = flushed, 0 = nothing to flush
(untracked, or mapped read-only), negative = write error**. Callers must
distinguish 1 from 0 before logging. An earlier revision returned 0 for both
"skipped" and "success", and the handlers cheerfully logged

```
MON 43B [CLOSE/CloseFile]: segment 4 written back to './GUEST/B.NRF'
```

for **read-only** input libraries that were never written at all. The guard was
working (md5 of `CAT-LIB.NRF` and `NC-LIB.NRF` stayed byte-identical to the
originals in `/mnt/d/ND/500/c-libs/`), but the log was a false report. Only
writable mappings are ever written.

## Verification

Sandbox `/home/ronny/repos/nd500x/build/link_sandbox`, full vendor C recipe from
`/mnt/d/ND/500/nd-linker/linker-auto-c.job`:

```
rm -f GUEST/BPROG.DOM
../bin/diag_linkdrive /mnt/d/ND/500/nd-linker/linker-b01.dom \
 'OPEN-DOMAIN "BPROG";;LOAD B;;LOAD NC-LIB;;LOAD CAT-LIB;;\
  DEFINE-ENTRY stack-space,400000,d;;DEFINE-ENTRY heap-space,400000,d;;\
  REFER-ENTRY stack-space,rts_stack_size,d,d;;\
  REFER-ENTRY heap-space,rts_heap_size,d,d;;CLOSE;;EXIT' 500000000
```

| | before | after |
|---|---|---|
| non-zero bytes in the `:DOM` | 8 | 13031 |
| header `0xD8` (start address) | `00 00 00 00` | `08 00 00 04` |
| loading it in nd500x | `Invalid instruction 0x00 at PC=0` | loads both segments and executes |

After the fix nd500x reports:

```
DOM Load: Seg[1] PROG: file=0x00402000..0x00404A06 size=10759
DOM Load: Seg[1] DATA: file=0x00602000..0x006046A7 size=9896
```

and begins executing at the entry point `0x08000004`.

Regression: `ctest` 16/18, the two long-standing failures only; `diag_fscnt`
still reports `PASS: mapped segment bytes match the file`; input libraries
verified byte-identical by md5.

## Still open

The linked program executes its entry stub and then calls address 0:

```
PC=0x08000004 ... at 0x0800000A: CALL -> 0x00000000
[CALL] Failed to fetch entry opcode from 0x00000000 (program space)
```

so one call target is not patched. UNVERIFIED whether that is a remaining
linker-side relocation issue, a gap in what our write-back captures, or an
nd500x DOM-loader question (note `load_addr=0x00000000` in the load lines above).
Investigate before assuming any of the three.

## What the C# side must mirror

- Implement segment write-back on 413B and on closing a still-connected file.
  Without it, any program that builds a file through a mapping (the ND Linker
  builds every `:DOM` this way) silently produces an empty file.
- Write only **mapped** pages, and only for **writable** mappings.
- Keep "flushed" and "skipped" distinguishable in the return value so logs cannot
  claim writes that never happened.
