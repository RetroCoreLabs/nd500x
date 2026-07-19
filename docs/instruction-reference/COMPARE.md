# ND-500 COMPARE Category - Authoritative Behavior Reference

Purpose: ground-truth reference for validating the nd500x emulator's COMPARE-class
instructions. Every statement below is taken directly from one of the two sources
named. Anything not confirmed by either source is marked "UNKNOWN (needs verification)".

## Instruction set (from the source tree)

Enumerated from `/home/ronny/repos/nd500x/src/cpu/instructions/COMPARE/*.c`:

- `Comp.c`   -> COMP   (register compare)
- `Comp2.c`  -> COMP2  (compare two operands)
- `Pcomp.c`  -> PCOMP  (packed BCD compare)
- `Scomp.c`  -> SCOMP  (byte string compare)  [manual files it under STRING, ch. 14]
- `Test.c`   -> TEST   (test against zero)

## Sources

- PRIMARY spec (manual):
  `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
- GROUND-TRUTH microcode (ND-5000, 5800-A30):
  `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
- Micro-op field decoder:
  `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`

## Decoder facts used below (read directly from the sources)

Manual, general rule (line 4040):
"Data status bits not mentioned in the instruction description are always cleared
after the instruction has been executed. If the status bit is conditionally set a
TRUE condition causes the bit to be set (1), a FALSE condition causes it to be reset (0)."

Manual status-bit abbreviations (lines 14785-14816): C=Carry, K=Flag, O=Integer
overflow, S=Sign, Z=Zero. Trap abbreviations: DR=Descriptor range, FO=Floating
overflow, FU=Floating underflow, IVO=Invalid operation, BO=BCD overflow.

Microcode status/K micro-ops (`mnemonics.md` lines 653-661):
- `K,ONE`   -> SET K (1 -> K)
- `K,ZRO`   -> CLEAR K (0 -> K)
- `K,1IFZ`  -> SET K TO 1 IF ALU OPERATION IS 0
- `ST,SAVA` -> SAVE STATUS FROM ALU OPERATION
- `ST,SAVC` -> SAVE STATUS FROM ALU IN COMPARE
- `ST,SAVB` -> SAVE STATUS FROM BCD OPERATION
- `ST,LOAD` -> LOAD ALU STATUS
Microcode ALU ops (lines 36-40): `ALU,A-B` = A minus B; `ALU,B-A` = B minus A;
`CRY,ONE` = carry-in one (two's-complement subtract).
Microcode conditions (lines 766-779): `COND,MSEXO` = EXOR of S and O from ALU result;
`COND,MZRO` = Z from ALU; `COND,MCRY` = C from ALU; `COND,MSGN` = S from ALU.

Note: `ST,SAVC` ("save status from ALU IN COMPARE") is the exact microcode primitive
that implements the manual's compare data-status rule (Z, S = signbit XOR overflow,
C = carry). It is emitted by COMP, COMP2 and TEST.

---

## COMP - register compare

- Manual section: 10.9 "Compare", Page 145 (manual lines 4485-4519).
- Opcodes (octal), from manual lines 4492-4497 (n = 1..4):
  - BIn COMP: 176030B + (n-1)
  - BYn COMP: 060B + (n-1)
  - Hn  COMP: 176034B + (n-1)
  - Wn  COMP: 064B + (n-1)
  - Fn  COMP: 070B + (n-1)
  - Dn  COMP: 074B + (n-1)
- Operation (line 4500): Rn - <operand>   (difference discarded, only status kept).
- Operands: 1  (`<operand/r/t>`).
- Microcode entry: octal `000241`, label `COMP` (MICRO-5800-A30.md line 175):
  `ALU,B-A CRY,ONE ... ST,SAVC ... COND,MSEXO`. Bit variant at `000242`/`000243`.

STATUS FLAGS

| Flag | Verdict | Manual evidence | Microcode evidence |
|------|---------|-----------------|--------------------|
| K | CLEARED | Not mentioned in 10.9 data-status list -> cleared by rule at line 4040 | COMP word `000241` has NO K micro-op; no explicit clear in this step (clear, if any, happens outside this word) - EXPLICIT CLEAR UNVERIFIED |
| Z | CONDITIONAL: set if (Rn - operand) == 0 | Line 4511 "result = 0 -> Z" | `ST,SAVC` @ `000241` saves compare status (Z) |
| C (carry) | CONDITIONAL: carry out of most significant bit | Line 4513 "carry from most significant bit -> C" | `ST,SAVC` @ `000241` |
| O (overflow) | CLEARED | Not listed in 10.9 data-status -> cleared by rule at line 4040 | Overflow is computed and folded into S (see MSEXO); whether O bit is separately written by `ST,SAVC` = UNKNOWN (needs verification) |
| S (sign) | CONDITIONAL: S = result.signbit XOR Overflow ("true comparison") | Line 4512 "result.signbit XOR Overflow -> S"; description line 4504 | `ST,SAVC` @ `000241`; `COND,MSEXO` = EXOR of S and O confirms sign-XOR-overflow is a hardware condition |

