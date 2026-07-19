# ND-500 ARITHMETIC Instruction Category - Authoritative Behavior Reference

Purpose: ground-truth reference for validating the nd500x emulator's ARITHMETIC
instruction implementations. Every statement here is taken directly from one of
the two primary sources below. Anything not confirmed by either source is marked
`UNKNOWN (needs verification)`. Nothing is inferred from the emulator C sources
(those are treated as an implementation under test, not as truth).

## Sources

- PRIMARY SPEC (behavior + documented flag effects):
  `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
  (Norsk Data ND-05.009.4 EN, "ND-500 Reference Manual"). Cited by section number.
- GROUND-TRUTH FLAG MICRO-BEHAVIOR (ND-5000/5800 microcode):
  `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
  (MICRO-5800-A30). Cited by octal control-store address / label.
- Microcode field decode:
  `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`
- Instruction file set enumerated from:
  `/home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/*.c`

## Data status bit definitions (manual section 6.5.1)

| Code | Name               | Bit no. |
|------|--------------------|---------|
| Z    | zero               | 5       |
| C    | carry              | 6       |
| S    | sign               | 7       |
| O    | overflow           | 9       |
| IVO  | invalid operation  | 11      |
| DZ   | divide by zero     | 12      |
| FU   | floating underflow | 13      |
| FO   | floating overflow  | 14      |
| BO   | BCD overflow       | 15      |
| K    | flag               | 62 (signalling group, section 6.5.4) |

Manual section 6.5.1, verbatim rules that govern every table below:

- "Bits that are set, reset or left unaffected are mentioned explicitly. All data
  status bits not mentioned are reset." (So any Z/C/O/S/IVO/DZ/FU/FO/BO the
  manual does NOT list for an instruction is CLEARED, not left unchanged.)
- Z: set if operand/result was exactly zero, else cleared.
- S: holds the sign bit of the last operand/result.
- C: "may be set only when performing integer arithmetic; otherwise it is
  cleared. Set if a carry out of or borrowing into the most significant bit
  occurs." Used by ADDC, SUBC, INVC.
- O: "may be set only when performing integer arithmetic; otherwise it is
  cleared." Integer add overflow = both addend signs equal and result sign
  differs. On overflow, S and Z reflect the actual truncated result; the low 32
  bits are stored.
- DZ: division by zero. Leaves largest representable value in destination with
  sign of dividend, unless dividend also zero (0/0 -> 0).
- FU/FO: floating underflow/overflow (exponent needs > 9 bits).
- BO: BCD (packed) overflow - destination field too narrow (BCD is a hardware option).
- IVO: invalid operation.
- K (manual 6.5.4): general flag bit; for packed-decimal instructions the manual
  (section 17.1) states "The K flag is set up on BCD overflow or invalid
  operation, otherwise the flag is cleared." K is NOT touched by the ordinary
  binary integer/float arithmetic instructions.

NOTE on "K UNCHANGED" claims: for the binary integer/float instructions the
manual lists no K effect and the microcode entry carries no `K,*` micro-op, so
per the "not mentioned = reset" rule K is not modified by these instructions.
The reference below states K as UNCHANGED for those (they carry no K micro-op and
the manual assigns them no K semantics); the only ARITHMETIC instructions that
manipulate K are the packed-decimal (P*) group.

## Microcode STATUS-field micro-op legend (mnemonics.md, STATUS bits 75-72)

| Mnemonic   | Meaning (verbatim)                          | Flags it drives |
|------------|---------------------------------------------|-----------------|
| (hold)     | Hold status unchanged                       | none            |
| K,ONE      | SET K (FLAG) 1 TO K                          | K:=1            |
| K,ZRO      | CLEAR K (FLAG) 0 TO K                        | K:=0            |
| K,1IFZ     | SET K TO 1 IF ALU OPERATION IS 0            | K conditional   |
| ST,SAVA    | SAVE STATUS FROM ALU OPERATION              | Z,C,O,S         |
| ST,SAVC    | SAVE STATUS FROM ALU IN COMPARE             | Z,C,O,S (compare) |
| ST,SAVF    | SAVE STATUS FROM FLOATING OPERATION         | Z,S,FU,FO       |
| ST,SAVB    | SAVE STATUS FROM BCD OPERATION              | Z,S,BO (BCD)    |
| ST,SAVM    | SAVE MIXED STATUS FOR INTEGER MULTIPLY      | Z,S,O (mul)     |
| ST,ACCA/ACCM/ACCF | accumulate ALU/mixed/AAP status      | as above        |

