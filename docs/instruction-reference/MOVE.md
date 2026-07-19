# ND-500 CPU - MOVE category - Behavior Reference

Authoritative behavior reference for validating the nd500x emulator.
Every statement is read directly from one of the two primary sources listed
below. Anything not confirmed by either source is marked
`UNKNOWN (needs verification)`.

## Sources

- PRIMARY spec (manual):
  `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
  (Norsk Data ND-05.009.4 EN, ND-500 Reference Manual)
- GROUND-TRUTH microcode (ND-5000):
  `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
  (micro-addresses cited in octal, matching the `| <octal> | **LABEL** | ... |`
  rows of that file)
- Micro-op mnemonic decode:
  `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`

## Category source of the instruction set

The set is the 57 files in
`/home/ronny/repos/nd500x/src/cpu/instructions/MOVE/*.c`.
Those filenames only supplied the mnemonic used to locate each instruction in
the two primary sources; no behavior below is taken from the C source.

---

## Global conventions read from the manual (apply to every instruction)

### Data status bits and their numbering (manual section 6.5.1)

| Code | Name              | Bit no. |
|------|-------------------|---------|
| Z    | zero              | 5       |
| C    | carry             | 6       |
| S    | sign              | 7       |
| K    | flag (signalling) | 8       |
| O    | overflow          | 9       |

Note: `K` (bit 8) is classified by the manual (section 6.5.4) as a
signalling / synchronization bit, NOT a data status bit. It is set/cleared only
by dedicated instructions (SETK/CLRK), by string / CIND / LIND instructions, and
- for descriptor-addressed operands only - descriptor addressing "may set but
never clear the K flag". Ordinary MOVE-class operands do not use descriptor
addressing, so K is treated as UNCHANGED unless an operand is descriptor-addressed.

### The "unmentioned bits are cleared" rule (manual sections 6.5.1 and the
"DATA STATUS BITS" note at manual page 132)

Verbatim: "In the description of the instruction set, the effect on the data
status bits are listed with every instruction. Bits that are set, reset or left
unaffected are mentioned explicitly. All data status bits not mentioned are
reset." and "Data status bits not mentioned in the instruction description are
always cleared after the instruction has been executed."

Consequence for this whole category: for every instruction whose manual entry
lists only `Z` and `S`, the bits `C` and `O` (and IVO/DZ/FU/FO/BO) are CLEARED.

### Meaning of Z, S, C, O when they ARE set (manual section 6.5.1)

- Z: set if the operand/result of the last instruction was exactly zero, else cleared.
- S: holds the sign bit of the last operand/result.
- C (carry): "may be set only when performing integer arithmetic; otherwise it
  is cleared." A pure data transfer performs no integer arithmetic, so C = 0.
- O (overflow): "may be set only when performing integer arithmetic; otherwise
  it is cleared." A pure data transfer => O = 0.

### Microcode encoding of flags (decoded from
`/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md` lines 653-664)

| Micro-op    | Effect |
|-------------|--------|
| `ST,SAVA`   | SAVE STATUS FROM ALU OPERATION (updates Z / C / O / S from the ALU result) |
| `K,ONE`     | set K (flag) = 1 |
| `K,ZRO`     | clear K (flag) = 0 |
| `K,1IFZ`    | set K to 1 if the ALU operation result is 0 |
| `ALU,A`     | ALU passes A through unchanged (no carry, no overflow) |
| `ALU,FZRO`  | ALU forces a zero result |

So a data transfer coded `ALU,A ... ST,SAVA` yields Z from the moved value, S
from its sign, C = 0 and O = 0 - exactly matching the manual. `ALU,FZRO ...
ST,SAVA` yields Z = 1, S = 0, C = 0, O = 0.

### Opcode note (OCR)

Where the manual's printed HEX code disagrees with its printed OCTAL code, the
OCTAL code was taken as authoritative (the hex fields contain visible OCR
transposition typos, e.g. `0FD7BH` printed for octal `176667B` which is
`0xFDB7`). Opcodes below are given in octal (manual convention `B` suffix) with
the reconciled hex in parentheses.

---

## Instruction-to-source map (all 57 files)

