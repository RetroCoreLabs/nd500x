# ND-500 CPU Instruction Category: LOGICAL - Authoritative Behavior Reference

Scope: the five instructions whose implementation files exist under
`/home/ronny/repos/nd500x/src/cpu/instructions/LOGICAL/`:

    And.c  -> AND
    Inv.c  -> INV
    Invc.c -> INVC
    Or.c   -> OR
    Xor.c  -> XOR

Every statement below is taken directly from one of the two authoritative
sources listed here. Anything that neither source confirms is marked
`UNKNOWN (needs verification)`. Nothing is assumed.

## Sources

- PRIMARY (architecture / documented behavior):
  `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
  - Section 10.21 And   (manual page 157)
  - Section 10.22 Or    (manual page 158)
  - Section 10.23 Exclusive or (manual page 159)
  - Section 10.13 Invert (manual page 149)
  - Section 10.14 Invert with carry add (manual page 150)
  - Appendix G "Instruction Code Table" (manual page 396+): octal opcode table
  - Section 6.5 status-bit definitions (manual pages ~57-62, source lines 2008-2244)

- GROUND-TRUTH (ND-5000 microcode, flag micro-behavior):
  `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
  - AND  at octal micro-address `000341` (label **AND**),  source line 239
  - OR   at octal micro-address `000344` (label **OR**),   source line 242
  - XOR  at octal micro-address `000347` (label **XOR**),  source line 245
  - INV  at octal micro-address `000260` (label **INV**),  source line 190
  - INVC at octal micro-address `000261` (label **INVC**), source line 191

- Micro-op field decoding:
  `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`

## Status-flag definitions (from the manual, so the table columns are exact)

