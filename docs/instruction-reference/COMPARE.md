# ND-500 COMPARE Category - Functional Behavior Reference (microcode-traced)

Purpose: a FUNCTIONAL, data-path reference for the ND-500 COMPARE-class
instructions, produced by TRACING THE MICROCODE and cross-checking the printed
manual. Every statement is taken directly from one of the sources named below.
Nothing is assumed; anything not resolvable from the traced routine is marked
"UNKNOWN (needs deeper microtrace)".

## Instruction set (enumerated from the source tree)

From `src/cpu/instructions/COMPARE/*.c`:

- `Comp.c`   -> COMP   (register compare: Rn - operand)
- `Comp2.c`  -> COMP2  (compare two operands: op1 - op2)
- `Pcomp.c`  -> PCOMP  (packed-decimal / BCD compare: a - b)
- `Scomp.c`  -> SCOMP  (byte string compare)  [the manual files this under STRING, chapter 14]
- `Test.c`   -> TEST   (test against zero: operand - 0)

## Sources (full absolute paths)

- PRIMARY spec (documented intent):
  `docs/ND-05.009.4 EN ND-500 Reference Manual.md`
- GROUND-TRUTH microcode (ND-5000 / 5800-A30):
  `$ND5000UC/microcode/MICRO-5800-A30.md`
- Micro-op field decoder:
  `$ND5000UC/manual/mnemonics.md`

Microcode cells are quoted as `octal-address: field field field ...`. Field
meanings come from `mnemonics.md` and are noted inline the first time they appear.

## Decoder facts used throughout (read directly from the sources)

Manual general status-bit rule (manual line 2022 and line 4040):
"Bits that are set, reset or left unaffected are mentioned explicitly. All data
status bits not mentioned are reset." / "Data status bits not mentioned in the
instruction description are always cleared after the instruction has been
executed." -> For every instruction below, any of {K,Z,C,O,S} the manual does NOT
list is CLEARED (0), it is not left unchanged.

Manual line 2020: "The majority of control and special instructions ... leave the
data status bits unaffected." (COMPARE is NOT one of those - compares write status.)

Status-bit meanings (manual): C = carry, K = flag, O = integer overflow,
S = sign, Z = zero. Traps: DR = descriptor range, FO = floating overflow,
FU = floating underflow, IVO = invalid operation, BO = BCD overflow.

Micro-op fields that carry the semantics (from `mnemonics.md`):
- ALU function (bits 127-124): `ALU,B-A` = B minus A; `ALU,A-B` = A minus B;
  `ALU,A` = pass A; `ALU,A-1` = decrement; `ALU,FZRO` = force 0; `ALU,AND`, `ALU,XOR`.
  `CRY,ONE` supplies carry-in 1 so `A-B`/`B-A` is a true two's-complement subtract.
- `TYP,DR` (bits 100-98 = 7) = datatype taken from the macro-instruction (so the
  same microcell serves BY/H/W); `TYP,BY/HW/W/F/DF` = fixed width; `TYP,BI` = bit.
- A/B/D operand selects (bits 96-76): `A,ALU,REG37` with `ORCON` overrides the A
  input from the operand specifier; `ORB,IN` supplies the B register number from
  the instruction's register field (n); `A,SARG SARG=nnn` = an immediate constant;
  `B,SC5` etc = a scratch register; `A,X1/A,X2` = index registers I1/I2.
- STATUS field (bits 75-72): `ST,SAVC` = "save status from ALU in COMPARE"
  (this is the compare-specific save that produces S = sign XOR overflow);
  `ST,SAVA` = save raw ALU status (Z,C,O,S); `ST,SAVF` = save floating status;
  `ST,SAVB` = save BCD status; `K,ONE`/`K,ZRO`/`K,1IFZ` = set/clear/conditional-K.
- Memory (bits 41-32): `READ` = read data memory; `ADACT` = address-arithmetic
  activate (compute the effective address). `AAP2,SUBBA` = the extended arithmetic
  processor computes B-A in floating point; `AAPSYNC` = wait for AAP result.
- Sequencer: `G,OOPS` = fetch the NEXT macro-instruction and its first operand
  specifier (this marks the LAST microcell of a routine); `G,OPS` = fetch the next
  operand specifier of the CURRENT instruction; `T,RETURN` = return to caller.
  The `T,JMP COND,MSEXO` that decorates almost every cell is the pipeline's
  branch-prediction test (EXOR of S and O), not part of the compare arithmetic.