| File(s) | Mnemonic | Manual section | Opcode(s) octal |
|---------|----------|----------------|-----------------|
| Move.c | `t MOVE` | 10.7 | BI 176013, BY 031, H 176024, W 032, F 033, D 054 |
| Bmove.c | `t BMOVE` | 15.1 | BY 176440, H 177170, W 177171, F 177172, D 177173 |
| AssignTo.c | `tn :=` (load) | 10.1 | see 10.1 table |
| AssignFrom.c | `tn =:` (store) | 10.4 | see 10.4 table |
| AssignBaseRegTo.c | `B :=` | 10.2 | 176010 |
| AssignRecordRegTo.c | `R :=` | 10.3 | 030 |
| AssignToBaseReg.c | `B =:` | 10.5 | 176012 |
| AssignToRecordReg.c | `R =:` | 10.6 | 176011 |
| Clr.c | `tn CLR` | 10.16 | int 204+(n-1), F 210+(n-1), D 214+(n-1) |
| Stz.c | `t STZ` | 10.17 | BI 176205, BY 110, H 111, W 112, F 113, D 114 |
| Swap.c | `t SWAP` | 10.8 | BI 176275, BY 176276, H 176277, W 122, F 176334, D 176335 |
| Clrk.c | `CLRK` | 15.11 (manual page 279) | 177003 |
| LSet.c | `L :=` | 16.7 | 176473 |
| LGet.c | `L =:` | 16.8 | 176700 |
| HlSet.c | `HL :=` | 16.7 | 176667 |
| HlGet.c | `HL =:` | 16.8 | 176701 |
| LlSet.c | `LL :=` | 16.7 | 176670 |
| LlGet.c | `LL =:` | 16.8 | 176702 |
| St1Set.c | `ST1 :=` | 16.7 | 176671 |
| St1Get.c | `ST1 =:` | 16.8 | 176703 |
| Ote1Set.c | `OTE1 :=` | 16.7 | 176673 |
| Ote2Set.c | `OTE2 :=` | 16.7 | 176674 |
| Ote1Get.c | `OTE1 =:` | 16.8 | 176705 |
| Ote2Get.c | `OTE2 =:` | 16.8 | 176706 |
| TosSet.c | `TOS :=` | 16.7 | 176675 |
| TosGet.c | `TOS =:` | 16.8 | 176711 |
| ThaSet.c | `THA :=` | 16.7 | 176712 |
| ThaGet.c | `THA =:` | 16.8 | 176713 |
| CadSet.c | `CAD :=` | 16.33 ('87 ext, privileged) | 176672 |
| CadGet.c | `CAD =:` | 16.8 | 177125 |
| CedGet.c | `CED =:` | 16.8 | 177124 |
| Cte1Get.c | `CTE1 =:` | 16.8 | 177120 |
| Cte2Get.c | `CTE2 =:` | 16.8 | 177121 |
| Mte1Get.c | `MTE1 =:` | 16.8 | 176560 |
| Mte2Get.c | `MTE2 =:` | 16.8 | 176561 |
| Temm1Get.c | `TEMM1 =:` | 16.8 | 177122 |
| Temm2Get.c | `TEMM2 =:` | 16.8 | 177123 |
| PGet.c | `P =:` | 16.8 | 176542 |
| PsGet.c | `PS =:` | 16.8 | 177174 |
| PsSet.c | `PS :=` | not described in ND-05.009.4 | 177504 (0xFF44) |
| A1Set.c..A4Set.c | `An :=` (load MS of Dn) | 16.9 | 177060+(n-1) |
| A1Get.c..A4Get.c | `An =:` (store MS of Dn) | 16.9 | 177070+(n-1) |
| E1Set.c..E4Set.c | `En :=` (load LS of Dn) | 16.9 | 177064+(n-1) |
| E1Get.c..E4Get.c | `En =:` (store LS of Dn) | 16.9 | 177074+(n-1) |

---

# 1. MOVE - `Move.c`

- Mnemonic / opcode (octal): `BI MOVE` 176013 (0xFC0B), `BY MOVE` 031 (0x19),
  `H MOVE` 176024 (0xFC14), `W MOVE` 032 (0x1A), `F MOVE` 033 (0x1B),
  `D MOVE` 054 (0x2C). Manual section 10.7.
- Operation: `<source> -> <dest>`. The number of bits of the data type are moved
  from source to destination; source unaffected.
- Operands: `<source/r/t>`, `<dest/w/t>` (2). A constant destination is illegal.

STATUS FLAGS:

| Flag | Effect | Manual (10.7) | Microcode |
|------|--------|---------------|-----------|
| Z | CONDITIONAL: set if `<source>` = 0 | `<source> = 0 -> Z` | `ST,SAVA` at MOVE 000231 (also MOVEBI 000227->000230, MOVED 000233) |
| S | CONDITIONAL: = `<source>` sign bit | `<source>.signbit -> S` | `ST,SAVA` at 000231 |
| C | CLEARED (not mentioned; no integer arithmetic) | unmentioned => reset | `ALU,A` pass-through under `ST,SAVA` => C=0 |
| O | CLEARED (not mentioned; no integer arithmetic) | unmentioned => reset | `ALU,A` under `ST,SAVA` => O=0 |
| K | UNCHANGED (unless a descriptor-addressed operand sets it; see global note) | not mentioned as data-status | no `K,*` micro-op at 000231 |

TRAP conditions: Addressing traps (manual 10.7). Constant destination =>
Illegal Operand Specifier (IOS) per section 6.5.3.2.

---

# 2. BMOVE - Block Move and Fill - `Bmove.c`

- Mnemonic / opcode (octal): `BY BMOVE` 176440 (0xFD20), `H BMOVE` 177170
  (0xFE78), `W BMOVE` 177171 (0xFE79), `F BMOVE` 177172 (0xFE7A),
  `D BMOVE` 177173 (0xFE7B). Manual section 15.1.
- Operation (manual): `0->i; while i<m do source(i)->dest(i); i+1->i enddo`.
  `<m>` elements moved from `<source>` to `<dest>`; operands are pointers to the
  block starts; `<m>` is unsigned; overlap is taken care of. When the source is a
  register or a constant, the destination is FILLED with `<m>` copies of the
  source value. Constants and registers are illegal as destination.
- Operands: `<source/r/t>`, `<dest/w/t>`, `<m/r/w>` (3).

STATUS FLAGS:

| Flag | Effect | Manual (15.1) | Microcode |
|------|--------|---------------|-----------|
| Z | CLEARED | "All cleared" | see note |
| C | CLEARED | "All cleared" | see note |
| S | CLEARED | "All cleared" | see note |
| O | CLEARED | "All cleared" | see note |
| K | UNKNOWN (needs verification) | not stated | loop uses `COND,SAVC1` at BMOVEBY_M 005050/005054; net effect not isolated |

Microcode note: heads at BMOVEBY 001204, BMOVEHW 001212, BMOVEW 001220,
BMOVED 001226; loop/termination path BMOVEBY_M 005050, BMOVEBY_END 005054
(which carries a `ST,SAVA` used for the loop counter). The manual states the
architectural result is "All cleared"; the intermediate micro-level counter
status and exact final K were not fully traced - documented per manual, with the
loop-counter `ST,SAVA` flagged.

TRAP conditions: Addressing traps (manual 15.1). This is a restartable
("During") instruction per manual section 6.5.7 (may be interrupted by page
fault and continued).

---

# 3. LOAD (`tn :=`) - `AssignTo.c`

- Manual section 10.1. Format `tn := <source/r/t>`.
- Opcodes (octal), n = 1..4:

| Data type | Octal | Hex |
|-----------|-------|-----|
| BIn | 176004+(n-1) | 0xFC04+(n-1) |
| BYn | 004+(n-1) | 0x04+(n-1) |
| Hn | 010+(n-1) | 0x08+(n-1) |
| Wn | 014+(n-1) | 0x0C+(n-1) |
| Fn | 020+(n-1) | 0x10+(n-1) |
| Dn | 024+(n-1) | 0x14+(n-1) |

- Operation: `<source> -> Rn`. Value right-justified; for BI/BY/H the rest of
  the integer register is zero-filled; F/D load a floating register.
- Operands: `<source/r/t>` (1).

STATUS FLAGS:

| Flag | Effect | Manual (10.1) | Microcode |
|------|--------|---------------|-----------|
| Z | CONDITIONAL: set if `<source>` = 0 | `<source> = 0 -> Z` | `ST,SAVA` at LOADT 000206 (LOADD 000207; LOADBI 000203 shares LOAD path) |
| S | CONDITIONAL: = `<source>` sign bit | `<source>.signbit -> S` | `ST,SAVA` at 000206 |
| C | CLEARED | unmentioned => reset | `ALU,A` under `ST,SAVA` => C=0 |
| O | CLEARED | unmentioned => reset | `ALU,A` under `ST,SAVA` => O=0 |
| K | UNCHANGED (see global note) | not data-status | no `K,*` at 000206 |

TRAP conditions: Addressing traps.

---

# 4. STORE (`tn =:`) - `AssignFrom.c`

- Manual section 10.4. Format `tn =: <dest/w/t>`.
- Opcodes (octal), n = 1..4:

| Data type | Octal | Hex |
|-----------|-------|-----|
| BIn =: | 176014+(n-1) | 0xFC0C+(n-1) |
| BYn =: | 034+(n-1) | 0x1C+(n-1) |
| Hn =: | 176020+(n-1) | 0xFC10+(n-1) |
| Wn =: | 040+(n-1) | 0x20+(n-1) |
| Fn =: | 044+(n-1) | 0x24+(n-1) |
| Dn =: | 050+(n-1) | 0x28+(n-1) |

- Operation: datatype-dependent (least significant) part of `Rn -> <dest>`.
  Source register unaffected. Constant operands illegal. If destination is a
  register and type is BI/BY/H, its upper part is zero-filled.
- Operands: `<dest/w/t>` (1).

STATUS FLAGS:

| Flag | Effect | Manual (10.4) | Microcode |
|------|--------|---------------|-----------|
| Z | CONDITIONAL: set if datatype-part of register = 0 | `... = 0 -> Z` | `ST,SAVA` at STORE 000220 (STORED 000221) |
| S | CONDITIONAL: = datatype-part sign bit | `....signbit -> S` | `ST,SAVA` at 000220 |
| C | CLEARED | unmentioned => reset | `ALU,A` under `ST,SAVA` => C=0 |
| O | CLEARED | unmentioned => reset | `ALU,A` under `ST,SAVA` => O=0 |
| K | UNCHANGED (see global note) | not data-status | no `K,*` at 000220 |

TRAP conditions: Addressing traps. Constant destination => IOS.

---

# 5. LOAD LOCAL BASE REGISTER (`B :=`) - `AssignBaseRegTo.c`

- Manual section 10.2. Opcode 176010 (0xFC08). Format `B := <source/r/W>`.
- Operation: `<source> -> B`.
- Operands: `<source/r/W>` (1).

STATUS FLAGS:

| Flag | Effect | Manual (10.2) | Microcode |
|------|--------|---------------|-----------|
| Z | CONDITIONAL: set if `<source>` = 0 | `<source> = 0 -> Z` | `ST,SAVA` at LOADB 000213 |
| S | CONDITIONAL: = `<source>` sign bit | `<source>.signbit -> S` | `ST,SAVA` at 000213 |
| C | CLEARED | unmentioned => reset | `ALU,A` under `ST,SAVA` |
| O | CLEARED | unmentioned => reset | `ALU,A` under `ST,SAVA` |
| K | UNCHANGED | not data-status | no `K,*` at 000213 |

TRAP conditions: Addressing traps.

---

# 6. LOAD RECORD REGISTER (`R :=`) - `AssignRecordRegTo.c`

- Manual section 10.3. Opcode 030 (0x18). Format `R := <source/r/W>`.
- Operation: `<source> -> R`.
- Operands: `<source/r/W>` (1).

STATUS FLAGS:

| Flag | Effect | Manual (10.3) | Microcode |
|------|--------|---------------|-----------|
| Z | CONDITIONAL: set if `<source>` = 0 | `<source> = 0 -> Z` | `ST,SAVA` at LOADR 000211 |
| S | CONDITIONAL: = `<source>` sign bit | `<source>.signbit -> S` | `ST,SAVA` at 000211 |
| C | CLEARED | unmentioned => reset | `ALU,A` under `ST,SAVA` |
| O | CLEARED | unmentioned => reset | `ALU,A` under `ST,SAVA` |
| K | UNCHANGED | not data-status | no `K,*` at 000211 |

TRAP conditions: Addressing traps.

---

# 7. STORE LOCAL BASE REGISTER (`B =:`) - `AssignToBaseReg.c`

- Manual section 10.5. Opcode 176012 (0xFC0A). Format `B =: <operand/w/W>`.
- Operation: `B -> <operand>`.
- Operands: `<operand/w/W>` (1).

STATUS FLAGS:

| Flag | Effect | Manual (10.5) | Microcode |
|------|--------|---------------|-----------|
| Z | CONDITIONAL: set if B = 0 | `B register = 0 -> Z` | STOREB head 000225 has no in-line `ST,SAVA`; status set on shared store-completion path (see note) |
| S | CONDITIONAL: = B sign bit | `B register.signbit -> S` | shared completion path |
| C | CLEARED | unmentioned => reset | data transfer, no integer arithmetic |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Microcode note: the STOREB head microword (000225) reads the destination and
jumps; the `ST,SAVA` is not on the head word. The manual is explicit that Z/S
are set from the B value, so that is the authoritative result.

TRAP conditions: Addressing traps.

---

# 8. STORE RECORD REGISTER (`R =:`) - `AssignToRecordReg.c`

- Manual section 10.6. Opcode 176011 (0xFC09). Format `R =: <operand/w/W>`.
- Operation: `R -> <operand>`.
- Operands: `<operand/w/W>` (1).

STATUS FLAGS:

| Flag | Effect | Manual (10.6) | Microcode |
|------|--------|---------------|-----------|
| Z | CONDITIONAL: set if R = 0 | `R register = 0 -> Z` | STORER head 000223 has no in-line `ST,SAVA`; status set on shared store-completion path |
| S | CONDITIONAL: = R sign bit | `R register.signbit -> S` | shared completion path |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

TRAP conditions: Addressing traps.

---

# 9. MOVE - SWAP (`t SWAP`) - `Swap.c`

- Manual section 10.8. Opcodes (octal): `BI SWAP` 176275 (0xFCBD),
  `BY SWAP` 176276 (0xFCBE), `H SWAP` 176277 (0xFCBF), `W SWAP` 122 (0x52),
  `F SWAP` 176334 (0xFCDC), `D SWAP` 176335 (0xFCDD).
- Operation: `<op1> <-> <op2>` (contents exchanged). Same data type assumed.
- Operands: `<op1/rw/t>`, `<op2/rw/t>` (2).

STATUS FLAGS:

| Flag | Effect | Manual (10.8) | Microcode |
|------|--------|---------------|-----------|
| Z | CONDITIONAL: set if ORIGINAL `<op1>` = 0 | `original contents of <op1> = 0 -> Z` | `ST,SAVA` at SWAPF 000534 / SWAPD 000536; SWAPBI via SWAPBI_11 003572 (`ST,SAVA`) |
| S | CONDITIONAL: = ORIGINAL `<op1>` sign bit | `original contents of <op1>.signbit -> S` | `ST,SAVA` on the above |
| C | CLEARED | unmentioned => reset | data transfer, no integer arithmetic |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED (see global note) | not data-status | no `K,*` on the swap status word |

TRAP conditions: Addressing traps.

---

# 10. CLEAR REGISTER (`tn CLR`) - `Clr.c`

- Manual section 10.16. Opcodes (octal), n = 1..4: BIn/BYn/Hn/Wn CLR
  204+(n-1) (0x84+(n-1)); Fn CLR 210+(n-1) (0x88+(n-1)); Dn CLR 214+(n-1)
  (0x8C+(n-1)).
- Operation: `0 -> Rn`. Entire register cleared for all integer types.
- Operands: none (register encoded in opcode).

STATUS FLAGS:

| Flag | Effect | Manual (10.16) | Microcode |
|------|--------|----------------|-----------|
| Z | SET (result is always zero) | `1 -> Z` | `ALU,FZRO ... ST,SAVA` at CLR 000307 (CLRF 000310, CLRD 000311) => Z=1 |
| S | CLEARED (not mentioned; zero is non-negative) | unmentioned => reset | zero result under `ST,SAVA` => S=0 |
| C | CLEARED | unmentioned => reset | `ST,SAVA`, zero result => C=0 |
| O | CLEARED | unmentioned => reset | `ST,SAVA`, zero result => O=0 |
| K | UNCHANGED | not data-status | no `K,*` at 000307 |

TRAP conditions: None (manual 10.16).

---

# 11. STORE ZERO (`t STZ`) - `Stz.c`

- Manual section 10.17. Opcodes (octal): `BI STZ` 176205 (0xFC85), `BY STZ`
  110 (0x48), `H STZ` 111 (0x49), `W STZ` 112 (0x4A), `F STZ` 113 (0x4B),
  `D STZ` 114 (0x4C).
- Operation: `0 -> <operand>`.
- Operands: `<operand/w/t>` (1).

STATUS FLAGS:

| Flag | Effect | Manual (10.17) | Microcode |
|------|--------|----------------|-----------|
| Z | SET (result always zero) | `1 -> Z` | `ALU,FZRO ... ST,SAVA` at STZ 000316 (STZBI 000313->000315, STZD 000317) => Z=1 |
| S | CLEARED | unmentioned => reset | zero result under `ST,SAVA` => S=0 |
| C | CLEARED | unmentioned => reset | `ST,SAVA`, zero => C=0 |
| O | CLEARED | unmentioned => reset | `ST,SAVA`, zero => O=0 |
| K | UNCHANGED | not data-status | no `K,*` at 000316 |

TRAP conditions: Addressing traps (manual 10.17).

---

# 12. CLRK - Clear flag - `Clrk.c`

- Manual section 15.11 (manual page 279). Opcode 177003 (0xFE03). Format `CLRK`.
- Operation: `0 -> K bit of status register`. Clears the flag (K) bit only.
- Operands: none.

STATUS FLAGS:

| Flag | Effect | Manual (15.11) | Microcode |
|------|--------|----------------|-----------|
| K | CLEARED | `0 -> K` | `K,ZRO` at CLRK 000775 |
| Z | UNCHANGED | "Data status bits: Unaffected" | no `ST,SAVA` at 000775 |
| C | UNCHANGED | "Unaffected" | no `ST,SAVA` |
| S | UNCHANGED | "Unaffected" | no `ST,SAVA` |
| O | UNCHANGED | "Unaffected" | no `ST,SAVA` |

TRAP conditions: None (manual 15.11).

---

# Special-register LOAD instructions (manual section 16.7)

Manual 16.7 "Load special register", format `special register := <operand/r/W>`.
Shared manual statements for the whole group:

- Operation: `<operand> -> special register`.
- Operands: `<operand/r/W>` (1).
- Data status bits (16.7): `<operand> = 0 -> Z`, `<operand>.signbit -> S`.
  (So C and O are CLEARED; K UNCHANGED.)
- Trap conditions (16.7): Addressing traps, Illegal Operand Value (IOV).
  IOV applies specifically to the OTE loads (a non-modifiable OTE bit, checked
  against the TEMM mask, causes an IOV trap condition).

Per-instruction:

## 13. `L :=` - `LSet.c`
Opcode 176473 (0xFD3B). Loads the link (L) register.

| Flag | Effect | Manual 16.7 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: operand = 0 | yes | `ST,SAVA` at LOAL 001002 |
| S | CONDITIONAL: operand sign bit | yes | `ST,SAVA` at 001002 |
| C | CLEARED | unmentioned => reset | `ALU,A` under `ST,SAVA` |
| O | CLEARED | unmentioned => reset | `ALU,A` under `ST,SAVA` |
| K | UNCHANGED | not data-status | no `K,*` |

Traps: Addressing traps.

## 14. `HL :=` - `HlSet.c`
Opcode 176667 (0xFDB7). (Manual hex `0FD7BH` is an OCR typo; octal 176667 = 0xFDB7.)
Loads the high (upper) limit register.

| Flag | Effect | Manual 16.7 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: operand = 0 | yes | `ST,SAVA` at LOAHL 001004 |
| S | CONDITIONAL: operand sign bit | yes | `ST,SAVA` at 001004 |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps.

## 15. `LL :=` - `LlSet.c`
Opcode 176670 (0xFDB8). (Manual hex `0FD8BH` is an OCR typo.)
Loads the low (lower) limit register.

| Flag | Effect | Manual 16.7 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: operand = 0 | yes | `ST,SAVA` at LOALL 001007 |
| S | CONDITIONAL: operand sign bit | yes | `ST,SAVA` at 001007 |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps.

## 16. `ST1 :=` - `St1Set.c`
Opcode 176671 (0xFDB9). (Manual hex `0FD9BH` is an OCR typo.)
Loads the 1st status register (data status bits come FROM the operand).

Manual 16.7 special note: "The instruction ST1:= will load the data status bits
from the operand. Setting status bits that are modified after each instruction
is legal but meaningless, as they will be cleared before the next instruction
(bits 17-25, 27, 28)."

| Flag | Effect | Manual 16.7 | Microcode |
|------|--------|-------------|-----------|
| Z | = operand bit 5 (loaded directly), then subject to the general 16.7 line `operand=0 -> Z` - see note | ambiguous between the two manual statements | LOAST1 001012 has NO `ST,SAVA`; it writes the operand into the status register (`ALU,STS` at 001013 -> LOAD_ST1) |
| S | = operand bit 7 (loaded directly) | as above | LOAST1 001012 loads status directly |
| C | = operand bit 6 (loaded directly) | loaded from operand | direct status load |
| O | = operand bit 9 (loaded directly) | loaded from operand | direct status load |
| K | = operand bit 8 (loaded directly) | loaded from operand | direct status load |

Note (needs verification): ST1:= is the one member of the group that does NOT
recompute status via `ST,SAVA`; the microcode loads the operand straight into
the status register, so the resulting Z/S/C/O/K equal the corresponding bits of
the operand, not a zero/sign test of the value. The generic 16.7 line
(`operand=0 -> Z`, `operand.signbit -> S`) conflicts with this special behavior;
resolve by tracing microword LOAD_ST1. UNKNOWN whether any post-load zero/sign
recompute overrides the loaded bits.

Traps: Addressing traps, IOV (per group).

## 17. `OTE1 :=` - `Ote1Set.c`
Opcode 176673 (0xFDBB). Loads the 1st own trap enable register (masked by TEMM).

| Flag | Effect | Manual 16.7 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: operand = 0 | yes | `ST,SAVA` at LOATE1 001014 |
| S | CONDITIONAL: operand sign bit | yes | `ST,SAVA` at 001014 |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOV (attempt to modify a non-modifiable OTE bit).

## 18. `OTE2 :=` - `Ote2Set.c`
Opcode 176674 (0xFDBC). Loads the 2nd own trap enable register.

| Flag | Effect | Manual 16.7 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: operand = 0 | yes | `ST,SAVA` at LOATE2 001016 |
| S | CONDITIONAL: operand sign bit | yes | `ST,SAVA` at 001016 |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOV.

## 19. `TOS :=` - `TosSet.c`
Opcode 176675 (0xFDBD). Loads the top-of-stack register.

| Flag | Effect | Manual 16.7 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: operand = 0 | yes | `ST,SAVA` at LOATOS 001020 |
| S | CONDITIONAL: operand sign bit | yes | `ST,SAVA` at 001020 |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps.

## 20. `THA :=` - `ThaSet.c`
Opcode 176712 (0xFDCA). Loads the trap handler address register.

| Flag | Effect | Manual 16.7 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: operand = 0 | yes | `ST,SAVA` at LOATHA 001022 |
| S | CONDITIONAL: operand sign bit | yes | `ST,SAVA` at 001022 |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps.

---

# 21. `CAD :=` - Load CAD - `CadSet.c` (manual section 16.33, '87 extension)

- Opcode 176672 (0xFDBA). Format `CAD := <operand/r/W>`. PRIVILEGED instruction.
- Operation: `<operand> -> CAD` (current alternative domain register).
- Operands: `<operand/r/W>` (1).

STATUS FLAGS:

| Flag | Effect | Manual 16.33 | Microcode |
|------|--------|--------------|-----------|
| Z | CONDITIONAL: operand = 0 | `Operand = 0 -> Z` | LOACAD 001024 - head word has no in-line `ST,SAVA` (transfers into memory-management domain); Z/S per manual |
| S | CONDITIONAL: operand sign bit | `<Operand>.signbit -> S` | see note |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

TRAP conditions: Illegal Instruction Code (IIC) if executed with PIA reset
(privileged - manual 6.5.3.2); plus addressing traps on the operand.

---

# Special-register STORE instructions (manual section 16.8)

Manual 16.8 "Store special register", format `special register =: <operand/w/W>`.
Shared manual statements for the whole group:

- Operation: `special register -> <operand>`.
- Operands: `<operand/w/W>` (1).
- Data status bits (16.8): `special register = 0 -> Z`,
  `special register.signbit -> S`. EXCEPTION stated verbatim: "The instruction
  ST1=: does not affect the data status bits." (So for all others C and O are
  CLEARED, K UNCHANGED.)
