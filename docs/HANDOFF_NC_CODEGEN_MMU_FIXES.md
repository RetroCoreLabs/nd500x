# Handoff to RetroCore (C#): NC codegen MMU/loader/MON fixes

Date: 2026-07-21. Source: nd500x session 557c0950. All changes verified against the
real ND-500 toolchain (NC A06 + CAT-500 B06 + ND Linker B01) running end to end.

These fixes unblocked the Norsk Data C compiler's code generation under nd500x. The
C# emulator (`CpuND500` / RetroCore) needs the same three core-emulator changes;
the terminal-I/O and shell changes are nd500x-frontend-only and are NOT needed in C#.

Full absolute paths are nd500x-side; port the LOGIC into the equivalent C# files.

---

## 1. Loader: reserve stack/heap growth pages above initialized DATA

File: `/home/ronny/repos/nd500x/src/ndlib/ndlib_dom_loader.c` (commit bbd6a70)

**Problem.** A DOM segment descriptor (32-entry table at file offset 0x254, stride 56,
per-part 28 bytes; fields LB@0, SZ@4, ATT@8, FLA@12, FUA@16) carries only the
INITIALIZED (file) DATA size in SEG_OFF_SZ. FLA/FUA/AFA/MINP/MAXP read 0 in the vendor
DOMs, so the segment's uninitialized stack/heap/bss growth region is not described. The
loader mapped exactly `data_pages = ceil(data_size/2048)` pages. A program whose frame
pointer B or heap grows past the last mapped data page took a page fault.

**Byte evidence.** NC (nc-a06.dom): DATA file size 124145 = 61 pages -> mapped VA
0x08000000..0x0801E800. NC codegen read data at VA 0x0802324E (its stack, register B,
had grown there) -> beyond the 61-page data capability -> page fault (trap bit 38).

**Fix.** Reserve `DATA_GROWTH_RESERVE_PAGES = 512` (1 MB) extra zeroed physical pages
above each segment's initialized data, extend the page table to `data_pages + reserve`,
and size the data capability accordingly. Machine memory is zeroed at init, so the
reserve reads as 0 (correct for bss / fresh stack). C# equivalent: when building a
domain data segment's page table, allocate reserve pages beyond the file-data pages.

---

## 2. MMU: demand-map data work-segments on first access

File: `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.c` (commit 87c6617)

**Problem.** NC codegen references data work-segments (observed segments 2..6) that it
does not all explicitly allocate via 422B GSWSP. It makes 3 GSWSP calls (got segs 2,3,4)
but accesses segs 5,6 too - relying on the OS to map a scratch segment on first use (the
ND-500 paged-segment model; segments are demand-paged). Previously an access to a segment
with capability==0 raised "No data capability" -> protection violation -> fatal.

**Byte evidence.** After fix #1, NC read data at VA 0x28000000 (segment 5) and 0x30000000
(segment 6) via a `504B DVOUTS` buffer pointer (`IND(b.28)`) -> capability 0 -> PV. A
base-2-vs-5 GSWSP probe only shifted WHICH segs were unmapped (NC touches a range 2..6,
allocates only 3), confirming it is not a numbering bug.

**Fix.** In the data-translate path, when `capability == 0` for a DATA access (not an
instruction fetch) to a segment in the work range (2..24) and the VA != 0, allocate a
backed PS_ADI (demand-grown) segment for that segment number, re-read the capability, and
continue translating. Bounded to the work range, logged, gated by
`ND500X_NO_DEMAND_SEGMENTS=1` for diagnosis. Null (VA 0) reads still take the Address-Zero
path; out-of-range/instruction faults still trap. C# equivalent: same hook in
TranslateVirtualAddress at the capability==0 branch, calling the C# scratch-segment
allocator for the faulting segment, then retry.

---

## 3. MON 73B SMAX: 0xFFFFFFFF sentinel is "unlimited", not a 0-length truncation

File: `/home/ronny/repos/nd500x/src/libmon/handlers/mon_73B_SetMaxBytes.c` (commit 3642470)

**Problem.** `new_size = max_byte_ptr + 1` overflows to 0 when the caller passes the
SINTRAN "do not limit" sentinel 0xFFFFFFFF, and CLOSE then truncates the file to zero
bytes, silently destroying a just-written object.

**Fix.** If `max_byte_ptr == 0xFFFFFFFF`, return success WITHOUT recording a max length
(no CLOSE-time truncation). C# equivalent: same guard in the SetMaxBytes handler.

---

## 4. Two smaller MON return-value corrections (carved from ND-860228)

- `313B InBufferState (IBRSIZ)` - `/home/ronny/repos/nd500x/src/libmon/handlers/mon_313B_InBufferState.c`:
  NoUntilBreak (OUT param 3) = bytes up to and including the first break char (CR), 0 if
  none - NOT the same as NoInBuffer. (Manual ND-860228 lines 14054-14056.)
- `336B TerminalFunction (IOMTY)` - `/home/ronny/repos/nd500x/src/libmon/handlers/mon_336B_Terminal.c`:
  set W1 = Status1 = 0 on success (manual line 22542 "W1 =: Status1"); mon_set_success
  only clears K, leaving a stale W1 the caller reads as Status1.

---

## Verification (nd500x)

Full C -> NRF -> DOM -> run pipeline, three programs, all correct:

| Source | Exercises | Result |
|---|---|---|
| `int x; main(){ x = 42; }` | assignment | x=42 |
| `fact(5)` (recursion) | if, multiply, call frames, deep stack | x=120 |
| `for(i=1;i<=10;i++) s+=i` | loop, locals, iteration | x=55 |

Pipeline: NC A06 `check B,B,B` -> B:CAT (magic e0/da 02); CAT-500 B06 `generate-code
B:CAT B:NRF` -> B:NRF ("code generation : ok"); ND Linker B01 OPEN-DOMAIN/LOAD/CLOSE ->
:DOM; run -> correct value in data. No regression: the trivial x=42 case and the linker
itself still work.

Note: NC's own `generate-code` command does NOT emit the object NRF - it produces a text
listing. The NRF is produced by the SEPARATE CAT-500 back-end. An empty B.NRF after NC
alone is expected.
