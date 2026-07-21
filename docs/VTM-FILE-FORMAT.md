# SINTRAN III VTM terminal-table file format (reverse engineered)

Full path: `/home/ronny/repos/nd500x/docs/VTM-FILE-FORMAT.md`. Written 2026-07-21.

Byte-level RE of the VTM (VDU terminal-module) file, so nd500x can DUMP each
terminal type's capabilities and escape sequences. Reference binary:
`/home/ronny/repos/nd500x/build/link_sandbox/GUEST/DDBTABLES-G06.VTM` (72185 bytes).

No ND manual on disk specifies the VTM FILE layout - `ND-60.151.3` explicitly
defers VTM-COMPOUND to product sheet **ND 210455** (not scanned). So everything
below is reverse-engineered from bytes; each claim is marked PROVEN (byte-
anchored) or INFERRED.

## Terminal-type NUMBER bit encoding (PROVEN - ND-60.128.5 p.555)

The 16-bit type number (what SET-TERMINAL-TYPE takes) is bit-encoded:

| Bit | Meaning |
|---|---|
| 14 | terminal is a VDU (not hard copy) |
| 13 | handles ASCII backspace (BS) |
| 12 | ASCII form-feed (FF) clears screen / new page |
| 11 | VDU has cursor positioning |
| 10 | uses ASCII ESC within input sequences |
| 7-0 | terminal MODEL number |

## VTM file structure (PROVEN unless noted)

- `@0x00` BE32 = 1; `@0x04` BE32 = 19; `@0x08..` ASCII default "unknown terminal"
  prompt.
- **Directory** at `@0x78`, stride `0x40`, 19 entries. Each entry's first 12
  bytes = three BE32 words `(type, off1, off2)`; the remaining 52 bytes are zero
  here (INFERRED reserved/capability flags). `off1` increases monotonically; a
  sentinel entry `type=999` has `off1 = end-of-data`.
- Per terminal, two sub-tables:
  - **Output escape table** starts at `off1 + 0x9dc`.
  - **Input/function-key table** starts at `off2 + 0xc68`.
  The output table runs `[off1+0x9dc .. off2+0xc68)`; the input table runs from
  `off2+0xc68` to the next record. The two base constants `0x9dc`/`0xc68` are
  empirically exact for VT100 and consistent on types 2/3/53 (INFERRED origin).
- **Output escape encoding (PROVEN):** a flat array of length-prefixed slots
  `[len:1][len bytes]`, one slot per function in a fixed order; `len=0` = function
  not supported. Slot COUNT varies per terminal (until the input-table pointer).
- **Input key encoding (INFERRED):** 2-byte `[tag][char]` pairs from `off2+0xc68`;
  `tag` high-bit set, low 7 bits group key classes. Exact tag semantics unknown.
- **Terminal NAMES:** ASCII "`<num>: <name>`" list at `@0x11760..EOF` - the
  authoritative label source (already parsed by the shell's SET-TERMINAL-TYPE).

## Dumper recipe (PROVEN on VT100)

```
1. Read 19 dir entries @0x78 stride 0x40: (type, off1, off2) BE32; stop at type 999.
2. For type N: start = off1+0x9dc, in_tbl = off2+0xc68, end = next.off1+0x9dc.
3. Output escapes: p=start; while p<in_tbl: len=data[p]; seq=data[p+1..p+1+len];
   emit; p+=1+len.   (len 0 => unsupported)
4. Input keys: [tag][char] pairs from in_tbl..end.
5. Names: the "<num>: <name>" list at 0x11760.
```

VT100 (type 6, record @0xee5, input @0xf38) decoded output slots include
`ESC[2J` (`1b 5b 32 4a` @0xf23) = clear screen, `ESC[J`, `ESC[K`, `ESC H`, etc.

## The one real gap for a capability dump

The mapping of SLOT ORDINAL -> FUNCTION NAME ("slot 7 = clear screen", "slot 2 =
cursor up", ...) is defined only in product sheet **ND 210455**, which is not on
disk. So a dumper can faithfully show, per terminal type: the capability bits
(from the type number) and the raw escape sequence of every slot in order - but
it CANNOT label each slot's function without ND 210455 (or deriving the order by
comparing several known terminals). Do not invent slot names.

## Presentation (for the requested dump command)

Escape sequences must be printed readably, never as raw control bytes (which
would move the real cursor). Render:
- ESC (0x1B) as `<ESC>` (or `\e`), other control bytes as `^X` / `\xNN`,
  printable bytes as-is. Example: `1b 5b 32 4a` -> `<ESC>[2J`.
- Also show the raw hex alongside, e.g. `slot 12: <ESC>[2J   (1B 5B 32 4A)`.