- Trap conditions (16.8): Addressing traps, Illegal Operand Specifier (IOS).
- `P =:` special rule: the stored value is the address of the `P=:` instruction
  itself.

Microcode note for this group: several store heads begin with `ALU,XOR`/`ALU,FZRO`
(clearing) and jump to a shared completion path (`READ_RFEND`, `STOR_xxx`); the
`ST,SAVA` that produces Z/S is on that shared path for most members. Members
whose 2nd microword carries an in-line `ST,SAVA` are noted explicitly
(P=:, CED=:, CAD=:). The manual is authoritative for the Z/S result.

## 22. `L =:` - `LGet.c`
Opcode 176700 (0xFDC0). Stores the link register.

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: L = 0 | yes | `ST,SAVA` at STORL 001077->001100 (in-line) |
| S | CONDITIONAL: L sign bit | yes | `ST,SAVA` at 001100 |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOS.

## 23. `HL =:` - `HlGet.c`
Opcode 176701 (0xFDC1). Stores the high limit register.

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: HL = 0 | yes | STORHL 001101 head clears; Z/S on shared STOR_HL path |
| S | CONDITIONAL: HL sign bit | yes | shared path |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOS.

## 24. `LL =:` - `LlGet.c`
Opcode 176702 (0xFDC2). Stores the low limit register.

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: LL = 0 | yes | STORLL 001104 head clears; Z/S on shared STOR_LL path |
| S | CONDITIONAL: LL sign bit | yes | shared path |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOS.

