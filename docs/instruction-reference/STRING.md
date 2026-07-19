# ND-500 STRING Instruction Category - Functional Behavior Reference

Traced from microcode ground-truth, cross-checked against the documented manual.

Sources (full absolute paths):
- Microcode:    /mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md
- Field decode: /mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md
- Manual:       /home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md  (Chapter 14, plus 16.36)
- C impls:      /home/ronny/repos/nd500x/src/cpu/instructions/STRING/*.c

This file OVERWRITES a prior flags-only pass (recoverable from git).

--------------------------------------------------------------------------------
## 0. How to read this document / shared mechanisms

Every STRING routine shares a common skeleton. Understanding it once removes the
need to repeat it per instruction.

### 0.1 Field decode legend (from mnemonics.md)
- `ALU,<f>`  ALU function: `A` = pass A, `XOR`/`AND`/`OR`/`ANDCA`/`ANDCB`,
  `A-B`, `B-A`, `A+B`, `A-1`, `FZRO` = force 0, `ADIRC` = A complemented.
- `A,<src>`/`B,<src>`  ALU input buses. `BM00` = constant 0 (bit-mask 0).
  `X1`,`X2` = the two hardware index/counter registers driving the string loop
  (derived from user registers I1 source, I2 dest). `SCn` = scratch cell n.
  `DATA` = memory read data bus. `DAC,EAx` = data effective-address x.
  `ALU,REG37` = descriptor/operand register. `MARG=nnn` = literal mask.
- `D,<dest>`  result destination (register/scratch/`LC` loop-counter/`X1`/`X2`).
- `READ`/`WRITE`/`DAC`  memory access this microcycle. `AD_ARTI=1` drives the
  arithmetic address adder; `IX*2`/`IX*4`/`IX*8`/`IX/8` scale the index by the
  element size (H/W/D/bit).
- `K,ONE` set K=1; `K,ZRO` clear K=0; `K,1IFZ` set K=1 iff the ALU result is 0;
  `(hold)` leave K unchanged.
- `ST,SAVA` = SAVE STATUS FROM ALU OPERATION: writes the data-status arithmetic
  bits (Z, C, O, S) from this cycle's ALU result. `ST,LOAD` loads a status word.
- `COND,MZRO`/`MCRY`/`MSGN`/`MSORZ`/`MCNZ` = branch on the ALU's Z / C / S /
  (S OR Z) / (C AND NOT Z). `COND,PARITY` = parity of the low byte of the F-bus.
  `C,SEQ ... INVSEQ` = sequencer test with inverted sense.
- `TYP,BI/BY/HW/DR/F/DF` = element datatype for this cycle (bit / byte / halfword
  / 32-bit integer / single float / double float).
- `ORCON`, `TESTOBJ=46` = operand-descriptor dispatch / validity check.

### 0.2 Common prologue (all string ops)
Entry microcell(s) read the operand descriptor(s) (`READ ORCON=04`), latch the
element count and base into `X1`/`X2`, and validate the operand
(`TESTOBJ=46`). Instructions taking extra scalar operands (mask / test /
translate-table / pad / mode) call `DIS_IDESC` first (see 0.4).

### 0.3 SET_PD / RESET_PD  -  restartable "part-done" marking
`SET_PD` (octal 005240): `OR MIC,STS bit -> MIC,STS`  sets the micro-status
PART-DONE bit so a long string operation can be interrupted and resumed with
I1/I2 already advanced. `RESET_PD` (005247): clears PART-DONE, clears the
descriptor auto-increment flag (`SPEC,MOD` bit `BM04`), reloads L, and returns
to the instruction stream (`LOAD_L`). These are internal; they are NOT
user-visible status flags.

### 0.4 DIS_IDESC (005243) - suppress descriptor auto-increment
Sets `SPEC,MOD` bit so the immediately-following scalar-operand fetch does not
auto-increment the string descriptor, then `RETURN,T,POP`. Used to read
mask/test/table/pad/mode without disturbing I1/I2.

### 0.5 SOUR_RANGE / DEST_RANGE -> TRP_DR  (descriptor-range trap)
`SOUR_RANGE` (003117) and `DEST_RANGE` (003122) both set the descriptor-range
(DR) trap-condition bit (`A,BM31 -> SC12`) and fall into `TRP_DR` (003105):
`AND ALU,TE (trap-enable) with the DR bit; OR into data-status (ST,LOAD)`. If
the DR trap is enabled the microcode delivers `DEL_TRAP`; otherwise it only
records the condition in data-status and returns. SOUR_RANGE marks the
event with `K,ZRO` (source outside -> K=0); DEST_RANGE with `K,ONE`
(dest outside -> K=1). This is the mechanism behind every "DR trap condition"
row in the manual tables.

### 0.6 MOVE_BYTES (004702) - overlap-safe block copy
A shared subroutine that copies a run of element bytes either forward
(`MB_R_FORWARD`) or reverse (`MB_R_REVERSE`), the direction chosen from the
source/dest overlap test (`COND,MSORZ`). This is what implements the manual's
"Overlap is taken care of" for SMOVE / SMOVN / SMVTR.

### 0.7 Traps
- `ILL_OP_SPEC` (003136): sets IDU-status bit `BM02` = illegal operand specifier
  (e.g. a translate-table operand that is not a valid data descriptor).
- `SET_IOV` (003132): sets `BM20` = illegal-operand-value / overflow status
  (used by SSPAR / SCHPAR when `<mode>` is not 0..3).
- `TRP_DR` descriptor-range trap, per 0.5.

### 0.8 Flag-resolution rule
Where `ST,SAVA` executes in the terminating microcell, the arithmetic bits it
lists (Z, C, O, S) ARE written from that ALU result. Where a bit is not touched
by the routine and not documented by the manual, it is CLEARED per ND-500
Reference Manual data-status rule (unmentioned data-status bits are cleared).
K is written explicitly by `K,ONE`/`K,ZRO`/`K,1IFZ` at the terminating cell.

### 0.9 Opcode note (verify-before-trust)
The manual (authority) octal/hex codes are used below. Several C-file header
comments in `src/cpu/instructions/STRING/*.c` carry a DIFFERENT hex value
(e.g. `Smove.c` says 0xFD70..0xFD75; manual says 0FD66H..0FD6BH). The microcode
lists micro-addresses, not opcodes, so the opcode map cannot be confirmed from
it. Treat the manual codes as correct and the divergent C comments as suspect.

--------------------------------------------------------------------------------
## 1. SMOVE  -  String Move            (file Smove.c)

Opcode (octal, manual 14.2): BI 176546B, BY 176547B, H 176550B, W 176551B,
F 176552B, D 176553B  (hex 0FD66H..0FD6BH).
Microcode entry: BI 001235 SMOVEBI -> 005263 SMOVEBI_F01; BY 001237/005366
SMOVEBY_CONT; HW 001241/005411; W 001243/005441; D 001245/005473.

### Functional pseudocode
```
i1 = I1 (source index);  i2 = I2 (dest index)
read source and dest descriptors; validate
while not end-of-source and not end-of-dest:
    if source pointer outside its string:  K=0; SOUR_RANGE (DR trap); I1,I2 unchanged; stop
    if dest   pointer outside its string:  K=1; DEST_RANGE (DR trap); I1,I2 unchanged; stop
    determine overlap direction (SET_PD marks restartable)
    MOVE_BYTES: copy element(s) S(I1) -> D(I2)   ; element size = TYP (BI/BY/HW/W/DF)
    I1 += 1 ; I2 += 1
end
RESET_PD
if source exhausted:  K=0   ; I1,I2 point at next element
if dest   full:       K=1   ; I1,I2 point at next element
```
### Operands / datatypes
- `<source /r/t/I1>`  read, element type t, indexed by I1.
- `<dest /w/t/I2>`    write, element type t, indexed by I2.
- t in {BI,BY,H,W,F,D}. Overlap handled (MOVE_BYTES).

### Result / side-effects
Copies min(remaining-source, remaining-dest) elements; advances I1,I2.

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CONDITIONAL (0 source side / 1 dest side) | CLEARED | CLEARED | CLEARED | CLEARED |
K=0 when source empty or source-outside; K=1 when dest full or dest-outside.
Manual documents only K; Z/S/C/O not documented -> cleared (rule, 0.8).

### Traps
Descriptor-range trap when either pointer is outside its string (0.5).

### Citation
Microcode SMOVEBI 001235 / SMOVEBI_F01 005263 (SOUR_RANGE 005273, DEST_RANGE
005275, END_OVERLAP/RESET_PD 005342/005247, MOVE_BYTES 004702). Manual 14.2.

--------------------------------------------------------------------------------
## 2. SMVWH  -  String Move While       (file Smvwh.c)

Opcode: BY 176562B (0FD72H). Manual 14.3.
Microcode: 001247 SMVWH -> 006171 SMVWHBY_F01.

### Functional pseudocode
```
read source,dest descriptors; DIS_IDESC then read <mask>(SC13), <test>(SC12)
while not end-of-source and not end-of-dest:
    if source outside: K=0,Z=0; SOUR_RANGE; I1,I2 unchanged; stop
    if dest   outside: K=1,Z=0; DEST_RANGE; I1,I2 unchanged; stop
    b = S(I1)
    if (b AND mask) != test:            # while-condition broken
        Z=1; I1,I2 :- differing byte; RESET_PD; stop      # SMVWHBY_F13 not taken
    D(I2) = b ; I1+=1 ; I2+=1
end
if source empty: K=0,Z=1 ; I1,I2 :- next element
if dest   full:  K=1,Z=1 ; I1,I2 :- next element
```
Continue-while-equal: the byte is moved, THEN loop guard `(b AND mask)==test`
is re-checked (microcode 006214-006223: `AND SC7,SC13 -> SC11`, `XOR SC11,SC12`,
branch `COND,MZRO`). Overlap NOT handled (manual).

### Operands
`<source/r/BY/I1>`, `<dest/w/BY/I2>`, `<mask/r/BY>`, `<test/r/BY>`. Bytes only.

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CONDITIONAL (0/1 as above) | CONDITIONAL | CLEARED | CLEARED | CLEARED |
K=0 & Z=0 outside-source; K=1 & Z=0 outside-dest; K=0 & Z=1 different-byte or
source-empty; K=1 & Z=1 dest-full (per manual 14.3 table).

### Traps
DR trap when a pointer is outside its string.

### Citation
Microcode SMVWHBY_F01 006171 (guard 006214-006223, SOUR/DEST 006201/006203).
Manual 14.3.

--------------------------------------------------------------------------------
## 3. SMVUN  -  String Move Until        (file Smvun.c)

Opcode: BY 176563B (0FD73H). Manual 14.4.
Microcode: 001251 SMVUN -> 006224 SMVUNBY_F01.

### Functional pseudocode
```
read source,dest; DIS_IDESC; read <mask>(SC13), <test>(SC12)
while not end-of-source and not end-of-dest:
    if source outside: K=0,Z=0; SOUR_RANGE; stop
    if dest   outside: K=1,Z=0; DEST_RANGE; stop
    b = S(I1)
    if (b AND mask) == test:     # until-condition met, byte NOT moved
        Z=1; I1,I2 :- found byte in source; RESET_PD; stop
    D(I2)=b ; I1+=1 ; I2+=1
end
if source empty: K=0,Z=0 ; I1,I2 :- next element
if dest   full:  K=1,Z=0 ; I1,I2 :- next element
```
The until-byte is examined before moving (006247-006252 same AND/XOR guard as
SMVWH but `INVSEQ`). Byte satisfying the condition is NOT moved. Overlap not
handled.

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CONDITIONAL | CONDITIONAL | CLEARED | CLEARED | CLEARED |
byte-found K=0 Z=1; source-empty K=0 Z=0; dest-full K=1 Z=0; outside-source
K=0 Z=0; outside-dest K=1 Z=0 (manual 14.4).

### Traps  DR trap on out-of-range pointer.
### Citation  Microcode SMVUNBY_F01 006224. Manual 14.4.

--------------------------------------------------------------------------------
## 4. SMVTR  -  String Move Translated    (file Smvtr.c)

Opcode: BY 176564B (0FD74H). Manual 14.5.
Microcode: 001253 SMVTR -> 006257 SMVTRBY_F01.

### Functional pseudocode
```
read source,dest; DIS_IDESC; read <trans-table> base (EAO/SC13)
if <trans-table> operand is not a valid data descriptor: ILL_OP_SPEC trap
determine overlap direction (SET_PD)
while not end-of-source and not end-of-dest:
    if source outside: K=0; SOUR_RANGE; stop
    if dest   outside: K=1; DEST_RANGE; stop
    b  = S(I1)
    tb = table[b]                 # DAC,DPA indexed load, 006324/006336
    D(I2) = tb ; I1+=1 ; I2+=1    # MOVE via forward (F12) / reverse (O12) copy
end
RESET_PD
if source empty: K=0 ; I1,I2 :- next element
if dest   full:  K=1 ; I1,I2 :- next element
```
Byte is translated through the table before storing. Overlap handled (separate
forward SMVTRBY_F10.. and reverse SMVTRBY_O10.. copy loops).

### Operands  `<source/r/BY/I1>`, `<dest/w/BY/I2>`, `<trans table/aa/BY>`.

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CONDITIONAL (0 source / 1 dest) | CLEARED | CLEARED | CLEARED | CLEARED |
Manual documents only K (0.8 for the rest).

### Traps  DR trap (out-of-range); ILL_OP_SPEC (bad table operand, 006264).
### Citation  Microcode SMVTRBY_F01 006257 (ILL_OP_SPEC 006264, translate load
006324/006336, DUMMY_2 push). Manual 14.5.

--------------------------------------------------------------------------------
## 5. SMVTU  -  String Move Translated Until   (file Smvtu.c)

Opcode: BY 176565B (0FD75H). Manual 14.6.
Microcode: 001255 SMVTU -> 006344 SMVTUBY_F01.

### Functional pseudocode
```
read source,dest; DIS_IDESC; read <trans-table>
if table operand invalid: ILL_OP_SPEC trap
SET_PD
while not end-of-source and not end-of-dest:
    if source outside: K=0,Z=0; SOUR_RANGE; stop
    if dest   outside: K=1,Z=0; DEST_RANGE; stop
    tb = table[S(I1)]                       # 006366/006367 DAC,DPA
    if tb == ASCII ESC (033B / 0x1B):       # 006375 XOR with ORCON=0x1B, MZRO
        K=0,Z=1; I1,I2 :- position of escape; RESET_PD; stop   # escape not moved
    if tb != 0:                             # 006377 skip zero bytes
        D(I2) = tb ; I2 += 1
    I1 += 1                                  # I1 advances even for skipped zero
end
if source empty: K=0,Z=0 ; I1,I2 :- next element
if dest   full:  K=1,Z=0 ; I1,I2 :- next element
```
Zero-translations are dropped (I2 not advanced); the ESCAPE (033B) terminates
and is not moved. Overlap not handled.

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CONDITIONAL | CONDITIONAL | CLEARED | CLEARED | CLEARED |
escape-found K=0 Z=1; source-empty K=0 Z=0; dest-full K=1 Z=0; outside-source
K=0 Z=0; outside-dest K=1 Z=0 (manual 14.6).

### Traps  DR trap; ILL_OP_SPEC (bad table, 006351).
### Citation  Microcode SMVTUBY_F01 006344 (escape test 006375 ORCON=0x1B).
Manual 14.6.

--------------------------------------------------------------------------------
## 6. SMOVN  -  String Move m Elements    (file Smovn.c)

Opcode: BI 176566B, BY 176567B, H 176570B, W 176571B, F 176572B, D 176573B
(0FD76H..0FD7BH). (Manual 14.7 prints "176568B" for BI/BY - octal typo;
sequence is 176566B..176573B.) Manual 14.7.
Microcode: BI 001257/006404 SMOVNBI_F01; BY 001261/006431; HW 001263/006456;
W 001265/006503; D 001267/006530.

### Functional pseudocode
```
read source,dest; DIS_IDESC; read <m>(SC13)  # count, unsigned 32-bit W
i = 0
while not end-of-source and not end-of-dest and i < m:
    if source outside: K=0,Z=0; SOUR_RANGE; stop
    if dest   outside: K=1,Z=0; DEST_RANGE; stop
    MOVE element S(I1) -> D(I2)   # reuses SMOVE copy engine per type
    I1+=1 ; I2+=1 ; i+=1
end
# 006424/006451/... SMOVN..F66: compare i vs m
if i == m (m items moved):  K=0,Z=1 ; I1,I2 :- next element
elif source empty:          K=0,Z=0 ; I1,I2 :- next element
elif dest full:             K=1,Z=0 ; I1,I2 :- next element
```
Same per-type copy paths as SMOVE (branches into SMOVEBI_F06 / SMOVEBY_F06 /
SMOVEHW_F06 / SMOVEWF_F06 / SMOVEDF_F06). Overlap handled.

### Operands  `<source/r/t/I1>`, `<dest/w/t/I2>`, `<m/r/W>` (unsigned count).

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CONDITIONAL | CONDITIONAL | CLEARED | CLEARED | CLEARED |
m-moved K=0 Z=1; source-empty K=0 Z=0; dest-full K=1 Z=0; outside-source K=0
Z=0; outside-dest K=1 Z=0 (manual 14.7).

### Traps  DR trap on out-of-range pointer.
### Citation  Microcode SMOVNBI_F01 006404 (count test SMOVNBI_F66 006424).
Manual 14.7.

--------------------------------------------------------------------------------
## 7. SFILL  -  String Fill               (file Sfill.c)

Opcode (n = 1..4 registers, manual 14.8):
BIn 176574B+(n-1), BYn 176600B+(n-1), Hn 176604B+(n-1), Wn 176610B+(n-1),
Fn 176614B+(n-1), Dn 176620B+(n-1)  (0FD7C..0FD90H bases).
Microcode: BI 001271/005720 SFILL_BI0; BY 001273/005737; H 001275/006014;
W 001277/006030; D 001301/006044.

### Functional pseudocode
```
read dest descriptor; fetch fill value from register tn (n selects reg width)
while not end-of-dest:
    if dest outside: K=1; DEST_RANGE (DR trap); I2 unchanged; stop
    D(I2) = tn ; I2 += 1
end
# byte variant has a word-at-a-time fast path (SFILLBY_W0 005752) that packs the
# fill byte into a 32-bit pattern and writes whole words, plus head/tail byte
# loops (SFILLBY_ALIGN / SFILLBY_ELOOP) for alignment.
K=1 ; I2 :- next element   # string filled
```
Fills every remaining element of the destination string with the register
contents. n = number of the source register (BY3 uses byte reg 3, etc.).

### Operands  `<dest/w/t/I2>`; implicit source register `tn`.

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| SET (=1) | CLEARED | CLEARED | CLEARED | CLEARED |
Manual: K=1 both for "string filled" and "outside dest". Z/S/C/O undocumented ->
cleared (0.8).

### Traps  DR trap (dest outside string).
### Citation  Microcode SFILL_BI0 005720 / SFILL_BY0 005737 (word fast-path
SFILLBY_W0 005752, DEST_RANGE 005724). Manual 14.8.

--------------------------------------------------------------------------------
## 8. SFILLN  -  String Fill n Elements    (file Sfilln.c)

Opcode (manual 14.9): BIn 176624B+(n-1), BYn 176630B+, Hn 176634B+, Wn 176640B+,
Fn 176644B+, Dn 176650B+ (0FD94H..0FDA8H bases).
Microcode: BI 001303/006063 SFILN_BI0; BY 001305/006101; HW 001307/006117;
WF 001311/006135; DF 001313/006153.

### Functional pseudocode
```
DIS_IDESC then read <m>(SC10) (unsigned); read dest descriptor; fetch reg tn
i = 0
while not end-of-dest and i < m:
    if dest outside: K=1,Z=0; DEST_RANGE (DR trap); I2 unchanged; stop
    D(I2) = tn ; I2 += 1 ; i += 1
end   # reuses SFILL*_F02 body per type
if i == m (m elements filled): K=0,Z=1 ; I2 :- next element
elif dest full:                K=1,Z=0 ; I2 :- next element
```
Fills up to m elements, or to end of string, whichever first. m unsigned.

### Operands  `<dest/w/t/I2>`, `<m/r/W>`; implicit register tn.

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CONDITIONAL (0 if m done / 1 if dest full) | CONDITIONAL | CLEARED | CLEARED | CLEARED |
m-filled K=0 Z=1; dest-full K=1 Z=0; outside-dest K=1 Z=0 (manual 14.9).

### Traps  DR trap (dest outside string).
### Citation  Microcode SFILN_BI0 006063 (DIS_IDESC then count compare 006075).
Manual 14.9.

--------------------------------------------------------------------------------
## 9. SCOTR  -  String Compare Translated   (file Scotr.c)

Opcode: BY 176655B (0FDADH). Manual 14.11.
Microcode: 001317 SCOTR -> 006613 SCOTRBY_F01.

### Functional pseudocode
```
read src1,src2 descriptors; DIS_IDESC; read <trans-table> base (EAO/SC13)
if table operand invalid: ILL_OP_SPEC trap
while not end-of-src1 and not end-of-src2:
    a = table[S(I1)] ; b = table[D(I2)]     # 006646/006652 DAC,DPA loads
    d = a - b (unsigned)                    # 006656 A-B, SAVA
    if d != 0: break with (a vs b) result
    I1+=1 ; I2+=1
end
# both outside            : K=0 Z=1 S=0 ; DR trap ; I1,I2 unchanged
# src1 outside (shorter)  : K=0 Z=0 S=0 ; DR trap
# src2 outside (longer)   : K=0 Z=0 S=1 ; DR trap
# exact match             : K=0 Z=1 S=0 ; I1,I2 :- next element
# src1 longer             : K=0 Z=0 S=0 ; I1,I2 :- next element
# src2 longer             : K=0 Z=0 S=1 ; I1,I2 :- next element
# greater byte in src1    : K=1 Z=0 S=0 ; I1,I2 :- differing elements
# smaller byte in src1    : K=1 Z=0 S=1 ; I1,I2 :- differing elements
```
Translated-byte unsigned compare. S carries the greater/less result, Z the
equal result, K distinguishes "difference inside a string" (K=1) from
"one ran out" (K=0). `ST,SAVA` at the diff/termination cells writes Z/S (and
C/O as arithmetic side-effects of the subtract).

### Operands  `<source-1/r/BY/I1>`, `<source-2/r/BY/I2>`, `<trans table/aa/BY>`.

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CONDITIONAL | CONDITIONAL | written by compare (undocumented) | written by compare (undocumented) | CONDITIONAL |
K/Z/S per table above (manual 14.11). C and O are written by the compare
subtract via ST,SAVA but are not documented as meaningful.

### Traps  DR trap (a pointer outside string); ILL_OP_SPEC (bad table, 006620).
### Citation  Microcode SCOTRBY_F01 006613 (SOUR_RANGE 006626/006631, diff test
006656-006661). Manual 14.11.

--------------------------------------------------------------------------------
## 10. SCOPA  -  String Compare With Pad    (file Scopa.c)

Opcode: BY 176676B (0FDBEH). Manual 14.12.
Microcode: 001321 SCOPA -> 006663 SCOPABY_CONT.

### Functional pseudocode
```
read src1,src2 descriptors and lengths; DIS_IDESC; read <pad>(byte)
L1 = len(src1) ; L2 = len(src2)
# The shorter string is logically extended with <pad> bytes so both have
# length max(L1,L2). An operand entirely outside its string = all pad.
compare element-by-element (unsigned), substituting pad past a string's end:
    a = (I1 < L1) ? S(I1) : pad
    b = (I2 < L2) ? D(I2) : pad
    d = a - b ; if d != 0 break ; I1+=1 ; I2+=1   # padding side not incremented
# exact match          : K=0 Z=1 S=0 ; I1,I2 :- next element
# greater byte in src1 : K=1 Z=0 S=0 ; I1,I2 :- differing elements
# smaller byte in src1 : K=1 Z=0 S=1 ; I1,I2 :- differing elements
# both outside string  : "exact match" (K=0 Z=1 S=0) + DR trap condition
```
Microcode computes the length difference (SCOPABY_PD 006671 `A-B`), then runs a
word-at-a-time compare fast path (SCOPABY_WORD/_W1/_WL) with byte fallback
(SCOPABY_BYTE/_BL), and pad-tail handlers (SCOPABY_S1_EMP / SCOPABY_S2_EMP) that
compare the remaining bytes of the longer string against `pad`. `ST,SAVA` at
SCOPABY_UNEQ (007017) writes the final Z/S. Any operand-outside case still
raises the DR trap condition (TRP_DR at 006703/006706).

### Operands  `<source-1/r/BY/I1>`, `<source-2/r/BY/I2>`, `<pad/r/BY>`.

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CONDITIONAL | CONDITIONAL | written by compare (undoc.) | written by compare (undoc.) | CONDITIONAL |
exact-match K=0 Z=1 S=0; greater-in-src1 K=1 Z=0 S=0; smaller-in-src1 K=1 Z=0
S=1 (manual 14.12).

### Traps  DR trap when an operand is addressed outside its string.
### Citation  Microcode SCOPABY_CONT 006663 (pad diff 006671, unequal
SCOPABY_UNEQ 007017, TRP_DR 006703). Manual 14.12.

--------------------------------------------------------------------------------
## 11. SCOPT  -  String Compare Translated With Pad   (file Scopt.c)

Opcode: BY 176677B (0FDBFH). Manual 14.13.
Microcode: 001323 SCOPT -> 007022 SCOPTBY_F01.

### Functional pseudocode
```
read src1,src2; DIS_IDESC; read <trans-table>(EAO/SC13); read <pad>
if table operand invalid: ILL_OP_SPEC trap
tpad = table[pad]                         # pad is also translated
compare, substituting tpad past a string's end (index NOT incremented on the
padding side):
    a = table[ (I1<L1)? S(I1) : pad ]
    b = table[ (I2<L2)? D(I2) : pad ]
    d = a - b ; if d != 0 break ; else I1+=1, I2+=1
# exact match          : K=0 Z=1 S=0 ; I1,I2 :- next el. or end of string
# greater byte in src1 : K=1 Z=0 S=0 ; I1,I2 :- differing elements
# smaller byte in src1 : K=1 Z=0 S=1 ; I1,I2 :- differing elements
# operand(s) outside   : DR trap condition
```
Same structure as SCOPA but every byte (including pad) passes through the
translate table (007044/007053/... DAC,DPA loads). Mismatch handler
SCOPTBY_MIS (007114) sets the result status; `ST,SAVA` on the compare cells.
Manual note: index registers are not incremented while padding.

### Operands  `<source-1/r/BY/I1>`,`<source-2/r/BY/I2>`,`<trans table/aa/BY>`,`<pad/r/BY>`.

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CONDITIONAL | CONDITIONAL | written by compare (undoc.) | written by compare (undoc.) | CONDITIONAL |
K/Z/S as SCOPA (manual 14.13).

### Traps  DR trap; ILL_OP_SPEC (bad table, 007027); TRP_DR (007040 pad-index).
### Citation  Microcode SCOPTBY_F01 007022 (mismatch SCOPTBY_MIS 007114).
Manual 14.13.

--------------------------------------------------------------------------------
## 12. SSKIP  -  String Skip Elements       (file Sskip.c)

Opcode: BY 176656B (0FDAEH). Manual 14.14.
Microcode: 001325 SSKIP -> DIS_IDESC (001326) -> 007121 SSKIPBY_F01.

### Functional pseudocode
```
DIS_IDESC then read <test>(byte, into Q); read source descriptor
while not end-of-source and S(I1) == test:
    I1 += 1
end
# terminal compare S(I1) vs test (SSKIPBY_F12 007133: A-B, MZRO):
if source outside:      K=0 Z=1 S=0 ; DR trap ; I1 unchanged
elif source empty:      K=0 Z=1 S=0 ; I1 :- next element
elif S(I1) > test:      K=0 Z=0 S=0 ; I1 :- differing element
elif S(I1) < test:      K=0 Z=0 S=1 ; I1 :- differing element
```
Unsigned skip-while-equal. The trailing compare sets S from the greater/less
result via the `ALU,OR/ANDCB ... ST,LOAD` at SSKIPBY_MIS (007137). K stays 0
throughout (single-string scan). `ST,SAVA` at SSKIPBY_F02/_F12.

### Operands  `<source/r/BY/I1>`, `<test/r/BY>`.

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CLEARED (=0) | CONDITIONAL | CLEARED | CLEARED | CONDITIONAL |
byte>test K=0 Z=0 S=0; byte<test K=0 Z=0 S=1; source-empty/outside K=0 Z=1 S=0
(manual 14.14).

### Traps  DR trap (source outside string).
### Citation  Microcode SSKIPBY_F01 007121 (compare SSKIPBY_F12 007133, result
SSKIPBY_MIS 007137). Manual 14.14.

--------------------------------------------------------------------------------
## 13. SSCAN  -  String Scan               (file Sscan.c)

Opcode: BY 176661B (0FDB1H). Manual 14.16.
Microcode: 001336 SSCAN -> DIS_IDESC (001337) -> 007200 SSCANBY_F01.

### Functional pseudocode
```
DIS_IDESC; read <mask>(SC12) and <trans-table>(EAO/SC13); read source
if table operand invalid: ILL_OP_SPEC trap
while not end-of-source and (table[S(I1)] AND mask) == 0:
    I1 += 1
end
if source outside:              K=0 Z=1 ; DR trap ; I1 unchanged
elif (tr(byte) AND mask) != 0:  K=0 Z=0 ; I1 :- found element
elif source empty:              K=0 Z=1 ; I1 :- next element
```
Scan until a translated byte has any masked bit set. The guard is
`AND DATA,SC12` then `COND,MZRO` at 007217-007220. K always 0.

### Operands  `<source/r/BY/I1>`, `<mask/r/BY>`, `<trans table/aa/BY>`.

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CLEARED (=0) | CONDITIONAL | CLEARED | CLEARED | CLEARED |
found (AND>0) K=0 Z=0; source-empty K=0 Z=1; outside K=0 Z=1 (manual 14.16).

### Traps  DR trap (source outside); ILL_OP_SPEC (bad table, 007203).
### Citation  Microcode SSCANBY_F01 007200 (mask test 007217-007220). Manual 14.16.

--------------------------------------------------------------------------------
## 14. SSPAN  -  String Span               (file Sspan.c)

Opcode: BY 176662B (0FDB2H). Manual 14.17.
Microcode: 001341 SSPAN -> DIS_IDESC (001342) -> 007222 SSPANBY_F01.

### Functional pseudocode
```
DIS_IDESC; read <mask>(SC12), <trans-table>(SC13); read source
if table operand invalid: ILL_OP_SPEC trap
while not end-of-source and (table[S(I1)] AND mask) != 0:
    I1 += 1
end
if source outside:              K=0 Z=0 ; DR trap ; I1 unchanged
elif (tr(byte) AND mask) == 0:  K=0 Z=1 ; I1 :- found element
elif source empty:              K=0 Z=0 ; I1 :- next element
```
The complement of SSCAN: span (skip) while the masked translated bits are
nonzero; stop when they become zero. Guard at 007241-007242 (`AND DATA,SC12`,
`COND,MZRO INVSEQ`). K always 0.

### Operands  `<source/r/BY/I1>`, `<mask/r/BY>`, `<trans table/aa/BY>`.

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CLEARED (=0) | CONDITIONAL | CLEARED | CLEARED | CLEARED |
tr(byte) AND mask == 0 -> K=0 Z=1; source-empty K=0 Z=0; outside K=0 Z=0
(manual 14.17).

### Traps  DR trap; ILL_OP_SPEC (bad table, 007225).
### Citation  Microcode SSPANBY_F01 007222 (mask test 007241). Manual 14.17.

--------------------------------------------------------------------------------
## 15. SMATCH  -  String Match             (file Smatch.c)

Opcode: BY 176663B (0FDB3H). Manual 14.18.
Microcode: 001344 SMATCH -> 007244 SMATCHBY_F01.

### Functional pseudocode
```
read <substring>(src, I1) and <string>(dest, I2) descriptors
subptr = I1 (kept)                 # I1 left unmodified
while not end-of-string:
    if compare substring == string[I2 .. I2+sublen-1] byte-for-byte:  # inner loop
        Z=1 ; I2 :- first matching byte ; stop
    I2 += 1
end
# substring outside            : K=0 Z=1 ; DR trap ; I2 unchanged
# string outside (sub inside)  : K=0 Z=0 ; DR trap ; I2 unchanged
# substring found              : K=0 Z=1 ; I2 :- first matching byte
# string exhausted (no match)  : K=0 Z=0 ; I2 :- next element
```
Naive substring search: outer loop advances I2 (SMATCHBY_F11 007257), inner
loop (SMATCHBY_F12/_F13 007265-007273) compares the whole substring at the
current I2 with `XOR ... COND,MZRO`. I1 is restored/unmodified. `ST,SAVA` on the
compare cells; K always 0.

### Operands  `<substring/r/BY/I1>`, `<string/r/BY/I2>`.

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CLEARED (=0) | CONDITIONAL | CLEARED | CLEARED | CLEARED |
found K=0 Z=1; not-found/string-empty K=0 Z=0; outside-substring K=0 Z=1;
outside-string K=0 Z=0 (manual 14.18).

### Traps  DR trap (an operand outside its string).
### Citation  Microcode SMATCHBY_F01 007244 (outer 007257, inner 007265-007276).
Manual 14.18.

--------------------------------------------------------------------------------
## 16. SSPAR  -  Set Parity In String      (file Sspar.c)

Opcode: BY 176664B (0FDB4H). Manual 14.19.
Microcode: 001346 SSPAR -> DIS_IDESC (001347) -> 007300 SSPAR_F01.

### Functional pseudocode
```
DIS_IDESC; read <mode>(SC13) then read string descriptor
if mode not in {0,1,2,3}: SET_IOV (illegal-operand-value trap)   # 007302->DUMMY
dispatch on mode (JMPREL, SSPAR_MODE 007313):
  mode 0 (clear even): SSPAR_M00 - bit7 := 0    (ANDCA byte with 0x80 mask)
  mode 1 (set):        SSPAR_M10 - bit7 := 1    (OR  byte with 0x80)
  mode 2 (even):       SSPAR_M20 - bit7 := even-parity of low 7 bits (COND,PARITY)
  mode 3 (odd):        SSPAR_M30 - bit7 := odd-parity of low 7 bits  (COND,PARITY)
for each byte in string (until end):
    if dest outside: K set; DEST_RANGE (DR trap); I1 unchanged; stop
    compute bit7 per mode ; write byte back (WRITE) ; I1 += 1
K = 1
```
Read-modify-write of every byte's parity bit (bit 7). Modes 2/3 use the hardware
`COND,PARITY` (parity of the low byte on the F-bus) to derive the bit. String is
`rw` (read then written). `ST,SAVA` used inside the range checks.

### Operands  `<string/rw/BY/I1>`, `<mode/r/BY>` (0..3).

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| SET (=1) | CLEARED | CLEARED | CLEARED | CLEARED |
Manual: terminating condition K=1. Others undocumented -> cleared (0.8).

### Traps  DR trap (string outside); illegal-operand-value trap for mode>3
(SET_IOV via 007310).
### Citation  Microcode SSPAR_F01 007300 (mode dispatch SSPAR_MODE 007313,
even/odd via COND,PARITY 007342/007351, SET_IOV 007310). Manual 14.19.

--------------------------------------------------------------------------------
## 17. SCHPAR  -  Check Parity In String    (file Schpar.c)

Opcode: BY 176665B (0FDB5H). Manual 14.20.
Microcode: 001351 SCHPAR -> DIS_IDESC (001352) -> 007353 SCHPAR_F01.

### Functional pseudocode
```
Z = 0
DIS_IDESC; read <mode>(SC13); read string descriptor
if mode not in {0,1,2,3}: SET_IOV (illegal-operand-value trap)   # 007355->DUMMY
dispatch on mode (SCHPAR_MODE 007366):
  mode 0: SCHPAR_M00 - expected bit7 = 0
  mode 1: SCHPAR_M10 - expected bit7 = 1
  mode 2: SCHPAR_M20 - expected bit7 = even parity (COND,PARITY 007415)
  mode 3: SCHPAR_M30 - expected bit7 = odd  parity (COND,PARITY 007424)
while not end-of-string and bit7(S(I1)) == expected:
    I1 += 1
end
if source outside:            K=0 Z=0 ; DR trap ; I1 unchanged
elif string empty:            K=0 Z=0 ; I1 :- next element
elif bit7 != expected:        K=0 Z=1 ; I1 :- element with wrong parity
```
Read-only parity check (unlike SSPAR). Stops at the first byte whose bit-7
parity disagrees with `<mode>`, setting Z=1. Modes 2/3 use `COND,PARITY`. K
always 0. `ST,SAVA` on the range check / terminal cells (e.g. 007400).

### Operands  `<string/r/BY/I1>`, `<mode/r/BY>` (0..3).

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| CLEARED (=0) | CONDITIONAL | CLEARED | CLEARED | CLEARED |
wrong-parity K=0 Z=1; string-empty K=0 Z=0; outside K=0 Z=0 (manual 14.20).

### Traps  DR trap (string outside); illegal-operand-value trap for mode>3
(SET_IOV via 007363).
### Citation  Microcode SCHPAR_F01 007353 (mode dispatch SCHPAR_MODE 007366,
parity 007415/007424, SET_IOV 007363). Manual 14.20.

--------------------------------------------------------------------------------
## 18. SCPUNO  -  Store CPU Number ('87 extension)   (file Scpuno.c)

Opcode: 177774B (0FF7CCH per manual 16.36). NOTE: Scpuno.c header says 0xFFFC;
manual is the authority. Manual section 16.36.
Microcode: 001047 SCPUNO -> 011021 SCPUNO_1.

### Functional pseudocode
```
read <destination/w> descriptor / effective address
cpuno = SAMSON_CPU()                 # 011021: read hardware CPU-number register
D(destination) = cpuno               # SAVE_RES 011025: WRITE, ST,SAVA
# ST,SAVA writes the data-status arithmetic bits from the CPU-number value
GET_NEXT (fetch next instruction)
```
Not a looping string op despite living in the STRING opcode block: it reads the
machine's CPU number and stores it to one destination word. `SAVE_RES` (011025)
does the store with `ST,SAVA`, so the data-status bits reflect the stored value
("Status bit set according to CPU number", manual).

### Operands  `<destination/w>` (single write).

### Status flags
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED (no K effect in routine) | CONDITIONAL (from CPU# via ST,SAVA) | CONDITIONAL | CONDITIONAL | CONDITIONAL |
Manual: "Status bit set according to CPU number." The microcode `ST,SAVA` writes
Z/C/O/S from the CPU-number value; K is not touched by SCPUNO_1/SAVE_RES, so it
is UNCHANGED. (Exact per-bit mapping of CPU# -> status is UNKNOWN without the
SAMSON_CPU register-layout doc.)

### Traps  None documented; standard write-access faults on `<destination>`.
### Citation  Microcode SCPUNO 001047 / SCPUNO_1 011021 (SAMSON_CPU, SAVE_RES
011025). Manual 16.36.

--------------------------------------------------------------------------------
## 19. Unresolved / needs deeper microtrace

1. SCPUNO: exact CPU-number -> data-status bit mapping (SAMSON_CPU register
   layout not in the traced microcode; manual only says "set according to CPU
   number").
2. C / O (carry, overflow) bits for the compare family (SCOTR, SCOPA, SCOPT):
   the compare subtract runs under `ST,SAVA`, which physically writes C and O,
   but neither the manual nor the traced cells assign them documented meaning.
   Whether software may rely on them is UNKNOWN.
3. Opcode values: manual octal/hex used throughout; the microcode gives only
   micro-addresses, so the opcode-to-entry dispatch table was not traced and the
   divergent hex in several C headers is unverified (manual treated as truth).
