# ND-500 SYSTEM Instruction Category - Behavior Reference

Authoritative behaviour reference for the emulator instruction category **SYSTEM**,
built ONLY from primary sources. Where a source does not state a fact, it is marked
`UNKNOWN (needs verification)`. Nothing here is inferred from the emulator's own C
source; the C headers are treated as unverified claims and are only mentioned when
they CONFLICT with the primary sources.

## Sources

- PRIMARY spec (operation, operands, traps, documented status bits):
  `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
  (cited as "Manual sec X.Y / page N").
- GROUND-TRUTH flag micro-behaviour (ND-5000 microcode):
  `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
  (cited by octal micro-address / label).
- Micro-op field mnemonics decoded from:
  `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`

## Micro-op field mnemonics used below (from mnemonics.md)

| Field       | mnemonics.md line | Meaning                                            |
|-------------|-------------------|----------------------------------------------------|
| `ST,SAVA`   | 656               | SAVE STATUS FROM ALU OPERATION (updates Z,C,O,S)   |
| `ST,SAVF`   | 658               | SAVE STATUS FROM FLOATING OPERATION                |
| `ST,SAVC`   | 657               | SAVE STATUS FROM ALU IN COMPARE                    |
| `K,ONE`     | 653               | SET K (flag) = 1                                   |
| `K,ZRO`     | 654               | CLEAR K (flag) = 0                                 |
| `K,1IFZ`    | 655               | SET K = 1 IF ALU OPERATION RESULT IS 0             |
| `ALU,FZRO`  | 26                | FORCE ZERO ALU OUTPUT                              |
| `A,MIC,STS` | 381               | A-bus = MIC status bits (reads status, no write)   |

NOTE on the ND-500 status word: the flags requested (K, Z, C, O, S) are individual
bits of the status register. `ST,SAVA` writes Z (zero), C (carry), O (overflow) and
S (sign) together from the last ALU result. `ST,SAVF` writes the floating status.
`K` is a separate flag with its own dedicated micro-ops (`K,ONE`/`K,ZRO`/`K,1IFZ`).
The Manual's "Data status bits" line only lists the bits the instruction is
*documented* to define; bits set by `ST,SAVA` but not called out in the Manual are
noted as "written by SAVA, meaning not documented".

---

## CROSS-SOURCE DISCREPANCIES FOUND (read first)

1. **INTR is NOT an interrupt-register read.** Manual sec 10.35 (page 171) and
   microcode labels `INTRF` (002613) / `INTRD` (002615) both define INTR as
   *"float integer part with rounding"* - a FLOATING conversion, adjacent to INT.
   The emulator file `Intr.c` documents/implements INTR as "Read Interrupt Request
   Register". That contradicts BOTH primary sources. Documented here per the sources.

2. **WDUS is not documented in this Reference Manual.** No "store bypassing cache"
   section exists; the octal codes the emulator assigns to WDUS (0xFEB0.. = 177260B..)
   are assigned by the Manual to PADD (177260B, sec 16.x) and WPCONV (177270B). WDUS
   DOES exist in the microcode as a store-uncached op (`WDUSBY` 001651, `WDUSH` 001661,
   `WDUSW` 001671, completing via `WDUS_1` 004476 -> DUMMY). Opcode assignment =
   CONFLICT / needs verification.

3. **DCC, PCC, DDIRT, TUTTI privilege.** Manual lists their **Trap conditions: None**
   (secs 16.10, 16.12, 16.11, 16.2). The emulator marks DCC/PCC/DDIRT/TUTTI as
   privileged (IIC). Per the Manual they are not privileged. Needs verification.

4. **LIND / CIND status bits.** The emulator headers claim only K/IX. The Manual and
   microcode show these instructions ALSO define Z and S (and CIND: O). See sections.

5. **RDUS status bits.** Emulator header says "None affected". Manual sec 16.25 states
   RDUS defines Z and S. See section.

6. **RPHS / WPHS operands.** Manual (secs 16.31/16.32) define a SINGLE operand
   `<domain number>` and use registers I1-I4 for count/pointers; the emulator header
   describes a 3-operand form. Documented here per the Manual.

7. **Register-block mask numbering** (LREGBL/SREGBL/LCNTXT/SCNTXT): Manual page 320
   uses OCTAL bit numbers P=0,L=1,B=2,R=3,I1=4..I4=7,A1=10..A4=13,E1=14..E4=17,
   STS=20,PS=21,TOS=22,LL=23,HL=24,THA=25,CED=26,CAD=27,MIC=30,OTE=31,CTE=32,MTE=33,
   TEMM=34. The emulator header lists a different (decimal, 1-based) numbering.

Minor: the Manual's own tables contain several hex<->octal transcription typos
(e.g. RLADDR "BY" hex 0FC54H vs octal 176132B; 0xFC5A octal = 176132B). Where they
disagree the OCTAL column is used as authoritative below.

---

## Instruction index (35 files in src/cpu/instructions/SYSTEM/)