## 25. `ST1 =:` - `St1Get.c`
Opcode 176703 (0xFDC3). Stores the 1st status register.

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | UNCHANGED | "ST1=: does not affect the data status bits" | STORST1 001107 has NO `ST,SAVA` |
| C | UNCHANGED | as above | no `ST,SAVA` |
| S | UNCHANGED | as above | no `ST,SAVA` |
| O | UNCHANGED | as above | no `ST,SAVA` |
| K | UNCHANGED | as above | no `K,*` |

Traps: Addressing traps, IOS.

## 26. `OTE1 =:` - `Ote1Get.c`
Opcode 176705 (0xFDC5). Stores the 1st own trap enable register.

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: OTE1 = 0 | yes | STORTE1 001123 head clears; shared STOR_OTE1 path |
| S | CONDITIONAL: OTE1 sign bit | yes | shared path |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOS.

## 27. `OTE2 =:` - `Ote2Get.c`
Opcode 176706 (0xFDC6). Stores the 2nd own trap enable register.

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: OTE2 = 0 | yes | STORTE2 001125 head clears; shared STOR_OTE2 path |
| S | CONDITIONAL: OTE2 sign bit | yes | shared path |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOS.

## 28. `CED =:` - `CedGet.c`
Opcode 177124 (0xFE54). Stores the current executing domain register.
(Manual name: "current executing domain"; the C header's "Current Environment
Descriptor" is a misnomer - manual section 4.2.3.1 defines CED = Current
Executing Domain.)

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: CED = 0 | yes | `ST,SAVA` at STORCED 001142->001143 (in-line) |
| S | CONDITIONAL: CED sign bit | yes | `ST,SAVA` at 001143 |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOS.