TRAP CONDITIONS (manual line 4507): Addressing traps; Floating overflow (FO);
Floating underflow (FU) [FO/FU only for the F/D float variants].

---

## COMP2 - compare two operands

- Manual section: 10.10 "Compare two operands", Page 146 (manual lines 4530-4566).
- Opcodes (octal), from manual lines 4539-4544:
  - BI COMP2: 176025B
  - BY COMP2: 055B
  - H  COMP2: 176026B
  - W  COMP2: 056B
  - F  COMP2: 057B
  - D  COMP2: 100B
- Operation (line 4547): <op1> - <op2>   (difference discarded, only status kept).
- Operands: 2  (`<op1/r/t>`, `<op2/r/t>`).
- Microcode entry: octal `000244`, label `COMP2` (MICRO-5800-A30.md line 178). The
  subtract-and-status step is the following word `000245`:
  `ALU,B-A CRY,ONE ... ST,SAVC ... COND,MSEXO`. Bit variant at `000242`/`000243`.

STATUS FLAGS

| Flag | Verdict | Manual evidence | Microcode evidence |
|------|---------|-----------------|--------------------|
| K | CLEARED | Not mentioned in 10.10 -> cleared by rule at line 4040 | No K micro-op in `000244`/`000245`; EXPLICIT CLEAR UNVERIFIED |
| Z | CONDITIONAL: set if (op1 - op2) == 0 | Line 4558 "result = 0 -> Z" | `ST,SAVC` @ `000245` |
| C (carry) | CONDITIONAL: carry out of most significant bit | Line 4560 "carry from most significant bit -> C" | `ST,SAVC` @ `000245` |
| O (overflow) | CLEARED | Not listed in 10.10 -> cleared by rule at line 4040 | Folded into S via MSEXO; separate O-bit write by `ST,SAVC` = UNKNOWN (needs verification) |
| S (sign) | CONDITIONAL: S = result.signbit XOR Overflow | Line 4559 "result.signbit XOR Overflow -> S" | `ST,SAVC` @ `000245`; `COND,MSEXO` |

TRAP CONDITIONS (manual line 4554): Addressing traps; Floating underflow (FU);
Floating overflow (FO) [FU/FO only for F/D variants].

---

## TEST - test against zero

- Manual section: 10.11 "Test against zero", Page 147 (manual lines 4577-4611).
- Opcodes (octal), from manual lines 4584-4589:
  - BI TEST: 101B
  - BY TEST: 102B
  - H  TEST: 103B
  - W  TEST: 104B
  - F  TEST: 105B
  - D  TEST: 106B
- Operation (line 4592): <operand> - 0   (difference discarded, only status kept).
- Operands: 1  (`<operand/r/t>`).
- Microcode entry: octal `000250`, label `TEST` (MICRO-5800-A30.md line 182):
  `ALU,A-B CRY,ONE ... ST,SAVC`. Bit variant `000251`; single-float step `000252`
  (`TYP,F ... SC1 ... ST,SAVC`); double-float step `000253` (`TYP,DF ... ST,SAVC`).

STATUS FLAGS

| Flag | Verdict | Manual evidence | Microcode evidence |
|------|---------|-----------------|--------------------|
| K | CLEARED | Not mentioned in 10.11 -> cleared by rule at line 4040 | No K micro-op in `000250`-`000253`; EXPLICIT CLEAR UNVERIFIED |
| Z | CONDITIONAL: set if operand == 0 | Line 4603 "result = 0 -> Z" | `ST,SAVC` @ `000250` (and float `000252`/`000253`) |
| C (carry) | SET to 1 for INTEGER types (BI/BY/H/W). For F/D float variants: UNKNOWN (needs verification) | Line 4605 "1 -> C (integer)"; qualifier "(integer)" only | `ST,SAVC` @ `000250` with operand-0 subtract yields carry=1 for integer; float words `000252`/`000253` also use `ST,SAVC` but carry semantics for float NOT stated |
| O (overflow) | CLEARED | Not listed in 10.11 -> cleared by rule at line 4040; for x-0 no overflow occurs | Folded into S via MSEXO; separate O-bit write = UNKNOWN (needs verification) |
| S (sign) | CONDITIONAL: S = result.signbit XOR Overflow (overflow is 0 for x-0, so effectively the operand sign bit) | Line 4604 "result.signbit XOR Overflow -> S" | `ST,SAVC` @ `000250`; `COND,MSEXO` |

