# ND-500 Instruction Category: SHIFT - Authoritative Behavior Reference

Category enumerated from the emulator source directory
`/home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/*.c`:

| File | Instruction |
|------|-------------|
| `Shl.c`     | SHL     (logical shift) |
| `Sha.c`     | SHA     (arithmetical shift) |
| `Shr.c`     | SHR     (rotational shift) |
| `Pshift.c`  | PSHIFT  (packed shift) |
| `Pshiftr.c` | PSHIFTR (packed shift ROUNDED) |

This document states ONLY what is read directly from the two authoritative
sources listed below. Anything not confirmed by either source is marked
`UNKNOWN (needs verification)`. Emulator C source is NOT treated as authority;
where the emulator source disagrees with the sources it is noted as a FINDING.

## Sources

- PRIMARY spec (the TRUTH):
  `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
  - SHL   section 10.24 (page 160), file lines 5107-5137
  - SHA   section 10.25 (page 161), file lines 5149-5187
  - SHR   section 10.26 (page 162), file lines 5196-5228
  - PSHIFT / PSHIFTR section 17.6 (page 346), file lines 12027-12061
  - Status-bit summary table rows: file lines 15010 (PSHIFT), 15065 (SHA),
    15066 (SHL), 15067 (SHR)
- GROUND-TRUTH flag micro-behavior (ND-5000 microcode):
  `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
- Micro-op field decode:
  `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`

## Global rules that govern every instruction below

These are quoted from the PRIMARY manual and apply to every entry:

1. Data status bits (manual section 6.5.1, file lines 2004-2028). The DATA
   status bits are exactly: Z (bit 5), C carry (bit 6), S sign (bit 7),
   O overflow (bit 9), IVO (11), DZ (12), FU (13), FO (14), BO (15).
   K is NOT a data status bit; it is a separate FLAG (manual line 2244).
2. "All data status bits not mentioned are reset." (manual line 2022) and
   "Data status bits not mentioned in the instruction description are always
   cleared after the instruction has been executed. If the status bit is
   conditionally set a TRUE condition causes the bit to be set (1), a FALSE
   condition causes it to be reset (0)." (manual line 4040).
3. C carry: "may be set only when performing integer arithmetic; otherwise it
   is cleared." (manual line 2040).
4. O overflow: "Integer Overflow may be set only when performing integer
   arithmetic; otherwise it is cleared." (manual line 2043).
5. K flag: "descriptor addressing may set but never clear the K flag."
   (manual line 2244).

## Micro-op decode key (from mnemonics.md)

| Micro-op | Meaning | mnemonics.md line |
|----------|---------|-------------------|
| `ST,SAVA` | SAVE STATUS FROM ALU OPERATION (updates data status bits Z / C / S / O from the ALU result) | 656 |
| `ST,SAVB` | SAVE STATUS FROM BCD OPERATION | 659 |
| `K,ONE`   | set K = 1 | 653 |
| `K,ZRO`   | clear K = 0 | 654 |
| `K,1IFZ`  | set K = 1 if ALU operation result is 0 | 655 |
| `Q,Q*LOG` | Q <- Q*2, Q(00) <- 0  (logical shift LEFT) | 117 |
| `Q,Q/LOG` | Q <- Q/2, Q(sign) <- 0  (logical shift RIGHT) | 119 |
| `Q,Q/ARI` | Q <- Q/2, Q(sign) <- Q(sign)  (arithmetic shift RIGHT) | 118 |
| `Q,Q/ROT` | Q <- Q/2, Q(sign) <- Q(00)  (rotate RIGHT) | 120 |
| `Q,Q*ROT` | Q <- Q*2, Q(00) <- Q(sign)  (rotate LEFT) | 121 |
| `COND,SAVC1` | test: top bit of saved condition stack (used to branch positive- vs negative-count path) | 782 |
| `COND,LCZ`   | test: loop counter zero | 784 |
| `COND,MSORZ` | test: OR of S and Z | 767 |

NOTE on the SAVA-updated set: mnemonics.md line 656 states `ST,SAVA` = "SAVE
STATUS FROM ALU OPERATION". The task decode key specifies this updates
Z / C / O / S. The exact bit set written by SAVA is not spelled out
field-by-field in mnemonics.md; it is inferred to be the four ALU data status
bits Z, C, S, O. Marked inferred where relied upon.

---

# SHL - Logical shift