## 29. `CAD =:` - `CadGet.c`
Opcode 177125 (0xFE55). Stores the current alternative domain register.

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: CAD = 0 | yes | `ST,SAVA` at STORCAD 001144->001145 (in-line) |
| S | CONDITIONAL: CAD sign bit | yes | `ST,SAVA` at 001145 |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOS.

## 30. `CTE1 =:` - `Cte1Get.c`
Opcode 177120 (0xFE50). Stores the 1st child trap enable register.

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: CTE1 = 0 | yes | STORCTE1 001113 head clears; shared STOR_CTE1 path |
| S | CONDITIONAL: CTE1 sign bit | yes | shared path |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOS.

## 31. `CTE2 =:` - `Cte2Get.c`
Opcode 177121 (0xFE51). Stores the 2nd child trap enable register.

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: CTE2 = 0 | yes | STORCTE2 001115 head clears; shared STOR_CTE2 path |
| S | CONDITIONAL: CTE2 sign bit | yes | shared path |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOS.

## 32. `MTE1 =:` - `Mte1Get.c`
Opcode 176560 (0xFD70). Stores the 1st mother trap enable register.
(Manual 16.8 prints hex `0FD71H` for both MTE1 and MTE2 - an OCR duplication;
octal 176560 = 0xFD70 for MTE1.)

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: MTE1 = 0 | yes | STORMTE1 001127 head clears; shared STOR_MTE1 path |
| S | CONDITIONAL: MTE1 sign bit | yes | shared path |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOS.