TRAP CONDITIONS (manual line 4599): Addressing traps only.

---

## SCOMP - byte string compare

- Manual section: 14.10 "String compare", Page 254 (manual lines 8704-8742).
  (Source file lives in the COMPARE folder; the manual classifies it as a STRING
  instruction. Its own header comment `Scomp.c` also says "STRING class".)
- Opcode (octal), from manual line 8712:
  - BY SCOMP: 176654B
- Operation (manual lines 8714-8719): compare S(I1) with D(I2) byte-by-byte,
  incrementing I1 and I2, until unequal bytes are found or a string end is reached.
  Byte elements are treated as UNSIGNED.
- Operands: 2  (`<source-1/r/BY/I1>`, `<source-2/r/BY/I2>`). I1/I2 are used as
  running indices and are updated (see table).
- Microcode entry: octal `001315`, label `SCOMP` (MICRO-5800-A30.md line 731). The
  actual per-byte comparison + flag setting is the continuation routine
  `SCOMPBY_F01`..`SCOMPBY_F10`, octal `006555`-`006577` (lines 3451-3470):
  - `K,1IFZ` @ `006567` (SCOMPBY_F06) - set K if byte difference path taken
  - `K,ZRO`  @ `006571`, `006573`, `006575` - clear K on the length-mismatch paths
  - `ST,SAVA` @ `006564` (F03), `006572` (F07), `006574` (F08) - save ALU status (Z,S)

Terminating conditions and resulting K/Z/S (manual table, lines 8727-8736;
byte elements are unsigned):

| Condition | K | Z | S | I1, I2 result |
|-----------|---|---|---|---------------|
| both operands outside string | 0 | 1 | 0 | unmodified, DR trap condition |
| exact match | 0 | 1 | 0 | next element |
| source-1 longer | 0 | 0 | 0 | next element |
| source-2 longer (source-1 shorter) | 0 | 0 | 1 | next element |
| smaller byte in source-1 | 1 | 0 | 0 | differing elements |
| greater byte in source-1 | 1 | 0 | 1 | differing elements |

STATUS FLAGS

| Flag | Verdict | Manual evidence | Microcode evidence |
|------|---------|-----------------|--------------------|
| K | CONDITIONAL: set (1) when a byte inequality terminates the compare; cleared (0) on length-based termination or exact match | Terminating table, lines 8729-8736 | `K,1IFZ` @ `006567`; `K,ZRO` @ `006571`/`006573`/`006575` |
| Z | CONDITIONAL: set (1) on exact match / both-outside; cleared (0) otherwise | Terminating table (Z column) | `ST,SAVA` @ `006564`/`006572`/`006574` |
| S | CONDITIONAL: encodes ordering (see table): K=0,S=0 source-1 longer; K=0,S=1 source-1 shorter; K=1,S=0 smaller byte in source-1; K=1,S=1 greater byte in source-1 | Terminating table (S column) | `ST,SAVA` @ `006564`/`006572`/`006574` |
| C (carry) | CLEARED | Not listed in the SCOMP data-status/terminating table -> cleared by rule at line 4040 | No compare-carry status stored in the shown SCOMPBY path (`ST,SAVA` saves Z/S) - explicit C clear UNVERIFIED |
| O (overflow) | CLEARED | Not listed for SCOMP -> cleared by rule at line 4040 | Not written in the shown SCOMPBY path - explicit O clear UNVERIFIED |

TRAP CONDITIONS (manual line 8725): Descriptor range (DR) trap when an operand is
addressed outside its string (both outside -> compare as exact match, I1/I2 unmodified,
DR trap; source-1 outside -> "source-1 shorter", DR trap; source-2 outside ->
"source-1 longer", DR trap). Addressing traps also apply.

---

## PCOMP - packed BCD compare (Option)

- Manual section: 17.5 "Packed Compare", Page 345 (manual lines 11987-12019).
- Opcode (octal), from manual line 11996:
  - PCOMP: 177263B
- Operation (line 11999): <a> - <b>  (BCD difference discarded, only status kept).
  Operands are auto-aligned to the same decimal scale and zero-extended; unsigned is
  treated as positive; +0 and -0 are equal (lines 12005, 11826).
