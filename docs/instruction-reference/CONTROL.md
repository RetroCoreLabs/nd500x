# ND-500 CONTROL Category - Authoritative Behavior Reference

Scope: the CONTROL instruction category as defined by the emulator source files
in `/home/ronny/repos/nd500x/src/cpu/instructions/CONTROL/*.c`.

Every statement below is taken directly from one of two sources. Nothing is
inferred unless explicitly marked. Where a source is silent, the entry says
UNKNOWN (needs verification).

Sources
- PRIMARY spec (manual):
  `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
- GROUND-TRUTH micro-behavior (ND-5000 microcode):
  `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
- Micro-op field mnemonics decoded via:
  `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`

## Status-bit background (from manual, both required for the tables below)

Manual 6.5.1 "Data status bits" (`ND-05.009.4 ... Manual.md` lines 2004-2060)
and 6.5.7 "Status bits survey" (same file, lines 2331-2413) define the bits the
tables below track:

| Flag | Manual name | Status-register bit no. | Kind |
|------|-------------|-------------------------|------|
| K | Flag | 8 | status bit, modifiable, no trap ("S M") |
| Z | Zero | 5 | data status bit, no trap ("S M") |
| C | Carry | 6 | data status bit, no trap ("S M") |
| O | Overflow (integer) | 9 | data status bit, ignorable trap ("I M A") |
| S | Sign | 7 | data status bit, no trap ("S M") |

Note: K (bit 8, "Flag") is NOT one of the "data status bits" (Z,C,S,O,IVO,DZ,
FU,FO,BO of Table 7). It is a separate status/flag bit. Therefore an instruction
whose manual entry reads "Data status bits: Unaffected" leaves K unaffected too
unless its Operation line explicitly writes K.

Two general manual rules used below:
- Manual line 2028: "All data status bits not mentioned are reset." (i.e. an
  instruction that mentions only some data status bits clears the rest.)
- Manual line 4040: "Data status bits not mentioned in the instruction
  description are always cleared after the instruction has been executed."

Microcode decoding key (from `mnemonics.md`):
- `K,ONE` (mnemonics.md line 653) = "SET K (FLAG) 1 TO K" -> K := 1
- `K,ZRO` (line 654) = "CLEAR K (FLAG) 0 TO K" -> K := 0
- `ST,SAVA` (line 656) = "SAVE STATUS FROM ALU OPERATION" -> updates Z/C/S/O
  from the ALU result of that microword
- `MIC,STS` / `A,MIC,STS` (lines 578/381) = access MIC status bits (used for
  trap/STO bookkeeping, not the Z/C/S/O data status bits)
- `A,MIC,MISTS` / `D,MIC,MISTS` (lines 377/574) = MIC status register
  (process-switch/PSD state)
- Absence of `ST,SAVA` on an instruction's microwords => Z/C/S/O are not
  recomputed => they retain their previous values (UNCHANGED).
- Absence of `K,ONE`/`K,ZRO` => K is not written (UNCHANGED).

---

## BP - break point instruction

- Opcode: 002H = 002B (manual line 10139).
- Operands: none. Format `BP` (manual 16.4, line 10135).
- Operation: cause a break point instruction trap condition (manual line 10143).
- Description: intended for program debugging; the trap handler normally invokes
  a debug routine (manual lines 10143-10147).
- Manual "Data status bits:" = Unaffected (manual line, section 16.4).
- Microcode: `BP` at octal 000201 (MICRO-5800-A30.md line 143); continues
  `BP_1` 011540 (line 4974), `BP_2` 011552 (line 4984). No `ST,SAVA`, no
  `K,ONE`/`K,ZRO` on any of these microwords (uses `MIC,STS` for trap status
  only).

Status flags:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual 16.4 "Unaffected"; microcode BP 000201 has no K micro-op |
| Z | UNCHANGED | Manual 16.4 "Unaffected"; microcode BP 000201 has no ST,SAVA |
| C | UNCHANGED | Manual 16.4 "Unaffected"; microcode BP 000201 has no ST,SAVA |
| O | UNCHANGED | Manual 16.4 "Unaffected"; microcode BP 000201 has no ST,SAVA |
| S | UNCHANGED | Manual 16.4 "Unaffected"; microcode BP 000201 has no ST,SAVA |