## 33. `MTE2 =:` - `Mte2Get.c`
Opcode 176561 (0xFD71). Stores the 2nd mother trap enable register.

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: MTE2 = 0 | yes | STORMTE2 001131 head clears; shared STOR_MTE2 path |
| S | CONDITIONAL: MTE2 sign bit | yes | shared path |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOS.

## 34. `TEMM1 =:` - `Temm1Get.c`
Opcode 177122 (0xFE52). Stores the 1st trap enable modification mask.

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: TEMM1 = 0 | yes | STORTEM1 001117 head clears; shared STOR_TEM1 path |
| S | CONDITIONAL: TEMM1 sign bit | yes | shared path |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOS.

## 35. `TEMM2 =:` - `Temm2Get.c`
Opcode 177123 (0xFE53). Stores the 2nd trap enable modification mask.

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: TEMM2 = 0 | yes | STORTEM2 001121 head clears; shared STOR_TEM2 path |
| S | CONDITIONAL: TEMM2 sign bit | yes | shared path |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOS.

## 36. `P =:` - `PGet.c`
Opcode 176542 (0xFD62). Stores the program counter (value = address of the
`P=:` instruction, per manual 16.8).

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: P = 0 | yes | `ST,SAVA` at STORP 001137->001141 (in-line) |
| S | CONDITIONAL: P sign bit | yes | `ST,SAVA` at 001141 |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps, IOS.

