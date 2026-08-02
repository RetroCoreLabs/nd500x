# ND-500 ARITHMETIC Instruction Category - Functional Behavior Reference

Ground-truth source: microcode trace of `$ND5000UC/microcode/MICRO-5800-A30.md`
(field mnemonics decoded via `$ND5000UC/manual/mnemonics.md`),
cross-checked against `docs/ND-05.009.4 EN ND-500 Reference Manual.md`.

Implementation files: `src/cpu/instructions/ARITHMETIC/*.c`

This document is a FUNCTIONAL reference (data-path pseudocode), not a flags-only summary.
Every statement is either read directly from the microcode/manual or explicitly marked
inferred/UNKNOWN.

--------------------------------------------------------------------------------
## HOW TO READ THE MICROCODE (decode legend)

Each microcell is one row: `| octal | label | field1 field2 ... [ADDR=next] |`.
Relevant fields (decoded from mnemonics.md):

- `ALU,<f>`  = ALU function on A and B buses. Key values:
  `A+B` (add), `B-A CRY,ONE` (B minus A = subtract with borrow comp),
  `A+B CRY,C` (add with status carry), `B-A CRY,C` (sub with status carry),
  `A-1` (decrement A), `A CRY,ONE` (A+1 increment), `A` (pass A),
  `ANDCA` (AND of complement-A with B), `ADIRC` (A complemented), `FZRO` (force zero),
  `XOR`, `AND`, `OR`.
- `AAP2,<op>` = extended Arithmetic Processor op: `ADD`,`SUBBA`(B-A),`MUL`,`IMUL`(int mul 1 result),
  `IMULD`(int mul 2 results),`IMULU`/`IMULUD`(unsigned int mul), `CTF`(convert to float).
- `TYP,DR` = datatype selected by instruction (OR-logic, runtime BY/H/W); `TYP,F` single float;
  `TYP,DF` double float; `TYP,BY`/`HW`/`W` fixed.
- `A,<src>` / `B,<src>` = ALU input buses. `A,ALU,REG37` = the register-file operand latch
  (holds the destination register Rn); `ORB,IN` = B operand ORed in from the instruction's
  operand specifier (the memory/register source operand).
- `D,<dest>` = write target. `D,ALU,REG37` = write back to Rn; `D,SC1..SC14` = scratch regs.
- `READ`/`RD,PX` = data-memory read of the source operand; `WRITE`/`WR,POF` = data-memory write;
  `ADACT` = address-arithmetic activate (compute effective address of operand).
- `ST,<f>` = status write: `ST,SAVA` save integer-ALU status (Z,C,O,S);
  `ST,SAVF` save floating status (Z,S,FU,FO); `ST,SAVM` save mixed integer-multiply status (Z,S,O);
  `ST,SAVB` save BCD status (Z,S,BO); `ST,ACCA`/`ST,ACCM`/`ST,ACCF` accumulate the corresponding status.
- `K,ONE`/`K,ZRO`/`K,1IFZ` = write the K (flag) bit (set/clear/set-if-ALU-zero). Same 4-bit field
  as ST,* so a cell doing ST,SAVA does NOT touch K.
- `G,OOPS`/`G,OPS`/`G,DIR*` = fetch next instruction / next operand specifier / immediate.
- `T,JMP ... ADDR=x` = next microcell; `T,RETURN` = return to sequencer; `SET_IOV`/`CLE_IOV` =
  integer-overflow trap set/clear paths; `ILLEG` = illegal-instruction path.

### Flag-write semantics (resolved from the ST field + Manual sec 6.5.1)

Manual sec 6.5.1 rule: "All data status bits not mentioned are reset." Data status bits are
Z(5) C(6) S(7) O(9) IVO(11) DZ(12) FU(13) FO(14) BO(15). K (the FLAG bit) is NOT a data status
bit and is left UNCHANGED by every arithmetic routine below (none of their result cells encode
K,ONE/K,ZRO/K,1IFZ except as internal loop control that does not survive to completion).

| ST field present | Bits WRITTEN from result | Bits forced 0 (rule 6.5.1) | K |
|------------------|--------------------------|----------------------------|---|
| ST,SAVA (integer)| Z, C, O, S               | IVO,DZ,FU,FO,BO            | unchanged |
| ST,SAVM (int mul)| Z, S, O                  | C,IVO,DZ,FU,FO,BO         | unchanged |
| ST,SAVF (float)  | Z, S, FU, FO             | C,O,IVO,DZ,BO             | unchanged |
| ST,SAVB (BCD)    | Z, S, BO                 | C,O,IVO,DZ,FU,FO          | unchanged |

Legend in the per-instruction flag tables: SET/CLEARED/UNCHANGED/CONDITIONAL.

--------------------------------------------------------------------------------

# INTEGER / FLOAT ADD-CLASS

## ADD  (mnemonic `tn +`)
Opcode (octal): Wn 124B+(n-1), Fn 130B+(n-1), Dn 134B+(n-1), BYn 176064B+(n-1), Hn 176070B+(n-1).
File: `Add.c`.