- Operands: 2  (`<a/r/BCD>`, `<b/r/BCD>`).
- Microcode: no label literally named "PCOMP" exists in MICRO-5800-A30.md. The packed
  BCD compare routine is label `COMPBCD`, octal `002057`/`002060` (lines 1085-1086),
  chaining to `COMPBCD1` (octal `017571`, line 8071) and the descriptor-compare tail
  `COMPBCD_1` -> `DES_COMP_1..4` (octal `021613`, `021665`-`021675`, lines 9113,
  9145-9155). The PCOMP <-> COMPBCD mapping is INFERRED from the name ("compare BCD"
  = packed compare) and the shared 177263B/BCD context - see "Unresolved" below.
  Flag micro-ops in that chain:
  - `K,1IFZ` @ `002060` (COMPBCD second word)
  - `K,ZRO`  @ `017571` (COMPBCD1)
  - `ST,LOAD` (load ALU status) @ `021667`, `021672`, `021674` in the DES_COMP tail

STATUS FLAGS

| Flag | Verdict | Manual evidence | Microcode evidence |
|------|---------|-----------------|--------------------|
| K | CONDITIONAL: set on Invalid operation (IVO) | Line 12013 "IVO -> K" | `K,1IFZ` @ `002060`, `K,ZRO` @ `017571` in COMPBCD chain (mapping inferred) |
| Z | CONDITIONAL: set if BCD difference == 0 (a == b) | Line 12011 "difference = 0 -> Z" | `ST,LOAD` in DES_COMP tail @ `021667`/`021672`/`021674` (mapping inferred) |
| C (carry) | CLEARED | Not mentioned in 17.5 data-status list -> cleared by rule at line 4040. NOTE: emulator `Pcomp.c` comment claims "C unaffected"; the manual rule says cleared - DISCREPANCY, manual is authoritative | Not written by the shown COMPBCD path - UNVERIFIED |
| O (overflow) | CLEARED | Not mentioned in 17.5 -> cleared by rule at line 4040. (BCD overflow BO is a trap, not this status bit.) NOTE: `Pcomp.c` claims "O unaffected" - DISCREPANCY vs manual | Not written by the shown COMPBCD path - UNVERIFIED |
| S (sign) | CONDITIONAL: set if difference is negative (a < b) | Line 12012 "difference.signbit -> S" | `ST,LOAD` in DES_COMP tail (mapping inferred) |

TRAP CONDITIONS (manual line 12007): Addressing traps; Invalid operation (IVO).

---

## Summary matrix (manual verdicts)

| Instr | Opcode (octal) | K | Z | C | O | S |
|-------|----------------|---|---|---|---|---|
| COMP  | 060B+/064B+/070B+/074B+/176030B+/176034B+ | CLEARED | COND(result==0) | COND(carry MSB) | CLEARED | COND(sign XOR ovf) |
| COMP2 | 055B/056B/057B/100B/176025B/176026B | CLEARED | COND(result==0) | COND(carry MSB) | CLEARED | COND(sign XOR ovf) |
| TEST  | 101B/102B/103B/104B/105B/106B | CLEARED | COND(operand==0) | SET (integer) / UNKNOWN (float) | CLEARED | COND(sign XOR ovf) |
| SCOMP | 176654B | COND(byte-diff termination) | COND(equal/exact) | CLEARED | CLEARED | COND(ordering) |
| PCOMP | 177263B | COND(IVO) | COND(a==b) | CLEARED | CLEARED | COND(a<b) |

"CLEARED" verdicts rest on the manual's general rule at line 4040 (unmentioned bits
are cleared); several of these are not separately confirmed by an explicit clear
micro-op in the cited microcode words (marked UNVERIFIED in the per-flag tables above).

## Unresolved / needs verification

1. PCOMP <-> microcode label `COMPBCD` mapping is INFERRED, not read from a dispatch
   table. Needs confirmation that opcode 177263B decodes to microcode entry octal
   `002057` (COMPBCD).
2. Whether the O (overflow) status bit is separately written for COMP/COMP2/TEST, or
   only folded into S via `COND,MSEXO` - not resolved from the microcode words shown.
3. TEST carry (C) behavior for the F/D floating variants - manual states C=1 for
   integer only; float behavior unstated.
4. The "CLEARED" verdicts for K on COMP/COMP2/TEST and for C/O on SCOMP/PCOMP come
   from the manual rule at line 4040; no explicit clear micro-op was located in the
   cited words. Emulator `Pcomp.c` comments assert C/O are "unaffected", which
   contradicts the manual rule - needs a hardware/trace check to settle.
