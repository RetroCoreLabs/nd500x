# HANDOFF TO THE C# EMULATOR (RetroCore) - STRING element writes must translate

**Status:** nd500x side FIXED, BUILT, VALIDATED, COMMITTED (`e9111dd`, branch `main`).
**Action required on the C# side:** verify and, if the same defect exists, fix.
**Written:** 2026-07-17.

**This document:** `/home/ronny/repos/nd500x/docs/HANDOFF_CSHARP_STRING_WRITE_MMU.md`

---

## 0. One-paragraph summary

`nd500_string_write_element()` wrote string elements to the memory bus using an
**untranslated virtual address**. The ND-500 string descriptor's `base_address`
is a VIRTUAL address; the bus write primitive expects a PHYSICAL one. Virtual
addresses (e.g. `0xB0001C71`) fall far outside physical memory, so every store
was **silently dropped** - no trap, no error. The read path already translated,
so reads worked and writes vanished: a pure read/write asymmetry. Net effect:
**the entire ND-500 STRING instruction class (`smove` and friends) was a no-op.**

---

## 1. The defect in nd500x (before)

File: `/home/ronny/repos/nd500x/src/cpu/instruction_helpers.c`
Function: `nd500_string_write_element()`

```c
uint32_t addr = desc->base_address + (index * element_size);   /* VIRTUAL */

switch (dtype) {
    case ND500_DTYPE_BYTE:
        nd500_bus_write8(cpu->machine, addr, ...);    /* BUS = expects PHYSICAL */
        break;
    case ND500_DTYPE_HALFWORD:
        nd500_bus_write16(cpu->machine, addr, ...);
        break;
    case ND500_DTYPE_WORD:
    case ND500_DTYPE_FLOAT:
        nd500_bus_write32(cpu->machine, addr, ...);
        break;
    case ND500_DTYPE_DOUBLEWORD:
        nd500_bus_write32(cpu->machine, addr,     (uint32_t)((value >> 32) & 0xFFFFFFFF));
        nd500_bus_write32(cpu->machine, addr + 4, (uint32_t)(value & 0xFFFFFFFF));
        break;
    default:
        nd500_bus_write8(cpu->machine, addr, ...);
        break;
}
```

### Why it was invisible

The **read** counterpart was already correct:

`nd500_string_read_element()` -> `nd500_read_value_at_address()` ->
`nd500_read_memory_8/16/32/64()`, and `nd500_read_memory_*` performs
`nd500_mmu_translate()` when `machine->mmu_enabled`.

`nd500_load_string_descriptor()` also uses `nd500_read_memory_32()`, so
**descriptors loaded correctly** - which made the bug look like anything but a
write bug. Everything about the instruction appeared healthy right up to the
store.

---

## 2. The fix in nd500x (after)

```c
uint32_t addr = desc->base_address + (index * element_size);

/* base_address comes from the string descriptor and is a VIRTUAL address, so
 * these must go through the translating nd500_write_memory_* helpers - the
 * matching read path (nd500_read_value_at_address) already does. Writing
 * straight to the bus sent the untranslated address past physical memory and
 * the store was silently dropped, so every string move was a no-op. */
switch (dtype) {
    case ND500_DTYPE_BYTE:
        nd500_write_memory_8(cpu, addr, (uint8_t)(value & 0xFF));
        break;
    case ND500_DTYPE_HALFWORD:
        nd500_write_memory_16(cpu, addr, (uint16_t)(value & 0xFFFF));
        break;
    case ND500_DTYPE_WORD:
    case ND500_DTYPE_FLOAT:
        nd500_write_memory_32(cpu, addr, (uint32_t)(value & 0xFFFFFFFF));
        break;
    case ND500_DTYPE_DOUBLEWORD:
        nd500_write_memory_64(cpu, addr, value);
        break;
    default:
        nd500_write_memory_8(cpu, addr, (uint8_t)(value & 0xFF));
        break;
}
```