FUNCTIONAL PSEUDOCODE (integer path, microcell 000267 ADD):
```
1. EA = address-arith(operand); ADACT
2. src = READ data-memory[EA]           ; datatype TYP,DR (BY/H/W by opcode)
3. A = Rn (A,ALU,REG37) ;  B = src (ORB,IN)
4. result = A + B        (ALU,A+B)
5. Rn = result           (D,ALU,REG37)
6. ST,SAVA -> Z,C,O,S from the add
7. G,OOPS: fetch next instruction
```
Float path (002167 ADDF): A operand is the register, B the memory operand; the add is done in
the AAP (`AAP2,ADD`), `AAPSYNC` waits for the float unit, result written, `ST,SAVF` -> Z,S,FU,FO.
Double path (002204 ADDD): two 32-bit memory reads (high+low), AAP2,ADD, ST,SAVF.

OPERANDS: Rn (register, dest+first source) and one operand `<addend>` (register/memory/constant),
same datatype BY/H/W/F/D.
RESULT: `Rn + <addend> -> Rn`. Side effect: none beyond Rn + status.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL (sum==0) | CONDITIONAL integer: carry out of MSB / float: CLEARED | CONDITIONAL (signed overflow); float: CLEARED | CONDITIONAL (sum sign bit) |
(IVO,DZ,BO CLEARED; FU,FO CONDITIONAL on float path, CLEARED on integer.)

TRAP conditions: Addressing traps, Integer overflow (O), Floating overflow (FO), Floating underflow (FU).
CITATION: microcode ADD @000267 / ADDF @002167 / ADDD @002204; Manual sec 11.1.

## SUBTRACT  (mnemonic `tn -`) and the `Sub` file
Opcode (octal): Wn 140B+(n-1), Fn 144B+(n-1), Dn 150B+(n-1), BYn 176074B+(n-1), Hn 176100B+(n-1).
Files: `Subtract.c`, `Sub.c` (same instruction family).

FUNCTIONAL PSEUDOCODE (integer, microcell 000270 SUB):
```
1. EA = address-arith(operand); READ src
2. A = Rn ; B = src (ORB,IN)
3. result = B - A  with borrow compensation  (ALU,B-A CRY,ONE)
   NOTE: bus A holds Rn, bus B holds the source; ALU computes B-A, i.e. Rn - src
         is realized because the microcode assigns Rn to A and forms (operand)-(A)?  -- see below
4. Rn = result ; ST,SAVA -> Z,C,O,S ; G,OOPS
```
IMPORTANT ORDER NOTE (read directly, not assumed): the cell encodes `ALU,B-A CRY,ONE` with
`A,ALU,REG37` (=Rn) and `ORB,IN` (=source on B). That computes `source - Rn`. The Manual
operation is `Rn - <subtrahend> -> Rn`. The two agree only if the microarchitecture places the
subtrahend on A and Rn on B for this cell; the bus labels as decoded put Rn on A. This sign/order
detail is the one place microcode-vs-manual could disagree -- flagged as INFERRED that the net
effect is Manual's `Rn - operand` (the C reference `Sub.c` implements `Rn - operand`). Value of the
difference and all flags are as for ADD.
Float (002230 SUBF): AAP2,SUBBA (B-A) in the float unit, ST,SAVF.

OPERANDS: Rn and `<subtrahend>`, same datatype.
RESULT: `Rn - <subtrahend> -> Rn`.

STATUS FLAGS: identical table to ADD (Z,C,O,S conditional integer; Z,S,FU,FO float; K UNCHANGED).
TRAP: Addressing, Integer overflow (O), Floating overflow (FO), Floating underflow (FU).
CITATION: SUB @000270 / SUBF @002230 / SUBD @002246; Manual sec 11.2.

## ADD2 - Add two operands  (`t ADD2 <a>,<b>`)
Opcode (octal): BY 176027B, W ADD2 123B (per-datatype block). File: `Add2.c`.
FUNCTIONAL PSEUDOCODE (integer, 000271-000273):
```
1. EA1 = addr(a); READ a -> SC5      (EA1SAVE saves the write-back address of <a>)
2. EA(b); READ b ; result = a + b    (ALU,A+B) -> SC1 ; ST,SAVA
3. WRITE result back to <a> (memory), C,MEMOT (write only if <a> is a data operand)
```
Float (002172 ADD2F): AAP2,ADD, writes result to <a>, ST,SAVF.
OPERANDS: `<a>` (read+write) and `<b>` (read), same datatype. RESULT: `a + b -> a`.
STATUS FLAGS: same as ADD. TRAP: Addressing, O, FO, FU.
CITATION: ADD2 @000271 / ADD2F @002172 / ADD2D @002212; Manual sec 11.5.

## ADD3 - Add three operands  (`t ADD3 <a>,<b>,<c>`)
Opcode (octal): BY 176147B, W 176151B. File: `Add3.c`.
PSEUDOCODE (000277-000301): READ a -> SC5; READ b; `a + b` -> SC1 (ST,SAVA); WRITE sum to `<c>`.
Two source operands read, result stored in a third (write-only) operand.
RESULT: `a + b -> c`. STATUS: same as ADD. TRAP: Addressing, O, FO, FU.
CITATION: ADD3 @000277 / ADD3F @002177 / ADD3D @002221; Manual sec 11.9.