Address/index: LADDR, RLADDR, BLADDR, LIND, CIND, PHYLADR, RDUS, WDUS
Float: INT, INTR
String: SLOCA
Heap: FREEB
Process sync: TUTTI
Cache/TLB: DCC, PCC, DDIRT, PCTSB, DCTSB
MMU on/off: DMON, DMOF, PMON, PMOF
Page tables: RPGU, ZPGU, CPGU, RWIP, ZWIP, CWIP
Segment copy: RPHS, WPHS
Register/context blocks: LREGBL, SREGBL, LCNTXT, SCNTXT
Version: SVERS

===============================================================================

## LADDR - Load address

- Opcode (octal, Manual sec 15.4 / page 271): `BIn 177040B+(n-1)`, `BYn 177044B+(n-1)`,
  `Hn 177050B+(n-1)`, `Wn 176474B+(n-1)`, `Fn 176474B+(n-1)` (= Wn), `Dn 177054B+(n-1)`.
  Hex: BIn 0xFE20+, BYn 0xFE24+, Hn 0xFE28+, Wn/Fn 0xFD3C+, Dn 0xFE2C+.
- Operation: `addr(<operand>) -> Rn`.
- Operands: 1 - `<operand/aa/t>`. Registers and constants are illegal (no address).
- Microcode: `LADDRN` 000761 -> 000762 executes `ST,SAVA` (status saved from the
  loaded address). `LADDRD4` 000763 -> 000764/000765 also `ST,SAVA`.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual sec 15.4 lists only Z; no K micro-op in 000761/000762 |
| Z | CONDITIONAL: address = 0 -> Z=1 | Manual sec 15.4 "address = 0 -> Z"; microcode `ST,SAVA` @000762 |
| C | Written by SAVA, meaning not documented | microcode `ST,SAVA` @000762; Manual does not list C |
| O | Written by SAVA, meaning not documented | microcode `ST,SAVA` @000762; Manual does not list O |
| S | CONDITIONAL: sign bit of loaded address -> S | microcode `ST,SAVA` @000762 (Manual lists only Z) |

- Trap conditions: Addressing traps (Manual sec 15.4). Illegal operand if operand is
  a register/constant (general rule, Manual page ~3630).

## RLADDR - Load address into record register

- Opcode (octal, Manual sec 15.5 / page 272): `BI 176125B`, `BY 176132B`,
  `H 176261B`, `W 276B`, `F 276B` (= W), `D 176262B`. Hex: BI 0xFC55, W/F 0xBE,
  D 0xFCB2 (BY/H hex column in Manual is inconsistent; use octal).
- Operation: `addr(<operand>) -> R`.
- Operands: 1 - `<operand/aa/t>`. Registers/constants illegal.
- Microcode: `RLADDR` 000766 -> 000767 executes `ST,SAVA`.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual sec 15.5 lists only Z |
| Z | CONDITIONAL: address = 0 -> Z=1 | Manual sec 15.5; microcode `ST,SAVA` @000767 |
| C | Written by SAVA, meaning not documented | microcode @000767 |
| O | Written by SAVA, meaning not documented | microcode @000767 |
| S | CONDITIONAL: sign bit of address -> S | microcode `ST,SAVA` @000767 (Manual lists only Z) |

- Trap conditions: Addressing traps (Manual sec 15.5).

## BLADDR - Load address into base register

- Opcode (octal, Manual sec 15.6 / page 273): `BI 176263B`, `BY 176274B`,
  `H 176467B`, `W 176543B`, `F 176543B` (= W), `D 176470B`. Hex: BI 0xFCB3,
  BY 0xFCBC, H 0xFD37, W/F 0xFD63, D 0xFD38.
- Operation: `addr(<operand>) -> B`.
- Operands: 1 - `<operand/aa/t>`. Registers/constants illegal.
- Microcode: `BLADDR` 000771 -> 000772 executes `ST,SAVA`.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual sec 15.6 lists only Z |
| Z | CONDITIONAL: address = 0 -> Z=1 | Manual sec 15.6; microcode `ST,SAVA` @000772 |
| C | Written by SAVA, meaning not documented | microcode @000772 |
| O | Written by SAVA, meaning not documented | microcode @000772 |
| S | CONDITIONAL: sign bit of address -> S | microcode `ST,SAVA` @000772 (Manual lists only Z) |

- Trap conditions: Addressing traps (Manual sec 15.6).

## LIND - Load index (with bounds check)

- Opcode (octal, Manual sec 15.8 / page 275): `BYn 176441B+(n-1)`,
  `Hn 176420B+(n-1)`, `Wn 254B+(n-1)`, `Fn 177710B+(n-1)`, `Dn 177714B+(n-1)`.
- Operation: `<index> -> Rn`; if `<index> < <lower>` or `<index> > <upper>` then
  `1 -> K` and Illegal-index trap; else `0 -> K`.
