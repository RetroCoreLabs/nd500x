# ND-500 Instruction Category: MOVE - Functional Behavior Reference

Ground-truth functional reference for the ND-500 MOVE instruction category, built by
TRACING THE 5000/5800 MICROCODE and cross-checking against the ND-500 Reference Manual.

Nothing here is assumed. Every operation, memory effect and flag effect below is read
directly from:

- Microcode: `$ND5000UC/microcode/MICRO-5800-A30.md`
- Field decode: `$ND5000UC/manual/mnemonics.md`
- Documented intent: `docs/ND-05.009.4 EN ND-500 Reference Manual.md`
- Emulator source (opcodes): `src/cpu/instructions/MOVE/*.c`

Items that could not be resolved from the above are marked `UNKNOWN (needs deeper microtrace)`.

---

## How to read this document (microcode conventions used in the trace)

The ND-500 dispatches each macro-opcode to a micro-address. One micro-cell does an ALU
pass plus optional memory/register moves. Fields decoded per `mnemonics.md`:

- `ALU,A` = result is the A operand passed straight through (a pure copy). `ALU,FZRO` =
  force result 0. `A,<src>` / `B,<src>` select ALU inputs; `D,<dest>` is where the result
  is latched. `A,ALU,REG37` with `ORCON=04 (ORA,OP)` means the A operand register/number is
  OR-ed in from the current operand specifier - i.e. the real source operand of the macro.
  `D,ALU,REG37` with `ORCON=01 (ORD,OP)` = destination taken from the operand specifier.
- `READ` / `WRITE` = data-memory access (normal domain, MMS active); `ADACT` = address
  arithmetic active (compute effective address). `RD,DOM`/`WR,DOM` = explicit domain access.
- `G,OPS` = fetch the next operand specifier (mid-instruction). `G,OOPS` / `G,COOPS` = fetch
  the NEXT macro-instruction => the current instruction is COMPLETE at that cell. Tracing
  therefore follows `ADDR=` until a cell carrying `G,OOPS` (end of routine).
- Status field (bits 75-72): `ST,SAVA` = "save status from ALU operation" - writes the data
  status bits Z, C, O, S from the ALU result. `K,ONE`/`K,ZRO`/`K,1IFZ` = set/clear/conditional
  the K flag. A cell with neither leaves status untouched.

### Two flag rules that resolve every "UNKNOWN" the flags-only pass left

1. Reference Manual 6.5.1 (lines 2018-2024) + rule at line 4040: the data status bits are
   `Z(5) C(6) S(7) O(9) IVO(11) DZ(12) FU(13) FO(14) BO(15)`. **"All data status bits not
   mentioned in the instruction description are always cleared."** So when the manual lists
   only Z and S, then C and O (and the float/BCD bits) are CLEARED, not left unchanged.
2. `ST,SAVA` after an `ALU,A` copy produces exactly: Z = (value==0), S = value.signbit,
   C = 0 (a straight A-pass generates no carry), O = 0 (no overflow on a copy). This matches
   rule 1 bit-for-bit, so the microcode and the manual AGREE for every plain move/assign.

### The K flag is NOT a data status bit

Manual line 2244: "K : Flag ... There are special instructions for setting, resetting and
testing this condition." K lives in the status register but is a control/signal flag, not a
data status bit. None of the plain MOVE micro-routines touch K (no `K,*` field), so
**K is UNCHANGED** by every MOVE-category instruction EXCEPT `CLRK` (which clears it).

Cross-cutting caveat (manual lines 2244, 3750-3759): if any operand of a MOVE-category
instruction is reached through **descriptor (array) addressing**, the addressing hardware
"may set but never clear the K flag" to signal last-element / range-trap. That K side-effect
is a property of the addressing mode, not of the instruction, and can occur on any of the
memory-operand instructions below.

---

# GROUP A - Data move between operands / memory

## MOVE - move source to destination

Name/opcode(octal): `BI MOVE 176013B` (0xFC0B), `BY MOVE 031B` (0x19), `H MOVE 176024B`
(0xFC14), `W MOVE 032B` (0x1A), `F MOVE 033B` (0x1B), `D MOVE 054B` (0x2C).

