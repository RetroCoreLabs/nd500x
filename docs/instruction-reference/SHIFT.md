# ND-500 Instruction Category: SHIFT - Functional Behavior Reference

Ground-truth source: microcode listing
`/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
Field decode: `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`
Documented intent: `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
nd500x C sources: `/home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/*.c`

This document was built by TRACING THE MICROCODE routine for each instruction,
decoding the fields, and cross-checking against the ND-500 Reference Manual.
Statements are backed by the microcode/manual; anything not verifiable is marked
UNKNOWN.

--------------------------------------------------------------------------------
## Category contents (5 source files)

| File                                                                 | Mnemonic | Kind                                   |
|----------------------------------------------------------------------|----------|----------------------------------------|
| `.../instructions/SHIFT/Shl.c`                                       | SHL      | Logical shift (integer)                |
| `.../instructions/SHIFT/Sha.c`                                       | SHA      | Arithmetic shift (integer)             |
| `.../instructions/SHIFT/Shr.c`                                       | SHR      | Rotational shift / rotate (integer)    |
| `.../instructions/SHIFT/Pshift.c`                                    | PSHIFT   | Packed-decimal (BCD) scale shift       |
| `.../instructions/SHIFT/Pshiftr.c`                                   | PSHIFTR  | Packed-decimal (BCD) scale shift, rounded |

--------------------------------------------------------------------------------
## Shared microcode facts (decoded once, used by SHL/SHA/SHR)

Q-register shift field (mnemonics.md lines 116-121) - the actual bit engine:

| Field      | Effect                                            | Direction   |
|------------|---------------------------------------------------|-------------|
| Q,Q*LOG    | Q <- Q*2 ; Q(00) <- 0        (logical left)        | LEFT        |
| Q,Q/LOG    | Q <- Q/2 ; Q(SIGN) <- 0      (logical right)       | RIGHT       |
| Q,Q/ARI    | Q <- Q/2 ; Q(SIGN) <- Q(SIGN)(arithmetic right)    | RIGHT       |
| Q,Q*ROT    | Q <- Q*2 ; Q(00) <- Q(SIGN)  (rotate left)         | LEFT        |
| Q,Q/ROT    | Q <- Q/2 ; Q(SIGN) <- Q(00)  (rotate right)        | RIGHT       |

`*` = multiply-by-two = shift/rotate LEFT ; `/` = divide-by-two = shift/rotate RIGHT.

Loop structure (identical shape for all three):
1. Dispatch cell (e.g. SHLW 000360) reads operand-0 (the value) into Q via `Q,F`,
   `READ` fetches it, `EA1SAVE` stores its effective address for the write-back.
2. Next cell loads operand-1 (the shift count, a BY) into the Loop Counter `D,LC`.
3. IOV check cell: `ALU,A EXUC A,BM20 ... D,SC13 CSAVE COND,MSGN -> CLE_IOV`
   compares abs(count) against the operand width. Count >= width takes the
   `SET_IOV` path (003132: OR BM20 into MIC status = raise IOV); otherwise
   `CLE_IOV` (003131: AND-complement clears the IOV bit) and execution proceeds.
4. `..._C_1/_C_2` compute abs(count) and test the SIGN of the count (saved carry
   `SAVC1`). Positive count -> `_PSC_*` branch (Positive Shift Count).
   Negative count -> `_NSC_*` branch (Negative Shift Count).
5. `_PSC_2`/`_NSC_2` performs ONE Q-shift per pass, `LCDECR` decrements the loop
   counter, loop until `COND,LCZ` (loop counter zero).
6. On exit the result cell `ALU,A TYP,DR A,Q D,ALU,REG37 ST,SAVA ... WRITE C,MEMOT`
   writes Q back to the operand (REG37 = operand-0 register/memory target) and
   `ST,SAVA` (mnemonics.md line 656) SAVES ALU STATUS -> writes the data-status bits.