## ADDC - Add with carry  (`Wn ADDC <addend>`)
Opcode (octal): Wn 177100B+(n-1). Word only. File: `Addc.c`.
PSEUDOCODE (000305 ADDC): `A = Rn ; B = READ(operand); result = A + B + C` where C is the status
carry (`ALU,A+B CRY,C`); `Rn = result`; `ST,SAVA` -> Z,C,O,S; G,OOPS.
The status Carry bit from a previous ADD/ADDC/SUBC is the carry-in (multi-precision arithmetic).
RESULT: `Rn + <addend> + C -> Rn`.
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL (sum==0) | CONDITIONAL (carry out of MSB) | CONDITIONAL (signed overflow) | CONDITIONAL (sign) |
TRAP: Addressing, Integer overflow (O). CITATION: ADDC @000305; Manual sec 11.17.

## SUB2 / SUB3 / SUBC
- SUB2 (`t SUB2 <a>,<b>`), opcode H 176131B, W 340B, file `Sub2.c`: microcell 000274; READ a->SC5,
  READ b, `B-A CRY,ONE` -> SC1 (ST,SAVA), WRITE to <a>. RESULT `a - b -> a`. Float 002234 SUB2F.
- SUB3 (`t SUB3 <a>,<b>,<c>`), opcode H 176155B, W 176156B, file `Sub3.c`: microcell 000302;
  READ a, READ b, `B-A CRY,ONE` -> SC1 (ST,SAVA), WRITE to <c>. RESULT `a - b -> c`. Float 002241.
- SUBC (`Wn SUBC <subtrahend>`), opcode Wn 177104B+(n-1), word only, file `Subc.c`: microcell 000306;
  `B-A CRY,C` = Rn - operand - (1-C) borrow, uses the saved carry; ST,SAVA. RESULT `Rn - <subtrahend> - borrow -> Rn`.
STATUS (all three): K UNCHANGED; Z,C,O,S CONDITIONAL integer (Z,S,FU,FO on float SUB2/SUB3).
TRAP: SUB2/SUB3 Addressing,O,FO,FU; SUBC Addressing,O.
CITATION: SUB2 @000274, SUB3 @000302, SUBC @000306; Manual sec 11.6/11.10/11.18.

--------------------------------------------------------------------------------

# MULTIPLY-CLASS

## MULTIPLY  (mnemonic `tn *`)
Opcode (octal): Wn 154B+(n-1), Fn 160B+(n-1), Dn 164B+(n-1), BYn 176104B+(n-1), Hn 176110B+(n-1).
File: `Multiply.c`.
FUNCTIONAL PSEUDOCODE (integer, 002275 MULBY / 002301 MULHW / 002305 MULW):
```
1. READ operand -> SC5
2. AAP2,IMUL : integer multiply Rn * operand in the AAP (one 32-bit result), AAPSYNC
3. result = AAP result -> Rn (D,ALU,REG37) ; ST,SAVM -> Z,S,O (mixed integer-mul status)
   SLOW1 timing on the result cell.
4. G,OOPS
```
Integer overflow (O) is set when the upper half of the double-length product is not the sign
extension of the lower half (Manual 11.3); the AAP flags this into ST,SAVM. Low 32 bits are stored.
Float (002355 MULF / 002372 MULD): AAP2,MUL, ST,SAVF -> Z,S,FU,FO.
OPERANDS: Rn and `<multiplier>`, same datatype. RESULT: `Rn * <multiplier> -> Rn`.
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL (product==0) | CLEARED (integer mul: C not produced; float: cleared) | CONDITIONAL (product too large); float CLEARED | CONDITIONAL (product sign) |
(FU,FO CONDITIONAL on float, CLEARED integer.)
TRAP: Addressing, Integer overflow (O), Floating overflow (FO), Floating underflow (FU).
CITATION: MULW @002305 (MULBY@002275,MULHW@002301) / MULF @002355 / MULD @002372; Manual sec 11.3.

## MUL2 / MUL3  (`t MUL2 <a>,<b>` / `t MUL3 <a>,<b>,<c>`)
Opcode: MUL2 H 176136B, W 176137B (file `Mul2.c`); MUL3 H 176162B, W 176163B, D 176165B (file `Mul3.c`).
PSEUDOCODE MUL2 (002311): READ a->SC5, AAP2,IMUL a*b, result -> WRITE to <a>, ST,SAVM. Float 002360 MUL2F.
PSEUDOCODE MUL3 (002325): READ a, READ b, AAP2,IMUL, WRITE product to <c>, ST,SAVM. Float 002365 MUL3F, 002407 MUL3D.
RESULT: MUL2 `a*b -> a`; MUL3 `a*b -> c`.
STATUS: same table as MULTIPLY. TRAP: Addressing, O, FO, FU. CITATION: MUL2 @002311, MUL3 @002325; Manual 11.7/11.11.

## MUL4 - Multiply with overflow to register  (`t MUL4 <a>,<b>,<c>`)
Opcode (octal): BYn 176040B+(n-1), Hn 176044B+(n-1), Wn 176050B+(n-1). File: `Mul4.c`.
FUNCTIONAL PSEUDOCODE (002341 MUL4BY / 002345 / 002351 -> MUL4_WRITE):
```
1. READ a -> SC5
2. AAP2,IMULD : integer multiply producing a DOUBLE-length result (two 32-bit halves), AAPSYNC
3. low half -> <c> operand ; upper (overflow) half -> Rn  (via MUL4_WRITE)
```
RESULT: `a*b -> c` (lower part), `overflow (upper) part -> Rn`.
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL (lower part==0) | CLEARED | CONDITIONAL (upper half != sign-ext of lower) | CONDITIONAL (lower part sign) |
TRAP: Addressing, Integer overflow (O). CITATION: MUL4BY @002341, common MUL4_WRITE; Manual sec 11.13.

