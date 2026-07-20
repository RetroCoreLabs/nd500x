# HANDOFF: LOOPI read/wrote its index as WORD instead of the instruction's data type

Date: 2026-07-20
File changed: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Loopi.c`
Cross-emulator target: RetroCore `Emulated.HW/ND/CPU/ND500/Instructions` LOOPI

## Symptom

The ND Linker B01 rejected **every** NRF object file, including the genuine
vendor library `/mnt/d/ND/500/c-libs/CAT-LIB-B06.NRF`:

```
*** ERROR - "4" in module  is illegal control byte.                 (0054:16)
```

## Root cause

`nd500_instr_Loopi()` forced the index operand to WORD:

```c
uint64_t index = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
...
nd500_write_operand_value(cpu, &fi->operands[0], index, ND500_DTYPE_WORD);
```

with the comment *"Read/write full register width (WORD), data type only affects
comparison"*. That is wrong for any index operand living in **memory**.

The ND-500 Reference Manual, 13.4 *Loop with increment*, is explicit:

> The ⟨index⟩ and ⟨limit⟩ operands are of the same data type, which may be BY, H,
> W, F or D.

### How it broke the linker

The linker's NRF record scanner reads a control byte, then reads `(control & 7)`
length bytes in a loop (`/mnt/d/ND/500/nd-linker/linker-b01.dom.asm`):

```
B001D7F8: FC 94 07              h1 and  $0x7        ; count = control & 7
B001D7FB: h1 =: $0xB00265BE
B001D813: 1D C1 16              by2 =:  b.0x16      ; unrelated byte, ADJACENT
B001D816: 49 45                 h stz   b.0x14      ; index = 0   (2 bytes only)
B001D818: FC 40 01              h1 -    $0x1        ; limit = count - 1
B001D81B: FC 10 48              h1 =:   b.0x20
...body: reads one byte, stores to $0xB00265C2+ ...
B001D891: FC DF 45 48 93        H LOOPI:B b.0x14, b.0x20, -109
```

Note `FC DF` = octal 176337 = **H LOOPI:B** (halfword index).

`h stz b.0x14` zeroes only 2 bytes. A WORD read of `b.0x14` also swallowed
`b.0x16`, which had just been set to `control & 0xF8`. For the golden reference
file's first control byte `0x0A`, that made:

| step | correct | with the bug |
|------|---------|--------------|
| index read | 0 | 0x00000800 |
| index+1 | 1 | 0x0801 |
| compare vs limit 1 | 1 <= 1 -> loop | (int16_t)0x0801 = 2049 > 1 -> exit |
| iterations | 2 | **1** |

So the scanner consumed one length byte too few and desynchronised from the
record stream. The WORD write-back additionally clobbered the neighbouring
`b.0x16`.

## Byte-level evidence

`/mnt/d/ND/500/FraTor/test-real/test-real.nrf` (the golden ND-produced reference)
begins `0A 00 01 70 44 ...`. Control byte `0x0A` has `0x0A & 7 = 2`, so the next
control byte belongs at offset **3**. Breaking on the linker's control-byte fetch
(`B001D7E7`) and dumping the source address:

Before the fix:
```
[BREAK] PC=B001D7E7 I1=0000000A I2=20000000   <- control byte 0x0A at offset 0
[BREAK] PC=B001D7E7 I1=00000001 I2=20000002   <- next fetched at offset 2  (WRONG)
```
Offset 2 is a data byte, so the dispatcher `control >> 3 & 0x1F`
(`B001DF11: h sha r1,$0x3D`, a CONSTANT_SHORT of -3) selected the illegal-record
slot and raised `(0054:16)`.

The file cursor `0xB0027228` was verified to only ever increment by 1, so this
was a miscount, not a bad seek. Reads of the mapped NRF were also verified
byte-perfect and sequential, so the input data was never in question.

## Verification

- `test_instruction_validation --continue`: **ALL TESTS PASSED**.
- `ctest`: 16/18, the same two pre-existing failures (`float_arithmetic`,
  `mon_calls`); `mon_calls` reports exactly 3 sub-failures before and after.
- ND Linker, from `/home/ronny/repos/nd500x/build/link_sandbox`:
  ```
  : > GUEST/NEWDOM.DOM
  ../bin/diag_linkdrive /mnt/d/ND/500/nd-linker/linker-b01.dom \
      "OPEN-DOMAIN NEWDOM;;LOAD TEST;;EXIT" 80000000
  ```
  Before: `*** ERROR - "4" in module  is illegal control byte. (0054:16)`
  After:
  ```
  LOAD TEST
  Program:.......2466B P01   Data:..........1220B D01
  NDL(ADV)
  ...
  16 undefined entries on current domain.
  ```
  `LOAD CATLIB` (220 KB vendor library): zero control-byte errors.

## Still open

`Loop.c` and `Loopd.c` contain the **identical** pattern (index forced to
`ND500_DTYPE_WORD` at `Loop.c:298/328` and `Loopd.c:221/228`). The manual gives
LOOP and LOOPD the same `<index/rw/t>,<limit/r/t>` typing, so they are very
likely wrong in the same way. They were left untouched here so this fix could be
verified in isolation; they should be fixed and re-verified next.

## What the C# side must mirror

- LOOPI must read AND write its index at the instruction's own data type
  (BY/H/W/F/D), never forced to 32-bit. The bug is invisible when the index is a
  register and only bites when it is a memory operand with live neighbours.
- Check LOOP and LOOPD for the same pattern.