Important consequence of `ST,SAVC` vs the manual: the manual lists, for COMP /
COMP2 / TEST, exactly Z, (S = signbit XOR Overflow), and C. It does NOT list O.
By the line-4040 rule the O bit is therefore CLEARED; the "overflow" is consumed
inside the S computation (that is precisely what `ST,SAVC` does) and is not left
visible as a separate O bit.

---

## COMP - register compare

Opcode (manual section 10.9, lines 4485-4519), one variant per datatype/register n=1..4:

| Variant  | hex           | octal            | width |
|----------|---------------|------------------|-------|
| BIn COMP | 0FC18H+(n-1)  | 176030B+(n-1)    | bit   |
| BYn COMP | 030H+(n-1)    | 060B+(n-1)       | byte  |
| Hn COMP  | 0FC1CH+(n-1)  | 176034B+(n-1)    | half  |
| Wn COMP  | 034H+(n-1)    | 064B+(n-1)       | word  |
| Fn COMP  | 038H+(n-1)    | 070B+(n-1)       | float |
| Dn COMP  | 03CH+(n-1)    | 074B+(n-1)       | double|

### Functional pseudocode (integer BY/H/W)

Traced microcell (single cycle) - COMP `000241`:
`ALU,B-A CRY,ONE TYP,DR A,ALU,REG37 ORB,IN ST,SAVC ... G,OOPS READ ADACT ORCON=04`

```
1. operand = read_memory(effective_address)      ; READ ADACT, A input via ORCON (operand specifier)
2. B       = Rn                                   ; ORB,IN -> register field n from the opcode
3. diff    = B - A  =  Rn - operand               ; ALU,B-A with CRY,ONE (true subtract), width = TYP,DR
4. save COMPARE status from diff  (ST,SAVC):
       Z = (diff == 0)
       C = carry-out of the subtract  (1 = no borrow = Rn >= operand, unsigned)
       S = signbit(diff) XOR overflow(diff)       ; "true comparison" - sign corrected on overflow
       O = 0   (not mentioned by the manual -> cleared)
5. diff is DISCARDED (Rn not written)
6. G,OOPS -> fetch next macro-instruction
```

### Functional pseudocode (float Fn / double Dn)

Traced - COMPF `002143` (single float), COMPD `002147` (double):
`ALU,FZRO AAP2,SUBBA TYP,F A,ALU,REG37 ORB,IN ... READ ADACT` then
`002144: ALU,A ... ST,SAVF ... AAPSYNC` then `002145: ... ST,LOAD ... G,OOPS`.

```
1. operand = read_memory(effective_address)       ; the ALU is forced to 0 (ALU,FZRO)
2. B = Rn                                          ; ORB,IN
3. diff = AAP2 compute (B - A) in floating point   ; AAP2,SUBBA (extended FP unit)
4. wait for AAP (AAPSYNC), then save FLOATING status (ST,SAVF):
       Z = (diff == 0.0)                           ; +0.0 and -0.0 compare equal
       S = (diff < 0.0)
   (002146 branches to ILLEG if the FP unit flags an illegal/exception state)
5. diff discarded; G,OOPS fetch next
```

### Operands & datatypes
- One operand `<operand/r/t>` of the instruction datatype t in {BI,BY,H,W,F,D}.
- Register Rn (n=1..4) is the implicit first operand: integer register In for
  BI/BY/H/W, floating register An/Dn for F/D.

### Result / side-effects
- No register or memory is written. Only the data-status bits change.

### Status flags

| Flag | Effect (integer BY/H/W/BI)         | Effect (float F/D)              |
|------|------------------------------------|---------------------------------|
| K    | CLEARED (not listed)               | CLEARED (not listed)            |
| Z    | SET if Rn == operand, else CLEARED | SET if equal, else CLEARED      |
| C    | CONDITIONAL: 1 if Rn >= operand (unsigned, no borrow), else 0 | CLEARED (not listed for float) |
| O    | CLEARED (folded into S by ST,SAVC) | CLEARED                         |
| S    | CONDITIONAL: signbit(diff) XOR overflow | CONDITIONAL: 1 if Rn < operand |

### Traps
- Integer: addressing traps only.
- Float/double: addressing traps, Floating overflow (FO), Floating underflow (FU)
  (manual line 4507). Microcode routes AAP illegal states via `[ADDR=ILLEG]` at 002146/002153.

