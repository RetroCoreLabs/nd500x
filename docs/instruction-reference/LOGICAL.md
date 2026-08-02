# ND-500 Instruction Reference - LOGICAL Category

FUNCTIONAL behavior reference built by TRACING THE MICROCODE.

Sources of truth (full absolute paths):
- Microcode ROM:  $ND5000UC/microcode/MICRO-5800-A30.md
- Field decode:   $ND5000UC/manual/mnemonics.md
- Reference:      docs/ND-05.009.4 EN ND-500 Reference Manual.md
- Emulator C:     src/cpu/instructions/LOGICAL/*.c

Category members (from ls of the LOGICAL directory): AND, INV, INVC, OR, XOR.

---

## Conventions used below

Decoded microcode fields (from $ND5000UC/manual/mnemonics.md):

- `ALU,AND` / `ALU,OR` / `ALU,XOR` = the bitwise ALU function A op B.
- `ALU,ADIRC` = "ALU OUTPUT COMPLEMENTED" (one's-complement / NOT of the A input).
- `ALU,A-B CRY,C` = A minus B with the status Carry flag added as the ALU carry-in.
- `A,<src>` = A-bus operand source; `B,X1` = B-bus is index register X1 (dummy filler when B is unused).
- `ORB,IN` (mnemonics.md line 480) = "OR B-operand from instruction" - routes the instruction's
  operand (memory/immediate operand for AND/OR/XOR, or the register value for register-only ops)
  onto the B-bus.
- `A,ALU,REG37` / `D,ALU,REG37` = the instruction-selected register file port. mnemonics.md marks
  REG37 as "[UNDOCUMENTED GUESS] ... very common". INFERRED from routine structure to be the
  operand register Rn (the register named by the instruction). Marked inferred wherever relied on.
- `TYP,DR` (line 239) = "DATA TYPE CONTROLLED BY ICA" - the W/H/BY width comes from the opcode's
  datatype field. `TYP,BI` / `TYP,BY` = fixed bit / byte width.
- `D,<dest>` = where the result is written. `D,SC5`/`D,SC6` = scratch registers (temporaries).
- `ST,SAVA` (line 656) = "SAVE STATUS FROM ALU OPERATION" - writes the Z, C, O, S data-status
  bits from this ALU result.
- `READ` / `ADACT` = memory read with address-arithmetic active (this is where addressing traps
  can arise during operand fetch).
- `G,OOPS` (line 843) = "GET NEXT INSTRUCTION AND OPERAND SPECIFIER" - ends the routine and
  advances to the next instruction. Each of these five instructions is essentially ONE
  microinstruction (plus a separate tail for the BI datatype).
- `COND,MSEXO` = branch condition = XOR of S and O of the ALU result (used only for microcode
  dispatch sequencing, NOT a stored architectural flag).
- No `K,ONE` / `K,ZRO` / `K,1IFZ` field appears in any of these five routines -> the K flag is
  UNCHANGED by all of them (ground-truth: no K-modifier present in the microword).

Manual status-bit rule (Reference Manual, page 132, lines 2022 and 4040):
"All data status bits not mentioned are reset." and
"Data status bits not mentioned in the instruction description are always cleared after the
instruction has been executed. If the status bit is conditionally set a TRUE condition causes the
bit to be set (1), a FALSE condition causes it to be reset (0)."
This RESOLVES the previously-UNKNOWN C and O for AND/OR/XOR/INV: they are CLEARED. This agrees
with the microcode: a logical ALU function (AND/OR/XOR/ADIRC) produces carry-out = 0 and
overflow = 0, and ST,SAVA saves exactly those zeros.

Flag columns in the tables: K, Z, C, O, S.
- SET = always 1; CLEARED = always 0; UNCHANGED = not written; CONDITIONAL = 1 iff the stated
  condition holds, else 0.

Bit / BY / H datatypes: for the BI (single-bit) datatype the main opcode dispatches (via ORCON)
to a separate bit tail (AND_BI 003250, OR_BI 003252, XOR_BI 003255, INV_BI 003247, all in
$ND5000UC/microcode/MICRO-5800-A30.md). Those tails extract the single bit,
apply the logical function to it, write the low bit back, zero-fill the upper register (the
`CLEAR_SIGN` continuation), and set Z/S via ST,SAVA. The BY and H widths run the SAME main cell
with `TYP,DR` selecting the width; the "upper part zero filled" behaviour is the register-file
write masking to the datatype width. This matches the manual and the emulator's
`nd500_mask_to_datatype`.

---

## AND

Bitwise AND of a register with an operand.

Opcode (octal / hex, n = 1..4):
- BIn AND: 176714B + (n-1)  (0FDCCH)
- BYn AND: 176220B + (n-1)  (0FC90H)
- Hn  AND: 176224B + (n-1)  (0FC94H)
- Wn  AND: 000344B + (n-1)  (0E4H)

Microcode routine: label **AND**, octal address 000341
($ND5000UC/microcode/MICRO-5800-A30.md):
`ALU,AND TYP,DR A,ALU,REG37 ORB,IN D,ALU,REG37 ST,SAVA T,JMP COND,MSEXO TBC,NEXT G,OOPS READ ADACT [ADDR=ORBI] ORCON=04`
BI-datatype tail: **AND_BI** 003250 -> 003251 -> CLEAR_SIGN.

### Functional pseudocode
```
1. operand = fetch <operand> at the effective address    ; READ + ADACT (addressing traps here)
2. A = Rn                                                 ; A,ALU,REG37  (Rn, inferred)
3. B = operand                                            ; ORB,IN
4. result = A AND B                                       ; ALU,AND, width from TYP,DR (W/H/BY)
5. Rn = result                                            ; D,ALU,REG37, upper bits zero-filled for BY/H/BI
6. save Z,C,O,S from the ALU result                       ; ST,SAVA  (logical op -> C=0, O=0)
7. G,OOPS -> next instruction
; BI datatype: dispatch to AND_BI, AND the single extracted bit, zero-fill upper 31 bits, set Z/S.
```

### Operands + datatypes
- 1 operand: `<operand/r/t>` read at datatype width (BI = 1 bit, BY = 8, H = 16, W = 32 bits).
- Target register Rn (n = 1..4), integer register file.

### Result / side-effects
- Rn <- Rn AND operand. For BI/BY/H the upper part of Rn is zero-filled.
- No memory write (operand is read-only). Register file is the only architectural write.

### Status flags
| Flag | Effect      | Condition |
|------|-------------|-----------|
| K    | UNCHANGED   | no K field in microword |
| Z    | CONDITIONAL | 1 if result == 0 |
| C    | CLEARED     | logical ALU op yields carry 0; rule 4040 |
| O    | CLEARED     | logical ALU op yields overflow 0; rule 4040 |
| S    | CONDITIONAL | 1 if result sign bit (MSB of datatype) set |

### Trap conditions
- Addressing traps (during operand fetch: READ / ADACT). No arithmetic traps.

### Citation
- Microcode: **AND** @ 000341 (main), **AND_BI** @ 003250 in
  $ND5000UC/microcode/MICRO-5800-A30.md
- Manual: section 10.21 "And", Reference Manual page 157.
- Manual/microcode agree: op = Rn AND operand; status = Z, S (C, O cleared by rule 4040).

---

## OR

Bitwise inclusive OR of a register with an operand.

Opcode (octal / hex, n = 1..4):
- BIn OR: 176770B + (n-1)  (0FDF8H)   [manual page 158 prints "OFDB8H-(n-1)"; that hex and the
                                       minus sign are OCR errors - the correct base is 0FDF8H,
                                       confirmed by the emulator source And/Or opcode map]
- BYn OR: 176230B + (n-1)  (0FC98H)
- Hn  OR: 176234B + (n-1)  (0FC9CH)
- Wn  OR: 000240B + (n-1)  (0A0H)

Microcode routine: label **OR**, octal address 000344:
`ALU,OR TYP,DR A,ALU,REG37 ORB,IN D,ALU,REG37 ST,SAVA T,JMP COND,MSEXO TBC,NEXT G,OOPS READ ADACT [ADDR=XORBI] ORCON=04`
BI-datatype tail: **OR_BI** 003252 -> 003253 -> 003254 -> CLEAR_SIGN.

### Functional pseudocode
```
1. operand = fetch <operand> at effective address        ; READ + ADACT (addressing traps)
2. A = Rn                                                 ; A,ALU,REG37 (Rn, inferred)
3. B = operand                                            ; ORB,IN
4. result = A OR B                                        ; ALU,OR, width from TYP,DR
5. Rn = result                                            ; upper bits zero-filled for BY/H/BI
6. save Z,C,O,S from ALU result                           ; ST,SAVA  (C=0, O=0)
7. G,OOPS -> next instruction
; BI datatype: OR_BI extracts the bit, ORs it, zero-fills the upper 31 bits, sets Z/S.
```

### Operands + datatypes
- 1 operand `<operand/r/t>` at datatype width (BI/BY/H/W).
- Target register Rn (n = 1..4).

### Result / side-effects
- Rn <- Rn OR operand. BI/BY/H zero-fill upper part. No memory write.

### Status flags
| Flag | Effect      | Condition |
|------|-------------|-----------|
| K    | UNCHANGED   | no K field |
| Z    | CONDITIONAL | 1 if result == 0 |
| C    | CLEARED     | rule 4040 / logical ALU |
| O    | CLEARED     | rule 4040 / logical ALU |
| S    | CONDITIONAL | 1 if result sign bit set |

### Trap conditions
- Addressing traps only.

### Citation
- Microcode: **OR** @ 000344, **OR_BI** @ 003252 in
  $ND5000UC/microcode/MICRO-5800-A30.md
- Manual: section 10.22 "Or", page 158.
- Manual/microcode agree: op = Rn OR operand; status = Z, S (C, O cleared).

---

## XOR

Bitwise exclusive OR of a register with an operand.

Opcode (octal / hex, n = 1..4):
- BIn XOR: 176760B + (n-1)  (0FDF0H)   [manual page 159 prints "176714B+(n-1)" / "0FDCC4+(n-1)"
                                        for the BI form; those duplicate AND's code and are OCR
                                        errors - the correct base is 0FDF0H = 176760B, confirmed
                                        by the emulator source]
- BYn XOR: 176240B + (n-1)  (0FCA0H)
- Hn  XOR: 176244B + (n-1)  (0FCA4H)
- Wn  XOR: 000244B + (n-1)  (0A4H)

Microcode routine: label **XOR**, octal address 000347:
`ALU,XOR TYP,DR A,ALU,REG37 ORB,IN D,ALU,REG37 ST,SAVA T,JMP COND,MSEXO TBC,NEXT G,OOPS READ ADACT [ADDR=SHLBY] ORCON=04`
BI-datatype tail: **XOR_BI** 003255 -> 003256 -> 003257 -> CLEAR_SIGN.

### Functional pseudocode
```
1. operand = fetch <operand> at effective address        ; READ + ADACT (addressing traps)
2. A = Rn                                                 ; A,ALU,REG37 (Rn, inferred)
3. B = operand                                            ; ORB,IN
4. result = A XOR B                                       ; ALU,XOR, width from TYP,DR
5. Rn = result                                            ; upper bits zero-filled for BY/H/BI
6. save Z,C,O,S from ALU result                           ; ST,SAVA  (C=0, O=0)
7. G,OOPS -> next instruction
; BI datatype: XOR_BI extracts the bit, XORs it, zero-fills the upper 31 bits, sets Z/S.
```

### Operands + datatypes
- 1 operand `<operand/r/t>` at datatype width (BI/BY/H/W).
- Target register Rn (n = 1..4).

### Result / side-effects
- Rn <- Rn XOR operand. BI/BY/H zero-fill upper part. No memory write.

### Status flags
| Flag | Effect      | Condition |
|------|-------------|-----------|
| K    | UNCHANGED   | no K field |
| Z    | CONDITIONAL | 1 if result == 0 |
| C    | CLEARED     | rule 4040 / logical ALU |
| O    | CLEARED     | rule 4040 / logical ALU |
| S    | CONDITIONAL | 1 if result sign bit set |

### Trap conditions
- Addressing traps only.

### Citation
- Microcode: **XOR** @ 000347, **XOR_BI** @ 003255 in
  $ND5000UC/microcode/MICRO-5800-A30.md
- Manual: section 10.23 "Exclusive or", page 159.
- Manual/microcode agree: op = Rn XOR operand; status = Z, S (C, O cleared).

---

## INV

One's-complement (bitwise NOT) of a register. Register-only, no operand.

Opcode (octal / hex, n = 1..4):
- BIn INV: 177020B + (n-1)  (0FE10H)
- BYn INV: 177024B + (n-1)  (0FE14H)
- Hn  INV: 177030B + (n-1)  (0FE18H)
- Wn  INV: 000230B + (n-1)  (098H)

Microcode routine: label **INV**, octal address 000260:
`ALU,ADIRC TYP,DR A,ALU,REG37 B,X1 D,ALU,REG37 ST,SAVA T,JMP COND,MSEXO TBC,NEXT G,OOPS [ADDR=INVC] ORCON=00`
BI-datatype tail: **INVBI** 000257 -> **INV_BI** 003247.
(The `[ADDR=INVC]` link is only the next ROM cell reached by opcode dispatch, NOT a fall-through;
INV completes in this single microinstruction.)

### Functional pseudocode
```
1. A = Rn                                                 ; A,ALU,REG37 (Rn, inferred)
2. result = NOT A                                         ; ALU,ADIRC (ALU output complemented)
                                                          ;   width from TYP,DR (W/H/BY)
3. Rn = result                                            ; D,ALU,REG37; upper bits zero-filled for BY/H/BI
4. save Z,C,O,S from ALU result                           ; ST,SAVA (complement -> C=0, O=0)
5. G,OOPS -> next instruction
; NO memory access (no READ). Register-only.
; BI datatype: INVBI/INV_BI complement only the low bit and clear the upper 31 bits.
```

### Operands + datatypes
- 0 operands (register only). Datatype BI/BY/H/W selects width complemented.
- Target register Rn (n = 1..4).

### Result / side-effects
- Rn <- ~Rn. For BI/BY/H only the low part is complemented; the rest of the register is cleared.
- No memory access.

### Status flags
| Flag | Effect      | Condition |
|------|-------------|-----------|
| K    | UNCHANGED   | no K field |
| Z    | CONDITIONAL | 1 if result == 0 |
| C    | CLEARED     | complement op -> carry 0; rule 4040 |
| O    | CLEARED     | complement op -> overflow 0; rule 4040 |
| S    | CONDITIONAL | 1 if result sign bit set |

### Trap conditions
- None (manual 10.13). No memory read in the microcode -> no addressing trap either.

### Citation
- Microcode: **INV** @ 000260, **INV_BI** @ 003247 (via INVBI @ 000257) in
  $ND5000UC/microcode/MICRO-5800-A30.md
- Manual: section 10.13 "Invert", page 149.
- Manual/microcode agree: op = one's complement; status = Z, S (C, O cleared, no traps).

---

## INVC

One's-complement of a word register with the Carry flag added: Rn <- ~Rn + C.
Word datatype only. Used for multi-precision negation/arithmetic.

Opcode (octal / hex, n = 1..4):
- Wn INVC: 177420B + (n-1)  (0FF10H)

Microcode routine: label **INVC**, octal address 000261:
`ALU,A-B CRY,C TYP,DR A,SC14 ORB,IN D,ALU,REG37 ST,SAVA T,JMP COND,MSEXO TBC,NEXT G,OOPS ADDR=000262 ORCON=00`

### Functional pseudocode
```
1. A = SC14                                               ; A,SC14 = all-ones constant 0xFFFFFFFF
                                                          ;   (INFERRED: see notes below)
2. B = Rn                                                 ; ORB,IN brings the register value onto B
3. result = A - B + Carry                                 ; ALU,A-B with CRY,C (status Carry as carry-in)
                                                          ;   = 0xFFFFFFFF - Rn + C = (~Rn) + C
4. Rn = result                                            ; D,ALU,REG37 (Rn, inferred)
5. save Z,C,O,S from the ARITHMETIC ALU result           ; ST,SAVA (real subtract -> real C and O)
6. G,OOPS -> next instruction
; NO memory access. Word (32-bit) only.
```

Notes on the trace:
- The manual documents the operation as "~Rn + C". The microcode achieves it as an ARITHMETIC
  subtract A - B + C, not a dedicated NOT-plus-carry. For that to equal ~Rn + C, the A input
  (SC14) must supply the all-ones constant 0xFFFFFFFF, because (all-ones) - x == ~x bitwise
  (no borrows propagate). SC14 holding 0xFFFFFFFF is INFERRED from this requirement; the
  mnemonics table does not state SC14's contents. (Contrast INV @ 000260, which uses the
  dedicated ALU,ADIRC complement and therefore produces C=0/O=0.)
- Because INVC uses a real subtract, ST,SAVA captures a genuine carry-out and overflow -
  this is exactly why INVC has C and O effects while INV does not.

### Operands + datatypes
- 0 explicit operands (register only); the register value is the B input. Word (W, 32-bit) only.
- Target register Wn (n = 1..4).

### Result / side-effects
- Rn <- ~Rn + C. No memory access.

### Status flags
| Flag | Effect      | Condition |
|------|-------------|-----------|
| K    | UNCHANGED   | no K field |
| Z    | CONDITIONAL | 1 if result == 0 |
| C    | CONDITIONAL | 1 if the (~Rn + C) addition produced a carry out |
| O    | CONDITIONAL | 1 if signed overflow occurred |
| S    | CONDITIONAL | 1 if result sign bit (bit 31) set |

### Trap conditions
- Integer overflow (O) - manual 10.14; microcode's arithmetic A-B can raise overflow.

### Citation
- Microcode: **INVC** @ 000261 in $ND5000UC/microcode/MICRO-5800-A30.md
- Manual: section 10.14 "Invert with carry add", page 150.
- Manual/microcode agree: op = ~Rn + C; status Z, S, C, O; trap = integer overflow.
- Note vs emulator: src/cpu/instructions/LOGICAL/Invc.c computes
  ~value + C and sets C on carry-out, but does NOT set O (overflow) nor raise the integer-overflow
  trap that both the manual and the microcode (ST,SAVA on an arithmetic A-B) specify. Flagged as
  an emulator gap, not a microcode ambiguity.

---

## Emulator cross-check summary

The C implementations in src/cpu/instructions/LOGICAL/ match the traced
microcode for AND, OR, XOR, INV (Z/S set via nd500_set_flags_zs; C/O cleared implicitly by the
flag helper; upper bits cleared via nd500_mask_to_datatype). The one divergence found:

- Invc.c sets Z, S, C but not O, and does not raise the integer-overflow trap. Both the manual
  (10.14) and the microcode (ST,SAVA over an arithmetic A-B at cell 000261) call for O and the
  overflow trap. See the INVC section above.

## Residual UNKNOWNs (needs deeper microtrace / doc)

- `A,ALU,REG37` / `D,ALU,REG37` are labelled "[UNDOCUMENTED GUESS]" in
  $ND5000UC/manual/mnemonics.md. Their identification as the instruction-selected
  register Rn is INFERRED from routine structure and the documented per-instruction operation, not
  read from a field definition. UNKNOWN until the register-file addressing (ICA -> register port)
  is decoded.
- INVC's `A,SC14` is INFERRED to hold the all-ones constant 0xFFFFFFFF (required for A-B to equal
  ~Rn). The actual preload of SC14 was not traced. UNKNOWN until the scratch-register preload path
  is followed.