Cross-checks used below: `AAP2,IMUL`/`IMULD`/`IMULUD` = integer (double-length)
multiply via the arithmetic accelerator; `AAP2,MUL` = floating multiply;
`AAP2,ADD`/`AAP2,SUBBA` = floating add / (B-A) subtract; `AD_ARTI=1 ADACT` +
`AAP2,*` BCD ops = packed-decimal coprocessor path; `COND,MDZ` = divide-by-zero
from the floating AAP. `CSAVE` is a SEQUENCER bit ("PUSH TEST CONDITION TO
STACK"), NOT a status-save, and is ignored for flag purposes.

IMPORTANT microcode limitation for the packed (P*) group: this control-store
image (MICRO-5800-A30) contains ZERO `ST,SAVB` micro-ops. The packed routines
reach the BCD coprocessor via `AD_ARTI=1 ADACT`, and the final Z/S/BO/K result
status is produced by that coprocessor rather than by a decodable `ST,SAV*`
micro-op in this listing. The only K micro-op visible on the packed entry paths
is `K,1IFZ` on the digit-count loop step (loop control, not the final BO/IVO K).
Therefore the packed-group flag detail below rests on the MANUAL; the microcode
citation confirms the routine entry and the coprocessor dispatch only.

---

## Instruction set (38 files)

Enumerated from `/home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/`:
Abs, Add, Add2, Add3, Addc, Axi, Decr, Div2, Div3, Div4, Divide, Incr, Ixi,
Mul2, Mul3, Mul4, Mulad, Multiply, Neg, Padd, Paddr, Pmpy, Pmpyr, Ppack, Ppackr,
Psub, Psubr, Psum, Pupack, Pupackr, Rem, Sub, Sub2, Sub3, Subc, Subtract, Udiv, Umul.

Note: `Sub.c` and `Subtract.c` are two files for the SAME architectural
instruction (manual 11.2 Subtract, the `-` operator); `Divide.c` = manual 11.4
`/`; `Multiply.c` = manual 11.3 `*`; `Add.c` = manual 11.1 `+`. Each file gets
its own section below to remain 1:1 with the source set.

---

### ABS (Abs.c) - Absolute value

- Manual section: 10.15. Microcode: label `ABS` @ octal `000263` (continuation
  `000264`-`000266`, integer path `C,ALU ALU,A-B CRY,ONE ALUF,A+B`, float path
  `ALU,ANDCA` clears sign bit; all three continuation words carry `ST,SAVA`).
- Opcode (octal, manual 10.15): `BYn 177400B+(n-1)`, `Hn 177404B+(n-1)`,
  `Wn 177410B+(n-1)`, `Fn 177414B+(n-1)`, `Dn 177414B+(n-1)`. n = 1..4.
- Operation: absolute value of Rn -> Rn. BY/H store result in low bits and clear
  the rest of the register.
- Operands: 0 (register-only).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 10.15 lists no K; no K micro-op at `000263`-`000266` |
| Z | CONDITIONAL: set if result = 0 | manual 10.15 ("result = 0 -> Z"); mc `ST,SAVA` |
| C | CLEARED (not listed) | manual 10.15 (not mentioned -> reset per 6.5.1); mc `ST,SAVA` saves ALU carry, which is 0 for a non-negating pass - see note |
| O | CONDITIONAL: set on overflow (integer only: greatest negative integer negated) | manual 10.15 ("overflow -> O (integer)"); mc `ST,SAVA` |
| S | CLEARED (forced 0) | manual 10.15 ("0 -> S"; absolute value is never negative); mc float path forces sign bit off |

- Trap conditions: Integer overflow (O). (Manual 10.15.)
- Note: manual 10.15 explicitly lists only `Z`, `S=0`, `O`. C is therefore reset
  by the 6.5.1 "not mentioned" rule. The microcode `ST,SAVA` would latch the ALU
  carry; for a positive input (no two's-complement negate) the carry is 0.
  `UNKNOWN (needs verification)`: exact C value when a negative non-overflowing
  value is negated (two's complement carry-out) - manual says C is not a
  documented output of ABS.

---

### ADD (Add.c) - Add operand to register (`+`)

- Manual section: 11.1. Microcode: integer `ADD` @ octal `000267`
  (`ALU,A+B ... ST,SAVA`); float `ADDF` @ octal `002167`
  (`ALU,FZRO AAP2,ADD ... ADACT`, floating status via `ST,SAVF` continuation);
  double `ADDD` @ octal `002204`.
- Opcode (octal, manual 11.1): `BYn 176064B+(n-1)`, `Hn 176070B+(n-1)`,
  `Wn 124B+(n-1)`, `Fn 130B+(n-1)`, `Dn 134B+(n-1)`.
- Operation: Rn + <addend> -> Rn.
- Operands: 1 (`<addend/r/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.1 (no K); no K micro-op |
| Z | CONDITIONAL: set if sum = 0 | manual 11.1; mc `ST,SAVA` (int) / `ST,SAVF` (float) |
| C | CONDITIONAL (integer only): set on carry out of MSB; CLEARED for float | manual 11.1 ("carry from most significant bit -> C (integer)"); mc `ST,SAVA` |
| O | CONDITIONAL: integer overflow; float path forces `0 -> O` | manual 11.1 ("overflow -> O", "0 -> O (float)"); mc |
| S | CONDITIONAL: sum sign bit | manual 11.1 ("sum.signbit -> S") |
| FU | CONDITIONAL (float): floating underflow | manual 11.1; mc float `AAP2,ADD` + `ST,SAVF` |
| FO | CONDITIONAL (float): floating overflow | manual 11.1; mc float path |

- Trap conditions: Addressing traps, Integer overflow (O), Floating overflow
  (FO), Floating underflow (FU). (Manual 11.1.)

---

### ADD2 (Add2.c) - Add two operands

- Manual section: 11.5. Microcode: integer `ADD2` @ octal `000271`; float
  `ADD2F` @ octal `002172` (`EA1SAVE ADACT`, floating status via continuation).
- Opcode (octal, manual 11.5): `BY 176027B`, `H 176124B`, `W 123B`,
  `F 176126B`, `D 176127B`. (Manual gives one code per type; register-tagged
  variants Wn/etc. are per section 7.3.)
- Operation: manual 11.5 states `<a> + <b> -> <a>` (result to first operand).
  (The emulator header claims `<a> + Rn -> <b>`; the MANUAL is authoritative:
  two-operand form writes the first operand.)
- Operands: 2 (`<a/rw/t>, <b/r/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.5 |
| Z | CONDITIONAL: set if result = 0 | manual 11.5 |
| C | CONDITIONAL (integer only): carry from MSB; cleared for float | manual 11.5 ("carry from most significant bit -> C (integer)") |
| O | CONDITIONAL: overflow | manual 11.5 |
| S | CONDITIONAL: result sign bit | manual 11.5 |
| FU | CONDITIONAL (float) | manual 11.5 |
| FO | CONDITIONAL (float) | manual 11.5 |

- Trap conditions: Addressing traps, Integer overflow (O), FO, FU. (Manual 11.5.)

---

### ADD3 (Add3.c) - Add three operands

- Manual section: 11.9. Microcode: `ADD3` @ octal `000277`.
- Opcode (octal, manual 11.9): `BY 176147B`, `H 176150B`, `W 176151B`,
  `F 176152B`, `D 176153B`.
- Operation: `<a> + <b> -> <c>`.
- Operands: 3 (`<a/r/t>, <b/r/t>, <c/w/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.9 |
| Z | CONDITIONAL: sum = 0 | manual 11.9 |
| C | CONDITIONAL (integer only): carry from MSB | manual 11.9 |
| O | CONDITIONAL: overflow | manual 11.9 |
| S | CONDITIONAL: sum sign bit | manual 11.9 |
| FU | CONDITIONAL (float) | manual 11.9 |
| FO | CONDITIONAL (float) | manual 11.9 |

- Trap conditions: Addressing traps, Integer overflow (O), FO, FU. (Manual 11.9.)

---

### ADDC (Addc.c) - Add with carry

- Manual section: 11.17. Microcode: `ADDC` @ octal `000305`
  (`ALU,A+B CRY,C ... ST,SAVA`; `CRY,C` = carry-in taken from status C).
- Opcode (octal, manual 11.17): `Wn 177100B+(n-1)`. Word only.
- Operation: Rn + C + <addend> -> Rn.
- Operands: 1 (`<addend/r/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.17 |
| Z | CONDITIONAL: sum = 0 | manual 11.17; mc `ST,SAVA` |
| C | CONDITIONAL: carry from MSB (also consumed as carry-in) | manual 11.17 ("carry from most significant bit -> C"); mc `CRY,C` in, `ST,SAVA` out |
| O | CONDITIONAL: integer overflow | manual 11.17 ("integer overflow -> O") |
| S | CONDITIONAL: sum sign bit | manual 11.17 |

- Trap conditions: Addressing traps, Integer overflow (O). (Manual 11.17.)

---

### AXI (Axi.c) - A to the I'th power (floating exponentiation)

- Manual section: 12.1. Microcode: `AXIF` @ octal `001437` (`ST,SAVA` then
  `ADACT` float loop; IOV via `CLE_IOV` path @ `001441`); `AXID` @ octal `001446`.
- Opcode (octal, manual 12.1): `Fn 176300B+(n-1)`, `Dn 176304B+(n-1)`.
  Float/double base only.
- Operation: `<a> ** <i> -> Rn`. <a> float/double, <i> word integer. i=0 -> 1.
  Negative i with a=0 -> IOV trap and result set to largest float (~5.8E+76).
- Operands: 2 (`<a/r/t>, <i/r/W>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 12.1 lists no K |
| Z | CONDITIONAL: result = 0 | manual 12.1 |
| C | CLEARED (float op; not listed) | manual 12.1 (not mentioned) + 6.5.1 float rule |
| O | CLEARED (not listed; float instruction) | manual 12.1 (not mentioned) |
| S | CONDITIONAL: result sign bit | manual 12.1 |
| FU | CONDITIONAL: floating underflow | manual 12.1 |
| FO | CONDITIONAL: floating overflow | manual 12.1 |
| IVO | (trap only) - NOT a documented data-status output of AXI | see note |

- Trap conditions: Addressing traps, FO, FU, Illegal operand value (IOV).
  (Manual 12.1.)
- Note: manual 12.1 data-status list is Z,S,FU,FO only. IOV here is raised as a
  trap condition (negative exponent of zero base). `UNKNOWN (needs
  verification)`: whether the IVO status bit (bit 11) is set vs. only the IOV
  trap - manual 12.1 names the trap "Illegal operand value (IOV)" and does not
  list IVO among the data-status bits.

---

### DECR (Decr.c) - Decrement by one

- Manual section: 10.20. Microcode: `DECR` @ octal `000335`
  (`ALU,A-1 ... ST,SAVA`).
- Opcode (octal, manual 10.20): `BY 176214B`, `H 176215B`, `W 121B`,
  `F 176216B`, `D 176217B`.
- Operation: `<operand> - 1 -> <operand>`.
- Operands: 1 (`<operand/rw/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 10.20 |
| Z | CONDITIONAL: difference = 0 | manual 10.20; mc `ST,SAVA` |
| C | CONDITIONAL: carry from most significant bit | manual 10.20 ("carry from most significant bit -> C"); mc `ST,SAVA` |
| O | CONDITIONAL: overflow | manual 10.20 |
| S | CONDITIONAL: difference sign bit | manual 10.20 |

- Trap conditions: Addressing traps, Integer overflow (O). (Manual 10.20.)
- Note: manual defines C as the raw "carry from the most significant bit" of the
  adder (operand + two's-complement of 1). The emulator header's "C = 1 if NO
  borrow" is an interpretation, not the manual's wording; the manual wording is
  the authority.

---

### DIV2 (Div2.c) - Divide two operands

- Manual section: 11.8. Microcode: `DIV2BY` @ octal `002435`, `DIV2W` @ octal
  `002451` (integer, `ADACT` + `EA1SAVE`); `DIV2F` @ octal `002504`,
  `DIV2D` @ octal `002524` (float).
- Opcode (octal, manual 11.8): `BY 176142B`, `H 176143B`, `W 176144B`,
  `F 176145B`, `D 176146B`.
- Operation: `<a> / <b> -> <a>`. Integer remainder has sign of `<a>` (truncate
  toward 0). Integer overflow iff largest negative integer / -1.
- Operands: 2 (`<a/rw/t>, <b/r/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.8 |
| Z | CONDITIONAL: quotient = 0 | manual 11.8 |
| C | CLEARED (division; not listed) | manual 11.8 (not mentioned) + 6.5.1 |
| O | CONDITIONAL: overflow (MIN/-1) | manual 11.8 |
| S | CONDITIONAL: quotient sign bit | manual 11.8 |
| FU | CONDITIONAL (float) | manual 11.8 |
| FO | CONDITIONAL (float) | manual 11.8 |
| DZ | CONDITIONAL: set if `<b>` = 0 | manual 11.8 ("<b> = 0 -> DZ"); mc `COND,MDZ` |

- Trap conditions: Addressing traps, O, FO, FU, Divide by zero (DZ). (Manual 11.8.)

---

### DIV3 (Div3.c) - Divide three operands

- Manual section: 11.12. Microcode: `DIV3BY` @ octal `002457`, `DIV3W` @ octal
  `002473`; float `DIV3F`/`DIV3D` follow in the same block.
- Opcode (octal, manual 11.12): `BY 176166B`, `H 176167B`, `W 176170B`,
  `F 176171B`, `D 176172B`.
- Operation: `<a> / <b> -> <c>`. Same integer truncation/overflow rule as 11.8.
- Operands: 3 (`<a/r/t>, <b/r/t>, <c/w/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.12 |
| Z | CONDITIONAL: quotient = 0 | manual 11.12 |
| C | CLEARED (not listed) | manual 11.12 + 6.5.1 |
| O | CONDITIONAL: overflow | manual 11.12 |
| S | CONDITIONAL: quotient sign bit | manual 11.12 |
| FU | CONDITIONAL (float) | manual 11.12 |
| FO | CONDITIONAL (float) | manual 11.12 |
| DZ | CONDITIONAL: `<b>` = 0 | manual 11.12 |

- Trap conditions: Addressing traps, O, FO, FU, DZ. (Manual 11.12.)

---

### DIV4 (Div4.c) - Divide with remainder to register (modulo)

- Manual section: 11.14. Microcode: `DIV4BY` @ octal `002553`, `DIV4W` @ octal
  `002563` (`ST,SAVA` in the block; `CSAVE` here is the sequencer bit).
- Opcode (octal, manual 11.14): `BYn 176054B+(n-1)`, `Hn 176060B+(n-1)`,
  `Wn 176174B+(n-1)`. Integer only (BY/H/W).
- Operation: `<a> / <b> -> <c>`; remainder -> Rn (ADA/SIMULA remainder rule).
  Manual note: "Separate testing must be done to obtain status."
- Operands: 3 (`<a/r/t>, <b/r/t>, <c/w/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.14 |
| Z | CONDITIONAL: quotient = 0 | manual 11.14 |
| C | CLEARED (not listed) | manual 11.14 + 6.5.1 |
| O | CONDITIONAL: overflow | manual 11.14 |
| S | CONDITIONAL: quotient sign bit | manual 11.14 |
| DZ | CONDITIONAL: `<b>` = 0 | manual 11.14 |

- Trap conditions: Addressing traps, Integer overflow (O), Divide by zero (DZ).
  (Manual 11.14.) Note: no FU/FO (integer only).

---

### DIVIDE (Divide.c) - Divide register by operand (`/`)

- Manual section: 11.4. Microcode: `DIVBY` @ octal `002416`, `DIVW` @ octal
  `002430` (integer, `ADACT`); `DIVF` @ octal `002501`, `DIVD` @ octal `002516`
  (float).
- Opcode (octal, manual 11.4): `BYn 176114B+(n-1)`, `Hn 176120B+(n-1)`,
  `Wn 170B+(n-1)`, `Fn 174B+(n-1)`, `Dn 350B+(n-1)`.
- Operation: Rn / <divisor> -> Rn. Integer remainder has sign of register
  (truncate toward 0). Overflow iff MIN / -1.
- Operands: 1 (`<divisor/r/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.4 |
| Z | CONDITIONAL: quotient = 0 | manual 11.4 |
| C | CLEARED (division; not listed) | manual 11.4 + 6.5.1 |
| O | CONDITIONAL: overflow | manual 11.4 |
| S | CONDITIONAL: quotient sign bit | manual 11.4 |
| FU | CONDITIONAL (float) | manual 11.4 |
| FO | CONDITIONAL (float) | manual 11.4 |
| DZ | CONDITIONAL: divisor = 0 | manual 11.4 ("divisor = 0 -> DZ"); mc `COND,MDZ` |

- Trap conditions: Addressing traps, O, FO, FU, DZ. (Manual 11.4.)

---

### INCR (Incr.c) - Increment by one

- Manual section: 10.19. Microcode: `INCR` @ octal `000333`
  (`ALU,A CRY,ONE ... ST,SAVA`; i.e. operand + 1 via carry-in).
- Opcode (octal, manual 10.19): `BY 176212B`, `H 116B`, `W 117B`, `F 120B`,
  `D 176213B`.
- Operation: `<operand> + 1 -> <operand>`. Manual: carry set iff integer -1
  incremented.
- Operands: 1 (`<operand/rw/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 10.19 |
| Z | CONDITIONAL: sum = 0 | manual 10.19; mc `ST,SAVA` |
| C | CONDITIONAL (integer): carry from MSB (only when -1 incremented) | manual 10.19; mc `ST,SAVA` |
| O | CONDITIONAL: overflow | manual 10.19 |
| S | CONDITIONAL: sum sign bit | manual 10.19 |

- Trap conditions: Addressing traps, Integer overflow (O). (Manual 10.19.)

---

### IXI (Ixi.c) - I to the J'th power (integer exponentiation)

- Manual section: 12.2. Microcode: `IXIBY` @ octal `001460`, `IXIHW` @ octal
  `001465`, `IXIW` @ octal `001472` (each: `ADACT` loop, `CLE_IOV` path for IOV,
  `K,1IFZ`-free entry; status via continuation).
- Opcode (octal, manual 12.2): `BYn 176310B+(n-1)`, `Hn 176314B+(n-1)`,
  `Wn 176320B+(n-1)`. Integer only.
- Operation: `<i> ** <j> -> Rn`. j=0 -> 1. Negative j with i != 1,-1 -> 0.
  Negative j with i=0 -> IOV trap and zero result. On overflow, low part stored,
  rest lost, O flagged.
- Operands: 2 (`<i/r/t>, <j/r/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED (no K,* on entry path; IXI is not a K-setting op per manual) | manual 12.2 lists no K; mc entries `001460/001465/001472` carry no `K,ONE/ZRO` |
| Z | CONDITIONAL: result = 0 | manual 12.2 |
| C | CLEARED (not listed) | manual 12.2 + 6.5.1 |
| O | CONDITIONAL: overflow | manual 12.2 ("overflow -> O") |
| S | CONDITIONAL: result sign bit | manual 12.2 |

- Trap conditions: Addressing traps, Illegal operand value (IOV), Integer
  overflow (O). (Manual 12.2.)
- Note: manual 6.5.1 special-cases IXI ("Floating underflow ... except in the
  POLY and IXI instructions") but for the integer IXI the listed data-status
  bits are Z,S,O only.

---

### MUL2 (Mul2.c) - Multiply two operands

- Manual section: 11.7. Microcode: integer `MUL2BY` @ octal `002311`,
  `MUL2W` @ octal `002321` (`AAP2,IMUL` + `ST,SAVM`); float `MUL2F` @ octal
  `002360` (`AAP2,MUL` + `ST,SAVF`), `MUL2D` @ octal `002400`.
- Opcode (octal, manual 11.7): `BY 176135B`, `H 176136B`, `W 176137B`,
  `F 176140B`, `D 176141B`.
- Operation: `<a> * <b> -> <a>`. Integer overflow iff upper half of the
  double-length product != sign extension of lower half.
- Operands: manual 11.7 format `t MUL2 <a/r/t>,<b/r/t>,<c/w/t>` (the emulator
  treats it as 2 operands; the MANUAL format line shows 3 fields - see note).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.7 |
| Z | CONDITIONAL: product = 0 | manual 11.7; mc `ST,SAVM` (int) / `ST,SAVF` (float) |
| C | CLEARED (multiply does not set C; not listed) | manual 11.7 (not mentioned) + 6.5.1 |
| O | CONDITIONAL: overflow | manual 11.7; mc `ST,SAVM` |
| S | CONDITIONAL: product sign bit | manual 11.7 |
| FU | CONDITIONAL (float) | manual 11.7; mc `ST,SAVF` |
| FO | CONDITIONAL (float) | manual 11.7; mc `ST,SAVF` |

- Trap conditions: Addressing traps, Integer overflow (O), FO, FU. (Manual 11.7.)
- Note (operand count): manual 11.7 header prints a 3-field format
  `<a>,<b>,<c>` but the operation line and section title say "two operands"
  (`<a> * <b> -> <a>`). `UNKNOWN (needs verification)`: whether MUL2 is truly
  2-operand (write-back to `<a>`) or 3-operand - the manual text is internally
  inconsistent; the operation line ("-> <a>") favors 2 operands.

---

### MUL3 (Mul3.c) - Multiply three operands

- Manual section: 11.11. Microcode: `MUL3BY` @ octal `002325`, `MUL3W` @ octal
  `002335` (`AAP2,IMUL` + `ST,SAVM`); float `MUL3F` @ octal `002365`,
  `MUL3D` @ octal `002407` (`AAP2,MUL` + `ST,SAVF`).
- Opcode (octal, manual 11.11): `BY 176161B`, `H 176162B`, `W 176163B`,
  `F 176164B`, `D 176165B`.
- Operation: `<a> * <b> -> <c>`. Integer overflow iff upper half != sign
  extension of lower half.
- Operands: 3 (`<a/r/t>, <b/r/t>, <c/w/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.11 |
| Z | CONDITIONAL: product = 0 | manual 11.11 |
| C | CLEARED (not listed) | manual 11.11 + 6.5.1 |
| O | CONDITIONAL: overflow | manual 11.11 |
| S | CONDITIONAL: product sign bit | manual 11.11 |
| FU | CONDITIONAL (float) | manual 11.11 |
| FO | CONDITIONAL (float) | manual 11.11 |

- Trap conditions: Addressing traps, Integer overflow (O), FO, FU. (Manual 11.11.)

---

### MUL4 (Mul4.c) - Multiply with overflow to register

- Manual section: 11.13. Microcode: `MUL4BY` @ octal `002341`, `MUL4HW` @ octal
  `002345`, `MUL4W` @ octal `002351` (`AAP2,IMULD` = double-length integer
  multiply).
- Opcode (octal, manual 11.13): `BYn 176040B+(n-1)`, `Hn 176044B+(n-1)`,
  `Wn 176050B+(n-1)`. Integer only (BY/H/W); NO float/double.
- Operation: `<a> * <b> -> <c>`; overflow (upper half of double-length product)
  -> Rn.
- Operands: 3 (`<a/r/t>, <b/r/t>, <c/w/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.13 |
| Z | CONDITIONAL: lower part of double-length result = 0 | manual 11.13 |
| C | CLEARED (not listed) | manual 11.13 + 6.5.1 |
| O | CONDITIONAL: overflow | manual 11.13 |
| S | CONDITIONAL: lower part sign bit | manual 11.13 |

- Trap conditions: Addressing traps, Integer overflow (O). (Manual 11.13.)

---

### MULAD (Mulad.c) - Multiply and add

- Manual section: 11.19. Microcode: integer `MULADBY` @ octal `002617`,
  `MULADW` @ octal `002627` (`AAP2,IMUL`); float `MULADF` @ octal `002633`,
  `MULADD` @ octal `002635` (`AAP2,MUL`).
- Opcode (octal, manual 11.19): `BYn 176350B+(n-1)`, `Hn 176354B+(n-1)`,
  `Wn 250B+(n-1)`, `Fn 176360B+(n-1)`, `Dn 176364B+(n-1)`.
- Operation: Rn * <x> + <y> -> Rn.
- Operands: 2 (`<x/r/t>, <y/r/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.19 |
| Z | CONDITIONAL: result = 0 | manual 11.19 |
| C | CONDITIONAL (integer): carry from MSB | manual 11.19 ("carry from most significant bit -> C (integer)") |
| O | CONDITIONAL: overflow | manual 11.19 |
| S | CONDITIONAL: result sign bit | manual 11.19 |
| FU | CONDITIONAL (float) | manual 11.19 |
| FO | CONDITIONAL (float) | manual 11.19 |

- Trap conditions: Addressing traps, Integer overflow (O), FO, FU. (Manual 11.19.)
- Note: MULAD is the one multiply-family instruction whose manual data-status
  list DOES include C ("carry from most significant bit -> C (integer)"),
  because of the trailing add. (Plain MUL/MUL2/MUL3/MUL4 do not list C.)

---

### MULTIPLY (Multiply.c) - Multiply register by operand (`*`)

- Manual section: 11.3. Microcode: integer `MULBY` @ octal `002275`,
  `MULHW` @ octal `002301`, `MULW` @ octal `002305` (`AAP2,IMUL` + `ST,SAVM`);
  float `MULF` @ octal `002355`, `MULD` @ octal `002372` (`AAP2,MUL` + `ST,SAVF`).
- Opcode (octal, manual 11.3): `BYn 176104B+(n-1)`, `Hn 176110B+(n-1)`,
  `Wn 154B+(n-1)`, `Fn 160B+(n-1)`, `Dn 164B+(n-1)`.
- Operation: Rn * <multiplier> -> Rn. Integer overflow iff upper half of
  double-length result != sign extension of lower half.
- Operands: 1 (`<multiplier/r/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.3 |
| Z | CONDITIONAL: product = 0 | manual 11.3; mc `ST,SAVM`/`ST,SAVF` |
| C | CLEARED (multiply; not listed) | manual 11.3 (not mentioned) + 6.5.1 |
| O | CONDITIONAL: overflow | manual 11.3; mc `ST,SAVM` |
| S | CONDITIONAL: product sign bit | manual 11.3 |
| FU | CONDITIONAL (float) | manual 11.3; mc `ST,SAVF` |
| FO | CONDITIONAL (float) | manual 11.3; mc `ST,SAVF` |

- Trap conditions: Addressing traps, Integer overflow (O), FO, FU. (Manual 11.3.)
- Note: the emulator header for Multiply.c claims "C = 1 if carry from MSB (upper
  part non-zero)". The MANUAL does NOT list C for MULTIPLY, and `ST,SAVM` is the
  mixed-multiply status (Z,S,O); by 6.5.1 C is CLEARED. Treat the emulator's C
  claim as a likely bug to validate.

---

### NEG (Neg.c) - Negate register

- Manual section: 10.12. Microcode: `NEG` @ octal `000254`
  (`ALU,B-A CRY,ONE ... ST,SAVA`; two's complement 0 - Rn).
- Opcode (octal, manual 10.12): `BYn 177010B+(n-1)`, `Hn 177014B+(n-1)`,
  `Wn 220B+(n-1)`, `Fn 224B+(n-1)`, `Dn 224B+(n-1)`.
- Operation: -Rn -> Rn. Integer = two's complement; float = invert sign bit.
  BY/H clear the upper part. Overflow iff greatest negative integer negated.
  Manual: "Carry is zero except when integer zero is negated."
- Operands: 0 (register-only).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 10.12 |
| Z | CONDITIONAL: negated register = 0 | manual 10.12; mc `ST,SAVA` |
| C | CONDITIONAL: carry (set only when integer zero is negated) | manual 10.12 ("carry -> C"; and text: carry zero except when negating 0); mc `ST,SAVA` |
| O | CONDITIONAL: overflow (greatest negative integer negated) | manual 10.12 |
| S | CONDITIONAL: negated register sign bit | manual 10.12 ("negated register.signbit -> S") |

- Trap conditions: Integer overflow (O). (Manual 10.12.)

---

### PADD (Padd.c) - Packed add

- Manual section: 17.2. Microcode: `ADDBCD` @ octal `002043` (loop step
  `002044` carries `K,1IFZ`; BCD result status via `AD_ARTI=1 ADACT` coprocessor
  - see the packed-group microcode limitation note in the legend).
- Opcode: manual 17.2 gives `PADD` hex `0FE80H`, octal `177260B`. (The emulator
  header's `0xFEB0` differs from the manual `0xFE80`; MANUAL is authoritative.)
- Operation: `<a> + <b> -> <c>`, scaled to `<c>` descriptor. Packed BCD.
- Operands: 3 (`<a/r/BCD>, <b/r/BCD>, <c/w/BCD>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: set if BO or IVO, else cleared | manual 17.1 ("K flag is set up on BCD overflow or invalid operation, otherwise cleared") and 17.2 ("BO or IVO -> K") |
| Z | CONDITIONAL: sum = 0 (after scaling) | manual 17.2 |
| C | CLEARED (BCD; not listed) | manual 17.2 (not mentioned) + 6.5.1 |
| O | CLEARED (not listed; integer-only bit) | manual 17.2 (not mentioned) |
| S | CONDITIONAL: sum sign bit | manual 17.2 ("sum.signbit -> S") |
| BO | CONDITIONAL: BCD overflow (destination too narrow) | manual 17.2 |
| IVO | CONDITIONAL: invalid operation (bad digit/sign code) | manual 17.1 (implied), 17.2 trap list |

- Trap conditions: Addressing traps, BCD overflow (BO), Invalid operation (IVO).
  (Manual 17.2.) BCD instructions require the BCD hardware option (manual 17.1).

---

### PADDR (Paddr.c) - Packed add rounded

- Manual section: 17.2 (PADDR row). Microcode: `ADDBCDR` @ octal `002045`.
- Opcode: manual 17.2 gives `PADDR` hex `0FE85H`, octal `177205B`.
- Operation: `<a> + <b> -> <c>` with rounding, scaled to `<c>` descriptor.
- Operands: 3 (`<a/r/BCD>, <b/r/BCD>, <c/w/BCD>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: BO or IVO -> set, else cleared | manual 17.1 / 17.2 |
| Z | CONDITIONAL: value after rounding = 0 | manual 17.2 + 17.1 |
| C | CLEARED (not listed) | 6.5.1 |
| O | CLEARED (not listed) | 6.5.1 |
| S | CONDITIONAL: result sign bit | manual 17.2 |
| BO | CONDITIONAL: BCD overflow | manual 17.2 |
| IVO | CONDITIONAL: invalid operation | manual 17.2 |

- Trap conditions: Addressing traps, BO, IVO. (Manual 17.2.)

---

### PMPY (Pmpy.c) - Packed multiply

- Manual section: 17.4. Microcode: `MPYBCD` @ octal `002061`.
- Opcode: manual 17.4 gives `PMPY` hex `0FE34H`, octal `177264B`. (Emulator
  header `0xFEB4` differs; MANUAL authoritative.)
- Operation: `<a> * <b> -> <c>`, scaled to `<c>` descriptor. Special case:
  invalid digit * ZRO gives result 0 (not IVO).
- Operands: 3 (`<a/r/BCD>, <b/r/BCD>, <c/w/BCD>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: BO or IVO -> set, else cleared | manual 17.1 / 17.4 ("BO or IVO -> K") |
| Z | CONDITIONAL: product = 0 | manual 17.4 |
| C | CLEARED (not listed) | 6.5.1 |
| O | CLEARED (not listed) | 6.5.1 |
| S | CONDITIONAL: product sign bit | manual 17.4 (printed as "product.signbit -> C", an OCR typo for S; the general rule 17.1 and every other packed op set S from the sign bit) |
| BO | CONDITIONAL: BCD overflow | manual 17.4 |
| IVO | CONDITIONAL: invalid operation | manual 17.4 |

- Trap conditions: Addressing traps, BO, IVO. (Manual 17.4.)
- Note: manual 17.4 data-status line literally reads "product.signbit -> C" -
  this is an OCR/transcription error; S (sign, bit 7) is the sign output per
  section 17.1 and the identical wording in PADD/PSUB/PMPYR. `UNKNOWN (needs
  verification)` only in the sense that the OCR is corrupt; the intended flag is S.

---

### PMPYR (Pmpyr.c) - Packed multiply rounded

- Manual section: 17.4 (PMPYR row). Microcode: `MPYBCDR` @ octal `002063`.
- Opcode: manual 17.4 gives `PMPYR` hex `0FE91H`, octal `177221B`.
- Operation: `<a> * <b> -> <c>` with rounding. Same invalid-digit*0 special case.
- Operands: 3 (`<a/r/BCD>, <b/r/BCD>, <c/w/BCD>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: BO or IVO -> set, else cleared | manual 17.1 / 17.4 |
| Z | CONDITIONAL: value after rounding = 0 | manual 17.4 + 17.1 |
| C | CLEARED (not listed) | 6.5.1 |
| O | CLEARED (not listed) | 6.5.1 |
| S | CONDITIONAL: result sign bit | manual 17.4 / 17.1 |
| BO | CONDITIONAL: BCD overflow | manual 17.4 |
| IVO | CONDITIONAL: invalid operation | manual 17.4 |

- Trap conditions: Addressing traps, BO, IVO. (Manual 17.4.)

---

### PPACK (Ppack.c) - Convert ASCII to packed

- Manual section: 17.7. Microcode: `PACK` @ octal `002065`.
- Opcode: manual 17.7 gives `PPACK` hex `0FE95H`, octal `177265B`. (Emulator
  header `0xFEB5` differs; MANUAL authoritative.)
- Operation: `<source>` (ASCII decimal) -> `<dest>` (packed BCD). Unsigned if
  `<dest>` descriptor bit 26 set, else sign taken from `<source>`.
- Operands: 2 (`<source/r/ASCII>, <dest/w/BCD>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: BO or IVO -> set, else cleared | manual 17.1 / 17.7 |
| Z | CONDITIONAL: value after rounding = 0 | manual 17.7 |
| C | CLEARED (not listed) | 6.5.1 |
| O | CLEARED (not listed) | 6.5.1 |
| S | CONDITIONAL: value sign bit | manual 17.7 |
| BO | CONDITIONAL: BCD overflow | manual 17.7 |
| IVO | CONDITIONAL: invalid operation | manual 17.7 |

- Trap conditions: Addressing traps, BO, IVO. (Manual 17.7.)

---

### PPACKR (Ppackr.c) - Convert ASCII to packed, rounded

- Manual section: 17.7 (PPACKR row). Microcode: `PACKR` @ octal `002067`.
- Opcode: manual 17.7 gives `PPACKR` hex `0FE92H`, octal `177222B` (manual prints
  `17722B`, a truncated OCR of `177222B`).
- Operation: as PPACK, with rounding before store.
- Operands: 2 (`<source/r/ASCII>, <dest/w/BCD>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: BO or IVO -> set, else cleared | manual 17.1 / 17.7 |
| Z | CONDITIONAL: value after rounding = 0 | manual 17.7 |
| C | CLEARED (not listed) | 6.5.1 |
| O | CLEARED (not listed) | 6.5.1 |
| S | CONDITIONAL: value sign bit | manual 17.7 |
| BO | CONDITIONAL: BCD overflow | manual 17.7 |
| IVO | CONDITIONAL: invalid operation | manual 17.7 |

- Trap conditions: Addressing traps, BO, IVO. (Manual 17.7.)

---

### PSUB (Psub.c) - Packed subtract

- Manual section: 17.3. Microcode: `SUBBCD` @ octal `002047`.
- Opcode: manual 17.3 gives `PSUB` hex `0FEB1H`, octal `177261B`.
- Operation: `<a> - <b> -> <c>`, scaled to `<c>` descriptor.
- Operands: 3 (`<a/r/BCD>, <b/r/BCD>, <c/w/BCD>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: BO or IVO -> set, else cleared | manual 17.1 / 17.3 |
| Z | CONDITIONAL: difference = 0 | manual 17.3 |
| C | CLEARED (not listed) | 6.5.1 |
| O | CLEARED (not listed) | 6.5.1 |
| S | CONDITIONAL: difference sign bit | manual 17.3 |
| BO | CONDITIONAL: BCD overflow | manual 17.3 |
| IVO | CONDITIONAL: invalid operation | manual 17.3 |

- Trap conditions: Addressing traps, BO, IVO. (Manual 17.3.)

---

### PSUBR (Psubr.c) - Packed subtract rounded

- Manual section: 17.3 (PSUBR row). Microcode: `SUBBCDR` @ octal `002051`.
- Opcode: manual 17.3 gives `PSUBR` hex `0FE86H`, octal `177206B`.
- Operation: `<a> - <b> -> <c>` with rounding, scaled to `<c>` descriptor.
- Operands: 3 (`<a/r/BCD>, <b/r/BCD>, <c/w/BCD>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: BO or IVO -> set, else cleared | manual 17.1 / 17.3 |
| Z | CONDITIONAL: value after rounding = 0 | manual 17.3 + 17.1 |
| C | CLEARED (not listed) | 6.5.1 |
| O | CLEARED (not listed) | 6.5.1 |
| S | CONDITIONAL: result sign bit | manual 17.3 |
| BO | CONDITIONAL: BCD overflow | manual 17.3 |
| IVO | CONDITIONAL: invalid operation | manual 17.3 |

- Trap conditions: Addressing traps, BO, IVO. (Manual 17.3.)

---

### PSUM (Psum.c) - Sum of products (multiply-accumulate)

- Manual section: 11.20. Microcode: integer `PSUMBY` @ octal `002640`,
  `PSUMW` @ octal `002650`; float `PSUMF` @ octal `002655`.
- Opcode (octal, manual 11.20): `BYn 176370B+(n-1)`, `Hn 176374B+(n-1)`,
  `Wn 176400B+(n-1)`, `Fn 176404B+(n-1)`, `Dn 176410B+(n-1)`.
- Operation: `<x> * <y> + Rn -> Rn`.
- Operands: 2 (`<x/r/t>, <y/r/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.20 |
| Z | CONDITIONAL: result = 0 | manual 11.20 |
| C | CONDITIONAL (integer): carry from MSB | manual 11.20 ("carry from most significant bit -> C (integer)") |
| O | CONDITIONAL: overflow | manual 11.20 |
| S | CONDITIONAL: result sign bit | manual 11.20 |
| FU | CONDITIONAL (float) | manual 11.20 |
| FO | CONDITIONAL (float) | manual 11.20 |

- Trap conditions: Addressing traps, Integer overflow (O), FO, FU. (Manual 11.20.)

---

### PUPACK (Pupack.c) - Convert packed to ASCII

- Manual section: 17.8. Microcode: `UNPACK` @ octal `002071`.
- Opcode: manual 17.8 gives `PUPACK` hex `0FBE6H` (OCR; likely `0FE96H`), octal
  `177266B`. (Emulator header `0xFEB6` differs; MANUAL octal `177266B`
  authoritative.)
- Operation: `<source>` (packed BCD) -> `<dest>` (ASCII). Sign from `<dest>` SGN
  field; leading ASCII zeros added; parity bit zero.
- Operands: 2 (`<source/r/BCD>, <dest/w/ASCII>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: BO or IVO -> set, else cleared | manual 17.1 / 17.8 |
| Z | CONDITIONAL: value after rounding = 0 | manual 17.8 |
| C | CLEARED (not listed) | 6.5.1 |
| O | CLEARED (not listed) | 6.5.1 |
| S | CONDITIONAL: value sign bit | manual 17.8 |
| BO | CONDITIONAL: BCD overflow | manual 17.8 |
| IVO | CONDITIONAL: invalid operation | manual 17.8 |

- Trap conditions: Addressing traps, BO, IVO. (Manual 17.8.)

---

### PUPACKR (Pupackr.c) - Convert packed to ASCII, rounded

- Manual section: 17.8 (PUPACKR row). Microcode: `UNPACKR` @ octal `002073`.
- Opcode: manual 17.8 gives `PUPACKR` hex `0FE93H`, octal `177223B`.
- Operation: as PUPACK, with rounding before store.
- Operands: 2 (`<source/r/BCD>, <dest/w/ASCII>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL: BO or IVO -> set, else cleared | manual 17.1 / 17.8 |
| Z | CONDITIONAL: value after rounding = 0 | manual 17.8 |
| C | CLEARED (not listed) | 6.5.1 |
| O | CLEARED (not listed) | 6.5.1 |
| S | CONDITIONAL: value sign bit | manual 17.8 |
| BO | CONDITIONAL: BCD overflow | manual 17.8 |
| IVO | CONDITIONAL: invalid operation | manual 17.8 |

- Trap conditions: Addressing traps, BO, IVO. (Manual 17.8.)

---

### REM (Rem.c) - Floating point remainder

- Manual section: 10.33. Microcode: `REMF` @ octal `002133`
  (`ST,SAVA` then float `ADACT` loop, `CSAVE` sequencer); `REMD` @ octal `002136`.
- Opcode (octal, manual 10.33): `Fn 177130B+(n-1)`, `Dn 177134B+(n-1)`.
  Float/double ONLY - REM has no integer variants.
- Operation: `<q> = int(<x>/<y>)` (float format) -> `<q>`;
  `Rn = <x> - <q>*<y>` (remainder, float format) -> register n.
- Operands: 3 (`<x/r/t>, <y/r/t>, <q/w/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 10.33 lists no K |
| Z | CONDITIONAL: remainder = 0 | manual 10.33 ("remainder = 0 -> Z") |
| C | CLEARED (float; not listed) | manual 10.33 (not mentioned) + 6.5.1 float rule |
| O | CLEARED (float; not listed) | manual 10.33 (not mentioned) |
| S | CONDITIONAL: remainder sign bit | manual 10.33 |
| FU | CONDITIONAL: floating underflow | manual 10.33 |
| FO | CONDITIONAL: floating overflow | manual 10.33 |
| DZ | CONDITIONAL: `<y>` = 0 | manual 10.33 ("<y> = 0 -> DZ") |

- Trap conditions: Addressing traps, FO, FU, Divide by zero (DZ). (Manual 10.33.)

---

### SUB (Sub.c) - Subtract from register (`-`)

Same architectural instruction as Subtract.c (manual 11.2). Documented here as its
own file per the enumerated set.

- Manual section: 11.2. Microcode: integer `SUB` @ octal `000270`
  (`ALU,B-A CRY,ONE ... ST,SAVA`; two's-complement subtract); float `SUBF` @
  octal `002230` (`AAP2,SUBBA` = B minus A, float status via continuation);
  double `SUBD` @ octal `002246`.
- Opcode (octal, manual 11.2): `BYn 176074B+(n-1)`, `Hn 176100B+(n-1)`,
  `Wn 140B+(n-1)`, `Fn 144B+(n-1)`, `Dn 150B+(n-1)`.
- Operation: Rn - <subtrahend> -> Rn. (Manual: subtraction is addition of the
  two's complement; C set by the same MSB-carry rule as ADD.)
- Operands: 1 (`<subtrahend/r/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.2 |
| Z | CONDITIONAL: difference = 0 | manual 11.2; mc `ST,SAVA` |
| C | CONDITIONAL (integer): carry from MSB (borrow), cleared for float | manual 11.2 ("carry from the most significant bit -> C (integer)"); mc `ST,SAVA` |
| O | CONDITIONAL: overflow | manual 11.2 |
| S | CONDITIONAL: difference sign bit | manual 11.2 |
| FU | CONDITIONAL (float) | manual 11.2 |
| FO | CONDITIONAL (float) | manual 11.2 |

- Trap conditions: Addressing traps, Integer overflow (O), FO, FU. (Manual 11.2.)
- Note: the emulator header's "C = 1 if NO borrow" is an interpretation. The
  MANUAL defines C as the raw carry out of the MSB of the (Rn + ~subtrahend + 1)
  adder; validate the emulator's polarity against that.

---

### SUB2 (Sub2.c) - Subtract two operands

- Manual section: 11.6. Microcode: `SUB2` @ octal `000274`; float `SUB2F` @
  octal `002234`.
- Opcode (octal, manual 11.6): `BY 176130B`, `H 176131B`, `W 340B`,
  `F 176133B`, `D 176134B`.
- Operation: manual 11.6 states `<a> - <b> -> <a>` (result to first operand).
  (Emulator header claims `<a> - Rn -> <b>`; MANUAL is authoritative:
  `<a> - <b> -> <a>`.)
- Operands: 2 (`<a/w/t>, <b/r/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.6 |
| Z | CONDITIONAL: difference = 0 | manual 11.6 |
| C | CONDITIONAL (integer): carry from MSB | manual 11.6 |
| O | CONDITIONAL: overflow | manual 11.6 |
| S | CONDITIONAL: difference sign bit | manual 11.6 |
| FU | CONDITIONAL (float) | manual 11.6 |
| FO | CONDITIONAL (float) | manual 11.6 |

- Trap conditions: Addressing traps, Integer overflow (O), FO, FU. (Manual 11.6.)

---

### SUB3 (Sub3.c) - Subtract three operands

- Manual section: 11.10. Microcode: `SUB3` @ octal `000302`.
- Opcode (octal, manual 11.10): `BY 176154B`, `H 176155B`, `W 176156B`,
  `F 176157B`, `D 176160B`.
- Operation: `<a> - <b> -> <c>`.
- Operands: 3 (`<a/r/t>, <b/r/t>, <c/w/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.10 |
| Z | CONDITIONAL: difference = 0 | manual 11.10 |
| C | CONDITIONAL (integer): carry from MSB | manual 11.10 |
| O | CONDITIONAL: overflow | manual 11.10 |
| S | CONDITIONAL: difference sign bit | manual 11.10 |
| FU | CONDITIONAL (float) | manual 11.10 |
| FO | CONDITIONAL (float) | manual 11.10 |

- Trap conditions: Addressing traps, Integer overflow (O), FO, FU. (Manual 11.10.)

---

### SUBC (Subc.c) - Subtract with carry

- Manual section: 11.18. Microcode: `SUBC` @ octal `000306`
  (`ALU,B-A CRY,C ... ST,SAVA`; carry-in from status C).
- Opcode (octal, manual 11.18): `Wn 177104B+(n-1)`. Word only.
- Operation (manual 11.18): Rn + C + (one's complement of <subtrahend>) -> Rn,
  i.e. "Rn + C - <subtrahend> - 1 -> Rn".
- Operands: 1 (`<subtrahend/r/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.18 |
| Z | CONDITIONAL: result = 0 | manual 11.18; mc `ST,SAVA` |
| C | CONDITIONAL: carry from MSB (also consumed as carry-in) | manual 11.18 ("carry -> C"); mc `CRY,C` in, `ST,SAVA` out |
| O | CONDITIONAL: integer overflow | manual 11.18 |
| S | CONDITIONAL: result sign bit | manual 11.18 |

- Trap conditions: Addressing traps, Integer overflow (O). (Manual 11.18.)

---

### SUBTRACT (Subtract.c) - Subtract from register (`-`)

Same architectural instruction as Sub.c (manual 11.2). See the SUB section above
for full detail; flag semantics are identical.

- Manual section: 11.2. Microcode: `SUB` @ octal `000270` (integer),
  `SUBF` @ octal `002230` (float), `SUBD` @ octal `002246` (double).
- Opcode (octal, manual 11.2): `BYn 176074B+(n-1)`, `Hn 176100B+(n-1)`,
  `Wn 140B+(n-1)`, `Fn 144B+(n-1)`, `Dn 150B+(n-1)`.
- Operation: Rn - <operand> -> Rn.
- Operands: 1 (`<operand/r/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.2 |
| Z | CONDITIONAL: difference = 0 | manual 11.2; mc `ST,SAVA` |
| C | CONDITIONAL (integer): carry from MSB; cleared for float | manual 11.2; mc `ST,SAVA` |
| O | CONDITIONAL: overflow | manual 11.2 |
| S | CONDITIONAL: difference sign bit | manual 11.2 |
| FU | CONDITIONAL (float) | manual 11.2 |
| FO | CONDITIONAL (float) | manual 11.2 |

- Trap conditions: Addressing traps, Integer overflow (O), FO, FU. (Manual 11.2.)

---

### UDIV (Udiv.c) - Unsigned divide

- Manual section: 11.16. Microcode: `UDIV` @ octal `002572`
  (`ST,SAVA` in the block; unsigned quotient/remainder).
- Opcode (octal, manual 11.16): `Wn 177110B+(n-1)`. Word only.
- Operation: unsigned `<a> / <b> -> <c>`; remainder -> Rn. BY/H integer
  constants are sign-extended then treated as unsigned.
- Operands: 3 (`<a/r/t>, <b/r/t>, <c/w/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.16 |
| Z | CONDITIONAL: quotient = 0 | manual 11.16 |
| C | CLEARED (not listed) | manual 11.16 (not mentioned) + 6.5.1 |
| O | CLEARED (not listed; unsigned divide lists no O) | manual 11.16 (not mentioned) |
| S | CONDITIONAL: quotient sign bit | manual 11.16 ("quotient.signbit -> S") |
| DZ | CONDITIONAL: `<b>` = 0 | manual 11.16 ("<b> = 0 -> DZ") |

- Trap conditions: Addressing traps, Divide by zero (DZ). (Manual 11.16.)
  Note: no Integer-overflow trap listed (unlike signed DIV/DIV4).

---

### UMUL (Umul.c) - Unsigned multiply with overflow to register

- Manual section: 11.15. Microcode: `UMUL` @ octal `002567`
  (`AAP2,IMULUD` = unsigned double-length integer multiply).
- Opcode (octal, manual 11.15): `Wn 176200B+(n-1)`. Word only.
- Operation: unsigned `<a> * <b> -> <c>`; upper half of double-length product
  -> Rn. BY/H constants sign-extended then treated unsigned. Overflow iff upper
  part != 0.
- Operands: 3 (`<a/r/t>, <b/r/t>, <c/w/t>`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | manual 11.15 |
| Z | CONDITIONAL: product = 0 | manual 11.15 |
| C | CLEARED (not listed) | manual 11.15 (not mentioned) + 6.5.1 |
| O | CONDITIONAL: overflow (upper part != 0) | manual 11.15 ("overflow -> O") |
| S | CONDITIONAL: product sign bit | manual 11.15 ("product.signbit -> S") |

- Trap conditions: Addressing traps, Integer overflow (O). (Manual 11.15.)

---

## Cross-source discrepancies flagged for emulator validation

These are places where the emulator C-source headers disagree with the two
authoritative sources; each is a concrete validation target:

1. MULTIPLY (Multiply.c) claims C is set from the product upper half. Manual 11.3
   lists NO C for MULTIPLY, and the microcode uses `ST,SAVM` (mixed multiply
   status = Z,S,O). Per manual 6.5.1, C is CLEARED. Likely emulator bug.
2. SUB / SUBTRACT / DECR: emulator headers describe C as "1 if NO borrow". Manual
   defines C as the raw adder carry-out of the MSB (Rn + ~subtrahend + 1). The
   polarity/definition should be validated against the manual wording, not the
   "no borrow" paraphrase.
3. ADD2 / SUB2: emulator headers describe the destination as a separate `<b>`
   operand (`<a> op Rn -> <b>`). Manual 11.5/11.6 operation lines write the FIRST
   operand (`<a> op <b> -> <a>`). Validate operand semantics.
4. MUL2 operand count: manual 11.7 is internally inconsistent (title/operation
   say two operands `-> <a>`; format line prints three fields). `UNKNOWN (needs
   verification)`.
5. Packed opcodes: several emulator hex opcodes (PADD 0xFEB0, PMPY 0xFEB4,
   PPACK 0xFEB5, PUPACK 0xFEB6, PADDR 0xFE85, ...) differ from the manual's
   octal codes (PADD 177260B, PMPY 177264B, PPACK 177265B, PUPACK 177266B,
   PADDR 177205B). The manual octal values are authoritative; reconcile the
   dispatch table against them.
6. AXI IVO: manual 12.1 raises an "Illegal operand value (IOV)" TRAP but does not
   list the IVO data-status bit among AXI outputs. Whether the IVO status bit
   (bit 11) is set is `UNKNOWN (needs verification)`.

## Residual UNKNOWNs (not resolvable from the two sources)

- Exact per-bit contents of `ST,SAVM` (mixed integer-multiply status) beyond
  "Z, S, O" - the mnemonics file gives only the textual description.
- Packed (P*) final Z/S/BO/K micro-op: this control-store image contains no
  `ST,SAVB`; BCD result status is produced by the `AD_ARTI ADACT` BCD coprocessor
  whose internal status logic is not decodable from MICRO-5800-A30. Packed flag
  detail therefore rests on the MANUAL (sections 17.1-17.8).
- ABS integer C output for a negative, non-overflowing input (two's-complement
  carry-out) - manual does not document C as an ABS output.