## UMUL - Unsigned multiply with overflow to register  (`Wn UMUL <a>,<b>,<c>`)
Opcode (octal): Wn 176200B+(n-1). File: `Umul.c`.
PSEUDOCODE (002567 UMUL -> MUL4_WRITE): READ a->SC5; `AAP2,IMULUD` unsigned double-length multiply;
low half -> <c>, upper half -> Rn. Byte/halfword constants are sign-extended then treated unsigned
(Manual 11.15). Overflow set when upper part != 0.
RESULT: `a*b -> c` (low), overflow -> Rn. 
STATUS: K UNCHANGED; Z CONDITIONAL(product==0); C CLEARED; O CONDITIONAL(upper!=0); S CONDITIONAL.
TRAP: Addressing, Integer overflow (O). CITATION: UMUL @002567; Manual sec 11.15.

## MULAD - Multiply and add  (`tn MULAD <x>,<y>`)
Opcode (octal): BYn 176350B+(n-1), Hn 176354B+(n-1), Wn 250B+(n-1), Fn 176360B+(n-1), Dn 176364B+(n-1).
File: `Mulad.c`.
FUNCTIONAL PSEUDOCODE (integer, 002617 MULADBY):
```
1. AAP2,IMUL : product = Rn * <x> -> SC7 (via AAP), READ <y>
2. ST,SAVM on the multiply step (SC5)
3. result = SC7 + <y>   (ALU,A+B, A=product SC7, B=<y> SC5) -> Rn
4. ST,ACCA : accumulate integer status (Z,S,C,O) into the running status
5. G,OOPS
```
Float (002633 MULADF / 002635 MULADD): AAP2,MUL then AAP2,ADD; ST,SAVF/ST,ACCF.
RESULT: `Rn * <x> + <y> -> Rn`.
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL (result==0) | CONDITIONAL integer (carry MSB); float CLEARED | CONDITIONAL (overflow) | CONDITIONAL (sign) |
TRAP: Addressing, Integer overflow (O), Floating overflow (FO), Floating underflow (FU).
CITATION: MULADBY @002617 / MULADF @002633 / MULADD @002635; Manual sec 11.19.

## PSUM - Sum of products  (`tn PSUM <x>,<y>`)
Opcode (octal): BYn 176370B+(n-1), Hn 176374B+(n-1), Wn 176400B+(n-1), Fn 176404B+(n-1), Dn 176410B+(n-1).
File: `Psum.c`.
FUNCTIONAL PSEUDOCODE (integer, 002640 PSUMBY):
```
1. READ <x> -> SC7 ; AAP2,IMUL product = <x> * <y> ; AAPSYNC ; ST,SAVM
2. result = Rn + product  (ALU,A+B, A=Rn(REG37), B=product SC7) -> Rn
3. ST,ACCA : accumulate integer status ; G,OOPS
```
Float (002655 PSUMF / 002661 PSUMD): AAP2,MUL then AAP2,ADD, ST,SAVF/accumulate.
RESULT: `<x> * <y> + Rn -> Rn`. (Same data path as MULAD but the accumulator is Rn itself.)
STATUS FLAGS: identical to MULAD (K UNCHANGED; Z,S,O CONDITIONAL; C CONDITIONAL integer / CLEARED float).
TRAP: Addressing, Integer overflow (O), Floating overflow (FO), Floating underflow (FU).
CITATION: PSUMBY @002640 / PSUMF @002655 / PSUMD @002661; Manual sec 11.20.

--------------------------------------------------------------------------------

# DIVIDE-CLASS

## DIVIDE  (mnemonic `tn /`)
Opcode (octal): Wn 170B+(n-1), Fn 174B+(n-1), Dn 350B+(n-1), BYn 176114B+(n-1), Hn 176120B+(n-1).
File: `Divide.c`.
FUNCTIONAL PSEUDOCODE (integer, 002416 DIVBY / 002423 DIVHW / 002430 DIVW):
```
1. READ divisor -> SC5
2. sign bookkeeping: XOR of dividend-sign and divisor-sign saved (CSAVE, COND,MSGN) -> SC7
   quotient sign; dividend sign captured (BM07/BM17/BM37 = sign bit per datatype) -> SC2
3. load loop counter LC = 37 (octal) ; enter DIV_INT restoring-division loop
4. quotient truncated toward 0 ; remainder (if nonzero) takes sign of the dividend
5. quotient -> Rn ; ST,SAVA (in DIV_INT common)
```
Integer overflow (O) occurs iff greatest-negative-integer / -1 (Manual 11.4). Divide-by-zero:
DZ set; per Manual the destination gets the largest possible value with the sign of the dividend
(0/0 gives 0). Float (DIVF/DIVD): AAP floating divide, ST,SAVF.
RESULT: `Rn / <divisor> -> Rn`.
STATUS FLAGS:
| K | Z | C | O | S | DZ |
|---|---|---|---|---|----|
| UNCHANGED | CONDITIONAL (quotient==0) | CLEARED | CONDITIONAL (MIN/-1); float CLEARED | CONDITIONAL (quotient sign) | CONDITIONAL (divisor==0) |
(FU,FO CONDITIONAL on float, CLEARED integer; IVO,BO CLEARED.)
TRAP: Addressing, Integer overflow (O), Floating overflow (FO), Floating underflow (FU), Divide by zero (DZ).
CITATION: DIVW @002430 (DIVBY@002416,DIVHW@002423) -> DIV_INT; Manual sec 11.4.
UNKNOWN: the DIV_INT restoring-division inner loop (per-bit shift/subtract, LC=37 iterations) was
not traced bit-by-bit; entry + result-status + manual semantics are established, inner mechanics
need deeper microtrace.