FUNCTIONAL PSEUDOCODE (traced MOVE/MOVEBI/MOVED at octal 000231/000227/000233):
```
; integer/float word path (MOVE, 000231 -> 000232)
1. SC5   <- source          ; ALU,A A=<source-op> D,SC5  READ  ST,SAVA ; G,OPS (get 2nd op-spec)
2. dest  <- SC5             ; ALU,A A=SC5 D=<dest-op>     WRITE          ; G,OOPS (done)
; bit path (MOVEBI 000227 -> 000230 -> MOVE_BI_0 003163 -> MOVE_BI_1 003170)
1. read source byte, isolate the addressed bit with post-index bitmask (A,PXBM AND)
2. read the destination word, insert the bit (C,ALU ANDCA/OR), WRITE it back
; double path (MOVED 000233..000236) copies two 32-bit words the same way.
```
OPERANDS + datatypes: `<source/r/t>`, `<dest/w/t>`; t in {BI,BY,H,W,F,D}. Source unaffected;
a constant destination is illegal.
RESULT/side-effects: `source -> dest`. Datatype-sized transfer; bit/byte/half zero-extend
within the destination cell as addressed.
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if source==0 | CLEARED | CLEARED | SET if source.signbit |
(`ST,SAVA` on the `ALU,A` source pass writes Z/S and forces C=O=0; manual 10.7 lists Z,S only
=> C,O cleared by rule 4040.)
TRAP conditions: addressing traps (manual 10.7). Descriptor operands may set K.
CITATION: microcode MOVE=000231, MOVEBI=000227, MOVED=000233 (+ MOVE_BI_0=003163,
MOVE_BI_1=003170); Manual sec 10.7 (Move).

## SWAP - exchange two operands

Name/opcode(octal): `BI SWAP 176275B` (0xFCBD), `BY SWAP 176276B` (0xFCBE), `H SWAP 176277B`
(0xFCBF), `W SWAP 122B` (0x52), `F SWAP 176334B` (0xFCDC), `D SWAP 176335B` (0xFCDD).

FUNCTIONAL PSEUDOCODE (traced SWAPF 000534 -> 000535 -> SWAP_1 003617; SWAPD 000536; SWAPBI
000532):
```
; word / float
1. SC1  <- op1            ; ALU,A A=op1 D,SC1 READ EA1SAVE ST,SAVA   ; save op1 status
2. op1  <- (op2 read next); write SC1 to op2 location, read op2 ...   ; G,OPS
3. op2  <- SC1 / op1<-SC2 ; SWAP_1: read op2 into SC2, write SC1->op2, write SC2->op1 ; G,OOPS
; bit swap (SWAPBI/SWAPBI_0..3) reads both bytes, masks each addressed bit, and writes each
;   bit into the other operand's word with conditional ANDCB/OR inserts.
; double (SWAPD/SWAPD_1) exchanges two 32-bit words each way.
```
OPERANDS + datatypes: `<op1/rw/t>`, `<op2/rw/t>`, same type t; both read then both written.
RESULT/side-effects: `op1 <-> op2`. EA1/EA2 saved so the second phase can rewrite op1.
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if original op1==0 | CLEARED | CLEARED | SET if original op1.signbit |
(`ST,SAVA` taken on the op1 read; manual 10.8 lists Z,S of original op1 only.)
TRAP conditions: addressing traps (manual 10.8). Descriptor operands may set K.
CITATION: microcode SWAPBI=000532, SWAPF=000534, SWAPD=000536, SWAP_1=003617, SWAPD_1=003622,
SWAPBI_0..3=003563/003576/003603/003611; Manual sec 10.8 (Swap).

## BMOVE - block move / block fill

Name/opcode(octal): `BY BMOVE 176440B` (0xFD20), `H BMOVE 177170B` (0xFE78), `W BMOVE 177171B`
(0xFE79), `F BMOVE 177172B` (0xFE7A), `D BMOVE 177173B` (0xFE7B).

FUNCTIONAL PSEUDOCODE (traced BMOVEBY 001204 -> BMOVEBY_M 005050 ... loop family 005050-005170):
```
i = 0
while i < m:                 ; m = 3rd operand, unsigned Word
    if source is register/constant:   dest(i) <- source        ; FILL mode
    else:                             dest(i) <- source(i)      ; COPY mode
    i = i + 1
; the micro-loop maintains element pointers (EA1=source, EA2=dest), decrements the loop
; counter (LCDECR), scales the index by element size (IX*1/*2/*4/*8), and picks copy
; direction so overlapping regions are handled ("Overlap is taken care of" - manual 15.1).
; This is a restartable "During"-trap instruction (manual 6.x, 2349).
```
OPERANDS + datatypes: `<source/r/t>, <dest/w/t>, <m/r/W>`; t in {BY,H,W,F,D}. A register or
constant SOURCE => fill the destination with m copies of that value. Register/constant
DESTINATION is illegal. `m` is unsigned.
RESULT/side-effects: m elements copied (or filled) starting at the operand pointers.
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CLEARED | CLEARED | CLEARED | CLEARED |
(Manual 15.1: "Data status bits: All cleared." The micro-loop's terminal cell does a counter
`ST,SAVA`, but the documented net architectural result is all data status bits cleared.)
TRAP conditions: addressing traps; restartable during execution (partial progress preserved).
CITATION: microcode BMOVEBY=001204, BMOVEHW=001212, BMOVEW=001220, BMOVED=001226, loop
BMOVEBY_M=005050 and BMOVEHW_M/BMOVEWF_M families 005135-005170; Manual sec 15.1 (Block Move
and Fill).