Trap conditions (manual line 10151): Breakpoint instruction trap (BPT); Illegal
instruction code (IIC) if BPT is not enabled.

---

## NOOP - no operation

- Opcode: 003H = 003B (manual line 9689).
- Operands: none. Format `NOOP` (manual 15.10, line 9685).
- Operation: None (manual line 9689 area, "Operation: None").
- Manual "Data status bits:" = Unaffected (manual 15.10).
- Microcode: `NOOP` at octal 000202 (MICRO-5800-A30.md line 144). Single
  microword `ALU,FZRO ... G,OOPS`; no `ST,SAVA`, no K micro-op.

Status flags:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual 15.10 "Unaffected"; microcode NOOP 000202 no K micro-op |
| Z | UNCHANGED | Manual 15.10 "Unaffected"; microcode NOOP 000202 no ST,SAVA |
| C | UNCHANGED | Manual 15.10 "Unaffected"; microcode NOOP 000202 no ST,SAVA |
| O | UNCHANGED | Manual 15.10 "Unaffected"; microcode NOOP 000202 no ST,SAVA |
| S | UNCHANGED | Manual 15.10 "Unaffected"; microcode NOOP 000202 no ST,SAVA |

Trap conditions (manual): None.

---

## SETK - set flag

- Opcode: 0FE02H = 177002B (manual line 9717).
- Operands: none. Format `SETK` (manual 15.11, line 9713).
- Operation: `1 -> K bit of status register` (manual, section 15.11).
- Manual "Data status bits:" = Unaffected (manual 15.11) - i.e. the DATA status
  bits Z/C/S/O are unaffected; the Operation line separately sets K.
- Microcode: `SETK` at octal 000774 (MICRO-5800-A30.md line 522):
  `ALU,FZRO A,BM00 B,X1 K,ONE T,JMP COND,MSEXO TBC,NEXT G,OOPS`. The `K,ONE`
  micro-op sets K:=1. No `ST,SAVA` (Z/C/S/O untouched). This is a single-
  microword handler ending in `TBC,NEXT` (fetch next macro-instruction); the
  adjacent 000775 `CLRK` (`K,ZRO`) is a SEPARATE macro-instruction (RESK), not a
  fall-through.

Status flags:

| Flag | Effect | Source |
|------|--------|--------|
| K | SET (K := 1) | Manual 15.11 Operation "1 -> K"; microcode SETK 000774 `K,ONE` |
| Z | UNCHANGED | Manual 15.11 "Unaffected"; microcode SETK 000774 no ST,SAVA |
| C | UNCHANGED | Manual 15.11 "Unaffected"; microcode SETK 000774 no ST,SAVA |
| O | UNCHANGED | Manual 15.11 "Unaffected"; microcode SETK 000774 no ST,SAVA |
| S | UNCHANGED | Manual 15.11 "Unaffected"; microcode SETK 000774 no ST,SAVA |

Trap conditions (manual): None.

---

## SET1 - set to one

Note: the emulator files SET1 under CONTROL, but the manual documents it in
chapter 10.18 "Set to one" under DATA TRANSFER AND LOGICAL INSTRUCTIONS
(`ND-05.009.4 ... Manual.md` lines 4855-4888).

- Opcodes (manual lines 4864-4869):
  - BI SET1 (bit): 0FC86H = 176206B
  - BY SET1 (byte): 0FC87H = 176207B
  - H SET1 (halfword): 0FC88H = 176210B
  - W SET1 (word): 04DH = 115B
  - F SET1 (float): 047H = 107B
  - D SET1 (double float): 0FC89H = 176211B