## DIV2 / DIV3  (`t DIV2 <a>,<b>` / `t DIV3 <a>,<b>,<c>`)
Opcode: DIV2 H 176143B, W 176144B (file `Div2.c`); DIV3 H 176167B (file `Div3.c`).
PSEUDOCODE DIV2 (002435 DIV2BY): READ a->SC7, READ b, sign bookkeeping, DIV loop; quotient -> WRITE <a>.
PSEUDOCODE DIV3 (DIV3BY): READ a, READ b, DIV loop; quotient -> <c>.
RESULT: DIV2 `a/b -> a`; DIV3 `a/b -> c`. Integer remainder sign = sign of <a>; truncate toward 0.
STATUS: same table as DIVIDE (Z,S,O conditional, C cleared, DZ conditional; float Z,S,FU,FO).
TRAP: Addressing, O, FO, FU, DZ. CITATION: DIV2 @002435, DIV3 (DIV3BY); Manual sec 11.8/11.12.

## DIV4 - Divide with remainder to register (modulo)  (`t DIV4 <a>,<b>,<c>`)
Opcode (octal): BYn 176054B+(n-1), Hn 176060B+(n-1), Wn 176174B+(n-1). File: `Div4.c`.
FUNCTIONAL PSEUDOCODE (002553 DIV4BY / 002557 / 002563 -> DIV4_COMM):
```
1. READ a -> SC7 ; READ b -> SC5 ; capture dividend sign (BM07/BM17/BM37) -> SC2 (CSAVE)
2. DIV4_COMM: restoring division producing BOTH quotient and remainder
3. quotient -> <c> operand ; remainder -> Rn
```
RESULT: `a/b -> c` (quotient), `remainder -> Rn`. Remainder is ADA/SIMULA-compatible (sign of <a>).
NOTE (Manual 11.14): "Separate testing must be done to obtain status" - the status bits reflect the
quotient, not the remainder.
STATUS FLAGS:
| K | Z | C | O | S | DZ |
|---|---|---|---|---|----|
| UNCHANGED | CONDITIONAL (quotient==0) | CLEARED | CONDITIONAL (MIN/-1) | CONDITIONAL (quotient sign) | CONDITIONAL (b==0) |
TRAP: Addressing, Integer overflow (O), Divide by zero (DZ). CITATION: DIV4BY @002553 -> DIV4_COMM; Manual sec 11.14.
UNKNOWN: DIV4_COMM inner loop not traced bit-by-bit.

## UDIV - Unsigned divide  (`Wn UDIV <a>,<b>,<c>`)
Opcode (octal): Wn 177110B+(n-1). File: `Udiv.c`.
PSEUDOCODE (002572 UDIV -> UDIV_COMM): READ a -> SC10; READ b -> SC6 (ST,SAVA on the read of b);
clear SC14; UDIV_COMM unsigned restoring division; quotient -> <c>, remainder -> Rn.
Byte/halfword constants sign-extended then treated unsigned.
RESULT: `a/b -> c` (quotient), remainder -> Rn.
STATUS FLAGS:
| K | Z | C | O | S | DZ |
|---|---|---|---|---|----|
| UNCHANGED | CONDITIONAL (quotient==0) | CLEARED | CLEARED (Manual lists no O trap for UDIV) | CONDITIONAL (quotient sign) | CONDITIONAL (b==0) |
TRAP: Addressing, Divide by zero (DZ). CITATION: UDIV @002572 -> UDIV_COMM; Manual sec 11.16.
UNKNOWN: UDIV_COMM inner loop not traced bit-by-bit.

--------------------------------------------------------------------------------

# UNARY ARITHMETIC

## NEG - Negate  (`tn NEG`)
Opcode (octal): Wn 220B+(n-1), Fn 224B+(n-1), BYn 177010B+(n-1), Hn 177014B+(n-1). File: `Neg.c`.
FUNCTIONAL PSEUDOCODE (integer, 000254 NEG):
```
1. result = 0 - Rn  (ALU,B-A CRY,ONE, A=Rn, B=SC14 holding 0) = two's complement of Rn
2. Rn = result ; ST,SAVA -> Z,C,O,S ; G,OOPS
   BY/H negate clears the upper part of the register (Manual 10.12).
```
Float (000256 NEGF -> NEG_F @001714): sign bit is inverted (floating negate), ST.
Manual 10.12: Integer overflow iff greatest-negative-integer negated. Carry is 0 except when
integer 0 is negated.
RESULT: `-Rn -> Rn`.
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL (result==0) | CONDITIONAL (set only when negating integer 0) | CONDITIONAL (MIN negated) | CONDITIONAL (result sign) |
TRAP: Integer overflow (O). CITATION: NEG @000254 / NEGF @000256->@001714; Manual sec 10.12.