- Manual section 10.24 (file lines 5107-5137).
- Opcodes (octal / hex, manual lines 5116-5118):
  - `BY SHL` byte shift logically:     176250B (0FCA8H)
  - `H SHL`  halfword shift logically: 176251B (0FCA9H)
  - `W SHL`  word shift logically:     176252B (0FCAAH)
- Operation (manual line 5120): `logically shifted <operand> -> <operand>`.
- Operands (manual line 5110): `t SHL <operand/rw/t>,<shiftcount/r/BY>`
  - operand: read/write, data type t (BY, H, or W).
  - shiftcount: read, data type BY (a SIGNED byte).
- Direction (manual line 5124): "Positive <shiftcount> implies left shift,
  negative <shiftcount> implies right shift." Zeros are shifted in; bits
  shifted out are lost. A shiftcount of zero is legal and leaves the operand
  unchanged.

Microcode (MICRO-5800-A30.md):
- Entry `SHL_C_1` at octal `003260` (file line 1726); count-setup at
  `003261`-`003263`.
- Positive-count shift loop: `SHL_PSC_1` `003273` / `SHL_PSC_2` `003276`
  (file lines 1737-1740). `003276` uses `Q,Q*LOG` = logical shift LEFT
  (confirms manual: positive = left).
- Negative-count shift loop: `SHL_NSC_1` `003267` / `SHL_NSC_2` `003272`
  (file lines 1733-1736). `003272` uses `Q,Q/LOG` = logical shift RIGHT
  (confirms manual: negative = right).
- Result write micro-ops `003271` and `003275` carry `ST,SAVA` (save ALU
  data status) and `WRITE`.
- Over-range count path: `003264`/`003265` -> `SET_IOV` (`003132`, file line
  1640), which ORs the illegal-operand-value trap bit into status.
- There is NO K micro-op (`K,ONE`/`K,ZRO`/`K,1IFZ`) anywhere in the SHL loop.

## SHL status flags

| Flag | Effect | Source (manual + microcode) |
|------|--------|-----------------------------|
| K (flag)     | UNCHANGED by the shift itself (may be SET, never cleared, if the operand is reached via descriptor addressing) | Manual line 2244 (K not a data status bit; descriptor addressing may set never clear). Microcode SHL loop 003260-003276 contains no K micro-op. |
| Z (zero)     | CONDITIONAL: SET if shifted operand == 0, else CLEARED | Manual line 5130 ("shifted operand = 0 -> Z"). Microcode `ST,SAVA` at 003271/003275. |
| S (sign)     | CONDITIONAL: set to the sign bit of the shifted operand | Manual line 5131 ("shifted operand.signbit -> S"). Microcode `ST,SAVA` at 003271/003275. |
| C (carry)    | CLEARED | Manual: not mentioned in 10.24 -> reset (lines 2022, 4040); carry only set by integer arithmetic (line 2040). Microcode final ALU op feeding SAVA is `ALU,A` (pass-through), not an arithmetic add/sub, so no carry generated. |
| O (overflow) | CLEARED | Manual: not mentioned in 10.24 -> reset (lines 2022, 4040); overflow only set by integer arithmetic (line 2043). Microcode: pass-through ALU, no arithmetic overflow. |

Other data status bits (IVO, DZ, FU, FO, BO): not mentioned -> CLEARED
(manual lines 2022, 4040).

## SHL trap conditions

- Addressing traps (manual line 5126).
- Illegal operand value (IOV) (manual line 5126): "A shiftcount equal to or
  greater than the size of the operand will produce an illegal operand value
  trap condition." (manual line 5124). Microcode: over-range path
  003264/003265 -> `SET_IOV` 003132.

FINDING (emulator vs source): `/home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/Shl.c`
matches the manual (positive = left, negative = right, IOV when
abs(count) >= width). It updates only Z and S (`nd500_set_flags_zs`); it does
not explicitly clear C/O/K, so those depend on prior state in the emulator -
the manual requires C and O CLEARED.

---

# SHA - Arithmetical shift

- Manual section 10.25 (file lines 5149-5188).
- Opcodes (octal / hex, manual lines 5161-5163):
  - `BY SHA`: 176253B (0FCABH)
  - `H SHA`:  176254B (0FCACH)
  - `W SHA`:  176255B (0FCADH)
- Operation (manual line 5166): `arithmetically shifted <operand> -> <operand>`.
- Operands (manual line 5154): `t SHA <operand/rw/r>,<shiftcount/r/BY>`
  - operand: read/write; shiftcount: read, SIGNED byte.
