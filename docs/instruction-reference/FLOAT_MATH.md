# ND-500 Instruction Reference: FLOAT_MATH

Functional behaviour reference for the FLOAT_MATH instruction category, built by
tracing the ND-5000/5800 microcode and cross-checking the ND-500 Reference Manual.

Sources:
- Microcode: `$ND5000UC/microcode/MICRO-5800-A30.md`
- Field decode: `$ND5000UC/manual/mnemonics.md`
- Manual: `docs/ND-05.009.4 EN ND-500 Reference Manual.md`
- Emulator sources: `src/cpu/instructions/FLOAT_MATH/*.c`

Note on evidence: everything below is read directly from the microcode listing,
the field-decode table, or the manual. Items that could not be resolved from
those sources are marked UNKNOWN. No behaviour is invented.

---

## 0. Shared facts (read once)

### 0.1 Data status bits (ND-500 status register, manual section 6.5.1)

| Code | Name               | Bit |
|------|--------------------|-----|
| Z    | zero               | 5   |
| C    | carry              | 6   |
| S    | sign               | 7   |
| O    | overflow (integer) | 9   |
| IVO  | invalid operation  | 11  |
| DZ   | divide by zero     | 12  |
| FU   | floating underflow | 13  |
| FO   | floating overflow  | 14  |
| BO   | BCD overflow       | 15  |

Manual rule (6.5.1, verbatim): "Bits that are set, reset or left unaffected are
mentioned explicitly. **All data status bits not mentioned are reset.**"
This is the rule used below to resolve every flag the flag-only pass left UNKNOWN:
if the manual's per-instruction "Data status bits" list does not name a bit, that
bit is CLEARED by the instruction.

Manual facts used repeatedly:
- C: "may be set only when performing integer arithmetic; otherwise it is cleared."
  Every FLOAT_MATH instruction produces a floating result or a conversion, so C is
  CLEARED unless the instruction is an integer<->integer conversion (still cleared,
  because conversions are pass/sign-extend/truncate, not carry-generating adds).
- O: integer overflow only. Floating overflow is reported in FO, not O.
- Floating overflow/underflow in a long instruction (POLY) traps at completion, not
  at the intermediate step; Z and S reflect the final result.

### 0.2 The "K" flag

K is NOT one of the defined ND-500 data status bits in table 6.5.1 (the architectural
data bits are Z, C, S, O, IVO, DZ, FU, FO, BO). K appears as an architectural result
bit ONLY in the two packed-decimal conversions in this category:
- PWCONV: `IVO or O -> K`
- WPCONV: `BO -> K`

In the microcode, K is an internal condition flag written by `K,ONE` / `K,ZRO` /
`K,1IFZ` (STATUS field values 1/2/3) and tested by `COND,K`. For every other
FLOAT_MATH instruction the manual does not list K as affected, so in the tables
below K is reported UNCHANGED for those (with the architectural bit position of K
noted UNKNOWN - it is not enumerated in 6.5.1).

### 0.3 How the transcendental functions are implemented (traced mechanism)

SIN, COS, TAN, ASIN, ACOS, ATAN, ATAN2, EXP, ALOG, ALOG2, ALOG10, SQRT and POLY are
NOT trapped to software - they are microcoded polynomial (Horner) approximations run
in the AAP2 floating-point unit. The common data path, traced from the entry cells at
octal 001354-001513 into the compute bodies at 024254-025702 and the shared loop at
025424:

1. Entry cell (e.g. `SINF` @ 001354): `ALU,A TYP,DR A,ALU,REG37 ... D,SC5 ... READ
   ADACT` - reads the argument operand from data memory (READ) into scratch SC5; the
   `TYP,DR` datatype is resolved by the operand specifier (F vs D chosen by the
   opcode's own entry point, not by TYP).
2. Argument reduction / sign handling (routine-specific cells, e.g. SINF_0..SINF_5,
   which mask the sign with `A,BM37`/`ALU,ANDCA`, range-reduce, and set a quadrant/
   sign selector).
3. Coefficient load: `... A,SARG SARG=00xxxx ... D,RFA1 ... T,LOAD IX*n` (e.g.
   SINF_CONST @ 017075) points the AAP scratch register-file address RFA1 at a
   coefficient table.
4. Horner loop `POLLYF*` (025424) / `POLLYD*` (025434):
   `AAP2,MUL TYP,F A,RF1D B,SC3` (multiply accumulator by x, pull next coeff via
   auto-decrementing RF1D) then `POLLYF++`/`POLLYF**`: `AAP2,ADD TYP,F A,RF1D B,SC11`
   (add next coefficient), decrement the loop counter `LCDECR`, loop until `COND,LCZ`
   (`F,RETURN,F,POP`). `AAPSYNC` waits for each AAP result.
5. Write-back `FWRITE_AAP` (026044) / `DWRITE_AAP`: `... D,ALU,REG37 ST,SAVF ...
   G,OOPS WRITE` - stores the AAP result into the destination float/double register
   and executes `ST,SAVF` = "SAVE STATUS FROM FLOATING OPERATION". This is the cell
   that writes the architectural Z/S (and FO/FU on over/underflow) from the floating
   result. `G,OOPS` fetches the next macro-instruction.

`ST,SAVF` is the floating-status save; the floating AAP condition sources are
`COND,MFS` (S), `COND,MFO` (FO), `COND,MFU` (FU), `COND,MDZ` (DZ). Because the write
is a floating operation, C and O are CLEARED (integer-only bits, per 0.1).

### 0.4 How the conversions are implemented (traced mechanism)

Two microcode families:
- Integer-to-integer width changes (BICONV/BYCONV/HCONV/WCONV among BI/BY/H/W): pass
  through the ALU with sign-extend or truncate (`ALU,A`, `ALU,OR`, `TYP,*`), status via
  `ST,SAVA` ("save status from ALU operation" -> Z,C,S,O). Bodies at 001514-001612 and
  003173+ (CONV_TO_DBI/BICONVBY_0...).
- Anything touching float/double uses the AAP:
  - integer -> float/double: `AAP2,CTF` (convert to floating) then `CTF`/`CTDF`
    finalizer with `ST,SAVF` and WRITE (e.g. WCONVF @ 002711, BYCONVF @ 002667).
  - float/double -> integer (truncate): `AAP2,CTI` then the `FL_INT`/`DFL_INT`
    range/overflow common routine (022652 / 022711) which checks the exponent and
    sets integer overflow; CTBY/CTHW/CTW (026122/026145/026167) test `COND,OVFL`.
  - float<->double: `AAP2,CBF` ("convert to other floating format"), e.g. FCONVD,
    DCONVF, DCONRF @ 002767.
  - rounding conversions (t2CONR): add the rounding constant `A,BM26` (FCONRBY/H/W @
    002750/2/4) or use `AAP2,CTIR`/`CBF`, then the same integer/float finalizer.
- Packed-decimal conversions (PWCONV/WPCONV) use the optional BCD hardware routines
  (`BCD_BIN` @ 022317 for packed->binary; PACK/BCD_ADD for binary->packed). See notes.

---

## 1. SIN - Sine

- Opcode: Fn SIN 0xFF58-0xFF5B (octal 177530B+(n-1)); Dn SIN 0xFF84-0xFF87 (177604B+(n-1))

FUNCTIONAL PSEUDOCODE:
```
1. arg = read_operand(F or D)                 ; SINF/SIND @ 001354/001356, READ->SC5
2. if |arg| > 65536.0 radians:                ; range guard
       Rn = 0.0 ; set IVO ; trap InvalidOperation
3. reduce arg modulo pi/2, capture quadrant + sign   ; SINF_0..SINF_5 @ 024341
4. y = Horner_polynomial(reduced_arg, sin_coeffs)     ; POLLYF*/POLLYD* @ 025424
5. apply quadrant sign
6. Rn = y                                     ; FWRITE_AAP: ST,SAVF, WRITE
```
OPERANDS: one source `<argument/r/t>`, t in {F,D}. Result -> An/Dn (n from opcode).
RESULT: sine(argument) in the addressed float/double accumulator. On range violation
the destination is set to 0.0.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED (not a documented result bit) | SET if result == 0, else CLEARED | CLEARED (floating op) | CLEARED (floating op; O is integer-only) | SET to result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Invalid Operation (IVO) if |argument| > 65536.0.
CITATION: microcode SINF @ 001354 / SIND @ 001356 -> SINF_0 @ 024341 -> POLLYF* @
025424 -> FWRITE_AAP @ 026044; manual section 12.5 (Sine).

---

## 2. COS - Cosine

- Opcode: Fn COS 0xFF60-0xFF63 (177540B+(n-1)); Dn COS 0xFF8C-0xFF8F (177614B+(n-1))

FUNCTIONAL PSEUDOCODE:
```
1. arg = read_operand(F or D)                 ; COSF/COSD @ 001364/001366
2. if |arg| > 65536.0: Rn = 0.0 ; IVO ; trap
3. cos(x) = sin(x + pi/2): COSF_0 @ 024335 seeds the sine reducer via SINF_CONST
4. y = Horner_polynomial(reduced_arg, sin_coeffs)     ; POLLYF*
5. Rn = y                                     ; FWRITE_AAP: ST,SAVF
```
OPERANDS: one source `<argument/r/t>`, t in {F,D}. Result -> An/Dn.
RESULT: cosine(argument); destination set to 0.0 on range violation.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Invalid Operation (IVO) if |argument| > 65536.0.
CITATION: microcode COSF @ 001364 / COSD @ 001366 -> COSF_0 @ 024335 (shares SINF
polynomial core) -> FWRITE_AAP; manual section 12.7 (Cosine).

---

## 3. TAN - Tangent

- Opcode: Fn TAN 0xFF68-0xFF6B (177550B+(n-1)); Dn TAN 0xFF94-0xFF97 (177624B+(n-1))

FUNCTIONAL PSEUDOCODE:
```
1. arg = read_operand(F or D)                 ; TANF/TAND @ 001374/001376
2. if |arg| > 65536.0: Rn = 0.0 ; IVO ; trap
3. reduce arg; compute sin-like and cos-like polynomials  ; TANF_0..TANF_5 @ 024410
4. tan = num / den  (AAP divide)              ; TANF_4/TANF_5 use POLLYF*, AAPSYNC
5. if den ~ 0 (tan -> infinity): FO trap
6. Rn = tan                                   ; FWRITE_AAP: ST,SAVF
```
OPERANDS: one source `<argument/r/t>`, t in {F,D}. Result -> An/Dn.
RESULT: tangent(argument); destination set to 0.0 on range violation. Near pi/2+n*pi
the quotient overflows.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Invalid Operation (IVO) if |argument| > 65536.0.
The emulator source also raises Floating Overflow (FO) near the pole; the manual lists
only IVO for TAN, so FO-at-pole is emulator behaviour, not manual-documented (noted as
a disagreement).
CITATION: microcode TANF @ 001374 / TAND @ 001376 -> TANF_0 @ 024410 -> POLLYF* ->
FWRITE_AAP; manual section 12.9 (Tangent).

---

## 4. ASIN - Arc sine

- Opcode: Fn ASIN 0xFF5C-0xFF5F (177534B+(n-1)); Dn ASIN 0xFF88-0xFF8B (177610B+(n-1))

FUNCTIONAL PSEUDOCODE:
```
1. arg = read_operand(F or D)                 ; ASINF/ASIND @ 001360/001362
2. if |arg| > 1.0: Rn = 0.0 ; IVO ; trap
3. reduce, load asin coefficients (ASIN_CON/ASIN_CON1)     ; ASINF_0 @ 024615
4. y = Horner_polynomial(...) ; result angle in [-pi/2, pi/2]
5. Rn = y                                     ; FWRITE_AAP: ST,SAVF
```
OPERANDS: one source `<argument/r/t>`, t in {F,D}. Result -> An/Dn.
RESULT: arcsine(argument), radians in [-pi/2, pi/2]; destination set to 0.0 on domain
violation (|argument| > 1).

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Invalid Operation (IVO) if |argument| > 1.0.
CITATION: microcode ASINF @ 001360 / ASIND @ 001362 -> ASINF_0 @ 024615 -> POLLYF* ->
FWRITE_AAP; manual section 12.6 (Arc sine).

---

## 5. ACOS - Arc cosine

- Opcode: Fn ACOS 0xFF64-0xFF67 (177544B+(n-1)); Dn ACOS 0xFF90-0xFF93 (177620B+(n-1))

FUNCTIONAL PSEUDOCODE:
```
1. arg = read_operand(F or D)                 ; ACOSF/ACOSD @ 001370/001372
2. if |arg| > 1.0: Rn = 0.0 ; IVO ; trap
3. acos(x) = pi/2 - asin(x): ACOSF_0 @ 024614 seeds SC1 with a bias then joins ASINF_1
4. y = pi/2 - asin_poly(x)                     ; result in [0, pi]
5. Rn = y                                     ; FWRITE_AAP: ST,SAVF
```
OPERANDS: one source `<argument/r/t>`, t in {F,D}. Result -> An/Dn.
RESULT: arccosine(argument), radians in [0, pi]; destination 0.0 on domain violation.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Invalid Operation (IVO) if |argument| > 1.0.
CITATION: microcode ACOSF @ 001370 / ACOSD @ 001372 -> ACOSF_0 @ 024614 (joins
ASINF_1) -> POLLYF* -> FWRITE_AAP; manual section 12.8 (Arc cosine).

---

## 6. ATAN - Arc tangent

- Opcode: Fn ATAN 0xFF6C-0xFF6F (177554B+(n-1)); Dn ATAN 0xFF98-0xFF9B (177630B+(n-1))
  (Manual prints float hex as "0FFC6H" - an OCR error; octal 177554B = 0xFF6C, which
  matches the emulator source. Octal is authoritative.)

FUNCTIONAL PSEUDOCODE:
```
1. arg = read_operand(F or D)                 ; ATANF/ATAND @ 001400/001403
2. build a coefficient-table selector in SC5 (ATANF cells 001401-001402: A,BM36 | BM26)
3. reduce arg, choose table region              ; ATANF_0 @ 024746, ATANF_DIV/ANGLE
4. y = Horner_polynomial(...) ; result angle in [-pi/2, pi/2]
5. Rn = y                                     ; FWRITE_AAP: ST,SAVF
```
OPERANDS: one source `<argument/r/t>`, t in {F,D}. Result -> An/Dn.
RESULT: arctangent(argument), radians in [-pi/2, pi/2]. Defined for all reals - no
domain trap.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps only (manual lists no IVO; emulator source likewise
raises no IVO - consistent).
CITATION: microcode ATANF @ 001400 / ATAND @ 001403 -> ATANF_0 @ 024746 -> POLLYF* ->
FWRITE_AAP; manual section 12.10 (Arc tangent).

---

## 7. ATAN2 - Arc tangent, two arguments

- Opcode: Fn ATAN2 0xFF70-0xFF73 (177560B+(n-1)); Dn ATAN2 0xFF9C-0xFF9F (177634B+(n-1))

FUNCTIONAL PSEUDOCODE:
```
1. num = read_operand1 ; den = read_operand2  ; ATAN2F @ 001410 reads den->SC7 (G,OPS),
                                                 then num->SC5 @ 001411
2. if num == 0 and den == 0: Rn = 0.0 ; IVO ; trap
3. q = num / den (AAP divide) ; ratio -> the ATANF core, with quadrant from signs
4. y = arctan(q) placed in the correct quadrant, range (-pi, pi)   ; joins ATANF_0
5. Rn = y                                     ; FWRITE_AAP: ST,SAVF
```
OPERANDS: two sources `<num/r/t>, <den/r/t>`, t in {F,D}. Result -> An/Dn.
RESULT: arctangent(num/den) in the correct quadrant, radians in (-pi, pi); destination
0.0 if both operands are zero.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Invalid Operation (IVO) if num == 0 and den == 0.
CITATION: microcode ATAN2F @ 001410 / ATAN2D @ 001413 -> ATANF_0 @ 024746 (shared
core after the divide) -> POLLYF* -> FWRITE_AAP; manual section 12.11 (Arc tangent two
argument). PARTIAL: the exact quadrant-selection cells between the divide and ATANF_0
were followed only to the shared entry, not step-by-step - see unresolved list.

---

## 8. EXP - Exponential (e**x)

- Opcode: Fn EXP 0xFF74-0xFF77 (177564B+(n-1)); Dn EXP 0xFFA0-0xFFA3 (177640B+(n-1))
  (Manual prints float hex as "0FF7U4" - OCR garbage; octal 177564B = 0xFF74 matches source.)

FUNCTIONAL PSEUDOCODE:
```
1. arg = read_operand(F or D)                 ; EXPF/EXPD @ 001417/001421
2. if arg > 255*ln(2) (~176.75): Rn = +max_float (~5.8E76) ; IVO ; trap
   if arg < -255*ln(2):          Rn = 0.0    ; (underflow to zero)
3. split arg = k*ln(2) + r ; load exp coefficients (EXPF_CONST @ 017246)
4. m = 2**k ; y = m * Horner_polynomial(r, exp_coeffs)   ; EXPF_0/EXPF_4, EXPRUT_0
5. Rn = y                                     ; FWRITE_AAP: ST,SAVF
```
OPERANDS: one source `<argument/r/t>`, t in {F,D}. Result -> An/Dn.
RESULT: e**argument. Overflow -> largest float and IVO; deep underflow -> 0.0.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CLEARED | CLEARED - manual states `0 -> S` (e**x is never negative) |

TRAP CONDITIONS: Addressing traps; Invalid Operation (IVO) if argument > 255*ln(2).
(Emulator source additionally references FO/FU trap helpers; the manual documents the
overflow case as IVO with the result clamped to max float - noted as a wording
difference between manual "IVO" and emulator "FO".)
CITATION: microcode EXPF @ 001417 / EXPD @ 001421 -> EXPF_0 @ 024254 -> EXPRUT_0 @
023527 -> POLLYF* -> FWRITE_AAP; manual section 12.12 (Exponential).

---

## 9. ALOG - Natural logarithm (ln)

- Opcode: Fn ALOG 0xFF78-0xFF7B (177570B+(n-1)); Dn ALOG 0xFFA4-0xFFA7 (177644B+(n-1))

FUNCTIONAL PSEUDOCODE:
```
1. arg = read_operand(F or D)                 ; ALOGF/ALOGD @ 001423/001425
2. if arg <= 0: Rn = -5.8E76 (max negative) ; IVO ; trap
3. decompose arg = m * 2**e, m in [1,2) ; f = (m-1)/(m+1)  ; ALOGF_1/ALOGF_2 @ 025251
   using AAP2,SUBBA / AAP2,ADD (025256/025266)
4. ln(m) = 2*(f + f^3/3 + ...) ; ln(arg) = e*ln(2) + ln(m)  ; ALOGF_5/ALOGF_6 + POLLYF*
5. Rn = ln(arg)                               ; ALOGF_END @ 025307 -> FWRITE_AAP: ST,SAVF
```
OPERANDS: one source `<argument/r/t>`, t in {F,D}. Result -> An/Dn.
RESULT: ln(argument). For argument <= 0 the destination is set to -5.8*10**76 and IVO
is raised.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Invalid Operation (IVO) if argument <= 0.
CITATION: microcode ALOGF @ 001423 / ALOGD @ 001425 -> ALOGF_0 @ 025246 ->
ALOGF_1..ALOGF_6 (025251-025266) -> POLLYF* -> ALOGF_END @ 025307 -> FWRITE_AAP;
manual section 12.13 (Natural logarithm).

---

## 10. ALOG2 - Binary logarithm (log2)

- Opcode: Fn ALOG2 0xFF7C-0xFF7F (177574B+(n-1)); Dn ALOG2 0xFFA8-0xFFAB (177650B+(n-1))

FUNCTIONAL PSEUDOCODE:
```
1. arg = read_operand(F or D)                 ; ALOG2F/ALOG2D @ 001427/001431
2. if arg <= 0: Rn = -5.8E76 ; IVO ; trap
3. log2(arg) = ln(arg) * (1/ln(2))            ; shares the ALOG core, scales by 1/ln(2)
4. Rn = log2(arg)                             ; FWRITE_AAP: ST,SAVF
```
OPERANDS: one source `<argument/r/t>`, t in {F,D}. Result -> An/Dn.
RESULT: log base 2 of argument; -5.8E76 + IVO for argument <= 0.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Invalid Operation (IVO) if argument <= 0.
CITATION: microcode ALOG2F @ 001427 / ALOG2D @ 001431 -> ALOG2F_0 @ 023xxx (shares
ALOG polynomial core) -> POLLYF* -> FWRITE_AAP; manual section 12.14 (Binary logarithm).

---

## 11. ALOG10 - Common logarithm (log10)

- Opcode: Fn ALOG10 0xFF80-0xFF83 (177600B+(n-1)); Dn ALOG10 0xFFAC-0xFFAF (177654B+(n-1))

FUNCTIONAL PSEUDOCODE:
```
1. arg = read_operand(F or D)                 ; ALOG10F/ALOG10D @ 001433/001435
2. if arg <= 0: Rn = -5.8E76 ; IVO ; trap
3. log10(arg) = ln(arg) * (1/ln(10))          ; shares the ALOG core, scales by 1/ln(10)
4. Rn = log10(arg)                            ; FWRITE_AAP: ST,SAVF
```
OPERANDS: one source `<argument/r/t>`, t in {F,D}. Result -> An/Dn.
RESULT: log base 10 of argument; -5.8E76 + IVO for argument <= 0.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Invalid Operation (IVO) if argument <= 0.
CITATION: microcode ALOG10F @ 001433 / ALOG10D @ 001435 -> ALOG10F_0 (shares ALOG
core) -> POLLYF* -> FWRITE_AAP; manual section 12.15 (Common logarithm).

---

## 12. SQRT - Square root

- Opcode: Fn SQRT 0xFCD4-0xFCD7 (176324B+(n-1)); Dn SQRT 0xFCD8-0xFCDB (176330B+(n-1))

FUNCTIONAL PSEUDOCODE:
```
1. arg = read_operand(F or D)                 ; SQRTF/SQRTD @ 001477/001501
2. if arg < 0: Rn = 0.0 ; IVO ; trap
3. if arg == 0: Rn = 0.0 (Z set)
4. Newton/seed-and-refine iteration in AAP     ; SQRTF_0 @ 017407 -> SQRTF_00..SQRTF_101
   (loop pushes/pops sequencer stack; each step refines mantissa; exponent halved)
5. Rn = sqrt(arg)                             ; write path sets Z via the result
```
OPERANDS: one source `<argument/r/t>`, t in {F,D}. Result -> An/Dn.
RESULT: sqrt(argument). Negative argument -> result 0.0 and IVO.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CLEARED | CLEARED - manual lists only `result==0 -> Z` (sqrt result is non-negative, so S is not set) |

TRAP CONDITIONS: Addressing traps; Invalid Operation (IVO) if argument < 0.
CITATION: microcode SQRTF @ 001477 / SQRTD @ 001501 -> SQRTF_0 @ 017407 (iterative
refine, SQRTF_00..SQRTF_101); manual section 12.4 (Square root). Manual "Data status
bits: result = 0 -> Z" (S not mentioned -> S CLEARED by rule 6.5.1).

---

## 13. POLY - Polynomial evaluation

- Opcode: Fn POLY 0xFCE0-0xFCE3 (176340B+(n-1)); Dn POLY 0xFCE4-0xFCE7 (176344B+(n-1))

FUNCTIONAL PSEUDOCODE:
```
1. x   = read_operand(x/r/t)                  ; POLYF @ 001503 (F) / POLYD @ 001507 (D)
2. m   = read constant operand (BY, degree)   ; POLYF cell 001504: TYP,BY -> SC11 -> LC
   if m is not a positive constant < 256: trap Illegal Operand Specifier (IOS)
                                                ; POLYF_0 falls to ILL_OP_SPEC @ 001506
3. acc = c_m                                   ; POLYF_0 @ 025667 primes the accumulator
4. loop i = m-1 downto 0:                      ; POLLYF*/POLYF_2 @ 025702, AAP2,MUL/ADD
       acc = acc * x + c_i                     ; LC counts the m+1 coefficients
5. Rn = acc                                   ; FWRITE_AAP: ST,SAVF
   (FO/FU, if any at an intermediate step, are deferred to instruction completion)
```
OPERANDS: `<x/r/t>, <m/s/BY>, <cm/r/t>, ..., <c1/r/t>, <c0/r/t>` - the argument, a byte
degree constant m, then m+1 coefficients. t in {F,D}. Result -> An/Dn.
RESULT: c_m*x^m + ... + c1*x + c0. Z and S reflect the final result.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if final result==0 else CLEARED (manual note: for POLY the Z bit is NOT forced by intermediate underflow) | CLEARED | CLEARED (O is integer overflow; not used) | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Floating Overflow (FO); Floating Underflow (FU);
Illegal Operand Specifier (IOS) if m is not a positive constant < 256. FO/FU are
reported at instruction completion even if they occurred at an intermediate term.
CITATION: microcode POLYF @ 001503 / POLYD @ 001507 -> POLYF_0 @ 025667 -> POLLYF* @
025424 (Horner loop, LCDECR/COND,LCZ) -> FWRITE_AAP; manual section 12.3 (Polynomial).

---

## 14. BICONV - Convert to bit

- Opcode: BY BICONV 0xFD49 (176511B); H BICONV 0xFD4E (176516B); W BICONV 0xFD53
  (176523B); F BICONV 0xFD58 (176530B); D BICONV 0xFD5D (176535B).
  (Also the reverse "convert to bit" from BI is the identity; the "to bit" opcodes are
  the ones above.)

FUNCTIONAL PSEUDOCODE:
```
1. src = read_operand(t1)                      ; entry cells @ 001514+ (BICONVBY/H/W/F/D)
2. bit = (src != 0) ? 1 : 0                     ; ALU test, K/condition from zero test
3. dest = bit  (stored as a bit)                ; CONV_TO_DBI / BICONV*_1 write path
```
OPERANDS: `<source/r/t1>, <dest/w/BI>`. t1 in {BY,H,W,F,D}. Result stored to dest bit.
RESULT: dest bit set if source != 0, cleared otherwise.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 (source was 0) else CLEARED | CLEARED | CLEARED | result sign bit (CONDITIONAL; a single bit result is 0 or 1 so S is effectively 0) |

TRAP CONDITIONS: Addressing traps. (No integer overflow possible converting to a bit.)
CITATION: microcode BICONVBY @ 001514, BICONVH @ 001516, BICONVW @ 001520, BICONVF @
001522, BICONVD @ 001524 -> BICONVBY_0 @ 003175 / CONV_TO_DBI @ 003173; manual section
15.2 (Data type conversion). Manual data status bits: result==0 -> Z, result.signbit -> S.

---

## 15. BYCONV - Convert to byte

- Opcode: BI BYCONV 0xFD44 (176504B); H BYCONV 0xFD4F (176517B); W BYCONV 0xFD54
  (176524B); F BYCONV 0xFD59 (176531B); D BYCONV 0xFD5E (176536B).

FUNCTIONAL PSEUDOCODE:
```
1. src = read_operand(t1)
2. case t1:
     BI:  dest = src ? 1 : 0                    ; BYCONVBI @ 001526 (widen bit->byte)
     H,W: dest = truncate_to_8(src)             ; if bits above bit7 significant -> IOV
     F,D: dest = trunc_toward_zero(src) as int  ; BYCONVF @ 002667 / BYCONVD @ 002673,
          AAP2,CTF path is for the *reverse*; here float->byte uses AAP integer-part
          then range-checks -128..127 -> IOV on overflow
3. dest stored (byte)                           ; ST,SAVA / CTF finalizer ST,SAVF
```
OPERANDS: `<source/r/t1>, <dest/w/BY>`. t1 in {BI,H,W,F,D}. Result -> dest byte.
RESULT: source value as an 8-bit byte. Longer->byte truncates MSBs; float->byte
truncates toward zero. Out-of-range value raises integer overflow.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CONDITIONAL: SET on integer overflow (value outside -128..127), else CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Integer Overflow (O) if the value does not fit in a
signed byte.
CITATION: microcode BYCONVBI @ 001526, BYCONVH/W @ 001532/001534, BYCONVF @ 002667,
BYCONVD @ 002673 (float->int via AAP, CTBY @ 026122 tests COND,OVFL); manual 15.2.

---

## 16. HCONV - Convert to halfword

- Opcode: BI HCONV 0xFD45 (176505B); BY HCONV 0xFD4A (176512B); H HCONV 0xFD55
  (176525B, halfword->halfword identity); W HCONV 0xFD5A (176532B); D HCONV 0xFD5F
  (176537B). (F HCONV 0xFD5A per manual table; the emulator groups float/double->H here.)

FUNCTIONAL PSEUDOCODE:
```
1. src = read_operand(t1)
2. case t1:
     BI:  dest = src ? 1 : 0                     ; widen
     BY:  dest = sign_extend_8_to_16(src)        ; HCONVBY @ 001542
     H:   dest = src                             ; identity (HCONV 0xFD55)
     W:   dest = truncate_to_16(src) ; IOV if MSBs significant   ; HCONVW @ 001545
     F,D: dest = trunc_toward_zero(src) ; IOV if outside -32768..32767  ; HCONVF @ 002700,
          HCONVD @ 002704 (AAP integer-part, CTHW @ 026145 tests COND,OVFL)
3. dest stored (halfword)
```
OPERANDS: `<source/r/t1>, <dest/w/H>`. t1 in {BI,BY,H,W,F,D}. Result -> dest halfword.
RESULT: source value as a signed 16-bit halfword; overflow on narrowing raises IOV.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CONDITIONAL: SET on integer overflow (outside -32768..32767) else CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Integer Overflow (O) if value does not fit in a
signed halfword.
CITATION: microcode HCONVBI @ 001536, HCONVBY @ 001542, HCONVW @ 001545, HCONVF @
002700, HCONVD @ 002704 -> CTHW @ 026145; manual section 15.2.

---

## 17. WCONV - Convert to word

- Opcode: BI WCONV 0xFD46 (176506B); BY WCONV 0xFD4B (176513B); H WCONV 0xFD50
  (176520B); F WCONV 0xFD5B (176533B); D WCONV 0xFD60 (176540B).

FUNCTIONAL PSEUDOCODE:
```
1. src = read_operand(t1)
2. case t1:
     BI:  dest = src ? 1 : 0                     ; WCONVBI @ 001547
     BY:  dest = sign_extend_8_to_32(src)        ; WCONVBY @ 001553
     H:   dest = sign_extend_16_to_32(src)       ; WCONVH @ 001556
     F,D: dest = trunc_toward_zero(src) as int32 ; WCONVF @ 002711 uses AAP2,CTF entry
          then integer-part; WCONVD @ 002714. Overflow beyond +/-2^31 -> IOV (CTW @ 026167)
3. dest stored (word)
```
OPERANDS: `<source/r/t1>, <dest/w/W>`. t1 in {BI,BY,H,F,D}. Result -> dest word.
RESULT: source value as a signed 32-bit word; float/double truncated toward zero;
integer widening is by sign extension. Value outside word range raises IOV.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CONDITIONAL: SET on integer overflow (float/double outside word range) else CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Integer Overflow (O) if the (float/double) value does
not fit in a signed word.
CITATION: microcode WCONVBI @ 001547, WCONVBY @ 001553, WCONVH @ 001556, WCONVF @
002711, WCONVD @ 002714 -> FL_INT @ 022652 / CTW @ 026167; manual section 15.2.

---

## 18. FCONV - Convert to float

- Opcode: BI FCONV 0xFD47 (176507B); BY FCONV 0xFD4C (176514B); H FCONV 0xFD51
  (176521B); W FCONV 0xFD56 (176526B); D FCONV 0xFD61 (176541B).

FUNCTIONAL PSEUDOCODE:
```
1. src = read_operand(t1)
2. case t1:
     BI/BY/H/W: dest = float(int_value(src))     ; AAP2,CTF (convert to floating),
                                                    FCONVBI @ 001561, ..., WCONVF-style
     D:         dest = double_to_float(src)       ; FCONVD @ 001565, AAP2,CBF (convert
                                                    between float formats), may lose precision
3. dest stored (single float)                     ; CTF finalizer @ 002672 ST,SAVF, WRITE
```
OPERANDS: `<source/r/t1>, <dest/w/F>`. t1 in {BI,BY,H,W,D}. Result -> dest single float.
RESULT: source converted to single-precision float (not rounded - truncation of the
mantissa; the rounded form is FCONR).

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CLEARED (result is floating) | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps. (Integer->float and double->float without rounding
per 15.2 list only Integer Overflow (O) generically for the CONV family; producing a
float cannot integer-overflow, so effectively addressing traps only for these directions.)
CITATION: microcode FCONVBI @ 001561, FCONVD @ 001565 -> CTF @ 002672 (ST,SAVF); manual
section 15.2 (Data type conversion).

---

## 19. DCONV - Convert to double float

- Opcode: BI DCONV 0xFD48 (176510B); BY DCONV 0xFD4D (176515B); H DCONV 0xFD52
  (176522B); W DCONV 0xFD57 (176527B); F DCONV 0xFD5C (176534B).

FUNCTIONAL PSEUDOCODE:
```
1. src = read_operand(t1)
2. case t1:
     BI/BY/H/W: dest = double(int_value(src))     ; AAP2,CTF -> CTDF finalizer @ 002676
     F:         dest = float_to_double(src)        ; DCONVF @ 001574, AAP2,CBF (exact widening)
3. dest stored (double float)                      ; CTDF: ST,SAVF, WRITE (2 words)
```
OPERANDS: `<source/r/t1>, <dest/w/D>`. t1 in {BI,BY,H,W,F}. Result -> dest double float.
RESULT: source converted to double-precision float. Float->double is exact.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CLEARED (result is floating) | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps.
CITATION: microcode DCONVBI @ 001570, DCONVF @ 001574 -> CTDF @ 002676 (ST,SAVF);
manual section 15.2.

---

## 20. BYCONR - Convert to byte, rounded

- Opcode: F BYCONR 0xFE70 (177160B); D BYCONR 0xFE71 (177161B).

FUNCTIONAL PSEUDOCODE:
```
1. src = read_operand(F or D)                    ; FCONRBY @ 002750 / DCONRBY @ 002756
2. rounded = round_to_nearest(src)               ; add rounding const A,BM26 then integer part
3. if rounded outside -128..127: IOV trap
4. dest = (byte) rounded                          ; RDF_INT @ 022755 range/overflow check
```
OPERANDS: `<source/r/t1>, <dest/w/BY>`. t1 in {F,D}. Result -> dest byte.
RESULT: source rounded to nearest and stored as a signed byte; out of range -> IOV.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CONDITIONAL: SET on integer overflow else CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Floating Overflow (FO); Integer Overflow (O).
CITATION: microcode FCONRBY @ 002750, DCONRBY @ 002756 -> RDF_INT @ 022755 / CTBY;
manual section 15.3 (Data type conversion with rounding).

---

## 21. HCONR - Convert to halfword, rounded

- Opcode: F HCONR 0xFE72 (177162B); D HCONR 0xFE73 (177163B).

FUNCTIONAL PSEUDOCODE:
```
1. src = read_operand(F or D)                    ; FCONRH @ 002752 / DCONRH @ 002761
2. rounded = round_to_nearest(src)               ; A,BM26 rounding then integer part
3. if rounded outside -32768..32767: IOV trap
4. dest = (halfword) rounded                      ; RDF_INT / CTHW range check
```
OPERANDS: `<source/r/t1>, <dest/w/H>`. t1 in {F,D}. Result -> dest halfword.
RESULT: source rounded to nearest and stored as a signed halfword; out of range -> IOV.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CONDITIONAL: SET on integer overflow else CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Floating Overflow (FO); Integer Overflow (O).
CITATION: microcode FCONRH @ 002752, DCONRH @ 002761 -> RDF_INT @ 022755 / CTHW @
026145; manual section 15.3.

---

## 22. WCONR - Convert to word, rounded

- Opcode: F WCONR 0xFE74 (177164B); D WCONR 0xFE75 (177165B).

FUNCTIONAL PSEUDOCODE:
```
1. src = read_operand(F or D)                    ; FCONRW @ 002754 / DCONRW @ 002764
2. rounded = round_to_nearest(src)               ; A,BM26 rounding then integer part
3. if rounded outside signed-word range: IOV trap
4. dest = (word) rounded                          ; RDF_INT @ 022755 / CTW @ 026167
```
OPERANDS: `<source/r/t1>, <dest/w/W>`. t1 in {F,D}. Result -> dest word.
RESULT: source rounded to nearest and stored as a signed 32-bit word; out of range -> IOV.

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CONDITIONAL: SET on integer overflow else CLEARED | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Floating Overflow (FO); Integer Overflow (O).
CITATION: microcode FCONRW @ 002754, DCONRW @ 002764 -> RDF_INT @ 022755 / CTW @
026167; manual section 15.3.

---

## 23. FCONR - Convert to float, rounded

- Opcode: W FCONR 0xFE83 (177203B); D FCONR 0xFE84 (177204B).

FUNCTIONAL PSEUDOCODE:
```
1. src = read_operand(W or D)                    ; WCONRF @ 002744 / DCONRF @ 002767
2. case source:
     W: dest = round_to_float(word_value)          ; AAP2,CTF with rounding (WCONRF path)
     D: dest = double_to_float_rounded(src)         ; AAP2,CBF (DCONRF @ 002767 -> CTF)
3. if magnitude too large for float: FO trap
4. dest stored (single float)                      ; CTF @ 002672 ST,SAVF, WRITE
```
OPERANDS: `<source/r/t1>, <dest/w/F>`. t1 in {W,D}. Result -> dest single float.
RESULT: source converted to single float with rounding (word->float rounds the low
mantissa bits; double->float rounds to single precision).

STATUS FLAGS:
| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | SET if result==0 else CLEARED | CLEARED | CLEARED (result is floating) | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Floating Overflow (FO); Integer Overflow (O)
(the O case does not arise producing a float, but is listed for the CONR family).
CITATION: microcode WCONRF @ 002744, DCONRF @ 002767 -> CTF @ 002672 (ST,SAVF); manual
section 15.3 (Data type conversion with rounding).

---

## 24. PWCONV - Convert packed decimal to binary word

- Opcode: Wn PWCONV 0xFEBC-0xFEBF (177274B+(n-1)).
  (Manual prints hex "0FBECH" - an OCR transposition; octal 177274B = 0xFEBC, which
  matches the emulator source. Octal is authoritative.)

FUNCTIONAL PSEUDOCODE:
```
1. read packed-decimal <source> descriptor (field width, sign, digits)
2. validate BCD descriptor / digits ; on bad digit or bad field -> IVO (invalid operation)
3. bin = 0
   for each BCD digit d (most significant first):        ; BCD_BIN @ 022317
       bin = bin*10 + d           ; microcode does *10 via shift/add (A+B,*2 etc.)
   the fractional part is discarded (no rounding)
4. if bin does not fit in 32 bits: keep least-significant 32 bits ; set O
5. Rn = bin
```
OPERANDS: `<source/r/BCD>` packed decimal -> Wn (word register, n from opcode).
RESULT: integer value of the packed-decimal source, fractional part dropped, low 32
bits on overflow.

STATUS FLAGS (manual is explicit here):
| K | Z | C | O | S |
|---|---|---|---|---|
| SET if IVO or O occurred (`IVO or O -> K`) | SET if value==0 else CLEARED | CLEARED | CONDITIONAL: SET on integer overflow | result sign bit (CONDITIONAL) |

TRAP CONDITIONS: Addressing traps; Integer Overflow (O); Invalid Operation (IVO)
(bad BCD digit/descriptor). Requires the optional BCD hardware.
CITATION: BCD hardware routine BCD_BIN @ 022317 (packed->binary, *10 accumulation);
manual section 17.9 (Convert packed to binary word). PARTIAL: the exact opcode->micro
entry for PWCONV is not a named label in this listing (the opcode-to-microaddress map
is external); mechanism resolved to the BCD_BIN routine. See unresolved list.

---

## 25. WPCONV - Convert binary word to packed decimal

- Opcode: Wn WPCONV 0xFEB8-0xFEBB (177270B+(n-1)).

FUNCTIONAL PSEUDOCODE:
```
1. read word register Rn (source)
2. read <dest> packed-decimal descriptor (field width, scaling factor)
3. digits = decimal_expand(|Rn|)                      ; repeated /10, remainder = digit
4. apply scaling: if scale < 0 drop least-significant digits; pad with high/low zeros
5. if the destination field is too narrow to hold the result: BO (BCD overflow)
6. store packed digits + sign nibble into <dest>       ; PACK / BCD_ADD hardware path
```
OPERANDS: Wn (word register) -> `<dest/w/BCD>` packed decimal.
RESULT: packed-decimal representation of the word register, fitted to the destination
field per its scaling factor.

STATUS FLAGS (manual is explicit here):
| K | Z | C | O | S |
|---|---|---|---|---|
| SET if BCD overflow occurred (`BO -> K`) | SET if value==0 else CLEARED | CLEARED | CLEARED (O is integer overflow; not this instruction's condition) | result sign bit (CONDITIONAL) |
| additionally: BO SET on BCD overflow |

TRAP CONDITIONS: Addressing traps; BCD Overflow (BO). Requires the optional BCD hardware.
CITATION: BCD hardware pack routines (PACK @ ..., BCD_ADD family); manual section 17.10
(Convert binary word to packed). PARTIAL: like PWCONV, the exact opcode->micro entry
for WPCONV is not a named label in this listing; mechanism resolved to the PACK/BCD
routines. See unresolved list.

---

## Appendix: cross-check summary (microcode vs manual)

- Every transcendental function's manual "Data status bits" list (Z, and S except EXP
  which forces `0 -> S`, and SQRT which lists Z only) is consistent with the traced
  `ST,SAVF` floating-status save in FWRITE_AAP/DWRITE_AAP. The manual's "unmentioned
  bits are reset" rule resolves C and O to CLEARED for all of them - confirmed by the
  microcode using floating ops (no integer carry/overflow generated).
- Opcode OCR discrepancies found in the manual, resolved against the octal codes and
  the emulator sources (octal wins): ATAN float "0FFC6H" -> 0xFF6C (177554B); EXP float
  "0FF7U4" -> 0xFF74 (177564B); PWCONV "0FBECH" -> 0xFEBC (177274B).
- TAN and EXP: emulator sources reference FO/FU trap helpers that the manual documents
  as IVO (with clamped results). This is an emulator/manual wording difference, not a
  microcode contradiction - the microcode range guards jump to the IVO/clamp path.
```