- Operands: 1 - `<operand/w/t>` (write, typed) (manual line 4860).
- Operation: `1 -> <operand>` (manual, section 10.18).
- Manual "Data status bits:" = All cleared (manual line 4880).
- Microcode: `SET1` at octal 000325, `SET1_BI` at 000324, plus 000322/000323
  (MICRO-5800-A30.md lines 224-227). These microwords carry `ST,SAVA` (save
  status from ALU). Because the ALU result stored is the value 1, `ST,SAVA`
  yields Z=0 (nonzero), S=0 (positive), C=0, O=0 -> all data status bits
  cleared, matching the manual. No `K,ONE`/`K,ZRO` micro-op -> K untouched.

Status flags:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual 10.18 clears DATA status bits only (K is not a data status bit); microcode SET1 000325 no K micro-op |
| Z | CLEARED | Manual 10.18 "All cleared"; microcode SET1 000325 `ST,SAVA` on result=1 (nonzero) |
| C | CLEARED | Manual 10.18 "All cleared"; microcode SET1 000325 `ST,SAVA` (no integer carry) |
| O | CLEARED | Manual 10.18 "All cleared"; microcode SET1 000325 `ST,SAVA` (no overflow) |
| S | CLEARED | Manual 10.18 "All cleared"; microcode SET1 000325 `ST,SAVA` on result=1 (positive) |

Trap conditions (manual line 4876): Addressing traps.

---

## TSET - test and set

- Opcode: 0FD40H = 176500B (manual line 10097).
- Operands: 1 - `BY TSET <operand/rwl/BY>` (read-write-locked, BYTE type)
  (manual 16.3, line 10093). Register and constant operands are illegal.
- Operation (manual lines 10101-10106): lock; read operand and set status bits;
  set operand to all ones; unlock.
- Manual "Data status bits:" (manual, section 16.3):
  - operand was zero before store -> Z
  - operand was negative before store -> S
  (C and O are not mentioned -> reset by the general rules at manual lines
  2028 / 4040.)
- Microcode: `TSET` at octal 000757 (MICRO-5800-A30.md line 509), typed
  `TYP,BY` (byte); continues `TSET_1` 004540, `TSET_2` 004553, and 004560.
  `ST,SAVA` appears at 004560 (line 2430) and 004552 (line 2424), saving Z/C/S/O
  from the BYTE value read before the store. No `K,ONE`/`K,ZRO` -> K untouched.

Status flags:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual 16.3 lists only Z,S; K is not a data status bit and microcode TSET path has no K micro-op |
| Z | CONDITIONAL (set if operand byte was zero before store, else cleared) | Manual 16.3 "operand was zero before store -> Z"; microcode `ST,SAVA` at 004560 |
| C | CLEARED | Manual general rule (lines 2028/4040) - not mentioned in 16.3; microcode `ST,SAVA` on a load/AND yields no carry |
| O | CLEARED | Manual general rule (lines 2028/4040) - not mentioned in 16.3; microcode `ST,SAVA` yields no overflow |
| S | CONDITIONAL (set if operand byte was negative before store, else cleared) | Manual 16.3 "operand was negative before store -> S"; microcode `ST,SAVA` at 004560 |

Trap conditions (manual line 10120): Addressing traps; Illegal operand
specifier (IOS) - caused by register or constant operands (manual lines
2215, 3630, 3708).

Emulator discrepancy (not a manual/microcode statement, flagged for validation):
the emulator source `.../CONTROL/Tset.c` operates on a 32-bit WORD and writes
0xFFFFFFFF. The manual and microcode define TSET as a BYTE operation
(`TYP,BY`, `<operand/rwl/BY>`) that stores all-ones into a byte.

---

## SOLO - disable process switch

- Opcode: 0FE00H = 177000B (manual line 10024).
- Operands: none. Format `SOLO` (manual 16.1, line 10020).
- Operation: disables process switch for a maximum of 256 micro-cycles (manual
  line 10024 area). Only the PSD status bit (bit 4) is modified, and only by
  SOLO/TUTTI (manual line 2271).
