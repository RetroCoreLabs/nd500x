# ND-500 Instruction Behavior Reference: FLOAT_MATH

Authoritative behavior reference for the FLOAT_MATH instruction category, for
validating the nd500x emulator. Every statement below is taken directly from one
of the two ground-truth sources; nothing is inferred unless explicitly labelled.

## Sources

1. PRIMARY SPEC (architectural truth):
   `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
   - Chapter 12 MATHEMATICAL FUNCTIONS (sections 12.3 - 12.15)
   - Chapter 15 MISCELLANEOUS INSTRUCTIONS (sections 15.2 Data type conversion,
     15.3 Data type conversion with rounding)
   - Chapter 17 (sections 17.9 PWCONV, 17.10 WPCONV)

2. GROUND-TRUTH MICROCODE (ND-5000 implementation):
   `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
   Field mnemonics decoded via
   `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`

## Category source files (nd500x)

`/home/ronny/repos/nd500x/src/cpu/instructions/FLOAT_MATH/*.c` - 25 files:
Acos, Alog, Alog10, Alog2, Asin, Atan, Atan2, Biconv, Byconr, Byconv, Cos,
Dconv, Exp, Fconr, Fconv, Hconr, Hconv, Poly, Pwconv, Sin, Sqrt, Tan, Wconr,
Wconv, Wpconv.

---

## Flag model and how it is read from the two sources

STATUS flags examined for every instruction (per task): K, Z, C (carry),
O (overflow), S (sign), plus any additional documented bits.

### From the PRIMARY SPEC

Each manual section has a "Data status bits" block. The manual only ever lists
the bits it defines for that instruction; a bit NOT listed is NOT documented by
the manual as being affected. The bits used in this category are:

- Z  = result is zero
- S  = result sign bit
- O  = integer overflow
- FO = floating overflow
- FU = floating underflow
- BO = BCD overflow
- K  = "error / trouble" flag (documented only for PWCONV and WPCONV in this
       category)

### From the MICROCODE (mnemonics.md, STATUS field bits 75-72)

The micro-op in the STATUS field that actually writes the condition flags:

| Value | Mnemonic    | Meaning (verbatim from mnemonics.md)          |
|-------|-------------|-----------------------------------------------|
| 0     | (hold)      | Hold status unchanged                         |
| 1     | K,ONE       | SET K (FLAG) 1 TO K                            |
| 2     | K,ZRO       | CLEAR K (FLAG) 0 TO K                          |
| 3     | K,1IFZ      | SET K TO 1 IF ALU OPERATION IS 0              |
| 4     | ST,SAVA     | SAVE STATUS FROM ALU OPERATION (Z/C/O/S)      |
| 5     | ST,SAVC     | SAVE STATUS FROM ALU IN COMPARE               |
| 6     | ST,SAVF     | SAVE STATUS FROM FLOATING OPERATION           |
| 7     | ST,SAVB     | SAVE STATUS FROM BCD OPERATION                |
| 8     | ST,LOAD     | LOAD ALU STATUS                               |
| 9     | ST,SAVM     | SAVE MIXED STATUS FOR INTEGER MULTIPLY        |
| 12    | ST,ACCA     | SAVE AND ACCUMULATE ALU STATUS                |

NOTE: the task brief mentions `K,1IFFZ`; that mnemonic does NOT appear in
mnemonics.md. The documented conditional-K op is `K,1IFZ` (value 3). Stated as
read, not assumed.

### Observed microcode flag-finalization pattern (ground truth)