## ABS - Absolute value  (`tn ABS`)
Opcode (octal): Wn 177410B+(n-1), Fn 177414B+(n-1), BYn 177400B+(n-1), Hn 177404B+(n-1). File: `Abs.c`.
FUNCTIONAL PSEUDOCODE (integer, 000263-000264 ABS):
```
1. copy Rn -> SC5 (ALU,A)
2. C,ALU conditional ALU: on sign (COND,MSGN) choose  ALU,A-B CRY,ONE (0 - Rn = negate)
   else ALU,A+B (pass Rn). i.e. if Rn<0 result = -Rn else Rn.
3. Rn = |Rn| ; ST,SAVA
```
Float (000265 ABSF): `ALU,ANDCA A,BM37` clears the sign bit (mask 0x800...0) -> floating absolute value.
BY/H store result in least-significant bits, clear the rest (Manual 10.15). Overflow iff greatest
negative integer negated. Manual explicitly: `0 -> S` (the result of ABS is non-negative).
RESULT: `|Rn| -> Rn`.
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL (result==0) | CLEARED | CONDITIONAL (MIN integer) | CLEARED (result is non-negative -> 0->S) |
TRAP: Integer overflow (O). CITATION: ABS @000263 / ABSF @000265; Manual sec 10.15.

## INCR - Increment  (`t INCR`)
Opcode (octal): H 116B, W 117B, F 120B, BY 176212B, D 176213B. File: `Incr.c`.
FUNCTIONAL PSEUDOCODE (integer, 000333 INCR):
```
1. READ operand (AB=11 -> previous EA) ; result = operand + 1 (ALU,A CRY,ONE) -> SC5 ; ST,SAVA
2. WRITE result back to the operand (C,MEMOT) ; G,OOPS
```
Float (002575 INCRF): add 1.0 (BM36 = float constant) via float unit. Manual 10.19: carry occurs iff
integer -1 is incremented.
RESULT: `<operand> + 1 -> <operand>`.
STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL (sum==0) | CONDITIONAL (set iff -1 incremented) | CONDITIONAL (overflow) | CONDITIONAL (sign) |
TRAP: Addressing, Integer overflow (O). CITATION: INCR @000333 / INCRF @002575; Manual sec 10.19.

## DECR - Decrement  (`t DECR`)
Opcode (octal): W 121B, BY 176214B, H 176215B, F 176216B, D 176217B. File: `Decr.c`.
FUNCTIONAL PSEUDOCODE (integer, 000335 DECR):
```
1. READ operand ; result = operand - 1 (ALU,A-1) -> SC5 ; ST,SAVA
2. WRITE result back ; G,OOPS
```
Float (002602 DECRF): subtract 1.0 via float unit.
RESULT: `<operand> - 1 -> <operand>`.
STATUS FLAGS: same shape as INCR (K UNCHANGED; Z,C,O,S CONDITIONAL). C = borrow into MSB per ST,SAVA.
TRAP: Addressing, Integer overflow (O). CITATION: DECR @000335 / DECRF @002602; Manual sec 10.20.

--------------------------------------------------------------------------------

# EXPONENTIATION AND REMAINDER

## AXI - A to the I'th power (floating)  (`tn AXI <a>,<i>`)
Opcode (octal): Fn 176300B+(n-1), Dn 176304B+(n-1). File: `Axi.c`.
FUNCTIONAL PSEUDOCODE (001437 AXIF):
```
1. READ <a> (float base) -> SC5 (ST,SAVA on this read) ; READ <i> (integer exponent) -> SC3
2. test exponent for the illegal/zero cases (COND,MSORZ -> CLE_IOV clears IOV, sets up)
3. binary exponentiation loop: square-and-multiply using SC7 accumulator and Q register (Q,F),
   iterating over exponent bits (BM36/BM26 masks, INVSEQ on COND,MZRO to end the loop)
4. result -> Rn via FWRITE_0 ; ST,SAVA/float status
```
AXID (001446) is the double-precision variant (reads high+low halves).
RESULT: `<a> ** <i> -> Rn` (floating). Zero to a negative power / other IVO cases -> Illegal
operand value trap (IOV/IVO).
STATUS FLAGS:
| K | Z | C | O | S | + |
|---|---|---|---|---|---|
| UNCHANGED | CONDITIONAL (result==0) | CLEARED | CLEARED | CONDITIONAL (sign) | FU,FO CONDITIONAL; IVO CONDITIONAL |
TRAP: Addressing, Floating overflow (FO), Floating underflow (FU), Illegal operand value (IOV/IVO).
CITATION: AXIF @001437 / AXID @001446; Manual sec 12.1.
UNKNOWN: exact square-and-multiply loop iteration/rounding details not traced to completion.