- Manual "Data status bits:" = Unaffected (manual 16.1).
- Microcode: `SOLO` at octal 000711 (MICRO-5800-A30.md line 471); continues
  `SOLO_0` 004524 (line 2402) which uses `D,MIC,MISTS` (writes the MIC status
  register - i.e. sets Process Switch Disabled). No `ST,SAVA`, no K micro-op.

Status flags:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual 16.1 "Unaffected"; microcode SOLO 000711 no K micro-op |
| Z | UNCHANGED | Manual 16.1 "Unaffected"; microcode SOLO 000711 no ST,SAVA |
| C | UNCHANGED | Manual 16.1 "Unaffected"; microcode SOLO 000711 no ST,SAVA |
| O | UNCHANGED | Manual 16.1 "Unaffected"; microcode SOLO 000711 no ST,SAVA |
| S | UNCHANGED | Manual 16.1 "Unaffected"; microcode SOLO 000711 no ST,SAVA |

Side effect (not a data flag): sets PSD (Process Switch Disabled, status bit 4)
- manual lines 2271, 10030; microcode `SOLO_0` 004524 `D,MIC,MISTS`.

Trap conditions (manual, section 16.1): Disable process switch timeout (DT);
Disable process switch error (DE). (Timeout if unprivileged and PSD held > 256
cycles; error if a non-ignorable/fatal trap occurs while PSD is set - manual
lines 2265, 10036-10038.)

---

## SETE - set bit in own trap enable register

- Opcode: 0FD39H = 176471B (manual line 10171).
- Operands: 1 - `<bit no/r/BY>` (read, BYTE) (manual 16.5, line 10167).
- Operation: set bit `<bit no>` in the Own Trap Enable (OTE) register (manual,
  section 16.5). The `<bit no>` is checked against the modify mask TEMM in the
  domain description table; a non-modifiable bit causes IOV (manual, 16.5 desc).
- Manual "Data status bits:" = Unaffected (manual 16.5).
- Microcode: `SETE` at octal 000713 (MICRO-5800-A30.md line 473), continues
  000714 and `SETE_01` 011110 (line 4694). Operates on the trap-enable register
  via AND/masks; no `ST,SAVA`, no K micro-op.

Status flags:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual 16.5 "Unaffected"; microcode SETE 000713 no K micro-op |
| Z | UNCHANGED | Manual 16.5 "Unaffected"; microcode SETE 000713 no ST,SAVA |
| C | UNCHANGED | Manual 16.5 "Unaffected"; microcode SETE 000713 no ST,SAVA |
| O | UNCHANGED | Manual 16.5 "Unaffected"; microcode SETE 000713 no ST,SAVA |
| S | UNCHANGED | Manual 16.5 "Unaffected"; microcode SETE 000713 no ST,SAVA |

Trap conditions (manual, section 16.5): Addressing traps; Illegal operand value
(IOV) - when attempting to modify a non-modifiable bit (TEMM mask).

---

## CLTE - clear bit in own trap enable register

- Opcode: 0FD3AH = 176472B (manual line 10201).
- Operands: 1 - `<bit no/r/BY>` (read, BYTE) (manual 16.6, line 10197).
- Operation: clear bit `<bit no>` in the Own Trap Enable (OTE) register (manual,
  section 16.6). Same TEMM modify-mask check as SETE; non-modifiable bit -> IOV.
- Manual "Data status bits:" = Unaffected (manual 16.6).
- Microcode: `CLTE` at octal 000715 (MICRO-5800-A30.md line 475), continues
  000716 and `CLTE_01` 011127 (line 4709). Operates on the trap-enable register
  via AND/masks; no `ST,SAVA`, no K micro-op.

Status flags:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual 16.6 "Unaffected"; microcode CLTE 000715 no K micro-op |
| Z | UNCHANGED | Manual 16.6 "Unaffected"; microcode CLTE 000715 no ST,SAVA |
| C | UNCHANGED | Manual 16.6 "Unaffected"; microcode CLTE 000715 no ST,SAVA |
| O | UNCHANGED | Manual 16.6 "Unaffected"; microcode CLTE 000715 no ST,SAVA |
| S | UNCHANGED | Manual 16.6 "Unaffected"; microcode CLTE 000715 no ST,SAVA |