### Citation
- Microcode: COMP `000241`; COMPBI `000237`->`000240`->COMP_BI `003232`;
  COMPF `002143`-`002146`; COMPD `002147`-`002154`
  (`$ND5000UC/microcode/MICRO-5800-A30.md`).
- Manual: section 10.9, lines 4485-4519.

---

## COMP2 - compare two operands

Opcode (manual section 10.10, lines 4530-4566):

| Variant  | hex     | octal    |
|----------|---------|----------|
| BI COMP2 | 0FC15H  | 176025B  |
| BY COMP2 | 02DH    | 055B     |
| H  COMP2 | 0FC16H  | 176026B  |
| W  COMP2 | 02EH    | 056B     |
| F  COMP2 | 02FH    | 057B     |
| D  COMP2 | 040H    | 100B     |

### Functional pseudocode (integer BY/H/W)

Traced two-cycle routine - COMP2 `000244` then `000245`:
```
; 000244: ALU,A TYP,DR A,ALU,REG37 B,X1 D,SC5 ... G,OPS READ ADACT ORCON=04
1. op1 = read_memory(ea1)          ; ALU,A passes op1 through the ALU
2. SC5 = op1                       ; D,SC5 stashes op1
3. G,OPS -> fetch the SECOND operand specifier

; 000245: ALU,B-A CRY,ONE TYP,DR A,ALU,REG37 B,SC5 ST,SAVC ... G,OOPS READ ADACT ORCON=04
4. op2 = read_memory(ea2)          ; A input via operand specifier
5. B   = SC5 = op1
6. diff = B - A = op1 - op2        ; ALU,B-A, CRY,ONE, width = TYP,DR
7. save COMPARE status (ST,SAVC):
       Z = (diff == 0)
       C = carry-out (1 = no borrow = op1 >= op2 unsigned)
       S = signbit(diff) XOR overflow
       O = 0 (not listed -> cleared)
8. diff discarded; G,OOPS fetch next
```

### Functional pseudocode (float F / double D)

Traced - COMP2F `002155`-`002160`, COMP2D `002161`-...:
```
1. op1 = read_memory(ea1) ; stash (D,SC7 / D,SC10)
2. G,OPS ; op2 = read_memory(ea2)
3. diff = AAP2 compute (op1 - op2) floating   ; AAP2,SUBBA
4. AAPSYNC, then ST,SAVF: Z=(diff==0.0), S=(diff<0.0)
5. diff discarded; G,OOPS fetch next
```

### Operands & datatypes
- Two operands `<op1/r/t>,<op2/r/t>`, both of datatype t in {BI,BY,H,W,F,D}.

### Result / side-effects
- Nothing written except the data-status bits.

### Status flags

| Flag | Effect (integer)                    | Effect (float F/D)            |
|------|-------------------------------------|-------------------------------|
| K    | CLEARED                             | CLEARED                       |
| Z    | SET if op1 == op2, else CLEARED     | SET if equal, else CLEARED    |
| C    | CONDITIONAL: 1 if op1 >= op2 (no borrow) | CLEARED (not listed)     |
| O    | CLEARED (folded into S)             | CLEARED                       |
| S    | CONDITIONAL: signbit(diff) XOR overflow | CONDITIONAL: 1 if op1 < op2 |

### Traps
- Addressing traps; for F/D also FU and FO (manual line 4554).

### Citation
- Microcode: COMP2 `000244`-`000245`; COMP2BI `000242`->`000243`->COMP2_BI `003235`;
  COMP2F `002155`-`002160`; COMP2D `002161`+.
- Manual: section 10.10, lines 4530-4566.

---

## TEST - test against zero

Opcode (manual section 10.11, lines 4577-4611):

| Variant | hex   | octal |
|---------|-------|-------|
| BI TEST | 041H  | 101B  |
| BY TEST | 042H  | 102B  |
| H  TEST | 043H  | 103B  |
| W  TEST | 044H  | 104B  |
| F  TEST | 045H  | 105B  |
| D  TEST | 046H  | 106B  |

### Functional pseudocode (integer BY/H/W)

Traced - TEST `000250`:
`ALU,A-B CRY,ONE TYP,DR A,ALU,REG37 B,SC14 ST,SAVC ... G,OOPS READ ADACT ORCON=04`
```
1. operand = read_memory(ea)                  ; A input via operand specifier
2. B = SC14  (a scratch register holding 0)   ; see note - zero subtrahend
3. diff = A - B = operand - 0 = operand       ; ALU,A-B, CRY,ONE, width TYP,DR
4. save COMPARE status (ST,SAVC):
       Z = (operand == 0)
       C = carry-out of (operand - 0) = 1 ALWAYS   ; subtracting 0 never borrows
       S = signbit(operand) XOR overflow            ; overflow is 0 for x-0, so S = signbit(operand)
       O = 0 (not listed -> cleared)
5. operand not written; G,OOPS fetch next
```