## 37. `PS =:` - `PsGet.c`
Opcode 177174 (0xFE7C). Stores the process segment / process status register.
(Manual 16.8 octal is printed truncated as `17714B`; the full code is 177174B =
0xFE7C.)

| Flag | Effect | Manual 16.8 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: PS = 0 | yes (group rule) | STORPS 001152->001153 head has no in-line `ST,SAVA` (uses SRF13 path); Z/S per manual |
| S | CONDITIONAL: PS sign bit | yes (group rule) | see note |
| C | CLEARED | unmentioned => reset | data transfer |
| O | CLEARED | unmentioned => reset | data transfer |
| K | UNCHANGED | not data-status | -- |

Note: microword 001153 does not carry a visible `ST,SAVA`; the manual group
rule (Z/S set) is used as authoritative. If exact hardware Z/S matters, trace
READ_RFEND from 001153.

Traps: Addressing traps, IOS.

---

# 38. `PS :=` - Load process segment/status register - `PsSet.c`

- Opcode 177504 (0xFF44). This mnemonic (`PS :=`) is NOT given a dedicated
  instruction description in ND-05.009.4; 177504B appears in the appendix G
  code table (manual line ~14428) in a `RES` (reserved) row, and the appendix H
  cross-reference lists `PS :=` (page 282) without an accompanying operational
  section in this manual edition.
- No `LOAPS` label was found in the microcode file
  `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`.

STATUS FLAGS:

| Flag | Effect |
|------|--------|
| Z | UNKNOWN (needs verification) - not documented in ND-05.009.4; no microcode label found |
| C | UNKNOWN (needs verification) |
| S | UNKNOWN (needs verification) |
| O | UNKNOWN (needs verification) |
| K | UNKNOWN (needs verification) |

TRAP conditions: UNKNOWN (needs verification). If PS load is privileged (as its
store counterpart and CAD load are), an IIC trap would arise with PIA reset -
INFERRED, not confirmed.

---

# Integer / float register communication (manual section 16.9)

Manual 16.9 "Integer float register communication". These transfer the most-
significant (An) or least-significant (En) 32-bit half of the double-float
register Dn to/from a word operand, WITHOUT type conversion. "a float register
is equivalent to the most significant part of a double float register." When a
register is named as the operand, the general integer registers are used.