`nd500_write_memory_64()` writes the high 32 bits at `vaddr` and the low 32 at
`vaddr + 4` (BIG-ENDIAN, ND-500 spec) - **byte-order-identical** to the two-write
form it replaces, verified by reading its body at
`/home/ronny/repos/nd500x/src/cpu/instruction_helpers.c:231`. The only change is
that the address is now translated.

---

## 3. What the C# side must check

C# reference tree:
`/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/STRING/`
(`Smove.cs` and the other STRING-class instructions), plus wherever
`StringDescriptor.WriteElementValue` / `ReadElementValue` live.

The nd500x code was ported from the C# side, and the C# names appear in the
nd500x comments (`based on WriteElementValue`, `based on ReadElementValue`,
`based on StringDescriptor.LoadFromMemory`), so **the same asymmetry is likely
present**. Confirm from the actual C# source - do not assume either way.

Checklist:

1. In the STRING element **write** path, is the descriptor's `base_address`
   passed to a **bus/physical** write, or to an **MMU-translating** write?
2. Does the STRING element **read** path translate? If read translates and write
   does not, that is exactly this bug.
3. If the C# emulator does not model an MMU at this layer at all (i.e. bus
   address == virtual address), then this defect **cannot occur there** and there
   is nothing to fix - record that and move on.
4. Check the DOUBLEWORD case preserves BIG-ENDIAN high-then-low ordering.

---

## 4. How to reproduce and verify (exact, reusable)

Binary: `/mnt/d/ND/500/nd-linker/linker-b01.dom`
Disassembly: `/mnt/d/ND/500/nd-linker/linker-b01.dom.asm`

The ND linker builds its startup file names with `smove`. The decisive site:

```
B004B4B9: ents $0x158                       ; parent routine, frame B=0xB0001C4C
B004B4D2: w2 := $0x1                        ; index 1
B004B4D4: by3 laddr b.0x24+                 ; descriptor over local b.0x24
B004B4DC: w4 := $0x14                       ; length 20
B004B4E4: bi1 clr                           ; I1 = 0
B004B4E5: bi2 clr                           ; I2 = 0
B004B4E6: by smove $0xB0054144,b.0x12C      ; <-- THE INSTRUCTION (opcode 0xFD67)
B004B4F2: by move $0x47,b.0x2F              ; 'G' -> config letter, char 11
```

State at `B004B4E6` (instr 5046), all read from live guest memory:

| item | value |
|------|-------|
| source descriptor @`0xB0054144` | `{count=0x14 (20), base=0xB0054130}` |
| string @`0xB0054130` | `DDBTABLES-      :VTM` |
| dest descriptor @`0xB0001D78` (`b.0x12C`) | `{count=0x14 (20), base=0xB0001C71}` |
| I1 / I2 | 0 / 0 |

**Expected:** 20 bytes copied to `0xB0001C71`.

**BEFORE the fix** - buffer byte-identical before and after the instruction, no
trap, no error printed:

```
@B004B4E6: B0 00 1C 1C B0 00 1C 0C B0 00 1C 10 B0 00 1C 14 ...
@B004B4F0: B0 00 1C 1C B0 00 1C 0C B0 00 1C 10 B0 00 1C 14 ...   IDENTICAL
```

**AFTER the fix:**

```
@B004B4F0: B0 44 44 42 54 41 42 4C 45 53 2D 20 20 20 20 20 20 3A 56 54 4D
              D  D  B  T  A  B  L  E  S  -                    :  V  T  M
```

A minimal C# unit test can assert exactly this: build the two descriptors,
zero I1/I2, execute `BY SMOVE`, assert the 20 destination bytes - with the MMU
enabled so virtual != physical. **That is the whole test.**

---

## 5. Observable impact on the ND linker (nd500x, measured)

Command fed: `HELP`.

| metric | before fix | after fix |
|--------|-----------|-----------|
| spin PC `0xB0047162` | 63,481 hits | 16 |
| `0xB004D4F4` | 4,253 hits | 17 |
| outcome | spun past 4,000,000 instr, never exited | **MON 0B LEAVE @`0xB0016205` at ~90,000 instr** |
| MON 50B OPEN FileName | `''` (empty) | `'DDBTABLES-G':VTM`, `'Linker':INIT`, `'Linker':HELP` |