- Direction (manual line 5170): "Positive <shiftcount> implies left shift,
  negative <shiftcount> implies right shift." A shiftcount of zero leaves the
  operand unchanged. Arithmetic right shift replicates (sign-extends) the sign
  bit; arithmetic left shift is identical to logical left.

Microcode (MICRO-5800-A30.md):
- Entry `SHA_C_1` at octal `003277` (file line 1741); count setup 003300-003302.
- Positive-count loop: `SHA_PSC_1` `003312` / `SHA_PSC_2` `003315`
  (file lines 1752-1755). `003315` uses `Q,Q*LOG` = shift LEFT (arithmetic
  left == logical left; confirms manual positive = left).
- Negative-count loop: `SHA_NSC_1` `003306` / `SHA_NSC_2` `003311`
  (file lines 1748-1751). `003311` uses `Q,Q/ARI` = ARITHMETIC shift RIGHT
  (sign preserved; confirms manual negative = arithmetic right).
- Result write micro-ops `003310`/`003314` carry `ST,SAVA` and `WRITE`.
- Over-range path `003303`/`003304` -> `SET_IOV` `003132`.
- No K micro-op anywhere in the SHA loop.

## SHA status flags

| Flag | Effect | Source (manual + microcode) |
|------|--------|-----------------------------|
| K (flag)     | UNCHANGED by the shift itself (descriptor addressing may set, never clear) | Manual line 2244. Microcode SHA loop 003277-003315 contains no K micro-op. |
| Z (zero)     | CONDITIONAL: SET if shifted operand == 0, else CLEARED | Manual line 5178 ("shifted operand = 0 -> Z"). Microcode `ST,SAVA` at 003310/003314. |
| S (sign)     | CONDITIONAL: set to sign bit of shifted operand | Manual line 5179 ("shifted operand.signbit -> S"). Microcode `ST,SAVA` at 003310/003314. |
| C (carry)    | CLEARED | Manual: not mentioned -> reset (lines 2022, 4040); carry only from integer arithmetic (line 2040). Microcode: pass-through ALU feeding SAVA. |
| O (overflow) | CLEARED | Manual: not mentioned -> reset (lines 2022, 4040); overflow only from integer arithmetic (line 2043). |

Other data status bits (IVO, DZ, FU, FO, BO): CLEARED (manual lines 2022, 4040).

## SHA trap conditions

- Addressing traps (manual line 5173).
- Illegal operand value (IOV) when abs(shiftcount) >= operand size
  (manual lines 5170, 5173). Microcode over-range path 003303/003304 ->
  `SET_IOV` 003132.

Note: the emulator's `Sha.c` raises `TRAP_IOV` (not `TRAP_IVO`) for the
over-range case, which is consistent with the manual's "Illegal operand
value (IOV)" trap-condition name.

---

# SHR - Rotational shift

- Manual section 10.26 (file lines 5196-5228).
- Opcodes (octal / hex, manual lines 5205-5207):
  - `BY SHR` byte shift rotationally:     176256B (0FCAEH)
  - `H SHR`  halfword shift rotationally: 176257B (0FCAFH)
  - `W SHR`  word shift rotationally:      176260B (0FCB0H) [manual prints
    "OFCBOH"; the digit is zero -> 0FCB0H]
- Operation (manual line 5210): `rotationally shifted <operand> -> <operand>`.
- Operands (manual line 5199): `t SHR <operand/rw/t>,<shiftcount/r/BY>`.
- Direction (manual line 5214): "Positive <shiftcount> implies left shift,
  negative <shiftcount> implies right shift." Rotate: bits shifted out on one
  end wrap to the other; no bits lost. Zero count leaves operand unchanged.

Microcode (MICRO-5800-A30.md):
- Entry `SHR_C_1` at octal `003316` (file line 1756); count setup 003317-003321.
- Positive-count loop: `SHR_PSC_1` `003331` / `SHR_PSC_2` `003334`
  (file lines 1767-1770). `003334` uses `Q,Q*ROT` = rotate LEFT
  (confirms manual: positive = left).
- Negative-count loop: `SHR_NSC_1` `003325` / `SHR_NSC_2` `003330`
  (file lines 1763-1766). `003330` uses `Q,Q/ROT` = rotate RIGHT
  (confirms manual: negative = right).
- Result write micro-ops `003327`/`003333` carry `ST,SAVA` and `WRITE`.
- Over-range path `003322`/`003323` -> `SET_IOV` `003132`.
- No K micro-op anywhere in the SHR loop.