Shared manual statements:
- Operands: 1 (`<operand/w/W>` for `=:` store, `<operand/r/W>` for `:=` load).
- Data status bits (16.9): `source register = 0 -> Z`,
  `source register.signbit -> S`. (=> C, O CLEARED; K UNCHANGED.)
- Trap conditions (16.9): Addressing traps.

Microcode: the whole family (LOADA1..LOADE4 001160-001167, STOREA1..STOREE4
001170-001177) carries `ST,SAVA` with `ALU,A` on every microword, confirming
Z/S from the transferred 32-bit value and C = O = 0.

## 39-42. `An :=` (load MS half of Dn from operand) - `A1Set.c`..`A4Set.c`
Opcodes 177060+(n-1) octal (0xFE30+(n-1)): A1:=177060, A2:=177061, A3:=177062,
A4:=177063.

| Flag | Effect | Manual 16.9 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: source (operand) = 0 | yes | `ST,SAVA` at LOADA1 001160 .. LOADA4 001163 |
| S | CONDITIONAL: source sign bit | yes | `ST,SAVA`, `ALU,A` |
| C | CLEARED | unmentioned => reset | `ALU,A` => C=0 |
| O | CLEARED | unmentioned => reset | `ALU,A` => O=0 |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps.

## 43-46. `An =:` (store MS half of Dn to operand) - `A1Get.c`..`A4Get.c`
Opcodes 177070+(n-1) octal (0xFE38+(n-1)): A1=:177070, A2=:177071, A3=:177072,
A4=:177073.

| Flag | Effect | Manual 16.9 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: source register An = 0 | yes | `ST,SAVA` at STOREA1 001170 .. STOREA4 001173 |
| S | CONDITIONAL: An sign bit | yes | `ST,SAVA`, `ALU,A` |
| C | CLEARED | unmentioned => reset | `ALU,A` => C=0 |
| O | CLEARED | unmentioned => reset | `ALU,A` => O=0 |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps.

## 47-50. `En :=` (load LS half of Dn from operand) - `E1Set.c`..`E4Set.c`
Opcodes 177064+(n-1) octal (0xFE34+(n-1)): E1:=177064, E2:=177065, E3:=177066,
E4:=177067.

| Flag | Effect | Manual 16.9 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: source (operand) = 0 | yes | `ST,SAVA` at LOADE1 001164 .. LOADE4 001167 |
| S | CONDITIONAL: source sign bit | yes | `ST,SAVA`, `ALU,A` |
| C | CLEARED | unmentioned => reset | `ALU,A` => C=0 |
| O | CLEARED | unmentioned => reset | `ALU,A` => O=0 |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps.

## 51-54. `En =:` (store LS half of Dn to operand) - `E1Get.c`..`E4Get.c`
Opcodes 177074+(n-1) octal (0xFE3C+(n-1)): E1=:177074, E2=:177075, E3=:177076,
E4=:177077.

| Flag | Effect | Manual 16.9 | Microcode |
|------|--------|-------------|-----------|
| Z | CONDITIONAL: source register En = 0 | yes | `ST,SAVA` at STOREE1 001174 .. STOREE4 001177 |
| S | CONDITIONAL: En sign bit | yes | `ST,SAVA`, `ALU,A` |
| C | CLEARED | unmentioned => reset | `ALU,A` => C=0 |
| O | CLEARED | unmentioned => reset | `ALU,A` => O=0 |
| K | UNCHANGED | not data-status | -- |

Traps: Addressing traps.

---

# Summary of unresolved / needs-verification items

1. `PS :=` (`PsSet.c`, opcode 177504B / 0xFF44): no operational description in
   ND-05.009.4 and no microcode label found; all flag effects and traps UNKNOWN.
2. `ST1 :=` (`St1Set.c`): microcode LOAST1 (001012) loads the operand directly
   into the status register with NO `ST,SAVA`, so the resulting Z/S/C/O/K equal
   the operand's bits 5/7/6/9/8 rather than a zero/sign test. This conflicts
   with the generic 16.7 line; final behavior needs a LOAD_ST1 microtrace.
3. Store-group members `HL=:, LL=:, OTE1=:, OTE2=:, CTE1=:, CTE2=:, MTE1=:,
   MTE2=:, TEMM1=:, TEMM2=:, TOS=:, THA=:, PS=:`: the head microword clears
   status and the `ST,SAVA` (Z/S) sits on a shared completion path not isolated
   in this pass. Documented Z/S per the explicit manual 16.8 group rule.
4. `B =:` / `R =:` (STOREB 000225 / STORER 000223): same shared-completion-path
   situation; Z/S taken from manual 10.5/10.6.
5. `CAD :=` (LOACAD 001024): head word has no in-line `ST,SAVA`; Z/S taken from
   the explicit manual 16.33 statement.
6. `BMOVE`: manual states "All cleared"; the microcode loop carries an
   `ST,SAVA` for the element counter and `COND,SAVC1` for K - the exact
   micro-level K during fill was not fully traced. Architectural result per
   manual = all data status bits cleared.
7. Manual HEX opcode fields for `HL:=`, `LL:=`, `ST1:=` (`0FD7BH/0FD8BH/0FD9BH`)
   and both `MTE=:` (`0FD71H`) are OCR typos; the OCTAL codes
   (176667/176670/176671, 176560/176561) are authoritative and were used.