Note on the zero subtrahend: the integer TEST subtracts a zero-valued B input
(`B,SC14`, and the sibling cell `000251` uses the explicit immediate `A,SARG
SARG=000000`). That the subtrahend is zero is confirmed by the semantics (TEST =
operand - 0) and by the always-1 carry the manual documents (C = 1 for integer).
The exact reason the ROM holds two integer cells (`000250` with `B,SC14` and
`000251` with `A,SARG=0 B,SC5`) - i.e. which operand kinds each dispatches for -
is UNKNOWN (needs the opcode->microaddress dispatch table, not in the traced file).

### Functional pseudocode (float F / double D)

Traced - F TEST `000252` (`TYP,F`), D TEST `000253` (`TYP,DF`):
`ALU,A TYP,F A,ALU,REG37 B,X1 D,SC1 ST,SAVC ... G,OOPS READ ADACT`
```
1. operand = read_memory(ea)          ; ALU,A simply passes the float value
2. save COMPARE status from the value itself (ST,SAVC):
       Z = (operand == 0.0)
       S = signbit(operand)           ; testing a float against 0 is just sign+zero of the value
3. G,OOPS fetch next
```
(No subtraction and no AAP are needed - the sign and zero of a float value ARE the
test-against-zero result. Note C is NOT forced for float; only integer TEST sets C=1.)

### Operands & datatypes
- One operand `<operand/r/t>`, t in {BI,BY,H,W,F,D}.

### Result / side-effects
- Nothing written except data-status bits.

### Status flags

| Flag | Effect (integer BY/H/W/BI)        | Effect (float F/D)          |
|------|-----------------------------------|-----------------------------|
| K    | CLEARED                           | CLEARED                     |
| Z    | SET if operand == 0, else CLEARED | SET if operand == 0.0       |
| C    | SET (always 1 for integer)        | CLEARED (not listed)        |
| O    | CLEARED                           | CLEARED                     |
| S    | CONDITIONAL: signbit(operand) (overflow=0) | CONDITIONAL: signbit(operand) |

### Traps
- Addressing traps only (manual line 4599). No FO/FU is listed for TEST.

### Citation
- Microcode: TEST `000250`-`000251`; F TEST `000252`; D TEST `000253`;
  BI TEST `000246`->`000247`->TEST_BI `003242`.
- Manual: section 10.11, lines 4577-4611.

---

## SCOMP - byte string compare  (manual files this under STRING, ch. 14.10)

Opcode (manual section 14.10, lines 8706-8740): `BY SCOMP` = 0FDACH = 176654B.
Format: `BY SCOMP (<source-1/r/BY/I1>=, <source-2/r/BY/I2>=)` - both operands are
implicit descriptors; I1 and I2 are the running byte indices.

### Functional pseudocode

Entry SCOMP `001315`-`001316` loads descriptor 1, fetches descriptor 2
(`G,OPSTRD`), saves the source EA (`EA1SAVE`), then jumps to the byte loop
`SCOMPBY_F01` (`006555`). The per-byte compare core is `006603`-`006612`:
```
; SCOMPBY_F12 006603..006606
loop:
    if I1 >= len(source1) or I2 >= len(source2): goto length_end
    s1 = read_byte(base1 + I1)            ; 006603 READ, TYP,BY -> SC10
    s2 = read_byte(base2 + I2)            ; 006604 READ        -> SC11
    diff = s1 - s2                        ; 006605/006606 ALU,A-B CRY,ONE TYP,BY, unsigned bytes
    if diff == 0 (COND,MZRO):             ; bytes equal
        I1 = I1 + 1                       ; 006611 SCOMPBY_F13: X1 = X1 + 1
        I2 = I2 + 1                       ; 006612:            X2 = X2 + 1
        goto loop
    else:                                 ; bytes differ -> 006607
        ST,SAVA from (s1 - s2)            ; C/S reflect the unsigned ordering of s1 vs s2
        K = 1                             ; 006610 K,ONE  (difference found)
        ; I1,I2 left pointing at the differing elements
        RETURN
length_end:
    ; one or both strings ended with no differing byte; K = 0
    ; Z / S set from which string is longer (see table)
    RETURN
```
Descriptor-range handling: if a pointer is outside its string on entry, the
routine reaches `SOUR_RANGE` (via `006563/006564/006566`) - no bytes are examined,
I1/I2 are left unmodified, and a Descriptor Range (DR) trap is raised.