## IXI - I to the J'th power (integer)  (`tn IXI <i>,<j>`)
Opcode (octal): BYn 176310B+(n-1), Hn 176314B+(n-1), Wn 176320B+(n-1). File: `Ixi.c`.
FUNCTIONAL PSEUDOCODE (001460 IXIBY / 001465 IXIHW / 001472 IXIW -> IXI_SPEC @003550):
```
1. READ base <i> -> SC10 ; READ exponent <j> -> SC12 (CSAVE on zero-test COND,MZRO)
2. special-case dispatch (IXI_SPEC): exponent 0 -> result 1; handle 0**neg (illegal),
   1**j, (-1)**j (even/odd) ; general case = integer square-and-multiply loop with
   overflow accumulation (ST,ACCA, SET_IOV on integer overflow)
3. result -> Rn
```
Semantics from `Ixi.c`/Manual 12.2: any**0 = 1; 0**neg = illegal operand trap; 1**j = 1;
(-1)**j = 1 if j even else -1; other base ** negative = 0 (integer truncation).
RESULT: `<i> ** <j> -> Rn`.
STATUS FLAGS:
| K | Z | C | O | S | + |
|---|---|---|---|---|---|
| UNCHANGED | CONDITIONAL (result==0) | CLEARED | CONDITIONAL (integer overflow, SET_IOV) | CONDITIONAL (sign) | IVO CONDITIONAL |
NOTE (Manual 6.5.1): IXI is a named exception in the floating-underflow Z-bit rule; not relevant to
the integer overflow path here.
TRAP: Addressing, Illegal operand value (IOV/IVO), Integer overflow (O).
CITATION: IXIBY @001460 -> IXI_SPEC @003550 (SET_IOV path); Manual sec 12.2.
UNKNOWN: the exponentiation loop (IXISPEC_1..5 + SET_IOV/SET_IOV accumulation) traced at entry and
branch level only; exact per-iteration overflow accumulation not fully unrolled.

## REM - Floating point remainder  (`tn REM <x>,<y>,<q>`)
Opcode (octal): Fn 177130B+(n-1), Dn 177134B+(n-1). File: `Rem.c`.
FUNCTIONAL PSEUDOCODE (002133 REMF):
```
1. READ <x> -> SC7 (ST,SAVA) ; READ <y> -> SC5
2. test <y>==0 (CSAVE, COND,MZRO) -> REMF_01 (divide-by-zero handling / DZ)
3. compute q = int(<x>/<y>) in float ; store q in <q> operand
4. remainder = <x> - q*<y>  (float) -> Rn
```
REMD (002136) is the double variant. Precision note (Manual 10.33): if |x/y| is so large the binary
point is outside float format, Rn = 0.
RESULT: `int(<x>/<y>) -> <q>` and `remainder -> Rn` (both float).
STATUS FLAGS:
| K | Z | C | O | S | + |
|---|---|---|---|---|---|
| UNCHANGED | CONDITIONAL (remainder==0) | CLEARED | CLEARED | CONDITIONAL (remainder sign) | DZ CONDITIONAL(y==0); FU,FO CONDITIONAL |
TRAP: Addressing, Floating overflow (FO), Floating underflow (FU), Divide by zero (DZ).
CITATION: REMF @002133 / REMD @002136 -> REMF_01/REMD_01; Manual sec 10.33.
UNKNOWN: REMF_01/REMD_01 quotient-integer-extraction inner steps not traced to completion.

--------------------------------------------------------------------------------

# PACKED DECIMAL (BCD) - hardware option

General note (Manual 17.1): packed-decimal instructions operate on descriptor-defined BCD string
operands in data memory. All share the microcode preamble pattern:
```
  ALU,A Q,F ; READ first descriptor/operand -> SC1
  ALU,A-1 Q,F K,1IFZ ; AD_ARTI=1 (address arithmetic) -> <op>1 continuation
```
The `K,1IFZ` here sets K to 1 if the (digit-count - 1) ALU result is zero: this is an INTERNAL
loop-termination flag used during the digit loop, NOT the architectural flag result. Manual 17.x
does not list K among the affected bits; K is treated as UNCHANGED at instruction completion
(INFERRED - the loop reuses K internally but the documented effect set is {Z,S,BO,(IVO)}).

## PADD / PADDR - Packed add (and rounded)
Opcode (octal): PADD 177260B, PADDR 177205B. Files: `Padd.c`, `Paddr.c`.
Microcode: ADDBCD @002043 (PADD), ADDBCDR @002045 (PADDR) -> ADDBCD1 (@8053) -> ADDBCD_2/BCD_ADD_* loop.
FUNCTION: `<a> + <b> -> <c>` in packed decimal; PADDR rounds before storing. BCD overflow (BO) if
the destination field is too narrow for the sum.
RESULT/side-effects: writes the packed-decimal `<c>` operand.
STATUS FLAGS:
| K | Z | C | O | S | + |
|---|---|---|---|---|---|
| UNCHANGED | CONDITIONAL (sum==0) | CLEARED | CLEARED | CONDITIONAL (sum sign) | BO CONDITIONAL; IVO CONDITIONAL |
TRAP: Addressing, BCD overflow (BO), Invalid operation (IVO). CITATION: ADDBCD @002043 / ADDBCDR @002045; Manual sec 17.2.