## SHR status flags

| Flag | Effect | Source (manual + microcode) |
|------|--------|-----------------------------|
| K (flag)     | UNCHANGED by the shift itself (descriptor addressing may set, never clear) | Manual line 2244. Microcode SHR loop 003316-003334 contains no K micro-op. |
| Z (zero)     | CONDITIONAL: SET if shifted operand == 0, else CLEARED | Manual line 5221 ("shifted operand = 0 -> Z"). Microcode `ST,SAVA` at 003327/003333. |
| S (sign)     | CONDITIONAL: set to sign bit of shifted operand | Manual line 5222 ("shifted operand.signbit -> S"). Microcode `ST,SAVA` at 003327/003333. |
| C (carry)    | CLEARED | Manual: not mentioned -> reset (lines 2022, 4040); carry only from integer arithmetic (line 2040). |
| O (overflow) | CLEARED | Manual: not mentioned -> reset (lines 2022, 4040); overflow only from integer arithmetic (line 2043). |

Other data status bits (IVO, DZ, FU, FO, BO): CLEARED (manual lines 2022, 4040).

## SHR trap conditions

- Addressing traps (manual line 5217).
- Illegal operand value (IOV) when abs(shiftcount) >= operand size
  (manual lines 5214, 5217). Microcode over-range path 003322/003323 ->
  `SET_IOV` 003132.

FINDING (emulator vs source): the emulator file
`/home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/Shr.c` implements
positive count = rotate RIGHT, negative = rotate LEFT, and its header claims
this was "empirically verified against nd500-as". This CONTRADICTS BOTH
authoritative sources: the manual (line 5214, "positive implies left") and the
microcode (`SHR_PSC_2` 003334 uses `Q,Q*ROT` = rotate LEFT for the
positive-count path). Per both sources: positive SHR = rotate LEFT. This
disagreement should be re-examined against real hardware/nd500-as before
trusting either.

---

# PSHIFT - Packed (BCD) shift  and  PSHIFTR - Packed shift ROUNDED

BCD arithmetic is a hardware OPTION (manual line 2058). PSHIFT and PSHIFTR are
documented together in manual section 17.6 (file lines 12027-12061).

- Opcodes (octal / hex, manual lines 12035-12036):
  - `PSHIFT`  packed shift:         177262B (0FE82H)
  - `PSHIFTR` packed shift rounded: 177207B (0FE87H)
- Operation (manual line 12038): `<source> -> <dest>`.
- Operands (manual line 12031): `PSHIFT <source/r/BCD=>,<dest/w/BCD=>`
  - source: read, packed BCD descriptor.
  - dest:   write, packed BCD descriptor.
- Description (manual lines 12042-12046): the source content is shifted to the
  scaling factor of the dest and (PSHIFTR only) rounded before being stored.
  The dest string is zero-extended if necessary. With the exception of
  rounding the value is not modified, only the number of decimal positions.
  If source and dest have the same scaling factor, a MOVE is performed.
  If bit 26 of the dest descriptor is set, the value is stored with sign code
  1111 (unsigned); otherwise dest is given the sign of the source value.

Microcode (MICRO-5800-A30.md) - the packed shift is the BCD "shift" engine
`SHTBCD` / `SHTBCDR`:
- `SHTBCD`  entry at octal `002053` (file line 1081); continuation
  `SHTBCD1` at `017563` (file line 8065).
- `SHTBCDR` entry at octal `002055` (file line 1083); continuation
  `SHTBCDR1` at `017566` (file line 8068).
- `002054` and `002056` carry `K,1IFZ` (set K=1 if ALU result is 0 - part of
  the BCD length/loop control).
- `017563` and `017566` carry `K,ZRO` (clear K) and `Q,Q/LOG`.
- `017564` and `017567` carry `ST,SAVA`.
- Both flow into the shared BCD engine `SHTBCD_1` `021446` -> `STRTBCD`
  (file line 9012), which performs the packed-decimal digit processing and
  overflow detection that drives BO / K.

## PSHIFT / PSHIFTR status flags

The manual gives an EXPLICIT status list for section 17.6 (file lines
12050-12055): "value after rounding = 0 -> Z; value.signbit -> S; BCD overflow
-> BO; BO or IVO -> K".