### Terminating conditions & flags (manual section 14.10, and the clean copy in 14.11)

The section-14.10 table is slightly garbled in the manual markdown; the identical
mechanism is stated cleanly for SCOTR at 14.11 (lines ~8760-8790). Canonical:

| Termination                         | K | Z | S | I1,I2 result           |
|-------------------------------------|---|---|---|------------------------|
| both operands outside string        | 0 | 1 | 0 | unmodified, DR trap    |
| exact match (equal, both ended)     | 0 | 1 | 0 | next element           |
| source-1 longer (src1 > src2)       | 0 | 0 | 0 | next element           |
| source-2 longer (src1 < src2)       | 0 | 0 | 1 | next element           |
| greater byte in source-1 (src1>src2)| 1 | 0 | 0 | differing elements     |
| smaller byte in source-1 (src1<src2)| 1 | 0 | 1 | differing elements     |

So the sign convention is: S = 1 exactly when source-1 sorts BEFORE source-2
(src1 < src2), for both the K=0 (length) and K=1 (byte) cases - consistent with
the COMP sign convention used by the shared conditional-jump instructions.
Bytes are treated as UNSIGNED (0..255).

### Operands & datatypes
- Two BY string descriptors; running indices in I1 (source-1) and I2 (source-2).

### Result / side-effects
- I1, I2 are UPDATED to the differing element (byte diff) or to the next element
  (length end / exact match); they are left UNMODIFIED if a DR trap fires.
- No memory is written.

### Status flags

| Flag | Effect                                                            |
|------|-------------------------------------------------------------------|
| K    | CONDITIONAL: 1 if a differing byte was found; 0 on length end / equal / DR |
| Z    | CONDITIONAL: 1 if the strings are equal (exact match), else 0     |
| S    | CONDITIONAL: 1 if source-1 < source-2 (see table), else 0         |
| C    | CLEARED for string operations (manual, string-status rule)        |
| O    | CLEARED for string operations                                     |

Microcode/manual note: the byte-difference cell uses `ST,SAVA` (which would carry
C from the s1-s2 subtract) but the architectural spec for string instructions
reports C=0 and O=0. The C-emulator clears C and O explicitly. Whether the real
hardware forces C=0 in a later fixup cell or leaves the last subtract's borrow in
C is not fully resolved by this trace; the DOCUMENTED result is C=0, O=0.

### Traps
- Descriptor Range (DR): any pointer outside its string on entry (no bytes touched).

### Citation
- Microcode: SCOMP `001315`-`001316` -> SCOMPBY_F01 `006555`; byte core
  `006603`-`006612`; DR path `SOUR_RANGE` via `006563`.
- Manual: section 14.10, lines 8706-8740 (flag table cross-checked against 14.11
  SCOTR, which is the un-garbled copy).

---

## PCOMP - packed-decimal (BCD) compare

Opcode (manual section 17.5, lines 11987-12019): `PCOMP` = 0FEB3H = 177263B.
(NOTE: the C source header `Pcomp.c` writes "176263 octal" - that is a typo;
0xFEB3 = 177263B. Verified by conversion.) Packed decimal is a hardware option.
Format: `PCOMP <=a/r/BCD=>, <=b/r/BCD=>` - both operands are BCD descriptors.

### Functional pseudocode

Traced entry chain: COMPBCD `002057` -> `002060` (`K,1IFZ`, address arithmetic)
-> COMPBCD1 `017571` (`K,ZRO`, read a halfword of BCD data) -> `017572`
(`ST,SAVA`, `EA1SAVE`, push) -> COMPBCD_1 `021613` -> STRTBCD `017620` (start-BCD
descriptor/scale setup). The per-decimal-digit subtract is the BCD arithmetic
core, e.g. BCD_10_COMP `021301`: `ALU,B-A CRY,ONE ... [ADDR=BCD_ADD_RND]`.
```
1. load descriptor A (operand 1) and descriptor B (operand 2)   ; STRTBCD setup
2. align both operands to a common decimal scale, zero-extend the shorter
   (manual: "operands are automatically shifted to the same decimal point
   position (scale) and extended with zeros")
3. validate BCD nibbles / sign codes; if any invalid -> IVO, set K = 1
4. diff = A - B                                                  ; digit-by-digit,
   ALU,B-A CRY,ONE per digit through the BCD path; sign/scale handled by the AAP
5. save BCD compare status:
       Z = (diff == 0)          ; +0 and -0 are equal (manual line 11826)
       S = signbit(diff)        ; PLAIN sign of the decimal difference (no XOR-overflow)
       K = IVO (invalid operation encountered)
6. diff DISCARDED (comparison only)
```