- Operands: 3 - `<index/r/t>, <lower/r/t>, <upper/r/t>`.
- Microcode: `LIND` 001576 carries `ST,SAVA` (index status saved). In-bounds path
  `LIND_2` 003632 executes `K,ZRO`. Bounds compare at `LIND_1` 003630 (`C,SEQ`/INVSEQ).

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: out of bounds -> K=1, in bounds -> K=0 | Manual sec 15.8; microcode `K,ZRO` @003632 (in-bounds) |
| Z | CONDITIONAL: `<index> = 0` -> Z=1 | Manual sec 15.8 "index = 0 -> Z"; microcode `ST,SAVA` @001576 |
| C | Written by SAVA, meaning not documented | microcode `ST,SAVA` @001576 |
| O | Written by SAVA, meaning not documented | microcode `ST,SAVA` @001576; Manual does not list O for LIND |
| S | CONDITIONAL: `<index>.signbit -> S` | Manual sec 15.8 "index.signbit -> S"; microcode SAVA |
| IX (trap bit) | SET on out-of-bounds | Manual sec 15.8 / sec 2 (IX set/reset by LIND/CIND) |

- Trap conditions: Addressing traps; Illegal index (IX) (Manual sec 15.8).

## CIND - Calculate index

- Opcode (octal, Manual sec 15.9 / page 276): `BYn 176424B+(n-1)`,
  `Hn 176430B+(n-1)`, `Wn 260B+(n-1)`, `Fn 177720B+(n-1)`, `Dn 177724B+(n-1)`.
- Operation: `Rn * (<upper> - <lower> + 1) + <index> -> Rn`; if `<index> < <lower>`
  or `<index> > <upper>` then `1 -> K` and Illegal-index trap; else `0 -> K`.
- Operands: 3 - `<index/r/t>, <lower/r/t>, <upper/r/t>`.
- Microcode: `CINDBY` 001613 (and H/W/F/D variants). Range subtraction with
  `ST,SAVA` at 001632/001636 (`CIND_F` path). In-bounds `CIND_CHK` 003144 executes
  `K,ZRO`; out-of-bounds paths execute `K,ONE`.

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: out of bounds -> K=1 (`K,ONE`), in bounds -> K=0 (`K,ZRO` @003144) | Manual sec 15.9; microcode |
| Z | CONDITIONAL: result = 0 -> Z=1 | Manual sec 15.9 "result = 0 -> Z"; microcode `ST,SAVA` @001632 |
| C | Written by SAVA, meaning not documented | microcode `ST,SAVA` |
| O | CONDITIONAL: overflow -> O=1 | Manual sec 15.9 "overflow -> O"; microcode `ST,SAVA` |
| S | CONDITIONAL: result.signbit = 0 -> S (per Manual wording) | Manual sec 15.9 "result.signbit = 0 -> S"; microcode SAVA |
| IX (trap bit) | SET on out-of-bounds | Manual sec 15.9 / sec 2 |

- Trap conditions: Addressing traps; Integer overflow (O); Illegal index (IX)
  (Manual sec 15.9).

## PHYLADR - Get physical address ('87 extension)

- Opcode (octal, Manual sec 16.37 / page 334): `tn PHYLADR 177760B+(n-1)`
  (hex 0xFFF0+(n-1)).
- Operation: `tr(addr(<operand>)) -> In` (logical address translated to physical).
- Operands: 1 - `<operand/aa/W>`.
- Microcode: `PHYLADR` 001026 (`DATOP`, `EA1SAVE`). No `ST,SAVA`/`K`/`SAVF` observed
  in the entry path.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual sec 16.37 "Data Status Bits:" line is BLANK; no micro-op seen |
| Z | UNKNOWN (needs verification) | Manual status line blank; microcode inconclusive |
| C | UNKNOWN (needs verification) | Manual status line blank |
| O | UNKNOWN (needs verification) | Manual status line blank |
| S | UNKNOWN (needs verification) | Manual status line blank |

- Trap conditions: Manual sec 16.37 "Trap Conditions:" line is BLANK -> UNKNOWN
  (needs verification). (Emulator header claims Addressing traps and Z; not confirmed
  by the Manual.)

## RDUS - Load bypassing cache

- Opcode (octal, Manual sec 16.25 / page 313): `BIn 177240B+(n-1)`,
  `BYn 177244B+(n-1)`, `Hn 177250B+(n-1)`, `Wn 177254B+(n-1)`.
- Operation: `<source> -> Rn` (read from main memory, bypassing cache).
- Operands: 1 - `<source/r/t>`. Register and constant operands ILLEGAL (IOS trap).
- Microcode: `RDUSBI` 000747, `RDUSW` 000751 (`LADDR EA1SAVE`), uncached read via
  `RDUS_1` 004515 (`EXUC`). No explicit `ST,SAVA` seen in the sampled uncached path;
  Manual is authoritative for the defined bits.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual sec 16.25 lists only Z,S |
| Z | CONDITIONAL: `<source> = 0` -> Z=1 | Manual sec 16.25 "{source} = 0 -> Z" |
| C | UNKNOWN (needs verification) | Manual lists only Z,S; microcode inconclusive |
| O | UNKNOWN (needs verification) | Manual lists only Z,S |
| S | CONDITIONAL: `<source>.signbit -> S` | Manual sec 16.25 "{source}.signbit -> S" |

- Trap conditions: Addressing traps; Illegal operand specifier (IOS) (Manual sec 16.25).
- (NOT privileged; if no cache present RDUS is equivalent to `:=`.)

## WDUS - Store bypassing cache