## STZ - store zero to operand

Name/opcode(octal): `BI STZ 176205B` (0xFC85), `BY STZ 110B` (0x48), `H STZ 111B` (0x49),
`W STZ 112B` (0x4A), `F STZ 113B` (0x4B), `D STZ 114B` (0x4C).

FUNCTIONAL PSEUDOCODE (traced STZ 000316 -> STZD 000317 -> 000320; STZBI 000313):
```
; word/half/byte/float
1. dest <- 0             ; ALU,FZRO D=<dest-op> WRITE ST,SAVA ; G,OOPS (done)
; double (STZD) writes two zero words.
; bit (STZBI 000313->000314): read dest word, clear the addressed bit (C,ALU ANDCA/FZRO),
;   WRITE it back.
```
OPERANDS + datatypes: `<operand/w/t>`. Constant operand illegal.
RESULT/side-effects: `0 -> operand`.
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET (=1, result is 0) | CLEARED | CLEARED | CLEARED (=0) |
(`ALU,FZRO` + `ST,SAVA`: Z=1, S=0, C=0, O=0. Manual 10.17: "1 -> Z", all else cleared.)
TRAP conditions: addressing traps (manual 10.17). Descriptor operands may set K.
CITATION: microcode STZBI=000313, STZ=000316, STZD=000317; Manual sec 10.17 (Store zero).

## CLR - clear register to zero

Name/opcode(octal): integer `BIn/BYn/Hn/Wn CLR 204B+(n-1)` (0x0084+(n-1)), `Fn CLR 210B+(n-1)`
(0x0088+(n-1)), `Dn CLR 214B+(n-1)` (0x008C+(n-1)); n=1..4 in low opcode bits.

FUNCTIONAL PSEUDOCODE (traced CLR 000307 -> CLRF 000310 -> CLRD 000311):
```
1. Rn <- 0               ; ALU,FZRO D=Rn(REG37 from opcode) ST,SAVA ; G,OOPS (done)
; integer variant clears the whole I-register; F clears An; D clears the An+En pair.
```
OPERANDS + datatypes: none (target register n encoded in opcode). Integer -> I1..I4;
F -> A1..A4; D -> A+E pair.
RESULT/side-effects: `0 -> Rn` (entire register for integer types).
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET (=1) | CLEARED | CLEARED | CLEARED (=0) |
(Manual 10.16: "1 -> Z"; all else cleared. No traps.)
TRAP conditions: NONE (manual 10.16).
CITATION: microcode CLR=000307, CLRF=000310, CLRD=000311; Manual sec 10.16 (Clear register).

## CLRK - clear the K flag

Name/opcode(octal): `CLRK 177003B` (0xFE03). No operands.

FUNCTIONAL PSEUDOCODE (traced CLRK 000775):
```
1. K <- 0                ; ALU,FZRO K,ZRO ; G,OOPS (done)   ; no ST,SAVA => data status bits untouched
```
OPERANDS + datatypes: none.
RESULT/side-effects: `0 -> K` bit of the status register only.
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| CLEARED (=0) | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |
(Micro-cell has `K,ZRO` and NO `ST,SAVA`. Manual 15.12: "Data status bits: Unaffected".)
TRAP conditions: NONE (manual 15.12).
CITATION: microcode CLRK=000775; Manual sec 15.12 (Clear Flag).

---

# GROUP B - Load / store integer register (I1-I4)

## AssignTo (tn :=) - load register from source

Name/opcode(octal), n=1..4 in low bits: `BIn:= 176004B+` (0xFC04+), `BYn:= 4B+` (0x04+),
`Hn:= 10B+` (0x08+), `Wn:= 14B+` (0x0C+), `Fn:= 20B+` (0x10+), `Dn:= 24B+` (0x14+).

FUNCTIONAL PSEUDOCODE (traced LOADT 000206; LOADBI 000203; LOADD 000207-000210):
```
; W / H / BY (LOADT, single cell)
1. Rn <- source          ; ALU,A A=<source-op> D=Rn(REG37 from opcode) READ ST,SAVA ; G,OOPS
; BI (LOADBI 000203-000205): read source, isolate addressed bit (A,PXBM AND), zero-extend
;   into Rn (C,ALU FZRO/A) - i.e. right-justified, upper bits zero-filled.
; D (LOADD): read two words into the register pair.
; F/D float variants load A1..A4 / A+E pair (uses_float_registers path).
```
OPERANDS + datatypes: `<source/r/t>`. BI/BY/H/W -> one of I1..I4 (right-justified,
zero-filled upper bits for BI/BY/H); F/D -> a floating register.
RESULT/side-effects: `source -> Rn`.
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if source==0 | CLEARED | CLEARED | SET if source.signbit |
(`ST,SAVA` on the `ALU,A` load; manual 10.1 lists Z,S only.)
TRAP conditions: addressing traps (manual 10.1). Descriptor operands may set K.
CITATION: microcode LOADBI=000203, LOADT=000206, LOADD=000207; Manual sec 10.1 (Load register).