### The full failure cascade this one bug produced

Worth reading, because every symptom below is a **red herring** that cost days:

```
smove no-op
  -> filename buffer keeps uninitialised stack garbage
  -> MON 50B OPEN issued with an empty name        (fails, -46)
  -> file 46 (56 octal) never opens
  -> MON 74B SETBT(46) -> "invalid file number"
  -> MON 144B MAGTP(func=0, dev=46, buffer @0xB00530BC)
       -> our provisional stub returns SUCCESS without writing its IO buffer
  -> linker checks word @0xB00530BC == 1, finds 0x000F0000
  -> raises error 0x106A @0xB004AFC7  (the ONLY setk in the entire run;
                                       all 24 setk sites were counted, 23 are zero)
  -> degenerates into the endless loop at 0xB0047162
```

None of the following were ever the problem, despite looking exactly like it -
**do not re-investigate any of them on the C# side either**:
the config prompts, batch mode, terminal type, the device-0 vs terminal input
routing, MON 313B IBRISZ, MON 503B DVINST, MON 71B DESCF, the `0xB0002000`
struct, and `0xB004A69C` (which is not a read primitive at all - it builds a
terminal attribute word and contains no MON call).

---

## 6. Regression evidence (nd500x)

- `ctest`: **16/18**. The 2 failures (`mon_calls`, `instruction_validation`) are
  **pre-existing**, proven earlier via `git stash` + rebuild - unchanged by this fix.
- `./build/bin/test_instruction_validation --continue`: still **exactly 12 `BY`
  failures**, identical to before the fix. No regression.

### The reason this survived (important for C# too)

```
./build/bin/test_instruction_validation --filter smove
Results: 0 passed, 0 failed, 39803 skipped (0.0% pass rate)
```

**The ND-500 STRING instruction class has ZERO validation coverage.** No test
case exercises it, which is how an entire instruction class could be a silent
no-op indefinitely. If the C# generators drive
`/home/ronny/repos/nd500x/test/nd500_tests.json`, adding STRING-class coverage
there fixes the blind spot for **both** emulators at once. Generators live at
`/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.Tests.ND500/Validation/Generators/`.

Any other instruction class with no coverage deserves the same suspicion.

---

## 7. Related / still open (NOT part of this fix)

1. **Stack overflow - DIAGNOSED, and it is NOT an emulator bug. Do NOT "fix" it.**

   ```
   [TRAP] ENTS at PC=0xB004D3D7: Stack overflow (newB=0xB00593E0, demand=0x4C, TOS=0xB005940C)
   *** ND LINKER abortion ***
   ```

   This is **two levels of symptom deep**. Measured chain:

   ```
   DDBTABLES-G:VTM missing (we do not have the file)
     -> the linker's tables are never populated; region 0xB003F040.. is ALL SPACES (0x20)
     -> the linker walks that blank region as a linked list of records
     -> node[0x10] + node[0x14] = 0x20202020 + 0x20202020 = 0x40404040
     -> "by scopa b.0x30,b.0x38" @0xB003A888 builds descriptor {count=1, addr=0x40404040}
     -> PROTECT VIOLATION (TRAP_PV, bit 36) at instr 30414
     -> the LINKER'S OWN trap handler (0xB0055xxx) installs a small stack:
        TOS=0xB005940C, B=0xB005910C, at instr 30416 (PC 0xB005571B)
     -> ENTS @0xB004D3D7 needs 0x4C, has 0x2C -> TRAP_STO at instr 88121
     -> *** ND LINKER abortion *** -> clean MON 0B LEAVE @0xB0016205
   ```

   **Our TOS handling is correct.** A live watch of every TOS change shows TOS is
   set correctly to 0xB0011ABC at instr 1 by INIT @0xB0013B41, and is moved to
   0xB005940C by the linker's own trap handler two instructions after the PV.
   The C# side should expect the same behaviour and must not "correct" the stack.

   Note both magic values are "nothing was loaded here" signatures, not data:
   `0x40` ('@') is the SINTRAN floppy fill/erase byte, and `0x20` is the
   blank-table filler.

   Real root cause: the missing table file `DDBTABLES-G06:VTM`, which resolves
   from the requested `DDBTABLES-G` via the SINTRAN abbreviated-name matcher
   (confirmed by a listing inside the vendor floppy image:
   `TABLE-G     DDBTABLES-G06:VTM       "DDBTABLES-G06:VTM"`).