- Opcode: **NOT DOCUMENTED in this Reference Manual** (see discrepancy #2).
  Emulator uses 0xFEB0+/0xFEB4+/0xFEB8+/0xFEBC+ (= octal 177260B+..) which the Manual
  assigns to other instructions (PADD 177260B, WPCONV 177270B) - CONFLICT / needs
  verification.
- Operation (from microcode): store register to memory bypassing cache. Store-uncached
  variants `WDUSBY` 001651, `WDUSH` 001661, `WDUSW` 001671; complete via `WDUSBI_1`
  004466 / `WDUS_1` 004476 -> DUMMY.
- Operands: 1 destination `<dest/w/t>`; register/constant destinations illegal (IOS)
  is the analogous rule to RDUS - UNKNOWN whether Manual would state it (no section).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED (store path shows no K micro-op) | microcode `WDUSBY` 001651 -> `WDUS_1` 004476; Manual: no section |
| Z | UNCHANGED (no `ST,SAVA` in store path) | microcode; Manual: no section |
| C | UNCHANGED | microcode; Manual: no section |
| O | UNCHANGED | microcode; Manual: no section |
| S | UNCHANGED | microcode; Manual: no section |

- Trap conditions: Manual: no section -> UNKNOWN. By analogy to RDUS: Addressing traps,
  IOS - needs verification.

## INT - Integer part

- Opcode (octal, Manual sec 10.34 / page 170): `Fn INT 177140B+(n-1)`,
  `Dn INT 177144B+(n-1)` (hex 0xFE60+, 0xFE64+).
- Operation: truncated integer part of `<x>` in float format -> Rn (no rounding).
- Operands: 1 - `<x/r/t>` (F or D).
- Microcode: `INTF` 002607 / `INTD` 002611 -> `INTF_0` 017345 executes `ST,SAVA`
  (twice, @017345 and @017346).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual sec 10.34 lists only Z,S |
| Z | CONDITIONAL: result = 0 -> Z=1 | Manual sec 10.34 "result = 0 -> Z"; microcode `ST,SAVA` @017345 |
| C | Written by SAVA, meaning not documented | microcode `ST,SAVA` @017345 |
| O | Written by SAVA, meaning not documented | microcode `ST,SAVA` @017345 |
| S | CONDITIONAL: result.signbit -> S | Manual sec 10.34 "result.signbit -> S"; microcode SAVA |

- Trap conditions: Addressing traps (Manual sec 10.34).

## INTR - Integer part with rounding (NOT interrupt read - see discrepancy #1)

- Opcode (octal, Manual sec 10.35 / page 171): `Fn INTR 177150B+(n-1)`,
  `Dn INTR 177154B+(n-1)` (hex 0xFE68+, 0xFE6C+).
- Operation: ROUNDED integer part of `<x>` in float format -> Rn.
- Operands: 1 - `<x/r/t>` (F or D).
- Microcode: `INTRF` 002613 / `INTRD` 002615 -> `INTRF_0` 017355 executes `ST,SAVA`.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual sec 10.35 lists only Z,S |
| Z | CONDITIONAL: result = 0 -> Z=1 | Manual sec 10.35; microcode `ST,SAVA` @017355 |
| C | Written by SAVA, meaning not documented | microcode `ST,SAVA` @017355 |
| O | Written by SAVA, meaning not documented | microcode `ST,SAVA` @017355 |
| S | CONDITIONAL: result.signbit -> S | Manual sec 10.35; microcode SAVA |

- Trap conditions: Addressing traps (Manual sec 10.35).

## SLOCA - String locate element

- Opcode (octal, Manual sec 14.15 / page ~259): `BI SLOCA 176657B` (0xFDAF),
  `BY SLOCA 176660B` (0xFDB0).
- Operation: scan `<source>` element by element until an element equals `<test>` or
  end-of-string; `I1` advances to the located/next element.
- Operands: `<source/r/t/I1=>, <test/r/BI,BY>`.
- Manual terminating conditions (sec 14.15):
  - outside source: `K=0`, `Z=1`, I1 unmodified, DR trap condition
  - element = `<test>`: `K=0`, `Z=1`, I1 = found element
  - source empty: `K=0`, `Z=0`, I1 = next element
- Microcode: `SLOCABI` 001330 / `SLOCABY` 001333. Compare `SLOCABI_F02` 007147 uses
  `ST,SAVA`; `SLOCABI_F03` 007151 uses `K,1IFZ` (set K if ALU result 0); `SLOCABY_F01`
  007164 uses `K,1IFZ`.

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: per terminating condition (0 on match/empty; set-if-zero in micro) | Manual sec 14.15; microcode `K,1IFZ` @007151 |
| Z | CONDITIONAL: end/ match state (see conditions above) | Manual sec 14.15; microcode `ST,SAVA` @007147 |
| C | Written by SAVA, meaning not documented | microcode `ST,SAVA` @007147 |
| O | Written by SAVA, meaning not documented | microcode `ST,SAVA` @007147 |
| S | Written by SAVA, meaning not documented | microcode `ST,SAVA` @007147 (Manual terminating table does not list S) |

- Trap conditions: Addressing traps; Descriptor range (DR) (Manual sec 14.15).

## FREEB - Free buddy element

- Opcode (octal, Manual sec 15.14 / page 281): `FREEB 176666B` (0xFDB6).
- Operation: release `<element>` of size `2**<log size>` words to the heap freelist.
  TOS must point to the heap-variable structure.
- Operands: 2 - `<log size/r/BY>, <element/s/W>`. Write access to `<element>` required.
- Microcode: `FREEB` 001000 -> `FREEB_1` 004403 (`ALU,FZRO`, `LADDR EA1SAVE`).
  No `ST,SAVA`/`K` micro-op in this path.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual sec 15.14 "Data status bits: Unaffected"; no K micro-op |
| Z | UNCHANGED | Manual sec 15.14 Unaffected |
| C | UNCHANGED | Manual sec 15.14 Unaffected |
| O | UNCHANGED | Manual sec 15.14 Unaffected |
| S | UNCHANGED | Manual sec 15.14 Unaffected |

- Trap conditions: Addressing traps (Manual sec 15.14).

## TUTTI - Enable process switch

- Opcode (octal, Manual sec 16.2 / page 290): `TUTTI 177001B` (0xFE01).
- Operation: process switch is enabled (complement of SOLO).
- Operands: 0.
- Microcode: `TUTTI` 000712 -> `TUTTI_0` 004534 (`A,SPEC,MOD`). No status micro-op.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual sec 16.2 "Data status bits: Unaffected" |
| Z | UNCHANGED | Manual sec 16.2 Unaffected |
| C | UNCHANGED | Manual sec 16.2 Unaffected |
| O | UNCHANGED | Manual sec 16.2 Unaffected |
| S | UNCHANGED | Manual sec 16.2 Unaffected |

- Trap conditions: Manual sec 16.2 "Trap conditions: None". (Emulator marks TUTTI
  privileged/IIC - NOT confirmed by the Manual; see discrepancy #3.)

## DCC - Data cache clear

- Opcode (octal, Manual sec 16.10 / page 298): `DCC 177425B` (0xFF15).
- Operation: mark data-cache data invalid; 'dirty' data dumped to memory. If no cache,
  no effect.
- Operands: 0.
- Microcode: `DCC` 000720 -> `DCC_IC`. No status micro-op.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNCHANGED | Manual sec 16.10 "Data status bits: Unaffected"; no micro-op |

- Trap conditions: Manual sec 16.10 "Trap conditions: None" (NOT privileged per Manual;
  emulator marks IIC - see discrepancy #3).

## PCC - Program cache clear

- Opcode (octal, Manual sec 16.12 / page 300): `PCC 177424B` (0xFF14).
- Operation: mark program-cache data invalid. If no cache, no effect.
- Operands: 0.
- Microcode: `PCC` 000717 -> `PCC_IC`. No status micro-op.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNCHANGED | Manual sec 16.12 "Data status bits: Unaffected" |

- Trap conditions: Manual sec 16.12 "Trap conditions: None" (NOT privileged per Manual).

## DDIRT - Dump 'dirty' ('87 extension)

- Opcode (octal, Manual sec 16.11 / page 299): `DDIRT 177772B` (0xFFFA).
  (Note: emulator header says 0xFE1E - CONFLICT; Manual octal 177772B is authoritative.)
- Operation: write 'dirty' data-cache lines back to memory. If no cache, no effect.
- Operands: 0.
- Microcode: no `DDIRT` label found in MICRO-5800-A30.md -> microcode behaviour
  UNKNOWN (needs verification).

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNCHANGED | Manual sec 16.11 "Data Status Bits: Unaffected" (microcode not located) |

- Trap conditions: Manual sec 16.11 "Trap Conditions: None" (NOT privileged per Manual).

## PCTSB - Clear program translation speedup buffer

- Opcode (octal, Manual sec 16.24 / page 312): `PCTSB 177434B` (0xFF1C).
- Operation: clear the entire program TSB (forces re-translation). Related cache cleared.
- Operands: 0.
- Microcode: `PCTSB` 000725 -> `PCTSB_1`. No status micro-op.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNCHANGED | Manual sec 16.24 "Data status bits: Unaffected" |

- Trap conditions: Illegal instruction code (IIC) (Manual sec 16.24 - privileged).

## DCTSB - Clear data translation speedup buffer

- Opcode (octal, Manual sec 16.24 / page 312): `DCTSB 177435B` (0xFF1D).
- Operation: clear the entire data TSB; when the data cache is cleared, 'dirty' data
  is written to memory.
- Operands: 0.
- Microcode: `DCTSB` 000726 -> `DCTSB_1`. No status micro-op.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNCHANGED | Manual sec 16.24 "Data status bits: Unaffected" |

- Trap conditions: Illegal instruction code (IIC) (Manual sec 16.24 - privileged).

## DMON - Data memory management on

- Opcode (octal, Manual sec 16.13 / page 301): `DMON 177426B` (0xFF16).
- Operation: turn on data memory management (data accesses mapped through MMU). If
  already on, no effect.
- Operands: 0.
- Microcode: `DMON` 000721 -> `DMON_1`. No status micro-op.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNCHANGED | Manual sec 16.13 "Data status bits: Unaffected" |

- Trap conditions: Illegal instruction code (IIC) (Manual sec 16.13 - privileged).

## DMOF - Data memory management off

- Opcode (octal, Manual sec 16.15 / page 303): `DMOF 177430B` (0xFF18).
- Operation: turn off data memory management (data accesses taken as physical
  addresses). If already off, no effect.
- Operands: 0.
- Microcode: `DMOF` 000723 -> `DMOF_1`. No status micro-op.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNCHANGED | Manual sec 16.15 "Data status bits: Unaffected" |

- Trap conditions: Illegal instruction code (IIC) (Manual sec 16.15 - privileged).

## PMON - Program memory management on

- Opcode (octal, Manual sec 16.14 / page 302): `PMON 177427B` (0xFF17).
- Operation: turn on program memory management; `L -> P`. If already on, control is
  transferred to the instruction pointed to by L and no further effect.
- Operands: 0.
- Microcode: `PMON` 000722 -> `PMON_1`. No status micro-op.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNCHANGED | Manual sec 16.14 "Data status bits: Unaffected" |

- Trap conditions: Illegal instruction code (IIC) (Manual sec 16.14 - privileged).

## PMOF - Program memory management off

- Opcode (octal, Manual sec 16.16 / page 304): `PMOF 177431B` (0xFF19).
- Operation: turn off program memory management; `L -> P` (physical address of next
  instruction in L). If already off, control transferred to physical address in L.
- Operands: 0.
- Microcode: `PMOF` 000724 -> `PMOF_1`. No status micro-op.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNCHANGED | Manual sec 16.16 "Data status bits: Unaffected" |

- Trap conditions: Illegal instruction code (IIC) (Manual sec 16.16 - privileged).

## RPGU - Read Page Used table

- Opcode (octal, Manual sec 16.20 / page 308): `BIn RPGU 177210B+(n-1)` (0xFE88+),
  `Hn RPGU 177214B+(n-1)` (0xFE8C+).
- Operation: specified PGU bit or 16-bit group -> Rn. Operand = physical page number
  (BIn) or page/16 (Hn). Only lower 25 bits significant; non-existing memory reads 0.
  Returns logical OR of the separate program/data PGU tables.
- Operands: 1 - `<bit or group no./r/W>`.
- Microcode: `RPGUBI` 000727 / `RPGUH` 000731 -> `RPGUBI_1` 011364 (`A,MIC,STS`,
  `ALU,AND`, `COND,MZRO`). No `ST,SAVA` in the sampled path; Manual authoritative for Z.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual sec 16.20 lists only Z |
| Z | CONDITIONAL: bit or bit group = 0 -> Z=1 | Manual sec 16.20 "bit or bit group = 0 -> Z" |
| C | UNKNOWN (needs verification) | Manual lists only Z |
| O | UNKNOWN (needs verification) | Manual lists only Z |
| S | UNKNOWN (needs verification) | Manual lists only Z |

- Trap conditions: Illegal instruction code (IIC); Illegal operand value (IOV)
  (Manual sec 16.20 - privileged).

## ZPGU - Clear Page Used bit

- Opcode (octal, Manual sec 16.21 / page 309): `BI ZPGU 177220B` (0xFE90).
- Operation: `0 -> specified PGU bit` (clears the bit in BOTH program and data PGU tables).
- Operands: 1 - `<bit no./r/W>`.
- Microcode: `ZPGUBI` 000733. No status micro-op in entry path.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNCHANGED | Manual sec 16.21 "Data status bits: Unaffected" |

- Trap conditions: Illegal instruction code (IIC); Illegal operand value (IOV)
  (Manual sec 16.21 - privileged).

## CPGU - Clear Page Used table

- Opcode (octal, Manual sec 16.22 / page 310): `CPGU 177432B` (0xFF1A).
- Operation: `0 -> entire PGU table`.
- Operands: 0.
- Microcode: `CPGU` 000735 -> `CPGU_1`. No status micro-op.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNCHANGED | Manual sec 16.22 "Data status bits: Unaffected" |

- Trap conditions: Illegal instruction code (IIC) (Manual sec 16.22 - privileged).

## RWIP - Read Written In Page table

- Opcode (octal, Manual sec 16.17 / page 305): `BIn RWIP 177224B+(n-1)` (0xFE94+),
  `Hn RWIP 177230B+(n-1)` (0xFE98+).
- Operation: specified WIP bit or 16-bit group -> Rn. Operand = physical page number
  (BIn) or page/16 (Hn). Only lower 25 bits significant; non-existing memory reads 0.
  Returns logical OR of the separate program/data WIP tables.
- Operands: 1 - `<bit or group no./r/W>`.
- Microcode: `RWIPBI` 000736 / `RWIPH` 000740 -> `RWIPBI_1` 011416 (`A,MIC,STS`,
  `ALU,AND`, `COND,MZRO`). No `ST,SAVA` in the sampled path.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual sec 16.17 lists only Z |
| Z | CONDITIONAL: bit or bit group = 0 -> Z=1 | Manual sec 16.17 "bit or bit group = 0 -> Z" |
| C | UNKNOWN (needs verification) | Manual lists only Z |
| O | UNKNOWN (needs verification) | Manual lists only Z |
| S | UNKNOWN (needs verification) | Manual lists only Z |

- Trap conditions: Addressing traps; Illegal instruction code (IIC)
  (Manual sec 16.17 - privileged).

## ZWIP - Clear Written In Page bit

- Opcode (octal, Manual sec 16.18 / page 306): `BI ZWIP 177234B` (0xFE9C).
- Operation: `0 -> specified WIP bit` (clears the bit in BOTH program and data WIP tables).
- Operands: 1 - `<bit no./r/W>`.
- Microcode: `ZWIPBI` 000742. No status micro-op in entry path.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNCHANGED | Manual sec 16.18 "Data status bits: Unaffected" |

- Trap conditions: Illegal instruction code (IIC); Illegal operand value (IOV)
  (Manual sec 16.18 - privileged).

## CWIP - Clear Written In Page table

- Opcode (octal, Manual sec 16.19 / page 307): `CWIP 177433B` (0xFF1B).
- Operation: `0 -> entire WIP table`.
- Operands: 0.
- Microcode: `CWIP` 000744 -> `CWIP_1`. No status micro-op.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNCHANGED | Manual sec 16.19 "Data status bits: Unaffected" |

- Trap conditions: Illegal instruction code (IIC) (Manual sec 16.19 - privileged).

## RPHS - Read from physical segment ('87 extension)

- Opcode (octal, Manual sec 16.31 / page 328): `RPHS 177765B` (0xFFF5).
- Operation (Manual):
  `while I1 > 0 do  S([I4,I3) -> D(<domain number>.I2); I3+1; I2+1; I1-1  enddo`.
  Registers: I1 = byte count, I2 = logical address on domain, I3 = address on physical
  segment, I4 = physical segment number. Stops when I1=0 or a page boundary is reached.
- Operands: 1 - `<domain number/r/W>` (NOT the 3-operand form in the emulator header).
- Microcode: `RPHS` 001057 -> `RPHS_1` 007702 (`A,MIC,STS`); byte loop with segment
  store variants (`RPHS_SW` 007745, etc.).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual sec 16.31 lists only Z conditions |
| Z | CONDITIONAL: no bytes left (I1=0) -> Z=1; stopped at page boundary with bytes still left -> Z=0 | Manual sec 16.31 "no bytes left = 0 : 1 -> Z" / "page boundary and no bytes left < 0 : 0 -> Z" |
| C | UNKNOWN (needs verification) | Manual lists only Z |
| O | UNKNOWN (needs verification) | Manual lists only Z |
| S | UNKNOWN (needs verification) | Manual lists only Z |

- Trap conditions: Manual sec 16.31 does not list an explicit trap line (privileged
  instruction). Emulator header adds Addressing traps + IIC - the IIC/privileged nature
  is consistent with "Privileged instruction" text; Addressing traps UNKNOWN.

## WPHS - Write to physical segment ('87 extension)

- Opcode (octal, Manual sec 16.32 / page 329): `WPHS 177764B` (0xFFF4).
- Operation (Manual):
  `while I1 > 0 do  S(<domain number>.I2) -> D(I4.I3); I3+1; I2+1; I1-1  enddo`.
  Registers as for RPHS. Stops when I1=0 or a page boundary is reached.
- Operands: 1 - `<domain number/r/W>` (NOT the 3-operand form in the emulator header).
- Microcode: `WPHS` 001061 -> `WPHS_1` 007607 (`A,MIC,STS`); byte loop store variants
  (`WPHS_SW` 007652, etc.).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual sec 16.32 lists only Z conditions |
| Z | CONDITIONAL: no bytes left (I1=0) -> Z=1; page boundary with bytes still left -> Z=0 | Manual sec 16.32 "no bytes left = 0 : 1 -> Z" / "page boundary and no bytes left < 0 : 0 -> Z" |
| C | UNKNOWN (needs verification) | Manual lists only Z |
| O | UNKNOWN (needs verification) | Manual lists only Z |
| S | UNKNOWN (needs verification) | Manual lists only Z |

- Trap conditions: privileged instruction (Manual sec 16.32). Explicit trap line not
  given -> IIC consistent with privileged; other traps UNKNOWN.

## LREGBL - Load register block ('87 extension)

- Opcode (octal, Manual sec 16.27.2 / page 322): `LREGBL 177766B` (0xFFF6).
- Operation: load registers named by `<mask>` from logical memory at
  `<address> + register_number*4`. In non-privileged mode the mask is reduced to
  registers modifiable in non-privileged mode (ST2/PS/CED/CAD/CTE/MTE/TEMM excluded).
- Operands: 2 - `<mask/r/W>, <address/r/W>`. Mask bit numbering (OCTAL): see discrepancy #7.
- Microcode: `LREGBL` 001030. No status micro-op in entry path.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNKNOWN (needs verification) | Manual sec 16.27.2 has NO "Data status bits" line; no status micro-op observed. NOTE: this instruction can LOAD the status register (STS) itself if selected in the mask. |

- Trap conditions: Addressing traps (Manual sec 16.27 general text). Privileged behaviour
  when modifying domain-info-table registers.

## SREGBL - Save register block ('87 extension)

- Opcode (octal, Manual sec 16.27.1 / page 321): `SREGBL 177767B` (0xFFF7).
- Operation: store registers named by `<mask>` to logical memory at
  `<address> + register_number*4`.
- Operands: 2 - `<mask/r/W>, <address/r/W>`.
- Microcode: `SREGBL` 001033. No status micro-op in entry path.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNKNOWN (needs verification) | Manual sec 16.27.1 has NO "Data status bits" line; no status micro-op observed (a store operation) |

- Trap conditions: Addressing traps (Manual sec 16.27 general text).

## SCNTXT - Save context block ('87 extension)

- Opcode (octal, Manual sec 16.27.3 / page 323): `SCNTXT 177771B` (0xFFF9).
- Operation: store the context block of the current process to PHYSICAL address per
  `<mask>`. If address = 0, the current process context save area is used, addressed by
  `(process number+1)*400B + OS-defined address`.
- Operands: 2 - `<mask/r/W>, <address/r/W>`.
- Microcode: `SCNTXT` 001042. No status micro-op in entry path.

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNKNOWN (needs verification) | Manual sec 16.27.3 has NO "Data status bits" line; no status micro-op observed |

- Trap conditions: Illegal instruction code (IIC) - privileged (Manual sec 16.27.3,
  "uses physical address"). Addressing traps also apply per general text.

## LCNTXT - Load context block ('87 extension)

- Opcode (octal, Manual sec 16.27.4 / page 324): `LCNTXT 177770B` (0xFFF8).
- Operation: load the context block of `<process number>` from PHYSICAL address per
  `<mask>`. If address = 0, current process context save area used; if process number
  < 0, current process number is maintained.
- Operands: 3 - `<mask/r/W>, <address/r/W>, <process number/r/W>`.
- Microcode: `LCNTXT` 001036. No status micro-op in entry path. (Can load STS if masked.)

| Flag | Effect | Source |
|------|--------|--------|
| K/Z/C/O/S | UNKNOWN (needs verification) | Manual sec 16.27.4 has NO "Data status bits" line; no status micro-op observed. NOTE: LCNTXT can LOAD the whole status word from the context block if selected in the mask. |

- Trap conditions: Illegal instruction code (IIC) - privileged (Manual sec 16.27.4).

## SVERS - Store microprogram version ('87 extension)

- Opcode (octal, Manual sec 16.35 / page 332): `SVERS 177773B` (0xFFFB).
- Operation: `<microprogram version> -> <destination>`.
- Operands: 1 - `<destination/w/W>`.
- Microcode: `SVERS` 001051 -> `SVERS_1` 011023 -> `SAVE_RES` 011025 executes
  `ST,SAVA` on the version value before writing it.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | no K micro-op @011025 |
| Z | CONDITIONAL: version = 0 -> Z=1 | Manual sec 16.35 "Status bit set according to version"; microcode `ST,SAVA` @011025 |
| C | Written by SAVA, meaning not documented | microcode `ST,SAVA` @011025 |
| O | Written by SAVA, meaning not documented | microcode `ST,SAVA` @011025 |
| S | CONDITIONAL: sign bit of version -> S | microcode `ST,SAVA` @011025 (Manual: "set according to version") |

- Trap conditions: Manual sec 16.35 gives no explicit trap line -> Addressing traps
  apply to the destination write (general rule); no other traps stated.

---

## Summary of microcode-verified flag behaviour

| Instr | Microcode status micro-op(s) | Addr (octal) |
|-------|------------------------------|--------------|
| LADDR   | ST,SAVA | 000762 |
| RLADDR  | ST,SAVA | 000767 |
| BLADDR  | ST,SAVA | 000772 |
| LIND    | ST,SAVA + K,ZRO | 001576 / 003632 |
| CIND    | ST,SAVA + K,ZRO/K,ONE | 001632 / 003144 |
| INT     | ST,SAVA | 017345 |
| INTR    | ST,SAVA | 017355 |
| SLOCA   | ST,SAVA + K,1IFZ | 007147 / 007151 |
| SVERS   | ST,SAVA | 011025 |
| FREEB   | none (no status op) | 001000 / 004403 |
| TUTTI   | none | 000712 / 004534 |
| DCC/PCC/DMON/DMOF/PMON/PMOF/PCTSB/DCTSB/CPGU/CWIP/ZPGU/ZWIP | none | entry labels 000717-000744, 000725/000726, 000733/000742 |
| RPGU/RWIP | reads MIC status (A,MIC,STS), no SAVA seen | 011364 / 011416 |
| RPHS/WPHS | reads MIC status (A,MIC,STS) | 007702 / 007607 |
| RDUS    | no SAVA in sampled uncached path | 000747 / 004515 |
| WDUS    | no status op (store) | 001651 / 004476 |
| PHYLADR | no status op observed | 001026 |
| LREGBL/SREGBL/LCNTXT/SCNTXT | none in entry path | 001030/001033/001036/001042 |
| DDIRT   | LABEL NOT FOUND in microcode | UNKNOWN |