The full per-digit micro-loop (STRTBCD scale alignment, BCD_SUB / BCD_10_COMP
digit iteration, sign-code resolution, IVO detection) is elaborate and only traced
here to the entry + digit-subtract cell. The exact per-digit status composition in
the BCD AAP is UNKNOWN (needs deeper microtrace), but the ARCHITECTURAL flag result
is fully resolved by the manual (below).

### Operands & datatypes
- Two packed-decimal (BCD) descriptor operands a and b (P datatype).

### Result / side-effects
- Nothing written; comparison only.

### Status flags (manual section 17.5)

| Flag | Effect                                            |
|------|---------------------------------------------------|
| K    | CONDITIONAL: 1 on Invalid Operation (bad BCD)     |
| Z    | CONDITIONAL: 1 if a == b (difference == 0)        |
| S    | CONDITIONAL: 1 if a < b (difference sign bit)     |
| C    | CLEARED (not listed by the manual -> reset rule)  |
| O    | CLEARED (not listed -> reset rule)                |

Discrepancy note: the manual mentions only Z, S, K; by the line-4040 rule C and O
are CLEARED. `Pcomp.c` currently comments "C and O flags are unaffected" - that is
inconsistent with the manual, which requires them reset. Unlike the integer
compares, PCOMP's S is the PLAIN difference sign (no sign-XOR-overflow), because
BCD magnitude/sign compare cannot integer-overflow.

### Traps
- Addressing traps, Invalid Operation (IVO). (Manual line ~12005.)

### Citation
- Microcode: COMPBCD `002057`-`002060` -> COMPBCD1 `017571`-`017572` ->
  COMPBCD_1 `021613` -> STRTBCD `017620`; BCD digit-subtract core BCD_10_COMP
  `021301`, BCD_SUB_B-A `021244`.
- Manual: section 17.5, lines 11987-12019; BCD format 17.1 lines 11655+;
  negative-zero equality line 11826.

---

## Summary of cross-checks (microcode vs manual)

- COMP / COMP2 / TEST: microcode `ALU,B-A`/`ALU,A-B` + `ST,SAVC` implements the
  manual's "true comparison" (S = signbit XOR Overflow, C = carry from MSB). O is
  not a listed bit and is cleared (folded into S by the compare-specific save).
  AGREE.
- Float COMP/COMP2/TEST: microcode uses the AAP2 (`AAP2,SUBBA`) + `ST,SAVF`;
  manual lists FO/FU traps for COMP/COMP2 (not for TEST). AGREE.
- SCOMP: microcode K/Z/S mechanism matches the manual's termination table; the
  clean flag mapping (S=1 <=> source-1 < source-2) is the SCOTR/14.11 copy, since
  the 14.10 table is garbled in the markdown. AGREE on K/Z/S; C=O=0 is the
  documented result (microcode uses ST,SAVA on the per-byte diff - final C/O
  forcing not separately traced).
- PCOMP: microcode is the BCD AAP path (STRTBCD scale-align + per-digit subtract);
  manual gives Z / S(plain sign) / K(IVO). C and O must be CLEARED per the general
  rule (the C-emulator's "unaffected" comment disagrees with the manual).

## Still-UNKNOWN after this trace (need deeper microtrace)

1. TEST integer: why the ROM holds two integer cells (`000250` `B,SC14` vs
   `000251` `A,SARG=0 B,SC5`) and which operand kind each dispatches for - needs
   the opcode->microaddress (ICA) dispatch table.
2. SCOMP: whether the hardware forces C=0 / O=0 in a fixup after the `ST,SAVA`
   per-byte difference save, or leaves the last borrow in C (documented value is 0).
3. PCOMP: the exact per-digit status composition and IVO/sign-code resolution
   inside the BCD AAP loop (STRTBCD -> BCD_SUB / BCD_10_COMP), traced only to the
   entry and the digit-subtract cell here.