| Flag | Effect | Source |
|------|--------|--------|
| K (flag)     | CONDITIONAL: SET if BCD overflow (BO) or invalid operation (IVO) occurs, else CLEARED | Manual line 12055 ("BO or IVO -> K") and line 11822 ("K flag is set up on BCD overflow or invalid operation, otherwise the flag is cleared"). Microcode: `K,1IFZ` (002054/002056), `K,ZRO` (017563/017566), overflow via `STRTBCD` 021446. |
| Z (zero)     | CONDITIONAL: SET if the value (after rounding) == 0, else CLEARED | Manual line 12052. Microcode `ST,SAVA` 017564/017567. |
| S (sign)     | CONDITIONAL: set to the sign bit of the value | Manual line 12053. Microcode `ST,SAVA`. |
| C (carry)    | CLEARED | Manual: not mentioned in 17.6 -> reset (lines 2022, 4040); carry only from integer arithmetic (line 2040). |
| O (overflow) | CLEARED | Manual: integer overflow not mentioned in 17.6 -> reset (lines 2022, 4040); O only from integer arithmetic (line 2043). Note: this is O bit 9 (integer overflow), distinct from BO bit 15 (BCD overflow) which IS set - see below. |
| BO (BCD overflow, bit 15) | CONDITIONAL: SET if the dest field is not wide enough to hold the result, else CLEARED | Manual line 12054 ("BCD overflow -> BO") and line 2058. Microcode `STRTBCD` engine. This is an ignorable trap condition. |
| IVO (invalid operation, bit 11) | CONDITIONAL: SET on an invalid packed-decimal operation, else CLEARED | Manual line 12048 lists IVO among trap conditions; line 12055 ties it to K. |

## PSHIFT / PSHIFTR trap conditions

Manual line 12048: Addressing traps, BCD overflow (BO), Invalid operation (IVO).

FINDINGS (emulator vs source):
1. `/home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/Pshiftr.c` documents
   PSHIFTR as "Packed Shift RIGHT" performing a logical right shift by a count.
   This is WRONG per the manual: PSHIFTR is "packed shift ROUNDED" (manual line
   12036) - the same operation as PSHIFT but with rounding to the dest scaling
   factor. It is NOT a right shift by a bit count.
2. `Pshiftr.c` opcode comment "0xFE87-0xFFF5 (9 variants)" and "0xFEB2" in
   `Pshift.c` do not match the manual's single opcodes 177262B (0FE82H) for
   PSHIFT and 177207B (0FE87H) for PSHIFTR. The manual octal is authoritative.
3. `Pshift.c` raises IVO/DR traps and sets Z/S/BO/K; its stated flag intent is
   broadly consistent with the manual, but its opcode (0xFEB2 / 176262 octal)
   conflicts with the manual's 177262B - verify.

---

## Cross-check: manual status-bit summary table

The manual's "Setting of Status Bits" summary table (file lines 15010, 15065,
15066, 15067) lists SHA, SHL, SHR, and PSHIFT rows. Those rows use the terse
`* C A I S :` column encoding whose per-column bit header spans three stacked
header rows (file lines 15031-15034) and is ambiguous to decode column-by-column
from the Markdown alone. The per-instruction "Data status bits" prose (sections
10.24-10.26, 17.6) is unambiguous and is used as the authority above. The
summary-table column decode is therefore marked UNKNOWN (needs verification) as
an independent cross-check and was NOT relied upon for any flag claim.

## Summary matrix (from the authoritative prose + microcode)

| Instr | K | Z | C | O | S | Other | IOV/traps |
|-------|---|---|---|---|---|-------|-----------|
| SHL     | unchanged (desc. may set) | cond result==0 | cleared | cleared | cond signbit | IVO/DZ/FU/FO/BO cleared | Addressing, IOV(count>=size) |
| SHA     | unchanged (desc. may set) | cond result==0 | cleared | cleared | cond signbit | IVO/DZ/FU/FO/BO cleared | Addressing, IOV(count>=size) |
| SHR     | unchanged (desc. may set) | cond result==0 | cleared | cleared | cond signbit | IVO/DZ/FU/FO/BO cleared | Addressing, IOV(count>=size) |
| PSHIFT  | cond BO or IVO | cond value==0 | cleared | cleared (bit9) | cond signbit | BO cond, IVO cond | Addressing, BO, IVO |
| PSHIFTR | cond BO or IVO | cond value(rounded)==0 | cleared | cleared (bit9) | cond signbit | BO cond, IVO cond | Addressing, BO, IVO |