Direction confirmed by microcode + manual: the `_PSC` (positive-count) branch
always produces a LEFT shift/rotate (Q*LOG / Q*ROT); the `_NSC` (negative-count)
branch always produces a RIGHT shift/rotate (Q/LOG / Q/ARI / Q/ROT). This matches
the manual: "Positive <shiftcount> implies left shift, negative implies right."

Flag resolution (was UNKNOWN in the flags-only pass):
- The write cell carries `ST,SAVA`, so Z/S ARE written from the final (pass-Q) ALU
  op: Z if the result value is 0, S = result sign bit.
- The final ALU op is `ALU,A` (pass-through), so carry and overflow it produces are
  0. Combined with Reference Manual rule 4040 (line 4040: "Data status bits not
  mentioned in the instruction description are always cleared") and line 2022,
  the manual's per-instruction list for SHL/SHA/SHR mentions ONLY Z and S -> K, C,
  O are CLEARED (reset to 0). Microcode and manual agree.

Microcode citations: dispatch 000350-000413; cores SHL_C_1 003260 / SHA_C_1
003277 / SHR_C_1 003316; loop bodies SHL_PSC_2 003276, SHL_NSC_2 003272,
SHA_PSC_2 003315, SHA_NSC_2 003311, SHR_PSC_2 003334, SHR_NSC_2 003330;
IOV set 003132 / clear 003131.

================================================================================
## SHL - Logical shift

Opcodes (octal): BY SHL 176250B, H SHL 176251B, W SHL 176252B
(hex 0xFCA8 / 0xFCA9 / 0xFCAA). No BI / F / D variants.
Microcode entries: SHLBY 000350, SHLHW 000354, SHLW 000360.

### Functional pseudocode
```
value  = read operand[0]              ; datatype t = BY / H / W  -> width 8/16/32
count  = (signed BY) read operand[1]  ; shift count, signed byte
width  = 8 | 16 | 32                  ; from datatype

if abs(count) >= width:               ; IOV check (dispatch cell -> SET_IOV 003132)
    raise IOV trap ; operand NOT changed ; STOP   ; (manual 6.5.3.1: dest unchanged)

if count >= 0:                        ; SHL_PSC branch, Q,Q*LOG (line 003276)
    repeat count times: Q <- Q*2 ; Q(0) <- 0      ; logical left, zero-fill
else:                                 ; SHL_NSC branch, Q,Q/LOG (line 003272)
    repeat |count| times: Q <- Q/2 ; Q(sign) <- 0 ; logical right, zero-fill

operand[0] <- Q                       ; write-back to same operand (REG37), ST,SAVA
Z <- (result == 0)
S <- result.signbit
```
Bits shifted out are lost; bits shifted in are 0 (both directions). count == 0 is
legal and leaves the operand unchanged (loop counter zero -> no Q-shift pass).

### Operands
1. `<operand/rw/t>` - value, read-modify-write, datatype t (BY/H/W).
2. `<shiftcount/r/BY>` - signed byte shift count (positive = left, negative = right).

### Result / side-effects
Shifted value written back to operand[0]. No memory side-effects beyond the
operand's own read/write.

### Status flags
| Flag | Effect                                  |
|------|-----------------------------------------|
| K    | CLEARED (not mentioned; rule 4040)      |
| Z    | CONDITIONAL: set if result == 0         |
| C    | CLEARED (not mentioned; rule 4040)      |
| O    | CLEARED (not mentioned; rule 4040)      |
| S    | CONDITIONAL: set = result sign bit      |

### Trap conditions
Addressing traps; Illegal Operand Value (IOV) when abs(shiftcount) >= operand width.
On IOV the destination is not modified.

### Citation
Microcode: SHLW 000360 -> SHL_C_1 003260 -> SHL_PSC_2 003276 (Q*LOG) /
SHL_NSC_2 003272 (Q/LOG); IOV 003132/003131.
Manual: section 10.24 "Logical shift" (data status bits: Z, S). Rule 4040 (line 4040).
nd500x: `/home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/Shl.c`.

CROSS-CHECK: microcode, manual, and Shl.c AGREE (positive = left, negative = right
logical; Z and S only). Shl.c raises TRAP_IOV and does not modify the destination.

================================================================================
## SHA - Arithmetic shift

Opcodes (octal): BY SHA 176253B, H SHA 176254B, W SHA 176255B
(hex 0xFCAB / 0xFCAC / 0xFCAD). No BI / F / D variants.
Microcode entries: SHABY 000364, SHAHW 000370, SHAW 000374.

### Functional pseudocode
```
value  = read operand[0]              ; datatype t = BY / H / W
count  = (signed BY) read operand[1]
width  = 8 | 16 | 32

if abs(count) >= width:               ; IOV check -> SET_IOV 003132
    raise IOV trap ; operand NOT changed ; STOP

if count >= 0:                        ; SHA_PSC branch, Q,Q*LOG (line 003315)
    repeat count times: Q <- Q*2 ; Q(0) <- 0        ; left = logical left
else:                                 ; SHA_NSC branch, Q,Q/ARI (line 003311)
    repeat |count| times: Q <- Q/2 ; Q(sign) <- Q(sign) ; arithmetic right, SIGN EXTEND

operand[0] <- Q                       ; write-back (REG37), ST,SAVA
Z <- (result == 0)
S <- result.signbit
```
Difference from SHL: the RIGHT case uses Q,Q/ARI which REPLICATES the sign bit into
the vacated high positions (sign extension). The LEFT case is identical to SHL
(Q,Q*LOG, zero-fill). count == 0 leaves the operand unchanged.

### Operands
1. `<operand/rw/r>` - value, read-modify-write. (Manual prints the type field as
   `r`; the three assembled variants are BY/H/W integer.)
2. `<shiftcount/r/BY>` - signed byte (positive = left, negative = arithmetic right).

### Result / side-effects
Sign-preserving shifted value written back to operand[0]. Used for signed
multiply/divide by powers of two.

### Status flags
| Flag | Effect                                  |
|------|-----------------------------------------|
| K    | CLEARED (not mentioned; rule 4040)      |
| Z    | CONDITIONAL: set if result == 0         |
| C    | CLEARED (not mentioned; rule 4040)      |
| O    | CLEARED (not mentioned; rule 4040)      |
| S    | CONDITIONAL: set = result sign bit      |

### Trap conditions
Addressing traps; Illegal Operand Value (IOV) when abs(shiftcount) >= operand width.
On IOV the destination is not modified.

### Citation
Microcode: SHAW 000374 -> SHA_C_1 003277 -> SHA_PSC_2 003315 (Q*LOG left) /
SHA_NSC_2 003311 (Q/ARI, arithmetic right, sign-extend); IOV 003132/003131.
Manual: section 10.25 "Arithmetical shift" (data status bits: Z, S). Rule 4040.
nd500x: `/home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/Sha.c`.

CROSS-CHECK: microcode, manual, and Sha.c AGREE (positive = logical left, negative =
sign-extending right; Z and S only). Sha.c raises TRAP_IOV.

================================================================================
## SHR - Rotational shift (rotate)

Opcodes (octal): BY SHR 176256B, H SHR 176257B, W SHR 176260B
(hex 0xFCAE / 0xFCAF / 0xFCB0). No BI / F / D variants.
Microcode entries: SHRBY 000400, SHRHW 000404, SHRW 000410.

### Functional pseudocode
```
value  = read operand[0]              ; datatype t = BY / H / W
count  = (signed BY) read operand[1]
width  = 8 | 16 | 32

if abs(count) >= width:               ; IOV check -> SET_IOV 003132
    raise IOV trap ; operand NOT changed ; STOP

if count >= 0:                        ; SHR_PSC branch, Q,Q*ROT (line 003334)
    repeat count times: Q <- Q*2 ; Q(0) <- Q(sign)  ; ROTATE LEFT (wrap)
else:                                 ; SHR_NSC branch, Q,Q/ROT (line 003330)
    repeat |count| times: Q <- Q/2 ; Q(sign) <- Q(0) ; ROTATE RIGHT (wrap)

operand[0] <- Q                       ; write-back (REG37), ST,SAVA
Z <- (result == 0)
S <- result.signbit
```
Circular shift: bits leaving one end re-enter the other; no bits are lost. count == 0
leaves the operand unchanged.

### Operands
1. `<operand/rw/t>` - value, read-modify-write, datatype t (BY/H/W).
2. `<shiftcount/r/BY>` - signed byte. Positive = rotate LEFT, negative = rotate RIGHT
   (per microcode and manual - see cross-check disagreement below).

### Result / side-effects
Rotated value written back to operand[0].

### Status flags
| Flag | Effect                                  |
|------|-----------------------------------------|
| K    | CLEARED (not mentioned; rule 4040)      |
| Z    | CONDITIONAL: set if result == 0         |
| C    | CLEARED (not mentioned; rule 4040)      |
| O    | CLEARED (not mentioned; rule 4040)      |
| S    | CONDITIONAL: set = result sign bit      |

### Trap conditions
Addressing traps; Illegal Operand Value (IOV) when abs(shiftcount) >= operand width.
On IOV the destination is not modified.

### Citation
Microcode: SHRW 000410 -> SHR_C_1 003316 -> SHR_PSC_2 003334 (Q,Q*ROT = rotate LEFT) /
SHR_NSC_2 003330 (Q,Q/ROT = rotate RIGHT); Q-field decode mnemonics.md 116-121;
IOV 003132/003131.
Manual: section 10.26 "Rotational shift" (line 5214: "Positive <shiftcount> implies
left shift, negative implies right"; data status bits: Z, S). Rule 4040.
nd500x: `/home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/Shr.c`.

CROSS-CHECK - DISAGREEMENT (direction):
- MICROCODE ground-truth: positive count -> SHR_PSC -> Q,Q*ROT -> ROTATE LEFT.
  negative count -> SHR_NSC -> Q,Q/ROT -> ROTATE RIGHT.
- MANUAL 10.26 (line 5214): positive = left, negative = right. AGREES with microcode.
- nd500x Shr.c: implements positive = rotate RIGHT, negative = rotate LEFT (the file
  header claims this was "empirically verified against nd500-as" and explicitly notes
  it contradicts manual 10.26). This is OPPOSITE to both the microcode and the manual.
  Ground truth (microcode) says positive count rotates LEFT. The C implementation's
  direction should be re-examined; the "nd500-as empirical" note may reflect an
  assembler/operand-encoding quirk rather than the CPU's rotate direction.

================================================================================
## PSHIFT - Packed (BCD) shift

Opcode (octal): 177262B (hex 0xFE82). Single opcode (BCD option instruction).
Reference Manual chapter 17.6 "Packed shift" (BINARY CODED DECIMAL INSTRUCTIONS
(Option)).

NOTE - opcode discrepancy: the nd500x file `Pshift.c` documents the opcode as
"0xFEB2 (176262 octal)". The Reference Manual (line 12035) states 0xFE82 / 177262B.
The MANUAL is the truth; the C header value looks wrong (0xFEB2 vs 0xFE82, 176262
vs 177262).

### Functional description (manual 17.6)
```
; Both operands are packed-decimal (BCD) descriptor operands, NOT plain integers.
source = packed-decimal value described by <source/r/BCD=>
dest   = packed-decimal field described by <dest/w/BCD=>

; Shift the source value to the SCALING FACTOR (decimal-point position) of dest.
; The numeric value is NOT changed - only the number of stored decimal positions.
; If source and dest scaling factors are equal, PSHIFT degenerates to a MOVE.
; The destination string is zero-extended as needed.
; Sign: if bit 26 of the dest descriptor is set, store sign code 1111 (unsigned);
;       otherwise dest takes the sign of the source value.

store scaled source into dest
Z  <- (value after rounding == 0)      ; PSHIFT does not round -> value == 0
S  <- value.signbit
BO <- BCD overflow (result too large for dest field)
K  <- (BO or IVO)
```
This is a DECIMAL scaling/formatting operation on packed BCD data. It is NOT a
binary bit shift.

### Operands
1. `<source/r/BCD=>` - read-only packed-decimal descriptor operand.
2. `<dest/w/BCD=>`   - write-only packed-decimal descriptor operand (target scaling).

### Result / side-effects
The scaled (re-aligned) packed-decimal value is written into the dest field in
memory. Dest field is zero-extended; sign written per dest descriptor bit 26.

### Status flags
| Flag | Effect                                                        |
|------|---------------------------------------------------------------|
| K    | CONDITIONAL: set if BCD overflow (BO) OR invalid operation (IVO)|
| Z    | CONDITIONAL: set if resulting value == 0                       |
| C    | CLEARED (not mentioned; rule 4040)                            |
| O    | CLEARED (not mentioned; rule 4040) - distinct from BO         |
| S    | CONDITIONAL: set = value sign bit                             |
| BO   | CONDITIONAL: set on BCD overflow (separate BCD-overflow status bit) |

### Trap conditions
Addressing traps; BCD Overflow (BO); Invalid Operation (IVO).

### Citation
Manual: section 17.6 "Packed shift" (lines 12031-12060). Rule 4040 for C/O clearing.
Microcode: NO standalone `PSHIFT` entry label exists in MICRO-5800-A30.md. Packed-
decimal scaling is performed by the shared descriptor-shift engine `DES_SHIFT`
(023561) reached from the BCD descriptor setup routines (`DES_SHIFTA` 021163,
`DES_SHIFTB_1` 021132; packed-decimal entry points PACK 002065 / PACKR 002067 /
BCDC 002077). A clean single-entry microtrace for opcode 177262B is NOT available
from this listing (the opcode->microaddress dispatch ROM is not represented as a
followable label). See UNKNOWN below.
nd500x: `/home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/Pshift.c`.

CROSS-CHECK: the nd500x Pshift.c models this as a packed-BCD scale shift
(load descriptors, read BCD, shift = dest.scaling - source.scaling, write BCD),
which matches the MANUAL's intent. Its opcode comment (0xFEB2/176262) disagrees
with the manual (0xFE82/177262).

UNKNOWN (needs deeper microtrace): the exact microcode routine and cell-by-cell
data path of PSHIFT. Missing: the opcode->microaddress mapping for 177262B, and
the precise DES_SHIFT scaling/round loop as invoked for PSHIFT. What is needed is
the decode/dispatch ROM (or a labeled PSHIFT entry) to follow the routine the way
SHL/SHA/SHR were followed.

================================================================================
## PSHIFTR - Packed (BCD) shift, rounded

Opcode (octal): 177207B (hex 0xFE87). Single opcode (BCD option instruction).
Reference Manual chapter 17.6 (same section as PSHIFT); name "packed shift rounded".

NOTE - semantic discrepancy: the nd500x file `Pshiftr.c` names this "Packed Shift
Right" and implements a plain binary LOGICAL RIGHT SHIFT of an integer operand
(`value >> abs(count)`), with 9 variants. The Reference Manual (line 12036) says
PSHIFTR is "packed shift ROUNDED" - the SAME packed-decimal scale operation as
PSHIFT, but with rounding applied when digits are dropped. The MANUAL is the truth:
PSHIFTR is a packed-decimal (BCD) instruction, not a binary bit shift.

### Functional description (manual 17.6)
```
; Identical to PSHIFT, plus rounding.
source = packed-decimal value described by <source/r/BCD=>
dest   = packed-decimal field described by <dest/w/BCD=>

; Shift source to the scaling factor of dest. When decimal positions are removed
; (scaling reduced), ROUND the result (rather than truncate as PSHIFT would).
; Value otherwise unchanged; equal scaling factors -> MOVE. Dest zero-extended.
; Sign: dest descriptor bit 26 set -> sign code 1111 (unsigned); else source sign.

store rounded, scaled source into dest
Z  <- (value after rounding == 0)
S  <- value.signbit
BO <- BCD overflow
K  <- (BO or IVO)
```

### Operands
1. `<source/r/BCD=>` - read-only packed-decimal descriptor operand.
2. `<dest/w/BCD=>`   - write-only packed-decimal descriptor operand (target scaling).

### Result / side-effects
Rounded, re-scaled packed-decimal value stored into the dest field.

### Status flags
| Flag | Effect                                                        |
|------|---------------------------------------------------------------|
| K    | CONDITIONAL: set if BCD overflow (BO) OR invalid operation (IVO)|
| Z    | CONDITIONAL: set if value after rounding == 0                  |
| C    | CLEARED (not mentioned; rule 4040)                            |
| O    | CLEARED (not mentioned; rule 4040) - distinct from BO         |
| S    | CONDITIONAL: set = value sign bit                             |
| BO   | CONDITIONAL: set on BCD overflow                              |

### Trap conditions
Addressing traps; BCD Overflow (BO); Invalid Operation (IVO).

### Citation
Manual: section 17.6 "Packed shift" (lines 12031-12060; PSHIFTR row line 12036).
Rule 4040 for C/O clearing.
Microcode: NO standalone `PSHIFTR` entry label exists; same shared BCD descriptor-
shift machinery as PSHIFT (DES_SHIFT 023561 etc.). The rounding step is expected to
use the BCD rounding cells (`BCD_ADD_RND` 020733), but this could NOT be tied to a
PSHIFTR entry from this listing.
nd500x: `/home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/Pshiftr.c`.

CROSS-CHECK - DISAGREEMENT (semantics): nd500x Pshiftr.c implements a binary logical
right shift on an integer operand and calls it "Packed Shift Right". The MANUAL says
PSHIFTR is a packed-DECIMAL scale shift with rounding (BCD operands, BO/IVO traps,
BO/K status bits). The C implementation appears to be a placeholder that does NOT
match the documented instruction; its "9 variants / IMPLEMENTATION STATUS: BASIC"
notes confirm it is unverified.

UNKNOWN (needs deeper microtrace): the exact microcode routine and rounding data
path of PSHIFTR (opcode 177207B). Missing: the opcode->microaddress dispatch and a
labeled entry to follow the DES_SHIFT + BCD-round loop cell-by-cell.

================================================================================
## Summary of cross-check findings

1. SHL, SHA fully traced; microcode, manual, and nd500x C sources AGREE.
2. SHR fully traced; microcode + manual say positive count = rotate LEFT, but
   nd500x `Shr.c` implements positive = rotate RIGHT (opposite). Microcode is ground
   truth: positive = LEFT.
3. SHL/SHA/SHR flag question RESOLVED: `ST,SAVA` on the write cell sets Z and S from
   the pass-through result; K/C/O are CLEARED by manual rule 4040 (only Z and S are
   documented data-status bits).
4. PSHIFT/PSHIFTR are packed-DECIMAL (BCD option) scale-shift instructions per manual
   17.6, NOT binary bit shifts. nd500x `Pshift.c` roughly matches the manual (but its
   opcode comment 0xFEB2/176262 disagrees with manual 0xFE82/177262); `Pshiftr.c`
   implements the WRONG operation (a binary logical right shift instead of a BCD
   rounded scale shift). Their exact microcode routines are UNKNOWN (no labeled entry).
```