Source: `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
lines 2008-2244.

- Z (status bit 5, "zero"): set if the operand/result of the last instruction
  was exactly zero, otherwise cleared. (line 2026)
- C (status bit 6, "carry"): set only when performing integer arithmetic,
  otherwise cleared; set if a carry out of / borrow into the most significant
  bit occurs; also consumed by ADDC, SUBC and INVC. (line 2040)
- S (status bit 7, "sign"): holds the sign bit of the last operand/result.
  (line 2028)
- K (status bit 8, "flag"): a signalling flag with dedicated set/reset/test
  instructions; also used by descriptor addressing, CIND/LIND, and string
  instructions. Descriptor addressing "may set but never clear" K. (line 2244)
- O (status bit 9, "overflow"): set only when performing integer arithmetic,
  otherwise cleared; set when the result is too large for the destination.
  (line 2043)

## Microcode-field decoding used below

Source: `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`

The STATUS field (micro-word bits 75-72) is a SINGLE 4-bit field; its values
are mutually exclusive (mnemonics.md lines 649-666):

    1 = K,ONE     set K to 1
    2 = K,ZRO     clear K to 0
    3 = K,1IFZ    set K to 1 if the ALU result is 0
    4 = ST,SAVA   save status (Z,C,O,S) from the ALU operation

CONSEQUENCE: an instruction whose STATUS field is `ST,SAVA` (value 4) does NOT
select any K,* encoding, therefore it does not modify the K flag. All five
LOGICAL instructions use `ST,SAVA`, so K is UNCHANGED for every one of them.

ALU function mnemonics (mnemonics.md lines 27-37):

    ALU,AND   (2)  = A AND B
    ALU,OR    (7)  = A OR B
    ALU,XOR   (5)  = A XOR B
    ALU,ADIRC (1)  = ALU output complemented (one's complement)
    ALU,A-B   (10) = A minus B (carry selects -1 or +C)

    CRY,C     (2)  = carry input taken from the C status bit (mnemonics.md line 49)
    COND,MSEXO(0)  = test condition = EXOR of S and O from the ALU result
                     (mnemonics.md line 766); this is the shared ALU
                     signed-overflow-trap check micro-op ("T,JMP ... G,OOPS").

NOTE on the `ST,SAVA` interaction with C and O for the bitwise ops (AND/OR/XOR)
and INV: the manual documents ONLY Z and S as the data status bits for those
four instructions, yet the microcode uses `ST,SAVA`, which physically saves the
full ALU status word (Z, C, O, S). For a purely logical ALU operation the C and
O outputs are hardware artifacts that the manual does not define. The two
sources therefore disagree on whether C and O are touched. This is recorded as
`UNKNOWN (needs verification)` in each affected table rather than guessed.

-------------------------------------------------------------------------------

## AND

- Manual section: 10.21 And (page 157). Microcode: octal `000341`, label **AND**.
- Operation: `Rn AND <operand> -> Rn` (bitwise AND).
- Operands: 1 (the operand value; addressing forms `<operand/r/t>`).
- Data-type / register variants (n = 1..4). Opcodes are octal, base is n=1,
  add (n-1) for n=2,3,4 (per section 10.21 and Appendix G, manual line 4001-)
  ref number 21:

  | Variant | Octal (n=1) | Hex (n=1) |
  |---------|-------------|-----------|
  | BIn AND (bit)      | 176714B | 0FDCCH |
  | BYn AND (byte)     | 176220B | 0FC90H |
  | Hn AND (halfword)  | 176224B | 0FC94H |
  | Wn AND (word)      | 344B    | 0E4H   |

- Description: bitwise AND of register and operand, result to register. For
  BI/BY/H the upper part of the register is zero-filled (manual 10.21).

- Microcode micro-ops (line 239):
  `ALU,AND  TYP,DR  ST,SAVA  T,JMP COND,MSEXO ...`

- STATUS FLAGS:

  | Flag | Effect | Manual evidence | Microcode evidence |
  |------|--------|-----------------|--------------------|
  | K | UNCHANGED | not listed in "Data status bits" (10.21) | STATUS field = `ST,SAVA` (not any K,* op) => K held |
  | Z | CONDITIONAL: set if result = 0, else cleared | "result = 0 -> Z" (10.21) | `ST,SAVA` saves ALU Z |
  | C | UNKNOWN (needs verification) | not listed (10.21 lists only Z,S) | `ST,SAVA` physically saves ALU carry-out; undocumented for a logical op |
  | O | UNKNOWN (needs verification) | not listed (10.21 lists only Z,S) | `ST,SAVA` physically saves ALU overflow; undocumented for a logical op |
  | S | CONDITIONAL: set = result sign bit | "result.signbit -> S" (10.21) | `ST,SAVA` saves ALU S |

- TRAP conditions: "Addressing traps" only (manual 10.21). These arise from
  operand fetch/addressing, not from the AND itself.

-------------------------------------------------------------------------------

## OR

- Manual section: 10.22 Or (page 158). Microcode: octal `000344`, label **OR**.
- Operation: `Rn OR <operand> -> Rn` (bitwise OR).
- Operands: 1 (`<operand/r/t>`).
- Variants (n = 1..4; octal base is n=1). Appendix G ref number 22:

  | Variant | Octal (n=1) | Hex (n=1) |
  |---------|-------------|-----------|
  | BIn OR (bit)       | 176770B | 0FDF8H |
  | BYn OR (byte)      | 176230B | 0FC98H |
  | Hn OR (halfword)   | 176234B | 0FC9CH |
  | Wn OR (word)       | 240B    | 0A0H   |

  NOTE: Section 10.22 prints the per-register offset for BIn OR as "-(n-1)"
  (decrement) whereas the byte/halfword/word variants use "+(n-1)". The bit
  base value itself is confirmed by Appendix G as 176770B (= 0xFDF8). The sign
  of the per-n step for the BIn variant is `UNKNOWN (needs verification)`
  because the section text shows a minus while all other variants (and the AND
  and XOR bit variants) increment; this may be an OCR artifact.

- Description: bitwise OR of register and operand, result to register. For
  BI/BY/H the upper part of the register is zero-filled (manual 10.22).

- Microcode micro-ops (line 242):
  `ALU,OR  TYP,DR  ST,SAVA  T,JMP COND,MSEXO ...`

- STATUS FLAGS:

  | Flag | Effect | Manual evidence | Microcode evidence |
  |------|--------|-----------------|--------------------|
  | K | UNCHANGED | not listed in "Data status bits" (10.22) | STATUS field = `ST,SAVA` (not any K,* op) => K held |
  | Z | CONDITIONAL: set if result = 0, else cleared | "result = 0 -> Z" (10.22) | `ST,SAVA` saves ALU Z |
  | C | UNKNOWN (needs verification) | not listed (10.22 lists only Z,S) | `ST,SAVA` physically saves ALU carry-out; undocumented for a logical op |
  | O | UNKNOWN (needs verification) | not listed (10.22 lists only Z,S) | `ST,SAVA` physically saves ALU overflow; undocumented for a logical op |
  | S | CONDITIONAL: set = result sign bit | "result.signbit -> S" (10.22) | `ST,SAVA` saves ALU S |

- TRAP conditions: "Addressing traps" only (manual 10.22).

-------------------------------------------------------------------------------

## XOR (Exclusive or)

- Manual section: 10.23 Exclusive or (page 159). Microcode: octal `000347`,
  label **XOR**.
- Operation: `Rn XOR <operand> -> Rn` (bitwise exclusive OR).
- Operands: 1 (`<operand/r/t>`).
- Variants (n = 1..4; octal base is n=1). Appendix G ref number 23:

  | Variant | Octal (n=1) | Hex (n=1) |
  |---------|-------------|-----------|
  | BIn XOR (bit)      | 176774B | 0FDFCH |
  | BYn XOR (byte)     | 176240B | 0FCA0H |
  | Hn XOR (halfword)  | 176244B | 0FCA4H |
  | Wn XOR (word)      | 244B    | 0A4H   |

  NOTE: The prose in section 10.23 prints the BIn XOR code as a garbled
  "0FDCC4+(n-1)" / "176714B+(n-1)"; 176714B is actually the AND bit code, so the
  section text is corrupted. Appendix G (manual line 14004) gives BIn XOR =
  176774B (= 0xFDFC), which is the value used above.
  DISCREPANCY WITH EMULATOR: the current implementation comment in
  `/home/ronny/repos/nd500x/src/cpu/instructions/LOGICAL/Xor.c` states the bit
  variant is 0xFDF0-0xFDF3; the manual's Appendix G says 0xFDFC. This mismatch
  needs verification against the committed dispatch table.

- Description: bitwise XOR of register and operand, result to register. For
  BI/BY/H the upper part of the register is zero-filled (manual 10.23).

- Microcode micro-ops (line 245):
  `ALU,XOR  TYP,DR  ST,SAVA  T,JMP COND,MSEXO ...`

- STATUS FLAGS:

  | Flag | Effect | Manual evidence | Microcode evidence |
  |------|--------|-----------------|--------------------|
  | K | UNCHANGED | not listed in "Data status bits" (10.23) | STATUS field = `ST,SAVA` (not any K,* op) => K held |
  | Z | CONDITIONAL: set if result = 0, else cleared | "result = 0 -> Z" (10.23) | `ST,SAVA` saves ALU Z |
  | C | UNKNOWN (needs verification) | not listed (10.23 lists only Z,S) | `ST,SAVA` physically saves ALU carry-out; undocumented for a logical op |
  | O | UNKNOWN (needs verification) | not listed (10.23 lists only Z,S) | `ST,SAVA` physically saves ALU overflow; undocumented for a logical op |
  | S | CONDITIONAL: set = result sign bit | "result.signbit -> S" (10.23) | `ST,SAVA` saves ALU S |

- TRAP conditions: "Addressing traps" only (manual 10.23).

-------------------------------------------------------------------------------

## INV (Invert)

- Manual section: 10.13 Invert (page 149). Microcode: octal `000260`, label
  **INV**.
- Operation: `one's complement of Rn -> Rn`.
- Operands: 0 (register-only).
- Variants (n = 1..4; octal base is n=1). Appendix G ref number 13:

  | Variant | Octal (n=1) | Hex (n=1) |
  |---------|-------------|-----------|
  | BIn INV (bit)      | 177020B | 0FE10H |
  | BYn INV (byte)     | 177024B | 0FE14H |
  | Hn INV (halfword)  | 177030B | 0FE18H |
  | Wn INV (word)      | 230B    | 098H   |

