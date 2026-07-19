# ND-500 BITFIELD Instruction Category - Authoritative Behavior Reference

Purpose: ground-truth reference for validating the nd500x emulator's BITFIELD
instruction implementations. Every claim below is read directly from one of the
two authoritative sources. Nothing is assumed. Anything not confirmed by either
source is marked "UNKNOWN (needs verification)".

## Sources

- PRIMARY spec (manual):
  /home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md
- GROUND-TRUTH microcode (ND-5000, MICRO-5800-A30):
  /mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md
- Micro-op field mnemonics decoded via:
  /mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md

## Instruction set enumerated (from source tree)

Enumerated from:
  ls /home/ronny/repos/nd500x/src/cpu/instructions/BITFIELD/*.c

  Clebi.c  -> CLEBI  (clear bit)
  Getb.c   -> GETB   (get buddy element from heap)
  Getbf.c  -> GETBF  (get bit field)
  Getbi.c  -> GETBI  (get bit)
  Putbf.c  -> PUTBF  (put bit field)
  Putbi.c  -> PUTBI  (put bit)
  Setbi.c  -> SETBI  (set bit)

Note: GETB (get buddy element) is a heap-allocation instruction that happens to
live in the emulator's BITFIELD source folder. It is NOT a bit-manipulation
instruction; the manual documents it under chapter 15 (Miscellaneous). It is
included here because it is a member of the enumerated category.

## Data-status-bit rules that apply to the whole category (from the manual)

- Manual, line 2022: "In the description of the instruction set, the effect on
  the data status bits are listed with every instruction. Bits that are set,
  reset or left unaffected are mentioned explicitly. All data status bits not
  mentioned are reset."
- Manual, line 4040: "Data status bits not mentioned in the instruction
  description are always cleared after the instruction has been executed. If the
  status bit is conditionally set a TRUE condition causes the bit to be set (1),
  a FALSE condition causes it to be reset (0)."
- Manual, line 2244 (K flag): "K : Flag. The flag bit is used for signalling
  purposes. There are special instructions for setting, resetting and testing
  this condition. ... descriptor addressing may set but never clear the K flag."
  => The K flag is a SPECIAL signalling flag, not a general arithmetic data
  status bit. None of the BITFIELD instructions list K in their description, and
  (see microcode below) none of them execute a K micro-op. Therefore K is left
  UNCHANGED by these instructions, EXCEPT that a DESCRIPTOR-addressed operand may
  SET K (last-element / illegal-index signalling, manual sections 8.15 / 3750);
  it is never cleared by these instructions.

## Microcode decode key (from mnemonics.md)

The ND-5000 microword has ONE "ST" status-control field. Only one of these can
be active per micro-instruction (mnemonics.md lines 653-664):

  K,ONE   (1) = SET K (flag) to 1
  K,ZRO   (2) = CLEAR K (flag) to 0
  K,1IFZ  (3) = SET K to 1 if ALU operation result is 0
  ST,SAVA (4) = SAVE STATUS FROM ALU OPERATION (writes Z, C, O, S; NOT K)
  ST,SAVC (5) = save status from ALU in compare
  ST,LOAD (8) = load ALU status (used to merge a constructed status word)

Because K,ONE / K,ZRO / K,1IFZ are distinct values of the SAME field as
ST,SAVA, an ST,SAVA micro-op does NOT touch K. In every BITFIELD flag routine
below, the ST field is only ever ST,SAVA or ST,LOAD - never a K,* value - so K
is never written by the microcode of these instructions.

Other decoded fields used below:
  ALU,FZRO  = force ALU output to zero (mnemonics.md line 26)
  COND,MZRO = microcode branch on "Z from ALU operation" (line 771) - a branch
              test, not a flag write
  MIC,STS   = microcode status register (used for trap bits such as STO/IOV);
              distinct from the data status bits Z/C/O/S/K

Compact table cross-reference (manual Appendix I "Setting of Status Bits",
symbol legend at manual lines 14774-14783):
  C = unconditionally cleared, S = unconditionally set, space = unaffected,
  * = set/reset depending on operand value, I = set/reset if integer instruction
  else cleared, A = addressing status. NOTE: this table's OCR in the .md is
  column-misaligned; individual cell-to-bit mapping is NOT reliable, so the raw
  row is quoted only as weak corroboration, never as the primary claim.

---

## GETBI - Get bit

- Manual section: 10.27 "Get bit", page 163 (manual lines 5239-5267).
- Opcodes (manual lines 5246-5248):
  - BYn GETBI: 0FCB4H + (n-1)  =  176264B + (n-1)  (octal)
  - Hn  GETBI: 0FCB8H + (n-1)  =  176270B + (n-1)
  - Wn  GETBI: 0FDD0H + (n-1)  =  176720B + (n-1)
- Operation (manual line 5251): bit <bit No.> of <operand> -> bit 0 of Rn.
- Operands (manual line 5242): tn GETBI <operand/r/t>, <bit No./r/BY>.
  <operand> data type t is BY, H, or W. Result register Rn = I1..I4 (n=1..4).
- Microcode entry points (MICRO-5800-A30.md lines 282, 286, 290):
  GETBIBY=000414, GETBIH=000420, GETBIW=000424.
  Range check -> CLE_IOV (lines 284/288/292); shared flag routine
  GET_BIT=003335 -> GET_BIT_1=003340 (lines 1771, 1774).

### Status flags - GETBI

| Flag | Effect | Manual evidence | Microcode evidence |
|------|--------|-----------------|--------------------|
| K | UNCHANGED (may be SET only if operand uses descriptor addressing; never cleared) | 10.27 does not list K; manual line 2244 (K is special flag) | GET_BIT 003335 / GET_BIT_1 003340: no K,* micro-op in path |
| Z | CONDITIONAL: SET if transferred bit == 0, else CLEARED | line 5261 "transferred bit = 0 -> Z" | GET_BIT_1 003340 `ST,SAVA` (COND,MZRO) saves Z from the bit value |
| C (carry) | CLEARED | not listed -> reset (lines 2022/4040) | GET_BIT_1 003340 ST,SAVA on logical/move result (no carry) |
| O (overflow) | CLEARED | not listed -> reset (lines 2022/4040) | GET_BIT_1 003340 ST,SAVA on logical/move result (no overflow) |
| S (sign) | CLEARED | not listed -> reset (lines 2022/4040) | GET_BIT_1 003340 ST,SAVA; transferred bit value 0/1 has sign bit 0 |

- Trap conditions (manual line 5258): Addressing traps; Illegal operand value
  (IOV) if <bit No.> >= number of bits in the data type, or <bit No.> negative.
  Microcode: range-check paths jump to CLE_IOV (000416/000422/000426) and
  GET_BIT 003337 -> SET_IOV (003132) which sets the IOV status bit via MIC,STS.
- Compact-table row: GETBI has NO row in the Appendix I table (not emitted /
  dropped in OCR); relies on section 10.27 text only.

---

## PUTBI - Put bit

- Manual section: 10.28 "Put bit", page 164 (manual lines 5275-5304).
- Opcodes (manual lines 5282-5284):
  - BYn PUTBI: 0FDD4H + (n-1)  =  176724B + (n-1)
  - Hn  PUTBI: 0FDD8H + (n-1)  =  176730B + (n-1)
  - Wn  PUTBI: 0FDDCH + (n-1)  =  176734B + (n-1)
- Operation (manual line 5287): bit 0 of Rn -> bit <bit No.> of <operand>.
- Operands (manual line 5278): tn PUTBI <operand/w/t>, <bit No./r/BY>.
  Manual line 5291: "The upper bits of the <operand> are unaffected, even when
  the destination is a word register."
- Microcode entry points (lines 318, 322, 326):
  PUTBIBY=000460, PUTBIH=000464, PUTBIW=000470.
  Shared routine PUT_BIT=003341 -> PUT_BIT_1=003343 (lines 1775, 1777).

### Status flags - PUTBI

| Flag | Effect | Manual evidence | Microcode evidence |
|------|--------|-----------------|--------------------|
| K | UNCHANGED (descriptor addressing may SET, never clears) | 10.28 does not list K; line 2244 | PUT_BIT 003341 / PUT_BIT_1 003343: no K,* micro-op |
| Z | CONDITIONAL: SET if transferred bit == 0, else CLEARED | line 5297 "transferred bit = 0 -> Z" | PUT_BIT 003341 `ST,SAVA` saves Z from the transferred bit |
| C (carry) | CLEARED | not listed -> reset (2022/4040) | ST,SAVA at 003341 on logical result |
| O (overflow) | CLEARED | not listed -> reset (2022/4040) | ST,SAVA at 003341 on logical result |
| S (sign) | CLEARED | not listed -> reset (2022/4040) | ST,SAVA at 003341; single-bit value sign 0 |

- Trap conditions (manual line 5294): Addressing traps; Illegal operand value
  (IOV) if <bit No.> >= data-type bit count or negative. Microcode range check
  000462/000466/000472 -> CLE_IOV; PUT_BIT 003342 -> SET_IOV.
- Compact-table row (manual line 15016), raw (column mapping unreliable):
  `PUTBI  | * C * : C:C C:C * C:A :A A:A :`

---

## CLEBI - Clear bit

- Manual section: 10.29 "Clear bit", page 165 (manual lines 5316-5342).
- Opcodes (manual lines 5323-5325):
  - BY CLEBI: 0FE7DH  =  177175B
  - H  CLEBI: 0FE7EH  =  177176B
  - W  CLEBI: 0FE7FH  =  177177B
- Operation (manual line 5328): 0 -> bit <bit No.> of <operand>.
- Operands (manual line 5319): t CLEBI <operand/w/t>, <bit No./r/BY>.
  (No result register; single-variant per data type, not indexed by n.)
- Microcode entry points (lines 306, 310, 314):
  CLEBIBY=000444, CLEBIHW=000450, CLEBIW=000454.
  Shared routine CLE_BIT_1=003346 -> CLE_BIT_2=003350 (lines 1780, 1782).

### Status flags - CLEBI

| Flag | Effect | Manual evidence | Microcode evidence |
|------|--------|-----------------|--------------------|
| K | UNCHANGED (descriptor addressing may SET, never clears) | 10.29 does not list K; line 2244 | CLE_BIT_1 003346 / CLE_BIT_2 003350: no K,* micro-op |
| Z | SET (always 1) | line 5336 "1 -> Z" | CLE_BIT_1 003346 `ALU,FZRO ... ST,SAVA`: forces ALU output to zero, so SAVA sets Z=1 unconditionally |
| C (carry) | CLEARED | not listed -> reset (2022/4040) | ST,SAVA at 003346 on forced-zero result |
| O (overflow) | CLEARED | not listed -> reset (2022/4040) | ST,SAVA at 003346 on forced-zero result |
| S (sign) | CLEARED | not listed -> reset (2022/4040) | ST,SAVA at 003346; forced-zero result sign 0 |

- Trap conditions (manual line 5334): Addressing traps; Illegal operand value
  (IOV) if <bit No.> >= data-type bit count or negative. Microcode CLE_BIT_2
  003350 -> 003351 -> CLE_IOV (003131) range check -> SET_IOV path.
- Compact-table row (manual line 14877), raw (column mapping unreliable):
  `CLEBI | S C: | C C:C C C*: | :A | :A A A:A | :A |`  (the leading `S` is
  consistent with Z being unconditionally SET.)

---

## SETBI - Set bit

- Manual section: 10.30 "Set bit", page 166 (manual lines 5350-5378).
- Opcodes (manual lines 5357-5359, corrected against Appendix octal at line
  14011 and against the hex values):
  - BY SETBI: 0FE80H  =  177200B
  - H  SETBI: 0FE81H  =  177201B
  - W  SETBI: 0FE82H  =  177202B
  NOTE / SOURCE DISCREPANCY: the section-10.30 octal column (manual line 5357)
  prints "176200B", but 0FE80H converts to 177200B (octal), which is what the
  Appendix instruction-code table shows (manual line 14011: "t SETBI | 177200
  177201 177202"). The section-10.30 "176200B" is judged an OCR/print error;
  the correct octal is 177200B/177201B/177202B.
- Operation (manual line 5362): 1 -> bit <bit No.> of <operand>.
- Operands (manual line 5353): t SETBI <operand/w/t>, <bit No./r/BY>.
- Microcode entry points (lines 294, 298, 302):
  SETBIBY=000430, SETBIHW=000434, SETBIW=000440.
  Shared routine SET_BIT_1=003344 (line 1778) -> PUT_BIT_1=003343 (write-back).

### Status flags - SETBI

Manual line 5372 states "All cleared."

| Flag | Effect | Manual evidence | Microcode evidence |
|------|--------|-----------------|--------------------|
| K | UNKNOWN (needs verification) - manual "All cleared" (ambiguous re K) vs microcode has NO K micro-op | line 5372 "All cleared"; but line 2244 says K is special/never cleared by normal addressing | SET_BIT_1 003344 / PUT_BIT_1 003343: no K,ZRO -> microcode does NOT clear K. Best-evidence conclusion: K UNCHANGED |
| Z | CLEARED | line 5372 "All cleared" | SET_BIT_1 003344 `ST,SAVA` writes Z/C/O/S (exact ALU,A operand value not independently traced; rely on manual for the "cleared" outcome) |
| C (carry) | CLEARED | line 5372 "All cleared" | ST,SAVA at 003344 |
| O (overflow) | CLEARED | line 5372 "All cleared" | ST,SAVA at 003344 |
| S (sign) | CLEARED | line 5372 "All cleared" | ST,SAVA at 003344 |

- Trap conditions (manual line 5369): Addressing traps; Illegal operand value
  (IOV) if <bit No.> >= data-type bit count or negative. Microcode range check
  000432/000436/000442 -> CLE_IOV.
- Compact-table row (manual line 15060), raw (column mapping unreliable):
  `| SETBI | C C C: | C C C:C C C C: | :* | :A | :A A:A A: | :A | * |`  (all-C
  cells are consistent with the manual's "All cleared" for Z/C/O/S.)
- KEY EMULATOR-VALIDATION NOTE: the manual says "All cleared" but the microcode
  never issues a K,ZRO. If the emulator currently clears K on SETBI, that must be
  reconciled: microcode ground truth = K left unchanged.

---

## GETBF - Get bit field

- Manual section: 10.31 "Get bit field", page 167 (manual lines 5386-5417).
- Opcodes (manual lines 5392-5394):
  - BYn GETBF: 0FDE0H + (n-1)  =  176740B + (n-1)
  - Hn  GETBF: 0FDE4H + (n-1)  =  176744B + (n-1)
  - Wn  GETBF: 0FDE8H + (n-1)  =  176750B + (n-1)
- Operation (manual line 5396): specified bit field -> Rn.
- Operands (manual line 5388):
  tn GETBF <operand/r/t>, <bit No./r/BY>, <field size/r/BY>.
  Manual line 5400: field is bits <bit No.> .. <bit No.>+<field size>-1 of the
  operand; upper bits of Rn are zero filled. Manual line 5400/2459: <bit No.> and
  <field size> are signed byte integers; the field is treated as a SIGNED
  quantity.
- Microcode entry points (lines 330, 335, 340):
  GETBFBY=000474, GETBFH=000501, GETBFW=000506.
  Shared routine GET_BIT_F=003437 (line 1837); status set at 003444 (ST,SAVA)
  and 003446 (ST,LOAD, merges the sign bit into S); IOV path GETBF_IOV=003353.

### Status flags - GETBF

| Flag | Effect | Manual evidence | Microcode evidence |
|------|--------|-----------------|--------------------|
| K | UNCHANGED (descriptor addressing may SET, never clears) | 10.31 does not list K; line 2244 | GET_BIT_F path 003437-003452: no K,* micro-op |
| Z | CONDITIONAL: SET if bit field == 0, else CLEARED | line 5410 "bit field = 0 -> Z" | 003444 `ST,SAVA` saves Z from assembled field value |
| S (sign) | CONDITIONAL: = leftmost (most significant) bit of the field | line 5411 "bit field.leftmost bit -> S" | 003446 `... ST,LOAD` loads a status word carrying the field sign bit into S |
| C (carry) | CLEARED | not listed -> reset (2022/4040) | 003444 ST,SAVA on field value (no carry) |
| O (overflow) | CLEARED | not listed -> reset (2022/4040) | 003444 ST,SAVA on field value (no overflow) |

- Trap conditions (manual line 5406): Addressing traps; Illegal operand value
  (IOV) if <bit No.> negative, if <field size> zero or negative, or if <bit No.>
  or <bit No.>+<field size> exceeds the number of bits in the data type.
  Microcode: GETBF_IOV=003353 -> SET_IOV=003132.
- Compact-table row (manual line 14928), raw (column mapping unreliable):
  `| GETBF | * C * | C:C:C:C:C * | :A | A:A:A:A:A | : | A * |`  (the `*` cells are
  consistent with Z and S being conditional/operand-dependent.)

---

## PUTBF - Put bit field

- Manual section: 10.32 "Put bit field", page 168 (manual lines 5425-5456).
- Opcodes (manual lines 5433-5435, hex OCR garbled; corrected from the +(n-1)
  pattern and the octal column):
  - BYn PUTBF: 0FDECH + (n-1)  =  176754B + (n-1)
  - Hn  PUTBF: 0FDF0H + (n-1)  =  176760B + (n-1)   (manual hex OCR "OFDOH")
  - Wn  PUTBF: 0FDF4H + (n-1)  =  176764B + (n-1)   (manual hex OCR "OFDFLH")
- Operation (manual line 5438): Rn -> specified bit field.
- Operands (manual line 5428):
  tn PUTBF <operand/w/t>, <bit No./r/BY>, <field size/r/BY>.
  Manual line 5441: stores bits 0..<field size>-1 of Rn into the field; <bit No.>
  and <field size> are signed byte integers.
- Microcode entry points (lines 345, 350, 355):
  PUTBFBY=000513, PUTBFH=000520, PUTBFW=000525.
  Status set at PUTBF_TAB=003503 (ST,SAVA) and 003547 (ST,LOAD); IOV path
  PUTBF_IOV=003473 (line 1865).

### Status flags - PUTBF

| Flag | Effect | Manual evidence | Microcode evidence |
|------|--------|-----------------|--------------------|
| K | UNCHANGED (descriptor addressing may SET, never clears) | 10.32 does not list K; line 2244 | PUTBF flag path: no K,* micro-op |
| Z | CONDITIONAL: SET if bit field == 0, else CLEARED | line 5450 "bit field = 0 -> Z" | PUTBF_TAB 003503 `ST,SAVA` saves Z from the field value |
| S (sign) | CONDITIONAL: = leftmost (most significant) bit of the field | line 5451 "bit field.leftmost bit -> S" | 003547 `... ST,LOAD` loads status carrying the field sign bit into S |
| C (carry) | CLEARED | not listed -> reset (2022/4040) | 003503 ST,SAVA on field value |
| O (overflow) | CLEARED | not listed -> reset (2022/4040) | 003503 ST,SAVA on field value |

- Trap conditions (manual line 5446): Addressing traps; Illegal operand value
  (IOV) if <bit No.> negative, if <field size> zero or negative, or if <bit No.>
  or <bit No.>+<field size> exceeds the number of bits in the data type.
  Microcode: PUTBF_IOV=003473.
- Compact-table row (manual line 15015), raw (column mapping unreliable):
  `PUTBF  | * C * : C:C:C C C * C:A :A A:A :`

---

## GETB - Get buddy element (heap allocation)

WARNING: not a bit-manipulation instruction. Present only because Getb.c sits in
the emulator's BITFIELD folder. The manual documents it in chapter 15.

- Manual section: 15.13 "Get buddy element", page 280 (manual lines 9769-9801).
- Opcodes (manual line 9777):
  - Wn GETB: 0FE4CH + (n-1)  =  177114B + (n-1)
- Operation (manual lines 9779-9781): allocate an element of size 2**<log size>
  words from the heap; address of the element -> Wn. The TOS register must point
  to the heap descriptor variables (manual line 9789).
- Operands (manual line 9772): Wn GETB <log size/r/BY>.
- Microcode entry point (line 524): GETB=000776 -> 000777 -> GETB_1=004376
  (line 2316) -> FINDBDY=004322 (line 2272). No-buddy path NOBUDDY=004332.

### Status flags - GETB

| Flag | Effect | Manual evidence | Microcode evidence |
|------|--------|-----------------|--------------------|
| Z | UNAFFECTED | line 9795 "Data status bits: Unaffected" | FINDBDY/LOOKBDY path (004322-004346): no ST,SAVA / ST,SAVC -> no Z write |
| C (carry) | UNAFFECTED | line 9795 "Unaffected" | no ALU status save in path |
| O (overflow) | UNAFFECTED | line 9795 "Unaffected" | no ALU status save in path |
| S (sign) | UNAFFECTED | line 9795 "Unaffected" | no ALU status save in path |
| K | UNAFFECTED | line 9795 "Unaffected" | no K,* micro-op in path |
| STO (stack overflow) | SET / RESET each execution | manual line 2192: "The STO status bit is set/reset for each ENTS, ENTSN, ENTB, INIT, ENTM and GETB instruction." | NOBUDDY 004333 `... MIC,STS ... -> TRAP` sets the stack-overflow trap status when no free block of the requested size or larger exists |

- Trap conditions (manual line 9792): Addressing traps; Stack overflow (STO) -
  occurs if there are no free data blocks of the requested size or larger.
  Microcode: NOBUDDY=004332 -> 004333 -> TRAP.
- Compact-table row (manual line 14927), raw (column mapping unreliable):
  `| GETB | : | : | A | :A | A:A:A * | : | A * |`  (mostly blank/`A` cells are
  consistent with the data status bits being unaffected and only addressing
  status being touched.)

---

## Summary matrix (data status bits Z / C / O / S / K)

| Instr | Z | C | O | S | K | Other |
|-------|---|---|---|---|---|-------|
| GETBI | COND: 1 if transferred bit==0 | CLEARED | CLEARED | CLEARED | UNCHANGED | IOV trap on bad bit no. |
| PUTBI | COND: 1 if transferred bit==0 | CLEARED | CLEARED | CLEARED | UNCHANGED | operand upper bits preserved; IOV trap |
| CLEBI | SET (always 1) | CLEARED | CLEARED | CLEARED | UNCHANGED | IOV trap |
| SETBI | CLEARED | CLEARED | CLEARED | CLEARED | UNKNOWN (manual "All cleared" vs microcode no K op -> best-evidence UNCHANGED) | IOV trap |
| GETBF | COND: 1 if field==0 | CLEARED | CLEARED | COND: field leftmost bit | UNCHANGED | zero-fills upper bits of Rn; signed field; IOV trap |
| PUTBF | COND: 1 if field==0 | CLEARED | CLEARED | COND: field leftmost bit | UNCHANGED | signed field; IOV trap |
| GETB  | UNAFFECTED | UNAFFECTED | UNAFFECTED | UNAFFECTED | UNAFFECTED | STO trap set/reset; heap alloc, not a bit op |

For all bit instructions (GETBI/PUTBI/CLEBI/SETBI/GETBF/PUTBF): K is never
written by the microcode. A DESCRIPTOR-addressed operand may SET K per manual
sections 8.15 / 3750 (last-element / illegal-index signalling); K is never
cleared by these instructions.

## Open items / unverified

- SETBI, K flag: manual line 5372 says "All cleared" while the microcode
  (SET_BIT_1 003344 / PUT_BIT_1 003343) issues no K,ZRO. Whether real hardware
  clears K on SETBI is UNKNOWN; microcode evidence says it does NOT.
- SETBI, Z/C/O/S = 0: the manual states "All cleared"; the microcode ST,SAVA at
  003344 saves status from an ALU,A operation whose A-operand value was not
  independently traced through the microsequencer, so the "cleared" outcome
  rests on the manual text, not on an independent microcode derivation.
- Appendix I "Setting of Status Bits" table (manual pages 415+): its OCR is
  column-misaligned in the .md, so per-cell bit mapping could not be verified;
  rows are quoted only as weak corroboration.