For the CHAPTER-12 transcendental/POLY/SQRT instructions the microcode routine
ends by forcing the float result through the ALU (`ALU,FZRO` = "FORCE ZERO ALU
OUTPUT", i.e. a zero/sign test of the result) with `ST,SAVA` set, then jumps to
the common float write-back `FWRITE_0` (octal 017544) / `DWRITE_0`
(octal 017545). Example finalize steps verified:
- `024636` ASINF_6:  `... ST,SAVA ... ADDR=FWRITE_0`
- `025026` ATANF_ANGLE: `... ST,SAVA ... ADDR=FWRITE_0`
- `024350` SINF_3:   `ALU,FZRO ALUF,XOR ... ST,SAVA ... ADDR=FWRITE`

`ST,SAVA` writes the full ALU status word (Z/C/O/S). The manual only DEFINES the
meaning of Z and S for these results. Therefore C and O are physically written
by the microcode but their post-conditions are NOT architecturally documented
for a float pass-through; they are reported below as UNKNOWN rather than guessed.

The CHAPTER-15 CONV/CONR routines likewise finalize with `ST,SAVA` before
`WRITE` (verified at octal `001543`, `001554`, `001557`, `001566`, `001575`).

Convention used in the per-instruction tables:
- SET / CLEARED / UNCHANGED / CONDITIONAL(cond) = confirmed by a cited source.
- UNKNOWN (needs verification) = not defined by the manual AND not isolable in
  the microcode as a defined post-condition.

---

# GROUP A - Chapter 12 mathematical functions

All Chapter-12 instructions take one float/double register target `Rn`
(n = 1..4), selected by the low 2 bits of the opcode. Float form uses register
Fn, double form uses register Dn. Operand `<argument>` is read (`/r/`), any
addressable type of the matching size.

---

## SQRT - Square root

Manual section 12.4 (page 202).

| Form       | Assembly | Octal opcode    | Hex     |
|------------|----------|-----------------|---------|
| Fn (float) | Fn SQRT  | 176324B+(n-1)   | 0FCD4H  |
| Dn (double)| Dn SQRT  | 176330B+(n-1)   | 0FCD8H  |

Operation: `sqrt(<argument>) -> Rn`.
Operands: 1 (`<argument/r/t>`).

Microcode: entry `SQRTF` octal `001477`, `SQRTD` octal `001501`; result-forming
steps carry `ST,SAVA` (e.g. `017415` SQRTF_9, `017460` SQRTD_9).

STATUS FLAGS:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 12.4 does not list K; no K micro-op observed in SQRT routine |
| Z | CONDITIONAL(set if result == 0) | Manual 12.4 "result = 0 -> Z"; microcode `ST,SAVA` at `017460`/`017415` |
| C | UNKNOWN (needs verification) | Not documented; `ST,SAVA` writes C but semantics undefined for this result |
| O | UNKNOWN (needs verification) | Not documented by manual 12.4 |
| S | UNKNOWN (needs verification) | Manual 12.4 lists ONLY Z (S is NOT listed); `ST,SAVA` physically writes S but manual does not define it |

TRAP conditions (Manual 12.4): Addressing traps, Invalid operation (IVO).
Note: a negative argument gives a result of zero and raises IVO.

---

## POLY - Polynomial evaluation

Manual section 12.3 (page 201).

| Form       | Assembly | Octal opcode    | Hex     |
|------------|----------|-----------------|---------|
| Fn (float) | Fn POLY  | 176340B+(n-1)   | 0FCE0H  |
| Dn (double)| Dn POLY  | 176344B+(n-1)   | 0FCE4H  |

Operation: `<cm>*<x>^m + ... + <c1>*<x> + <c0> -> Rn`.
Operands: variable - `<x>`, degree `<m>` (constant, 0..255), then m+1
coefficients `<cm>..<c0>`.

Microcode: entry `POLYF` octal `001503`, `POLYD` octal `001507`.

STATUS FLAGS:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 12.3 does not list K |
| Z | CONDITIONAL(set if result == 0) | Manual 12.3 "result = 0 -> Z" (reflects FINAL result); microcode float finalize `ST,SAVA` -> FWRITE_0 |
| C | UNKNOWN (needs verification) | Not documented |
| O | UNKNOWN (needs verification) | Not documented (see FO/FU below) |
| S | CONDITIONAL(set to result sign bit) | Manual 12.3 "result.signbit -> S" |
| FU | CONDITIONAL(set on floating underflow) | Manual 12.3 "floating underflow -> FU" |
| FO | CONDITIONAL(set on floating overflow) | Manual 12.3 "floating overflow -> FO" |

Manual note: on intermediate overflow/underflow the trap is deferred until the
instruction completes; Z and S reflect the FINAL result.

TRAP conditions (Manual 12.3): Addressing traps, Floating overflow (FO),
Floating underflow (FU), Illegal operand specifier (IOS - raised if `<m>` is not
a positive constant < 256).

---

## SIN - Sine

Manual section 12.5 (page 203).

| Form       | Assembly | Octal opcode    | Hex     |
|------------|----------|-----------------|---------|
| Fn (float) | Fn SIN   | 177530B+(n-1)   | 0FF58H  |
| Dn (double)| Dn SIN   | 177604B+(n-1)   | 0FF84H  |

Operation: `sine(<argument>) -> Rn`. Argument in radians.
Operands: 1.

Microcode: entry `SINF` octal `001354`, `SIND` octal `001356` (both carry
`ST,SAVA`); result finalize `SINF_3` octal `024350` `ST,SAVA` -> FWRITE.

STATUS FLAGS:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 12.5 does not list K |
| Z | CONDITIONAL(set if result == 0) | Manual 12.5 "result = 0 -> Z"; microcode `ST,SAVA` @ `024350` |
| C | UNKNOWN (needs verification) | Not documented |
| O | UNKNOWN (needs verification) | Not documented |
| S | CONDITIONAL(set to result sign bit) | Manual 12.5 "result.signbit -> S" |

TRAP conditions (Manual 12.5): Addressing traps, Invalid operation (IVO).
Note: |argument| > 65536.0 radians raises IVO and sets the register to zero.

---

## ASIN - Arc sine

Manual section 12.6 (page 204).

| Form       | Assembly | Octal opcode    | Hex     |
|------------|----------|-----------------|---------|
| Fn (float) | Fn ASIN  | 177534B+(n-1)   | 0FF5CH  |
| Dn (double)| Dn ASIN  | 177610B+(n-1)   | 0FF88H  |

Operation: `arcsine(<argument>) -> Rn`. Result in radians, range -pi/2..pi/2.
Operands: 1.

Microcode: entry `ASINF` octal `001360`, `ASIND` octal `001362`; finalize
`ASINF_6` octal `024636` `ST,SAVA` -> FWRITE_0.

STATUS FLAGS:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 12.6 does not list K |
| Z | CONDITIONAL(set if result == 0) | Manual 12.6 "result = 0 -> Z"; microcode `ST,SAVA` @ `024636` |
| C | UNKNOWN (needs verification) | Not documented |
| O | UNKNOWN (needs verification) | Not documented |
| S | CONDITIONAL(set to result sign bit) | Manual 12.6 "result.signbit -> S" |

TRAP conditions (Manual 12.6): Addressing traps, Invalid operation (IVO).
Note: argument outside -1..+1 raises IVO and sets the register to zero.

---

## ACOS - Arc cosine

Manual section 12.8 (page 206).

| Form       | Assembly | Octal opcode    | Hex     |
|------------|----------|-----------------|---------|
| Fn (float) | Fn ACOS  | 177544B+(n-1)   | 0FF64H  |
| Dn (double)| Dn ACOS  | 177620B+(n-1)   | 0FF90H  |

Operation: `arccosine(<argument>) -> Rn`. Result in radians, range 0..pi.
Operands: 1.

Microcode: entry `ACOSF` octal `001370`, `ACOSD` octal `001372` (share the
ASIN finalize path `ASINF_1`/`ASIND_1`).

STATUS FLAGS:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 12.8 does not list K |
| Z | CONDITIONAL(set if result == 0) | Manual 12.8 "result = 0 -> Z" |
| C | UNKNOWN (needs verification) | Not documented |
| O | UNKNOWN (needs verification) | Not documented |
| S | CONDITIONAL(set to result sign bit) | Manual 12.8 "result.signbit -> S" |

TRAP conditions (Manual 12.8): Addressing traps, Invalid operation (IVO).
Note: argument outside -1..+1 raises IVO and sets the register to zero.

---

## COS - Cosine

Manual section 12.7 (page 205).

| Form       | Assembly | Octal opcode    | Hex     |
|------------|----------|-----------------|---------|
| Fn (float) | Fn COS   | 177540B+(n-1)   | 0FF60H  |
| Dn (double)| Dn COS   | 177614B+(n-1)   | 0FF8CH  |

Operation: `cosine(<argument>) -> Rn`. Argument in radians.
Operands: 1.

Microcode: entry `COSF` octal `001364`, `COSD` octal `001366` (both carry
`ST,SAVA`); `COSF_0` octal `024335`, `COSD_0` octal `024362` route into the
SIN constant path.

STATUS FLAGS:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 12.7 does not list K |
| Z | CONDITIONAL(set if result == 0) | Manual 12.7 "result = 0 -> Z" |
| C | UNKNOWN (needs verification) | Not documented |
| O | UNKNOWN (needs verification) | Not documented |
| S | CONDITIONAL(set to result sign bit) | Manual 12.7 "result.signbit -> S" |

TRAP conditions (Manual 12.7): Addressing traps, Invalid operation (IVO).
Note: |argument| > 65536.0 radians raises IVO and sets the register to zero.

---

## TAN - Tangent

Manual section 12.9 (page 207).

| Form       | Assembly | Octal opcode    | Hex     |
|------------|----------|-----------------|---------|
| Fn (float) | Fn TAN   | 177550B+(n-1)   | 0FF68H  |
| Dn (double)| Dn TAN   | 177624B+(n-1)   | 0FF94H  |

Operation: `tangent(<argument>) -> Rn`. Argument in radians.
Operands: 1.

Microcode: entry `TANF` octal `001374`, `TAND` octal `001376` (both carry
`ST,SAVA`).

STATUS FLAGS:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 12.9 does not list K |
| Z | CONDITIONAL(set if result == 0) | Manual 12.9 "result = 0 -> Z" |
| C | UNKNOWN (needs verification) | Not documented |
| O | UNKNOWN (needs verification) | Not documented |
| S | CONDITIONAL(set to result sign bit) | Manual 12.9 "result.signbit -> S" |

TRAP conditions (Manual 12.9): Addressing traps, Invalid operation (IVO).
Note: |argument| > 65536.0 radians raises IVO and sets the register to zero.

---

## ATAN - Arc tangent

Manual section 12.10 (page 208).

| Form       | Assembly | Octal opcode    | Hex     |
|------------|----------|-----------------|---------|
| Fn (float) | Fn ATAN  | 177554B+(n-1)   | 0FF6CH  |
| Dn (double)| Dn ATAN  | 177630B+(n-1)   | 0FF98H  |

(Manual's hex column prints "0FFC6H" for the float form, which is inconsistent
with the octal 177554B and with the (n-1) sequence; 177554B = 0FF6CH. The octal
177554B is taken as authoritative and matches the nd500x source opcode 0xFF6C.)

Operation: `arctangent(<argument>) -> Rn`. Result in radians, range -pi/2..pi/2.
Operands: 1.

Microcode: entry `ATANF` octal `001400`, `ATAND` octal `001403`; finalize
`ATANF_ANGLE` octal `025026` `ST,SAVA` -> FWRITE_0.

STATUS FLAGS:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 12.10 does not list K |
| Z | CONDITIONAL(set if result == 0) | Manual 12.10 "result = 0 -> Z"; microcode `ST,SAVA` @ `025026` |
| C | UNKNOWN (needs verification) | Not documented |
| O | UNKNOWN (needs verification) | Not documented |
| S | CONDITIONAL(set to result sign bit) | Manual 12.10 "result.signbit -> S" |

TRAP conditions (Manual 12.10): Addressing traps ONLY. (Manual 12.10 does NOT
list Invalid operation - ATAN accepts any argument.)

---

## ATAN2 - Arc tangent, two argument

Manual section 12.11 (page 209).

| Form       | Assembly  | Octal opcode    | Hex     |
|------------|-----------|-----------------|---------|
| Fn (float) | Fn ATAN2  | 177560B+(n-1)   | 0FF70H  |
| Dn (double)| Dn ATAN2  | 177634B+(n-1)   | 0FF9CH  |

Operation: `arctangent(<num>/<den>) -> Rn`. Result in radians in the correct
quadrant, range -pi..pi.
Operands: 2 (`<num/r/t>`, `<den/r/t>`).

Microcode: entry `ATAN2F` octal `001410`, `ATAN2D` octal `001413`.

STATUS FLAGS:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 12.11 does not list K |
| Z | CONDITIONAL(set if result == 0) | Manual 12.11 "result = 0 -> Z" |
| C | UNKNOWN (needs verification) | Not documented |
| O | UNKNOWN (needs verification) | Not documented |
| S | CONDITIONAL(set to result sign bit) | Manual 12.11 "result.signbit -> S" |

TRAP conditions (Manual 12.11): Addressing traps, Invalid operation (IVO).
Note: `<num>` == 0 AND `<den>` == 0 raises IVO and sets the register to zero.

---

## EXP - Exponential

Manual section 12.12 (page 210).

| Form       | Assembly | Octal opcode    | Hex     |
|------------|----------|-----------------|---------|
| Fn (float) | Fn EXP   | 177564B+(n-1)   | 0FF74H  |
| Dn (double)| Dn EXP   | 177640B+(n-1)   | 0FFA0H  |

(Manual's hex column prints garbled "0FF7U4" for the float form; octal
177564B = 0FF74H is authoritative and matches nd500x source opcode 0xFF74.)

Operation: `e ** <argument> -> Rn`.
Operands: 1.

Microcode: entry `EXPF` octal `001417`, `EXPD` octal `001421` (both carry
`ST,SAVA`).

STATUS FLAGS:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 12.12 does not list K |
| Z | CONDITIONAL(set if result == 0) | Manual 12.12 "result = 0 -> Z" |
| C | UNKNOWN (needs verification) | Not documented |
| O | UNKNOWN (needs verification) | Not documented |
| S | CLEARED (always 0) | Manual 12.12 "0 -> S" (result is always non-negative; S forced to 0) |

TRAP conditions (Manual 12.12): Addressing traps, Invalid operation (IVO).
Note: argument > 255*ln(2) (~176.75) raises IVO and sets the register to the
largest float (~5.8E+76). argument < -255*ln(2) yields result zero (no trap
stated).

---

## ALOG - Natural logarithm (base e)

Manual section 12.13 (page 211).

| Form       | Assembly | Octal opcode    | Hex     |
|------------|----------|-----------------|---------|
| Fn (float) | Fn ALOG  | 177570B+(n-1)   | 0FF78H  |
| Dn (double)| Dn ALOG  | 177644B+(n-1)   | 0FFA4H  |

Operation: `ln(<argument>) -> Rn`.
Operands: 1.

Microcode: entry `ALOGF` octal `001423`, `ALOGD` octal `001425` (both carry
`ST,SAVA`).

STATUS FLAGS:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 12.13 does not list K |
| Z | CONDITIONAL(set if result == 0) | Manual 12.13 "result = 0 -> Z" |
| C | UNKNOWN (needs verification) | Not documented |
| O | UNKNOWN (needs verification) | Not documented |
| S | CONDITIONAL(set to result sign bit) | Manual 12.13 "result.signbit -> S" |

TRAP conditions (Manual 12.13): Addressing traps, Invalid operation (IVO).
Note: argument <= 0 raises IVO and yields result -5.8*10**76.

---

## ALOG2 - Binary logarithm (base 2)

Manual section 12.14 (page 212).

| Form       | Assembly  | Octal opcode    | Hex     |
|------------|-----------|-----------------|---------|
| Fn (float) | Fn ALOG2  | 177574B+(n-1)   | 0FF7CH  |
| Dn (double)| Dn ALOG2  | 177650B+(n-1)   | 0FFA8H  |

Operation: `log2(<argument>) -> Rn`.
Operands: 1.

Microcode: entry `ALOG2F` octal `001427`, `ALOG2D` octal `001431` (both carry
`ST,SAVA`).

STATUS FLAGS:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 12.14 does not list K |
| Z | CONDITIONAL(set if result == 0) | Manual 12.14 "result = 0 -> Z" |
| C | UNKNOWN (needs verification) | Not documented |
| O | UNKNOWN (needs verification) | Not documented |
| S | CONDITIONAL(set to result sign bit) | Manual 12.14 "result.signbit -> S" |

TRAP conditions (Manual 12.14): Addressing traps, Invalid operation (IVO).
Note: argument <= 0 raises IVO and yields result -5.8*10**76.

---

## ALOG10 - Common logarithm (base 10)

Manual section 12.15 (page 213).

| Form       | Assembly   | Octal opcode    | Hex     |
|------------|------------|-----------------|---------|
| Fn (float) | Fn ALOG10  | 177600B+(n-1)   | 0FF80H  |
| Dn (double)| Dn ALOG10  | 177654B+(n-1)   | 0FFACH  |

Operation: `log10(<argument>) -> Rn`.
Operands: 1.

Microcode: entry `ALOG10F` octal `001433`, `ALOG10D` octal `001435` (both carry
`ST,SAVA`).

STATUS FLAGS:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 12.15 does not list K |
| Z | CONDITIONAL(set if result == 0) | Manual 12.15 "result = 0 -> Z" |
| C | UNKNOWN (needs verification) | Not documented |
| O | UNKNOWN (needs verification) | Not documented |
| S | CONDITIONAL(set to result sign bit) | Manual 12.15 "result.signbit -> S" |

TRAP conditions (Manual 12.15): Addressing traps, Invalid operation (IVO).
Note: argument <= 0 raises IVO and yields result -5.8*10**76.

---

# GROUP B - Chapter 15.2 data type conversion (no rounding)

Format: `t1 t2CONV <source/r/t1>, <dest/w/t2>`. The source of type t1 is
converted to type t2 and stored (2 operands). Integer widening = sign extension;
integer narrowing = truncation of the most significant bits (may set integer
overflow); float-to-integer may set integer overflow. Convert-from-bit: result 0
if bit clear, 1 if bit set. Convert-to-bit: bit set if source != 0, else clear.
Result is NOT rounded.

Manual section 15.2 (pages 268-269) applies IDENTICAL "Data status bits" and
"Trap conditions" to ALL 30 CONV variants:

Common STATUS FLAGS for every CONV variant:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 15.2 does not list K |
| Z | CONDITIONAL(set if result == 0) | Manual 15.2 "result = 0 -> Z"; microcode CONV finalize `ST,SAVA` before WRITE (e.g. octal `001543`, `001554`, `001557`, `001566`, `001575`) |
| C | UNKNOWN (needs verification) | Not documented |
| O | CONDITIONAL(set on integer overflow) | Manual 15.2 lists Integer overflow (O) as a trap; overflow occurs on narrowing/float-to-int. Microcode `ST,SAVA` writes O. |
| S | CONDITIONAL(set to result sign bit) | Manual 15.2 "result.signbit -> S" |

Common TRAP conditions for every CONV variant (Manual 15.2): Addressing traps,
Integer overflow (O).

Microcode: the CONV opcode block dispatches at octal `001530`..`001576`;
integer results finalize with `ST,SAVA` then WRITE (`001543` byte, `001554`
byte, `001557` halfword, `001566` double, `001575` float verified). Per-source
paths route through `CONV_TO_DBI` / `CONV_TO_RBI`.

The five CONV instruction files in the category, with the octal opcode of each
variant (Manual 15.2 table, pages 268-269):

## BICONV - convert TO bit

| Assembly  | Meaning              | Octal   | Hex    |
|-----------|----------------------|---------|--------|
| BY BICONV | byte to bit          | 176511B | 0FD49H |
| H BICONV  | halfword to bit      | 176516B | 0FD4EH |
| W BICONV  | word to bit          | 176523B | 0FD53H |
| F BICONV  | float to bit         | 176530B | 0FD58H |
| D BICONV  | double to bit        | 176535B | 0FD5DH |

Operation: bit set if source != 0, else clear. Flags/traps = common CONV block.

## BYCONV - convert TO byte

| Assembly  | Meaning              | Octal   | Hex    |
|-----------|----------------------|---------|--------|
| BI BYCONV | bit to byte          | 176504B | 0FD44H |
| H BYCONV  | halfword to byte     | 176517B | 0FD4FH |
| W BYCONV  | word to byte         | 176524B | 0FD54H |
| F BYCONV  | float to byte        | 176531B | 0FD59H |
| D BYCONV  | double to byte       | 176536B | 0FD5EH |

Flags/traps = common CONV block.

## HCONV - convert TO halfword

| Assembly  | Meaning              | Octal   | Hex    |
|-----------|----------------------|---------|--------|
| BI HCONV  | bit to halfword      | 176505B | 0FD45H |
| BY HCONV  | byte to halfword     | 176512B | 0FD4AH |
| W HCONV   | word to halfword     | 176525B | 0FD55H |
| F HCONV   | float to halfword    | 176532B | 0FD5AH |
| D HCONV   | double to halfword   | 176537B | 0FD5FH |

Flags/traps = common CONV block.

## WCONV - convert TO word

| Assembly  | Meaning              | Octal   | Hex    |
|-----------|----------------------|---------|--------|
| BI WCONV  | bit to word          | 176506B | 0FD46H |
| BY WCONV  | byte to word         | 176513B | 0FD4BH |
| H WCONV   | halfword to word     | 176520B | 0FD50H |
| F WCONV   | float to word        | 176533B | 0FD5BH |
| D WCONV   | double to word       | 176540B | 0FD60H |

Flags/traps = common CONV block.

## DCONV - convert TO double float

| Assembly  | Meaning              | Octal   | Hex    |
|-----------|----------------------|---------|--------|
| BI DCONV  | bit to double        | 176510B | 0FD48H |
| BY DCONV  | byte to double       | 176515B | 0FD4DH |
| H DCONV   | halfword to double   | 176522B | 0FD52H |
| W DCONV   | word to double       | 176527B | 0FD57H |
| F DCONV   | float to double      | 176534B | 0FD5CH |

Flags/traps = common CONV block.

## FCONV - convert TO float

| Assembly  | Meaning              | Octal   | Hex    |
|-----------|----------------------|---------|--------|
| BI FCONV  | bit to float         | 176507B | 0FD47H |
| BY FCONV  | byte to float        | 176514B | 0FD4CH |
| H FCONV   | halfword to float    | 176521B | 0FD51H |
| W FCONV   | word to float        | 176526B | 0FD56H |
| D FCONV   | double to float      | 176541B | 0FD61H |

Flags/traps = common CONV block.

NOTE: The nd500x FCONV.c file also documents opcode 0xFD61 (D FCONV) which the
manual labels "double float to float convert" - this is the double-to-float
narrowing variant, correctly a member of the "convert TO float" group.

---

# GROUP C - Chapter 15.3 data type conversion WITH rounding

Format: `t1 t2CONR <source/r/t1>, <dest/w/t2>`. Same as CONV but the result is
ROUNDED (2 operands). Manual section 15.3 (page 270) applies IDENTICAL "Data
status bits" and "Trap conditions" to ALL 8 CONR variants:

Common STATUS FLAGS for every CONR variant:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 15.3 does not list K |
| Z | CONDITIONAL(set if result == 0) | Manual 15.3 "result = 0 -> Z" |
| C | UNKNOWN (needs verification) | Not documented |
| O | CONDITIONAL(set on integer overflow) | Manual 15.3 lists Integer overflow (O) as a trap |
| S | CONDITIONAL(set to result sign bit) | Manual 15.3 "result.signbit -> S" |
| FO | CONDITIONAL(set on floating overflow) | Manual 15.3 lists Floating overflow (FO) as a trap for the word/double-to-float variants |

Common TRAP conditions for every CONR variant (Manual 15.3): Addressing traps,
Floating overflow (FO), Integer overflow (O).

## BYCONR - convert TO byte, rounded

| Assembly | Meaning                   | Octal   | Hex    |
|----------|---------------------------|---------|--------|
| F BYCONR | float to byte, rounding   | 177160B | 0FE70H |
| D BYCONR | double to byte, rounding  | 177161B | 0FE71H |

Flags/traps = common CONR block.

## HCONR - convert TO halfword, rounded

| Assembly | Meaning                       | Octal   | Hex    |
|----------|-------------------------------|---------|--------|
| F HCONR  | float to halfword, rounding   | 177162B | 0FE72H |
| D HCONR  | double to halfword, rounding  | 177163B | 0FE73H |

Flags/traps = common CONR block.

## WCONR - convert TO word, rounded

| Assembly | Meaning                   | Octal   | Hex    |
|----------|---------------------------|---------|--------|
| F WCONR  | float to word, rounding   | 177164B | 0FE74H |
| D WCONR  | double to word, rounding  | 177165B | 0FE75H |

Flags/traps = common CONR block.

## FCONR - convert TO float, rounded

| Assembly | Meaning                   | Octal   | Hex    |
|----------|---------------------------|---------|--------|
| W FCONR  | word to float, rounding   | 177203B | 0FE83H |
| D FCONR  | double to float, rounding | 177204B | 0FE84H |

Flags/traps = common CONR block.

---

# GROUP D - packed-decimal (BCD) conversions

## PWCONV - convert packed decimal to binary word

Manual section 17.9 (page 349).

| Assembly  | Meaning                    | Octal          | Hex          |
|-----------|----------------------------|----------------|--------------|
| Wn PWCONV | convert packed to binary   | 177274B+(n-1)  | 0FEBCH+(n-1) |

(Manual's hex column prints "0FBECH"; octal 177274B = 0FEBCH is authoritative
and matches nd500x source opcode 0xFEBC. n = 1..4 selects target Wn = I1..I4.)

Operation: `<source (packed decimal)> -> Rn`. Fractional part of source is lost;
no rounding before conversion. On integer overflow the result is the least
significant 32 bits.
Operands: 2 (`<source/r/BCD>`, target register Wn from opcode).

Microcode: not locatable by the mnemonic "PWCONV" in MICRO-5800-A30.md. The
packed-to-binary routine family is labelled `BINC` (entry octal `002075`,
continuations `BINC_1` octal `022377` onward). Correspondence PWCONV==BINC is
INFERRED from the "convert packed to binary" description and is NOT byte-verified
against the opcode dispatch table - treat the microcode citation as unverified.

STATUS FLAGS (Manual 17.9 explicitly documents all four):

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL(set if IVO or O occurred) | Manual 17.9 "IVO or O -> K" |
| Z | CONDITIONAL(set if value == 0) | Manual 17.9 "value = 0 -> Z" |
| C | UNKNOWN (needs verification) | Not documented |
| O | CONDITIONAL(set on integer overflow) | Manual 17.9 "overflow -> O" |
| S | CONDITIONAL(set to value sign bit) | Manual 17.9 "value.signbit -> S" |

TRAP conditions (Manual 17.9): Addressing traps, Integer overflow (O),
Invalid operation (IVO).

## WPCONV - convert binary word to packed decimal

Manual section 17.10 (page 350).

| Assembly  | Meaning                    | Octal          | Hex          |
|-----------|----------------------------|----------------|--------------|
| Wn WPCONV | convert binary to packed   | 177270B+(n-1)  | 0FEB8H+(n-1) |

(n = 1..4 selects source Wn = I1..I4. Hex 0FEB8H matches nd500x source 0xFEB8.)

Operation: `Rn -> <dest (packed decimal)>`. If the scaling factor of `<dest>` is
negative the least significant digits are lost; `<dest>` is zero-extended (low or
high order) as required by the scaling factor.
Operands: 2 (source register Wn from opcode, `<dest/w/BCD>`).

Microcode: not locatable by the mnemonic "WPCONV". The binary-to-packed routine
family is labelled `PACK` (entry octal `002065`) / `PACKBCD_1` (octal `021775`).
Correspondence WPCONV==PACK is INFERRED from the "convert binary to packed"
description and is NOT byte-verified against the opcode dispatch table - treat
the microcode citation as unverified. (`ST,SAVB` = "save BCD status" was NOT
found in the grepped PACK region.)

STATUS FLAGS (Manual 17.10 explicitly documents all four):

| Flag | Effect | Source |
|------|--------|--------|
| K | CONDITIONAL(set if BO occurred) | Manual 17.10 "BO -> K" |
| Z | CONDITIONAL(set if value == 0) | Manual 17.10 "value = 0 -> Z" |
| C | UNKNOWN (needs verification) | Not documented |
| O | UNKNOWN (needs verification) | Manual 17.10 documents BCD overflow (BO), not integer O |
| S | CONDITIONAL(set to value sign bit) | Manual 17.10 "value.signbit -> S" |
| BO | CONDITIONAL(set on BCD overflow) | Manual 17.10 "BCD overflow -> BO" |

TRAP conditions (Manual 17.10): Addressing traps, BCD overflow (BO).

---

# Summary of unresolved / needs-verification items

1. K flag for ALL Chapter-12 mathematical functions (SQRT, POLY, SIN, ASIN,
   ACOS, COS, TAN, ATAN, ATAN2, EXP, ALOG, ALOG2, ALOG10) and for the CONV/CONR
   families: the manual does not document K and no defining K micro-op was
   isolated in the traced routines. Report as UNKNOWN.

2. C (carry) flag for every FLOAT_MATH instruction: never documented by the
   manual. The float/CONV finalize micro-op `ST,SAVA` physically writes C, but
   its post-condition is architecturally undefined for these results. UNKNOWN.

3. O (overflow) for the Chapter-12 functions: not documented (only FO/FU appear,
   and only for POLY). UNKNOWN for the transcendentals.

4. SQRT S flag: Manual 12.4 lists ONLY Z (not S), unlike every other Chapter-12
   function. Whether S is set to the (always-non-negative) result sign is not
   stated. UNKNOWN per the manual.

5. PWCONV / WPCONV microcode entry points: the mnemonics do not appear literally
   in MICRO-5800-A30.md. The BINC / PACK routine families are the inferred
   implementation but the opcode->microcode-address mapping was not byte-verified
   against the dispatch table. Needs verification.

6. Manual OCR artifacts noted and reconciled against the authoritative OCTAL
   codes: ATAN float hex "0FFC6H" (should be 0FF6CH = 177554B), EXP float hex
   "0FF7U4" (should be 0FF74H = 177564B), PWCONV hex "0FBECH" (should be
   0FEBCH = 177274B). In every case the octal code and the nd500x source opcode
   agree; the printed hex is a scan error.