2. **FLOAT gap:** `nd500_read_value_at_address()`
   (`/home/ronny/repos/nd500x/src/cpu/instruction_helpers.c:1315`) has **no**
   `ND500_DTYPE_FLOAT` case and returns 0 for it, while the write side handles
   `FLOAT` alongside `WORD`. Suspected latent bug, **NOT fixed**, flagged only.
   Check the C# side for the same asymmetry.

3. **MON 144B MAGTP is a deliberate provisional stub** at
   `/home/ronny/repos/nd500x/src/libmon/handlers/mon_144B_DeviceFunction.c`:
   it returns benign SUCCESS without writing its `IO` buffer because the linker
   halts if 144B returns the -1 unimplemented error. Same OUT-parameter defect
   class as 412B FSCNT. It is **not** the fix for anything above - it only ever
   saw device 46 because the OPEN had already failed. Do not port as final.

4. **Garbled console output:** every character the linker prints has the high bit
   set (parity / 7-bit masking). Separate issue, not investigated.

5. **Missing vendor files** the linker asks for and we do not have:
   `DDBTABLES-G:VTM`, `LINKER:INIT`, `LINKER:HELP`. Only `-C`/`-D`/`-E` DDBTABLES
   variants exist on disk (e.g. `/mnt/d/ND/cv/DDBTABLES-C.VTM`,
   `/mnt/d/ND/c3/2024/x/DDBTABLES-C-E04:VTM`). **Do not fabricate or rename these
   to fake a match.** `UE-ERMSG--C:ERR` does resolve correctly via the SINTRAN
   abbreviated-name matcher to `./GUEST/UE-ERMSG-EN-C06.ERR`.

---

## 8. Commits

| commit | branch | contents |
|--------|--------|----------|
| `e9111dd` | `main` | `fix(cpu): STRING element writes must translate the virtual address` |
| `f75abf6` | `main` | `test(diag): harnesses for locating the linker's startup failure` |

Repo: `git@github.com:HackerCorpLabs/nd500x.git`

### Diagnostic harnesses (reusable, hand-linked - `make` does NOT build them)

All under `/home/ronny/repos/nd500x/test/`; the build recipe is in each header:

- `diag_pccount.c` - exact hit count + first/last instruction number for an
  arbitrary PC list. This is what proved 23 of 24 `setk` sites never fire and
  named the single error. A PC histogram hides decisive low-frequency sites.
- `diag_dumpat.c` - dump guest memory at a breakpoint PC. This is what proved the
  buffer was byte-identical across the `smove`.
- `diag_pchist.c` - PC histogram over a window (names a loop body).
- `diag_pcseq.c` - per-instruction PC/B/ST1 trace.
- `diag_loopargs.c` - frame slots or integer registers at a PC.
- `diag_pollwatch.c` - memory trace over a window.

**Harness warning:** the linker uses a DUAL input source (command line once via
503B DVINST on the terminal; characters via 1B INBT on device 0). Priming only
one channel sends it down an unreachable path and produces a completely
convincing FALSE blocker. `diag_pchist`/`diag_pccount` prime BOTH, matching
`/home/ronny/repos/nd500x/test/diag_linkdrive.c` (separator `;;`, not `;`).
`/home/ronny/repos/nd500x/test/diag_linkterm.c` primes the terminal ONLY - do not
use it to drive the linker.