## AssignFrom (tn =:) - store register to destination

Name/opcode(octal), n=1..4: `BIn=: 176014B+` (0xFC0C+), `BYn=: 34B+` (0x1C+),
`Hn=: 176020B+` (0xFC10+), `Wn=: 40B+` (0x20+), `Fn=: 44B+` (0x24+), `Dn=: 50B+` (0x28+).

FUNCTIONAL PSEUDOCODE (traced STORE 000220; STOREBI 000215; STORED 000221):
```
; W / H / BY (STORE, single cell)
1. dest <- Rn            ; ALU,A A=Rn(REG37) D=<dest-op> WRITE ST,SAVA ; G,OOPS
; BI (STOREBI 000215->000216->STORE_BI_0/1 003160/003170): read the destination word, insert
;   the register's low bit at the addressed position, WRITE back.
; D (STORED): store two words.
```
OPERANDS + datatypes: `<dest/w/t>`. Only the datatype-sized low bits of Rn are stored;
source register unaffected. If destination is a register and t is BI/BY/H, the upper part is
zero-filled (same effect as a load). Constant operand illegal.
RESULT/side-effects: `datatype-part(Rn) -> dest`.
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if stored value==0 | CLEARED | CLEARED | SET if stored value.signbit |
(`ST,SAVA`; manual 10.4 lists Z,S of the datatype-dependent part only.)
TRAP conditions: addressing traps (manual 10.4). Descriptor operands may set K.
CITATION: microcode STOREBI=000215, STORE=000220, STORED=000221 (+ STORE_BI_0=003160,
STORE_BI_1=003161); Manual sec 10.4 (Store).

---

# GROUP C - Base (B) and record (R) base registers

## AssignBaseRegTo (b :=) - load local base register

Name/opcode(octal): `B:= 176010B` (0xFC08). Operand `<source/r/W>`.

FUNCTIONAL PSEUDOCODE (traced LOADB 000213 -> LOAD_L 003227 -> DGET_NEXT):
```
1. B <- source           ; ALU,A A=<source-op> D=DAC,REG04 (base transfer) READ ST,SAVA
2. continue via LOAD_L/DGET_NEXT to fetch next instruction.
```
OPERANDS + datatypes: `<source/r/W>` (Word). RESULT: `source -> B`.
STATUS FLAGS: K UNCHANGED; Z SET if source==0; C CLEARED; O CLEARED; S SET if source.signbit.
(`ST,SAVA`; manual 10.2 lists Z,S.)
TRAP conditions: addressing traps (manual 10.2). CITATION: microcode LOADB=000213; Manual 10.2.

## AssignRecordRegTo (r :=) - load record register

Name/opcode(octal): `R:= 30B` (0x18). Operand `<source/r/W>`.

FUNCTIONAL PSEUDOCODE (traced LOADR 000211):
```
1. R <- source           ; ALU,A A=<source-op> D=DAC,LDRES (record load result) READ ST,SAVA ; G,OOPS
```
OPERANDS: `<source/r/W>`. RESULT: `source -> R`.
STATUS FLAGS: K UNCHANGED; Z SET if source==0; C CLEARED; O CLEARED; S SET if source.signbit.
TRAP conditions: addressing traps (manual 10.3). CITATION: microcode LOADR=000211; Manual 10.3.

## AssignToBaseReg (b =:) - store local base register

Name/opcode(octal): `B=: 176012B` (0xFC0A). Operand `<operand/w/W>`.

FUNCTIONAL PSEUDOCODE (traced STOREB 000225 -> 000226):
```
1. compute dest address   ; ALU,FZRO ... ADACT (AB=11 = previous addr +4)
2. dest <- B              ; ALU,A A=DAC,B D=<dest-op> WRITE ST,SAVA C,MEMOT ; -> GET_NEXT
```
OPERANDS: `<operand/w/W>`. RESULT: `B -> operand`.
STATUS FLAGS: K UNCHANGED; Z SET if B==0; C CLEARED; O CLEARED; S SET if B.signbit.
TRAP conditions: addressing traps (manual 10.5). CITATION: microcode STOREB=000225; Manual 10.5.

## AssignToRecordReg (r =:) - store record register

Name/opcode(octal): `R=: 176011B` (0xFC09). Operand `<operand/w/W>`.

FUNCTIONAL PSEUDOCODE (traced STORER 000223 -> 000224):
```
1. compute dest address   ; ALU,XOR (=0) ADACT (AB=11)
2. dest <- R              ; ALU,A A=DAC,XFER (record transfer) D=<dest-op> WRITE ST,SAVA ; -> STOREB
```
OPERANDS: `<operand/w/W>`. RESULT: `R -> operand`.
STATUS FLAGS: K UNCHANGED; Z SET if R==0; C CLEARED; O CLEARED; S SET if R.signbit.
TRAP conditions: addressing traps (manual 10.6). CITATION: microcode STORER=000223; Manual 10.6.