## PSUB / PSUBR - Packed subtract (and rounded)
Opcode (octal): PSUB 177261B, PSUBR 177206B. Files: `Psub.c`, `Psubr.c`.
Microcode: SUBBCD @002047, SUBBCDR @002051 -> SUBBCD1 (@8059) -> SUBBCD_* loop.
FUNCTION: `<a> - <b> -> <c>` packed decimal; PSUBR rounds. BO if destination too narrow.
STATUS FLAGS: K UNCHANGED; Z CONDITIONAL(diff==0); C CLEARED; O CLEARED; S CONDITIONAL(diff sign);
BO CONDITIONAL; IVO CONDITIONAL.
TRAP: Addressing, BCD overflow (BO), Invalid operation (IVO). CITATION: SUBBCD @002047 / SUBBCDR @002051; Manual sec 17.3.

## PMPY / PMPYR - Packed multiply (and rounded)
Opcode (octal): PMPY 177264B, PMPYR 177221B. Files: `Pmpy.c`, `Pmpyr.c`.
Microcode: MPYBCD @002061, MPYBCDR @002063 -> MPYBCD1 (@8073) -> MPYBCD_1..4 (@9164+) loop.
FUNCTION: `<a> * <b> -> <c>` packed decimal; PMPYR rounds. Special case (Manual 17.4): an operand
with an invalid digit multiplied by zero gives result 0, not IVO.
STATUS FLAGS: K UNCHANGED; Z CONDITIONAL(product==0); C CLEARED; O CLEARED; S CONDITIONAL;
BO CONDITIONAL; IVO CONDITIONAL.
TRAP: Addressing, BCD overflow (BO), Invalid operation (IVO). CITATION: MPYBCD @002061 / MPYBCDR @002063; Manual sec 17.4.
UNKNOWN: MPYBCD_1..4 digit-by-digit multiply loop not traced to completion.

## PPACK / PPACKR - Convert ASCII to packed (and rounded)
Opcode (octal): PPACK 177265B, PPACKR 177222B. Files: `Ppack.c`, `Ppackr.c`.
Microcode: PACK @ (label `pack`, cell in 2079 region) / PACKR -> PACK1 (@8079) -> PACKBCD_1 (@9227) loop.
FUNCTION (Manual 17.7): ASCII-coded-decimal `<source>` -> packed `<dest>`; PPACKR rounds first.
If bit 26 of the dest descriptor is set, stored with sign code 1111 (unsigned); else dest gets the
sign of the source. Source sign taken from the SGN code of the source descriptor.
STATUS FLAGS: K UNCHANGED; Z CONDITIONAL(value-after-rounding==0); C CLEARED; O CLEARED;
S CONDITIONAL(value sign); BO CONDITIONAL; IVO CONDITIONAL.
TRAP: Addressing, BCD overflow (BO), Invalid operation (IVO). CITATION: PACK/PACKR -> PACK1 @8079 -> PACKBCD_1 @9227; Manual sec 17.7.
UNKNOWN: PACKBCD_1 packing loop not traced to completion.

## PUPACK / PUPACKR - Convert packed to ASCII (and rounded)
Opcode (octal): PUPACK 177266B, PUPACKR 177223B. Files: `Pupack.c`, `Pupackr.c`.
Microcode: UNPACK / UNPACKR -> UNPACK1 (@8085) / UNPACKR1 (@8087) loop.
FUNCTION (Manual 17.8): packed `<source>` -> ASCII `<dest>`; PUPACKR rounds first. Sign
representation from the SGN field of the dest descriptor; dest extended with leading ASCII zeros,
parity bit of all digits = 0.
STATUS FLAGS: K UNCHANGED; Z CONDITIONAL(value-after-rounding==0); C CLEARED; O CLEARED;
S CONDITIONAL; BO CONDITIONAL; IVO CONDITIONAL.
TRAP: Addressing, BCD overflow (BO), Invalid operation (IVO). CITATION: UNPACK/UNPACKR -> UNPACK1 @8085; Manual sec 17.8.
UNKNOWN: UNPACK1 unpacking loop not traced to completion.

--------------------------------------------------------------------------------
## STILL-UNRESOLVED ITEMS (need deeper microtrace)

1. Integer division inner loops (DIV_INT, DIV4_COMM, UDIV_COMM): restoring-division per-bit
   shift/subtract sequence and exact quotient/remainder register placement.
2. SUB / SUB2 / SUB3 A-vs-B bus ordering: the entry cell decodes as `B-A` with Rn on the A bus;
   the documented net effect is `Rn - operand`. The exact bus-to-operand binding for the subtract
   direction is INFERRED, not proven from the microcode alone.
3. AXI / IXI exponentiation loops (square-and-multiply, IXISPEC_1..5, SET_IOV accumulation):
   traced at entry/branch level only.
4. REM quotient-integer extraction (REMF_01 / REMD_01).
5. All packed-decimal digit loops (BCD_ADD_*, MPYBCD_1..4, PACKBCD_1, UNPACK1): the exact BO/IVO
   detection and rounding micro-steps, and the final disposition of the K flag after the internal
   K,1IFZ loop-control writes.

Cross-check result: NO substantive disagreement found between microcode and Manual on operation
semantics or documented flag effects. The one caveat is item 2 (subtract bus ordering) which the
microcode does not make unambiguous on its own.
