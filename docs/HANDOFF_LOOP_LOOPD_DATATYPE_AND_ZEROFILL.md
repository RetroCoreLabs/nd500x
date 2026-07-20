# HANDOFF: LOOP/LOOPD index data type, and 2 test expectations that contradict the manual

Date: 2026-07-20
Files changed:
- `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Loop.c`
- `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Loopd.c`

Cross-emulator target: RetroCore `Emulated.HW/ND/CPU/ND500/Instructions` LOOP/LOOPD,
**and** the validation-test generator `Emulated.Tests.ND500/Validation/`.

## Part 1 - the change

`LOOP` and `LOOPD` carried the same defect already fixed in `LOOPI`
(`docs/HANDOFF_LOOPI_INDEX_DATATYPE.md`, commit a3047b1): the index operand was
read and written as `ND500_DTYPE_WORD` regardless of the instruction's data type.

The manual gives all three the same operand typing:

- 13.4 LOOPI: *"The ⟨index⟩ and ⟨limit⟩ operands are of the same data type, which
  may be BY, H, W, F or D."*
- 13.5 LOOPD: *"The ⟨index⟩ and ⟨limit⟩ operands are of the same data type, which
  may be BY, H, W, F or D."*
- 13.6 LOOP: *"The ⟨index⟩, ⟨step⟩ and ⟨limit⟩ operands are of the same data type,
  which may be BY, H, W, F or D."*

Forcing WORD is wrong whenever the index is a **memory** operand: the read pulls
in neighbouring bytes and the write-back clobbers them. In `LOOPI` that
desynchronised the ND linker's NRF record scanner and made it reject every object
file. `LOOP`/`LOOPD` are fixed here pre-emptively, by the same manual sentences.

`<step>` and `<limit>` already used the correct data type; only `<index>` was wrong.

## Part 2 - two test expectations are wrong (deliberately left failing)

The change makes exactly 2 of the 40,061 validation cases fail:

```
Test 16429: Loopd_Index0_Limit0_Exit   BY LOOPD I1,$0,$-10
  Register i1: expected 0xFFFFFFFF, got 0x000000FF
Test 16432: Loopd_Index0_Limit0_Exit   H  LOOPD I1,$0,$-10
  Register i1: expected 0xFFFFFFFF, got 0x0000FFFF
```

**The emulator is right and the test file is wrong.** ND-05.009.4, line 2718:

> When using the integer registers for BIt, BYte and Halfword, the unused upper
> part of the register is always **zero-filled** rather than sign-extended when
> data is loaded to the register.

Corroborated elsewhere in the same manual:

- line 4680 (INV): *"When the datatype is BI, BY, or H only the lower part of the
  register is complemented and the rest of the register is cleared."*
- line 4758 (ABS): *"When the datatype is either BY or H, the result is stored in
  the least significant bits and the rest of the register is cleared."*
- line 4183 (load): *"With BI, BY, or H as data type, the rest of the register is
  zero filled."*

So `BY LOOPD` on register I1 stepping 0 -> -1 must leave `I1 = 0x000000FF`, and
`H LOOPD` must leave `I1 = 0x0000FFFF`. The expected `0xFFFFFFFF` is
sign-extension, which the manual explicitly rules out.

This was invisible for `LOOPI` because incrementing 0 -> 1 leaves the upper bits
zero under either rule; `LOOPD`'s 0 -> -1 is what exposes it.

Per the project rule ("The C# code is not a reference implementation... the
ND-500 CPU, linker, and assembler reference manuals are the TRUTH"), the code was
kept matching the manual and the two cases are left failing on purpose.

### New ctest baseline

**15/18**, with these known-failing tests:

| test | status |
|------|--------|
| `float_arithmetic` | pre-existing, unrelated |
| `mon_calls` | pre-existing, exactly 3 sub-failures |
| `instruction_validation` | **new**, exactly 2 sub-failures, both the LOOPD cases above - test file wrong, not the emulator |

Anything beyond those numbers is a real regression.

## Verification

- Linker unaffected and still working, from `/home/ronny/repos/nd500x/build/link_sandbox`:
  ```
  : > GUEST/NEWDOM.DOM
  ../bin/diag_linkdrive /mnt/d/ND/500/nd-linker/linker-b01.dom \
      "OPEN-DOMAIN NEWDOM;;LOAD TEST;;EXIT" 80000000
  ```
  still reports `Program:.......2466B P01   Data:..........1220B D01`.
- `instruction_validation`: 2 failures, both listed above, both BY/H LOOPD.

## What the C# side must do

1. Apply the same fix: LOOP, LOOPI and LOOPD must read AND write `<index>` at the
   instruction's own data type, never forced to 32-bit.
2. **Fix the test generator.** For BY/H/BI results written to an integer register,
   the upper part of the register is ZERO-FILLED, not sign-extended
   (ND-05.009.4 line 2718). The two `Loopd_Index0_Limit0_Exit` expectations in
   `test/nd500_tests.json` are wrong today. Regenerating with the corrected rule
   should bring nd500x back to a clean instruction-validation run.
3. Check whether the same sign-extend-instead-of-zero-fill assumption leaks into
   other generated expectations for BY/H register results.

## Part 3 - microcode confirmation (ND-5800-B30)

Checked against the real microcode listing
`/mnt/d/ND/5000/Decode/MICRO-5800-B30.LIST` (labels in
`MICRO-5800-B30_labels_simple.csv`; field decode in
`/mnt/d/ND/5000/Decode/MICROCODE-FIELDS.md`).

`LOOPDB` @ 000612 and `LOOPDH` @ 000616 (the :B/:H suffix is the DISPLACEMENT
size, not the data type - there is one integer LOOPD routine, not three):

```
000612 LOOPDB:
  ALU,A-1  TYP,DR A,ALU,REG37 B,X1 D,SC5 ... READ EA1SAVE ADACT ADDR=LOOPDB_0
000613 LOOPDB_0:
  ALU,B-A CRY,ONE TYP,DR A,ALU,REG37 B,SC5 ... ADDR=000614
000614:
  ALU,FZRO TYP,DR A,BM00 B,X1 ... COND,MSGN ... ADDR=000615
000615:
  ALU,A SLOW2 TYP,DR A,SC5 B,X1 D,ALU,REG37 ST,SAVA ... WRITE C,MEMOT
```

Per `MICROCODE-FIELDS.md` bits 100-98 (Data Type Control):

| Value | Mnemonic | Description |
|-------|----------|-------------|
| 000 | TYP,W | Word (32 bits) |
| 010 | TYP,HW | Half word (16 bits) |
| 011 | TYP,BY | Byte (8 bits) |
| 111 | **TYP,DR** | **Data type controlled by ICA** |

Two conclusions, both byte-level:

1. **Every** step of the integer LOOPD - the decrement `ALU,A-1`, the compare
   `ALU,B-A`, and the write-back `ALU,A ... WRITE` - runs under `TYP,DR`, i.e.
   the instruction's own data type. The hardware never widens the index to a
   word. This confirms the change in Part 1 directly.
2. There is **no sign-extension micro-operation anywhere in the write-back
   path**. Had the machine sign-extended a BY/H result into the full register,
   an explicit widening step would have to appear here; it does not. That is
   consistent with ND-05.009.4 line 2718 (zero-fill) and therefore with leaving
   the two `Loopd_Index0_Limit0_Exit` expectations failing as wrong.