---

# GROUP D - Floating registers A1-A4 and E1-E4

Each of these is a SINGLE micro-cell with `G,OOPS` (self-contained). The `ADDR=` chaining
`LOADA1->LOADA2->...->STOREE4` in the listing is physical layout only; at run time each cell
is reached independently by opcode dispatch and completes immediately.

## A1Set..A4Set (an :=) / E1Set..E4Set (en :=) - load floating register from source

Name/opcode(octal): `a1:= 177060B`(0xFE30) a2:=(0xFE31) a3:=(0xFE32) a4:=(0xFE33);
`e1:= 177064B`(0xFE34) e2:=(0xFE35) e3:=(0xFE36) e4:=(0xFE37).

FUNCTIONAL PSEUDOCODE (traced LOADA1..LOADA4=001160-001163, LOADE1..LOADE4=001164-001167):
```
1. An <- source          ; ALU,A A=<source-op> D=An  READ ST,SAVA ; G,OOPS   (An in {A1..A4})
   En <- source          ; ALU,A A=<source-op> D=En  READ ST,SAVA ; G,OOPS   (En in {E1..E4})
```
Note: `En:=` in this emulator writes only the low 32 bits of the 64-bit E register (upper 32
bits preserved) - the micro-cell's `D,En` latches a 32-bit word; the high half is not touched.
OPERANDS: `<source/r/t>`. RESULT: `source -> An` (or E-low).
STATUS FLAGS: K UNCHANGED; Z SET if value==0; C CLEARED; O CLEARED; S SET if value bit31.
(`ST,SAVA` computes Z/S on the raw 32-bit pattern - integer sign/zero, not float semantics.)
TRAP conditions: addressing traps. CITATION: microcode LOADA1=001160 .. LOADE4=001167;
Manual ch.10 (register load).

## A1Get..A4Get (an =:) / E1Get..E4Get (en =:) - store floating register to destination

Name/opcode(octal): `a1=: 177070B`(0xFE38) a2=:(0xFE39) a3=:(0xFE3A) a4=:(0xFE3B);
`e1=: 177074B`(0xFE3C) e2=:(0xFE3D) e3=:(0xFE3E) e4=:(0xFE3F).

FUNCTIONAL PSEUDOCODE (traced STOREA1..STOREA4=001170-001173, STOREE1..STOREE4=001174-001177):
```
1. dest <- An            ; ALU,A A=An D=<dest-op> WRITE ST,SAVA ; G,OOPS   (An in {A1..A4})
   dest <- En            ; ALU,A A=En D=<dest-op> WRITE ST,SAVA ; G,OOPS   (En = E-low 32 bits)
```
OPERANDS: `<dest/w/t>`. RESULT: `An -> dest` (En stores the low 32 bits).
STATUS FLAGS: K UNCHANGED; Z SET if value==0; C CLEARED; O CLEARED; S SET if value bit31.
TRAP conditions: addressing traps. CITATION: microcode STOREA1=001170 .. STOREE4=001177;
Manual ch.10 (register store).

---

# GROUP E - Special / system registers (Reference Manual 16.7 / 16.8)

These are the "Load special register" (`reg :=`) and "Store special register" (`reg =:`)
groups. The LOAD (`:=`) handlers read the source operand into a scratch register with
`ST,SAVA`, then move it into the target internal register (IAC/IDU/DMM domain register),
often maintaining paired state (e.g. LL/HL) and updating the Domain Information Table via the
`CED_TO_DIT` continuation. The STORE (`=:`) handlers compute the destination address
(`EA3SAVE`) then write the internal register out; most terminate through `READ_RFEND`
(`ALU,OR A=SC5 B=SC5 D=<dest> ST,SAVA WRITE ; G,OOPS`), which is where the Z/S/C/O of the
stored value are produced.

Common flag result for this whole group (from the traced `ST,SAVA` cells + manual 16.7/16.8):
- K UNCHANGED; Z SET if value==0; C CLEARED; O CLEARED; S SET if value.signbit.
Exceptions are called out per instruction.

Deep note (not fully traced): the exact Domain-Information-Table / limit-register maintenance
performed by the `CED_TO_DIT` and `READ_RFEND` continuations (for HL/LL/TOS/THA/OTE loads and
stores) is a system-level side effect beyond the flag-relevant data path and is
`UNKNOWN (needs deeper microtrace)` at the DIT-update level.

## LSet (l :=) / LGet (l =:) - link register