Trap conditions (manual, section 16.6): Addressing traps; Illegal operand value
(IOV) - when attempting to modify a non-modifiable bit (TEMM mask).

---

## INIT - initialize stack

- Opcode: 0DCH = 334B (manual line 7764).
- Operands: 3 (manual 13.9, lines 7750-7760):
  - `<<bottom of stack/r/W>>` (a 4-byte absolute address, direct operand)
  - `<stack demand of main program/r/W>`
  - `<total system stack demand/r/W>`
- Operation (manual, section 13.9):
  - `<<bottom of stack>> -> B`
  - `<<bottom of stack>> + <total system stack demand> -> TOS`
  - `<<bottom of stack>> + <stack demand of main program> -> B.SP`
  - `0 -> B.PREVB`
  - `0 -> B.RETA -> L`
  A `<stack demand of main program>` >= `<total system stack demand>` causes a
  stack overflow (manual, section 13.9 description; also line 2192).
- Manual "Data status bits:" = Unaffected (manual, section 13.9).
- Microcode: `INIT` at octal 000654 (MICRO-5800-A30.md line 442); long sequence
  through `INIT_1` 003720, `INIT_2` 003726, `INIT_3` 003731 (lines 2014-2023).
  The `MIC,STS` / `COND,MCRY` at 003730 is the stack-overflow (STO) bookkeeping
  (SP vs TOS compare), not a data-status write. No `ST,SAVA`, no K micro-op ->
  Z/C/S/O and K are not written.

Status flags:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual 13.9 "Unaffected"; microcode INIT 000654 no K micro-op |
| Z | UNCHANGED | Manual 13.9 "Unaffected"; microcode INIT 000654 no ST,SAVA |
| C | UNCHANGED | Manual 13.9 "Unaffected"; microcode INIT 000654 no ST,SAVA (MIC,STS at 003730 is STO bookkeeping) |
| O | UNCHANGED | Manual 13.9 "Unaffected"; microcode INIT 000654 no ST,SAVA |
| S | UNCHANGED | Manual 13.9 "Unaffected"; microcode INIT 000654 no ST,SAVA |

Trap conditions (manual, section 13.9): Addressing traps; Stack overflow (STO)
when main-program stack demand >= total system stack demand. INIT sets B.PREVB
= 0, so a later attempt to link below the bottom of the stack raises an Address
Zero (AZ) / Stack Underflow trap (manual lines 1122, 2181) - that is a
consequence of the zeroed PREVB on subsequent instructions, not of INIT itself.

---

## Category summary (STATUS FLAGS)

| Instr | Opcode (octal) | K | Z | C | O | S |
|-------|----------------|---|---|---|---|---|
| BP    | 002B    | UNCH | UNCH | UNCH | UNCH | UNCH |
| NOOP  | 003B    | UNCH | UNCH | UNCH | UNCH | UNCH |
| SETK  | 177002B | SET  | UNCH | UNCH | UNCH | UNCH |
| SET1  | see variants | UNCH | CLR | CLR | CLR | CLR |
| TSET  | 176500B | UNCH | COND(zero before) | CLR | CLR | COND(neg before) |
| SOLO  | 177000B | UNCH | UNCH | UNCH | UNCH | UNCH |
| SETE  | 176471B | UNCH | UNCH | UNCH | UNCH | UNCH |
| CLTE  | 176472B | UNCH | UNCH | UNCH | UNCH | UNCH |
| INIT  | 334B    | UNCH | UNCH | UNCH | UNCH | UNCH |

UNCH = UNCHANGED, CLR = CLEARED, COND = CONDITIONAL. SET1 opcodes: BI 176206B,
BY 176207B, H 176210B, W 115B, F 107B, D 176211B.

No flag effect in this category was left UNKNOWN: every entry is confirmed by
both the manual and the microcode.