- Description: one's complement of the register, stored back. For BI/BY/H only
  the lower part is complemented and the rest of the register is cleared
  (manual 10.13).

- Microcode micro-ops (line 190):
  `ALU,ADIRC  TYP,DR  ...  ST,SAVA  T,JMP COND,MSEXO ...`
  (`ALU,ADIRC` = ALU output complemented, i.e. one's complement.)

- STATUS FLAGS:

  | Flag | Effect | Manual evidence | Microcode evidence |
  |------|--------|-----------------|--------------------|
  | K | UNCHANGED | not listed in "Data status bits" (10.13) | STATUS field = `ST,SAVA` (not any K,* op) => K held |
  | Z | CONDITIONAL: set if result = 0, else cleared | "result = 0 -> Z" (10.13) | `ST,SAVA` saves ALU Z |
  | C | UNKNOWN (needs verification) | not listed (10.13 lists only Z,S) | `ST,SAVA` physically saves ALU carry-out; undocumented for INV |
  | O | UNKNOWN (needs verification) | not listed (10.13 lists only Z,S) | `ST,SAVA` physically saves ALU overflow; undocumented for INV |
  | S | CONDITIONAL: set = result sign bit | "result.signbit -> S" (10.13) | `ST,SAVA` saves ALU S |

- TRAP conditions: "None" (manual 10.13).

-------------------------------------------------------------------------------

## INVC (Invert with carry add)

- Manual section: 10.14 Invert with carry add (page 150). Microcode: octal
  `000261`, label **INVC**.
- Operation: `one's complement of Rn + C -> Rn`.
- Operands: 0 (register-only; uses the C flag as carry-in).
- Variants (n = 1..4; word only). Section 10.14 and Appendix G ref number 14:

  | Variant | Octal (n=1) | Hex (n=1) |
  |---------|-------------|-----------|
  | Wn INVC (word) | 177420B | 0FF10H |

- Description: one's complement of the specified WORD register, the carry (C) is
  added, and the result loaded back. Used for multiple-precision arithmetic
  (manual 10.14). No BI/BY/H forms exist.

- Microcode micro-ops (line 191):
  `ALU,A-B  CRY,C  TYP,DR  A,SC14  ...  ST,SAVA  T,JMP COND,MSEXO ...`
  (`ALU,A-B` with `CRY,C` performs the add-with-carry that realizes ~Rn + C;
  `ST,SAVA` saves Z,C,O,S; `COND,MSEXO` is the S-XOR-O overflow-trap check.)

- STATUS FLAGS:

  | Flag | Effect | Manual evidence | Microcode evidence |
  |------|--------|-----------------|--------------------|
  | K | UNCHANGED | not listed in "Data status bits" (10.14) | STATUS field = `ST,SAVA` (not any K,* op) => K held |
  | Z | CONDITIONAL: set if result = 0, else cleared | "result = 0 -> Z" (10.14) | `ST,SAVA` saves ALU Z |
  | C | CONDITIONAL: set on carry out of the add | "carry -> C" (10.14) | `ST,SAVA` saves ALU carry; `CRY,C` also uses C as carry-in |
  | O | CONDITIONAL: set on integer overflow | "overflow -> O" (10.14) | `ST,SAVA` saves ALU O; `COND,MSEXO` (S xor O) is the overflow-trap check |
  | S | CONDITIONAL: set = result sign bit | "result.signbit -> S" (10.14) | `ST,SAVA` saves ALU S |

- TRAP conditions: "Integer overflow (O)" (manual 10.14). This is the one
  LOGICAL instruction with a non-addressing trap. Per manual line 2043, on
  integer overflow the S and Z bits reflect the actual (truncated) result and
  the low 32 bits are stored in the destination.

  EMULATOR NOTE (not part of the spec): the current implementation
  `/home/ronny/repos/nd500x/src/cpu/instructions/LOGICAL/Invc.c` updates only
  Z, S and C (via `nd500_set_flags_zsc`) and does not compute the O bit or raise
  the integer-overflow trap that the manual specifies. Flagged for verification.

-------------------------------------------------------------------------------

## Summary table (documented flag effects, per manual "Data status bits")

| Instr | K | Z | C | O | S | Trap |
|-------|---|---|---|---|---|------|
| AND  | unchanged | cond(=0) | UNKNOWN | UNKNOWN | cond(sign) | Addressing |
| OR   | unchanged | cond(=0) | UNKNOWN | UNKNOWN | cond(sign) | Addressing |
| XOR  | unchanged | cond(=0) | UNKNOWN | UNKNOWN | cond(sign) | Addressing |
| INV  | unchanged | cond(=0) | UNKNOWN | UNKNOWN | cond(sign) | None |
| INVC | unchanged | cond(=0) | cond(carry) | cond(ovfl) | cond(sign) | Integer overflow (O) |

Legend: "cond(...)" = CONDITIONAL on the stated condition; "unchanged" =
UNCHANGED (firm: microcode STATUS field = ST,SAVA selects no K,* op);
"UNKNOWN" = UNKNOWN (needs verification) - manual documents only Z and S for the
bitwise ops and INV, while microcode ST,SAVA physically saves the full ALU
status (C and O included) whose values are undefined for a logical operation.