Opcode(octal): `l:= 176473B` (0xFD3B); `l=: 176700B` (0xFDC0).
PSEUDOCODE: set = LOAL 001002 `A=source D=IAC,L READ ST,SAVA` -> LOAD_L; get = STORL 001077->
001100 `A=IAC,L D=<dest> WRITE ST,SAVA` -> DGET_NEXT.
OPERANDS: `<operand/rw/W>`. RESULT: `source -> L` / `L -> operand`.
FLAGS: K UNCHANGED; Z if value==0; C CLEARED; O CLEARED; S if value.signbit.
TRAP: addressing traps (manual 16.7/16.8). CITATION: LOAL=001002, STORL=001077; Manual 16.7/16.8.

## HlSet (hl :=) / HlGet (hl =:) - upper-limit register

Opcode(octal): `hl:= 176667B` (0xFDB7); `hl=: 176701B` (0xFDC1).
PSEUDOCODE: set = LOAHL 001004-001006 reads source to SC5 (`ST,SAVA`), copies IDU,LL->SC6 and
SC5->IDU,HL (maintains the LL/HL pair) -> LOAD_LL_HL. get = STORHL 001101-001103 -> STOR_HL
-> CED_TO_DIT / READ_RFEND (writes HL to dest with `ST,SAVA`).
OPERANDS: `<operand/rw/W>`. RESULT: `source -> HL` / `HL -> operand`.
FLAGS: K UNCHANGED; Z if value==0; C CLEARED; O CLEARED; S if value.signbit.
TRAP: addressing traps. CITATION: LOAHL=001004, STORHL=001101, LOAD_LL_HL=011060, STOR_HL=011253;
Manual 16.7/16.8.

## LlSet (ll :=) / LlGet (ll =:) - lower-limit register

Opcode(octal): `ll:= 176670B` (0xFDB8); `ll=: 176702B` (0xFDC2).
PSEUDOCODE: set = LOALL 001007-001011 reads source to SC6 (`ST,SAVA`), copies IDU,HL->SC5 and
SC6->IDU,LL -> LOAD_LL_HL. get = STORLL 001104-001106 -> STOR_LL -> CED_TO_DIT/READ_RFEND.
OPERANDS: `<operand/rw/W>`. RESULT: `source -> LL` / `LL -> operand`.
FLAGS: K UNCHANGED; Z if value==0; C CLEARED; O CLEARED; S if value.signbit.
TRAP: addressing traps. CITATION: LOALL=001007, STORLL=001104, STOR_LL=011251; Manual 16.7/16.8.

## St1Set (st1 :=) / St1Get (st1 =:) - status register 1

Opcode(octal): `st1:= 176671B` (0xFDB9); `st1=: 176703B` (0xFDC3).
PSEUDOCODE: set = LOAST1 001012-001013 reads source to SC5 (NO `ST,SAVA`), then routes the
value through `ALU,STS` into LOAD_ST1 (011066: `AND` with mask 07773777740 - masks off the
non-loadable bits) -> the data status bits ARE loaded from the operand. get = STORST1
001107-001112 -> READST1 (014033: `AND` with mask 00200177740) -> FREEB; it does NOT pass the
value through a data-status `ST,SAVA` write.
OPERANDS: `<operand/rw/W>`.
FLAGS (st1 := load): the operand's bit pattern is loaded into the status register (Z/S per
operand per manual 16.7), but bits modified after each instruction (ranges 17-25, 27, 28) are
cleared before the next instruction regardless (manual note). K comes from the loaded operand.
FLAGS (st1 =: store): **data status bits UNAFFECTED** (manual 16.8 explicit: "The instruction
ST1=: does not affect the data status bits."). NOTE: the emulator's `St1Get.c` currently sets
Z/S from the value - that DISAGREES with the manual and the microcode (which routes through
READST1, not READ_RFEND). Flag row for st1 =::
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |
TRAP: addressing traps (16.7 adds Illegal operand value on non-loadable bits for some regs).
CITATION: LOAST1=001012, LOAD_ST1=011066, STORST1=001107, READST1=014033; Manual 16.7/16.8.

## Ote1Set/Ote2Set (ote :=) / Ote1Get/Ote2Get (ote =:) - own trap enable registers

Opcode(octal): `ote1:= 176673B`(0xFDBB) `ote2:= 176674B`(0xFDBC); `ote1=: 176705B`(0xFDC5)
`ote2=: 176706B`(0xFDC6).
PSEUDOCODE: set = LOATE1 001014-001015 / LOATE2 001016-001017 read source to SC5 (`ST,SAVA`)
-> LOAD_TE1 (011150) -> CED_TO_DIT. get = STORTE1 001123-001124 / STORTE2 001125-001126 ->
STOR_OTE1/STOR_OTE2 (011237/011241) -> CED_TO_DIT/READ_RFEND.
OPERANDS: `<operand/rw/W>`.
LOAD-SPECIFIC RULE (manual 16.7): on `OTE :=` the operand is compared against the trap-enable
modify mask (TEMM) in the domain-description table; only bits whose mask bit is set are
modifiable. Attempting to modify a non-modifiable bit raises Illegal Operand Value (IOV).
FLAGS: K UNCHANGED; Z if value==0; C CLEARED; O CLEARED; S if value.signbit.
TRAP: addressing traps; Illegal operand value (IOV) on `ote :=` (manual 16.7).
CITATION: LOATE1=001014, LOATE2=001016, STORTE1=001123, STORTE2=001125, LOAD_TE1=011150,
STOR_OTE1=011237; Manual 16.7/16.8. (Emulator `Ote1Set.c/Ote2Set.c` implement the TEMM gate.)

## TosSet (tos :=) / TosGet (tos =:) - top of stack register

Opcode(octal): `tos:= 176675B` (0xFDBD); `tos=: 176711B` (0xFDC9).
PSEUDOCODE: set = LOATOS 001020-001021 reads source to SC5 (`ST,SAVA`) -> LOAD_TOS (011204,
pushes to sequencer / CED_TO_DIT). get = STORTOS 001133-001134 -> READ_RFEND (writes TOS with
`ST,SAVA`).
OPERANDS: `<operand/rw/W>`. RESULT: `source -> TOS` / `TOS -> operand`.
FLAGS: K UNCHANGED; Z if value==0; C CLEARED; O CLEARED; S if value.signbit.
TRAP: addressing traps. CITATION: LOATOS=001020, STORTOS=001133, LOAD_TOS=011204; Manual 16.7/16.8.

## ThaSet (tha :=) / ThaGet (tha =:) - trap handler address register

Opcode(octal): `tha:= 176712B` (0xFDCA); `tha=: 176713B` (0xFDCB).
PSEUDOCODE: set = LOATHA 001022-001023 reads source to SC5 (`ST,SAVA`) -> LOAD_THA (011224) ->
CED_TO_DIT. get = STORTHA 001135-001136 -> STOR_THA -> READ_RFEND (writes THA with `ST,SAVA`).
OPERANDS: `<operand/rw/W>`. RESULT: `source -> THA` / `THA -> operand`.
FLAGS: K UNCHANGED; Z if value==0; C CLEARED; O CLEARED; S if value.signbit.
TRAP: addressing traps. CITATION: LOATHA=001022, STORTHA=001135, LOAD_THA=011224; Manual 16.7/16.8.

## CadSet (cad :=) / CadGet (cad =:) - current alternative domain

Opcode(octal): `cad:= 176672B` (0xFDBA, manual ch.16.33 CAD '87 extension); `cad=: 177125B`
(0xFE55).
PSEUDOCODE: set = LOACAD 001024-001025 reads source to SC3, writes low byte to `MM,ADOM`
(alternative-domain register) -> LOADCAD1 (004610) which installs the CAD/ADOM and loads the
capability. get = STORCAD 001144-001145 reads `SRF15` (the CAD image) with `ST,SAVA` ->
READ_RFEND (writes it to dest).
OPERANDS: `<operand/rw/W>`.
FLAGS (cad =: store): K UNCHANGED; Z if value==0; C CLEARED; O CLEARED; S if value.signbit
(`ST,SAVA` at STORCAD 001145 / READ_RFEND).
FLAGS (cad := load): LOACAD's traced cells carry NO `ST,SAVA`; the flag effect of `cad :=`
after the domain install (LOADCAD1) is `UNKNOWN (needs deeper microtrace)` - manual 16.33 is
the '87-extension CAD load and its documented flag effect was not captured in this trace.
TRAP: addressing traps (manual 16.8/16.33). CITATION: LOACAD=001024, LOADCAD1=004610,
STORCAD=001144; Manual 16.8 / 16.33.

## CedGet (ced =:) - current executing domain (store only)

Opcode(octal): `ced=: 177124B` (0xFE54). (No `ced :=` in this category; CED is loaded only by
the domain-call mechanism.)
PSEUDOCODE: STORCED 001142-001143 reads `SRF14` (the CED image) with `ST,SAVA` -> READ_RFEND
(writes to dest).
OPERANDS: `<operand/w/W>`. RESULT: `CED -> operand`.
FLAGS: K UNCHANGED; Z if CED==0; C CLEARED; O CLEARED; S if CED.signbit.
TRAP: addressing traps. CITATION: STORCED=001142; Manual 16.8.

## Cte1Get/Cte2Get (cte =:) - child trap enable registers (store only)

Opcode(octal): `cte1=: 177120B` (0xFE50); `cte2=: 177121B` (0xFE51).
PSEUDOCODE: STORCTE1 001113-001114 / STORCTE2 001115-001116 compute dest (`EA3SAVE`) ->
STOR_CTE1/STOR_CTE2 (011227/...) -> CED_TO_DIT/READ_RFEND (write value with `ST,SAVA`).
OPERANDS: `<operand/w/W>`. RESULT: `CTEn -> operand`.
FLAGS: K UNCHANGED; Z if value==0; C CLEARED; O CLEARED; S if value.signbit.
TRAP: addressing traps. CITATION: STORCTE1=001113, STORCTE2=001115; Manual 16.8.

## Mte1Get/Mte2Get (mte =:) - mother trap enable registers (store only)

Opcode(octal): `mte1=: 176560B` (0xFD70); `mte2=: 176561B` (0xFD71).
PSEUDOCODE: STORMTE1 001127-001130 / STORMTE2 001131-001132 -> STOR_MTE1/STOR_MTE2 ->
CED_TO_DIT/READ_RFEND (write value with `ST,SAVA`).
OPERANDS: `<operand/w/W>`. RESULT: `MTEn -> operand`.
FLAGS: K UNCHANGED; Z if value==0; C CLEARED; O CLEARED; S if value.signbit.
TRAP: addressing traps. CITATION: STORMTE1=001127, STORMTE2=001131; Manual 16.8.
(NOTE: manual 16.8 prints the MTE2 hex as 0FD71H, same as MTE1 - an apparent manual typo;
the emulator uses 0xFD70/0xFD71 for MTE1/MTE2.)

## Temm1Get/Temm2Get (temm =:) - trap enable modification masks (store only)

Opcode(octal): `temm1=: 177122B` (0xFE52); `temm2=: 177123B` (0xFE53).
PSEUDOCODE: STORTEM1 001117-001120 / STORTEM2 001121-001122 -> STOR_TEM1/STOR_TEM2 ->
CED_TO_DIT/READ_RFEND (write value with `ST,SAVA`).
OPERANDS: `<operand/w/W>`. RESULT: `TEMMn -> operand`.
FLAGS: K UNCHANGED; Z if value==0; C CLEARED; O CLEARED; S if value.signbit.
TRAP: addressing traps. CITATION: STORTEM1=001117, STORTEM2=001121; Manual 16.8.

## PGet (p =:) - store program counter (store only)

Opcode(octal): `p=: 176542B` (0xFD62).
PSEUDOCODE: STORP 001137-001141 computes dest, reads `IAC,NPC` into SC5 with `ST,SAVA` ->
READ_RFEND (writes it to dest). Manual 16.8: the value stored is the address of the `P=:`
instruction itself.
OPERANDS: `<operand/w/W>`. RESULT: `address(P=: instr) -> operand`.
FLAGS: K UNCHANGED; Z if value==0; C CLEARED; O CLEARED; S if value.signbit.
TRAP: addressing traps. CITATION: STORP=001137; Manual 16.8.

## PsGet (ps =:) - store process segment (store only)

Opcode(octal): `ps=: 177174B` (0xFE7C).
PSEUDOCODE: STORPS 001152-... computes dest (`EA3SAVE`) then writes the PS register value out
(continuation into the STOR_* / READ_RFEND family with `ST,SAVA`).
OPERANDS: `<operand/w/W>`. RESULT: `PS -> operand`.
FLAGS: K UNCHANGED; Z if PS==0; C CLEARED; O CLEARED; S if PS.signbit.
TRAP: addressing traps. CITATION: STORPS=001152; Manual 16.8.

## PsSet (ps :=) - load process segment  --  UNRESOLVED

Opcode(octal): emulator `PsSet.c` uses `ps:= 177504B` (0xFF44) and simply writes `cpu->PS`.
STATUS: `UNKNOWN (needs deeper microtrace)`.
- Reference Manual 16.7 (Load special register) does NOT list `PS:=`.
- No dedicated `LOAPS` micro-handler was located in `MICRO-5800-A30.md` (only the store path
  STORPS=001152 and the domain-machinery cells that write `D,MM,PS` during segment/domain
  setup at 014057/016747/017012 exist).
What is missing: the opcode-to-microaddress dispatch entry for 0xFF44 and its micro-routine.
Until that is traced, the exact operation, the modifiable-bit rules, the trap conditions, and
the flag effects of `ps :=` cannot be stated from ground truth.

---

## Summary of unresolved items

1. `PsSet` (`ps :=`, 0xFF44): no microcode load-PS handler located and not documented in
   Manual 16.7 - operation and flags UNKNOWN.
2. `CadSet` (`cad :=`, 0xFDBA): flag effect after the LOADCAD1 domain-install path not
   captured (traced LOACAD cells carry no `ST,SAVA`).
3. Deep DIT / limit-register maintenance in the `CED_TO_DIT` and `READ_RFEND` continuations
   (for HL/LL/TOS/THA/OTE load & store) - the system-table side effects below the
   flag-relevant data path are not fully traced.
4. `St1Get` (`st1 =:`): microcode routes through READST1 (mask, no data-status write) and the
   manual states it does NOT affect data status bits - the emulator's `St1Get.c` sets Z/S,
   which disagrees; flagged for correction, not a microtrace gap.
