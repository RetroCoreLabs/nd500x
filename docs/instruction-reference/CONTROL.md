# ND-500 Instruction Reference - CONTROL category

FUNCTIONAL behaviour reference built by TRACING THE MICROCODE, not by guessing from
flag tables. For every instruction the routine was followed cell-by-cell through the
ND-5800 microstore and cross-checked against the printed manual.

Sources of truth (full absolute paths):
- Microcode (ground truth mechanism):
  $ND5000UC/microcode/MICRO-5800-A30.md
- Microcode field decoding:
  $ND5000UC/manual/mnemonics.md
- ND-500 Reference Manual (documented intent):
  docs/ND-05.009.4 EN ND-500 Reference Manual.md
- Emulator implementations:
  src/cpu/instructions/CONTROL/*.c

Category enumerated from:
  src/cpu/instructions/CONTROL/
  Bp.c Clte.c Init.c Noop.c Set1.c Sete.c Setk.c Solo.c Tset.c  (9 instructions)

## Notation and conventions

- Opcodes are given in octal (B suffix) and hex (H suffix), as printed in the manual.
- Microstore addresses (e.g. 000201) are 6-digit OCTAL microcell addresses inside
  MICRO-5800-A30.md. They are NOT the instruction opcode.
- Microcode field meanings used below (from mnemonics.md):
    ALU,A       ALU output = A input          ALU,FZRO   force ALU output to zero
    ALU,AND     A AND B                        ALU,OR     A OR B
    ALU,XOR     A XOR B                        ALU,ANDCB  A AND (NOT B)
    ALU,ANDCA   (NOT A) AND B                  ALU,A-1    A minus 1     ALU,A+B  A plus B
    A,<x>/B,<x> ALU bus inputs (BMnn = internal bit masks, SCn = scratch regs,
                DAC,EAn = effective-address regs, MIC,TE = trap-enable bits,
                MIC,STS/MISTS = micro status/modus register, X1 = index reg)
    D,<x>       destination of ALU result (SCn scratch, ALU,REG37 = ALU work reg,
                MIC,STS/MISTS = micro status, SPEC,MOD = MODUS register)
    TYP,DR      datatype taken from instruction (integer path)  TYP,F single float
    TYP,DF      double float   TYP,BY byte   TYP,BI bit   TYP,HW halfword
    ST,SAVA     SAVE arithmetic status from the ALU result -> writes Z/C/O/S
    K,ONE / K,ZRO / K,1IFZ   set / clear / (set-if-ALU-zero) the K flag
    READ / WRITE / LADDR / ADACT / DAC   memory / effective-address activity
    ORCON=nn    OR the datatype into low microaddress bits to pick the variant cell
    C,SEQ + COND,MZRO (+INVSEQ)   conditional micro-sequence / trap dispatch
- "Data status bits not mentioned in the instruction description are always cleared"
  (Reference Manual, page 132 line 4040, and page ~2022). This resolves the flags the
  earlier flags-only pass left UNKNOWN: where ST,SAVA runs, the listed bits ARE written,
  and any data-status bit the manual does not mention is CLEARED.
- The K flag is treated by the manual as "the flag", separate from the Z/C/O/S data
  status bits. A microcell with no K,* field leaves K UNCHANGED.

---

## BP - Break point

Opcode: 002B / 002H. Zero operands.

Microcode entry: 000201 BP -> 011540 BP_1 -> 011541 -> 011542 -> 011552 BP_2.

### Functional pseudocode
```
1. mask := trap-enable bits (A,MIC,TE) AND (breakpoint bit mask)   ; 011541
2. test the BPT enable bit against MIC status (COND,MZRO, INVSEQ)  ; 011542
3. if BPT enabled:
       raise BPT (Break Point instruction Trap) -> invoke trap handler
   else:
       raise IIC (Illegal Instruction Code) trap
4. record the trap in the micro status register (D,MIC,STS)        ; BP_2 011552
```
No architectural register or memory is modified along the normal path; the only
effect is the trap.

### Operands / datatypes
None.

### Result / side-effects
Raises a trap (BPT if enabled, otherwise IIC). No data written.

### Status flags
| Flag | Effect     |
|------|------------|
| K    | UNCHANGED  |
| Z    | UNCHANGED  |
| C    | UNCHANGED  |
| O    | UNCHANGED  |
| S    | UNCHANGED  |

No ST,SAVA in the routine; manual "Data status bits: Unaffected".

### Trap conditions
- BPT (Breakpoint instruction Trap) if the BPT trap is enabled.
- IIC (Illegal Instruction Code) if the BPT trap is disabled.

### Citation
Microcode 000201 BP -> 011540 BP_1 -> 011541 (AND with MIC,TE) -> 011542 (BPT test)
-> 011552 BP_2. Manual section 16.4 "Break point" (page 292); trap definitions
page ~2086 (BPT) and page ~2213 (IIC).

Note: the emulator (Bp.c) currently only logs the hit and does not yet consult a real
BPT-enable bit (its own TODO). Mechanism above is from microcode/manual, not the C stub.

---

## CLTE - Clear bit in Own Trap Enable register

Opcode: 176472B / 0FD3AH. One operand: <bit no /r/ BY> (byte).

Microcode entry: 000715 CLTE -> 000716 -> 011127 CLTE_01 -> 011137 CLTE1 -> 011051 CED_TO_DIT.

### Functional pseudocode
```
1. read <bit no> (byte) from operand (READ, ADACT)                 ; 000715
2. bit_mask := A,BM05 combined for the addressed bit (ALU,AND)     ; 000716
3. gate against TEMM (Trap Enable Modification Mask):
      if TEMM bit for <bit no> = 0 -> raise IOV and abort          ; CLTE_01, COND,MZRO
4. OTE[bit no] := 0        ; clear the bit in Own Trap Enable       ; CLTE1
5. write the updated OTE back into the domain (CED_TO_DIT copies
   current-executing-domain trap-enable state to the Domain
   Information Table)                                              ; 011051 CED_TO_DIT
```

### Operands / datatypes
- <bit no> : BYTE (r). Selects which OTE bit (0..63; OTE is two 32-bit halves
  OTE1/OTE2 per the register model, page 922).

### Result / side-effects
Clears one bit of the Own Trap Enable register for the current domain and propagates
it to the Domain Information Table. Disabling an ignorable trap makes that condition be
ignored unless the corresponding MTE bit is set; non-ignorable conditions propagate to
the mother domain (manual 16.6).

### Status flags
| Flag | Effect     |
|------|------------|
| K    | UNCHANGED  |
| Z    | UNCHANGED  |
| C    | UNCHANGED  |
| O    | UNCHANGED  |
| S    | UNCHANGED  |

No ST,SAVA; manual "Data status bits: Unaffected".

### Trap conditions
- IOV (Illegal Operand Value) if the addressed bit is not modifiable in TEMM.
- Addressing traps on operand fetch.

### Citation
Microcode 000715 CLTE -> 000716 -> 011127 CLTE_01 -> 011137 CLTE1 -> 011051 CED_TO_DIT.
Manual section 16.6 "Clear bit in trap enable register" (page 294); TEMM rule page ~1905;
register model page 907-922. Emulator Clte.c matches (TEMM gating -> TRAP_IOV, clears
OTE1/OTE2).

---

## INIT - Initialize stack

Opcode: 334B / 0DCH. Three operands, all WORD:
<<bottom of stack /r/ W>>, <stack demand of main program /r/ W>, <total system stack demand /r/ W>.

Microcode entry: 000654 INIT -> 000655 -> 003720 INIT_1 -> 003721 -> 003722 -> 003723 ...

### Functional pseudocode
```
1. read bottom-of-stack (word) via effective-address arithmetic (AD_ARTI=1, EA1SAVE) ; 000654/003720
2. read the two demand operands (READ, ADACT)                                        ; 003721
3. compute address sums with ALU,A+B using DAC effective-address regs                ; 003722
4. B   := <<bottom of stack>>
5. TOS := <<bottom of stack>> + <total system stack demand>
6. B.SP    := <<bottom of stack>> + <stack demand of main program>
7. B.PREVB := 0
8. B.RETA  := 0 ; L := 0            ; bottom-of-stack sentinels
9. if <stack demand of main program> >= <total system stack demand> -> Stack Overflow (STO) trap
```

### Operands / datatypes
Three WORD (32-bit) operands, read-only. <<bottom of stack>> is a 4-byte absolute
data-memory address loaded into B.

### Result / side-effects
- B, TOS, L registers set as above.
- Stack-frame header written in memory: B.PREVB=0, B.RETA=0, B.SP=bottom+main-demand.
- PREVB=0 / RETA=0 act as underflow sentinels (a later RET past the bottom traps).

### Status flags
| Flag | Effect     |
|------|------------|
| K    | UNCHANGED  |
| Z    | UNCHANGED  |
| C    | UNCHANGED  |
| O    | UNCHANGED  |
| S    | UNCHANGED  |

Manual "Data status bits: Unaffected" (no ST,SAVA in the traced cells). Note: the STO
bit is a TRAP status bit, not a data-status flag; the emulator resets it on successful
completion (per the traps section), which is orthogonal to Z/C/O/S/K.

### Trap conditions
- Stack Overflow (STO) if stack demand of main program >= total system stack demand.
- Addressing traps on operand / memory access.

### Citation
Microcode 000654 INIT -> 000655 -> 003720 INIT_1 -> 003721 -> 003722 -> 003723.
Manual section 13.9 "Initialize stack" (page 229). Emulator Init.c matches (B/TOS/SP/PREVB/RETA/L,
STO check). The manual's page-229 example prints operands in an order that, taken
literally, would trap (main >= total); the emulator note flags this as a manual typo.

---

## NOOP - No operation

Opcode: 003B / 003H. Zero operands.

Microcode entry: 000202 NOOP. Single microcell.

### Functional pseudocode
```
1. ALU,FZRO (force zero), no destination written, G,OOPS (no operand)   ; 000202
2. fall through to next macroinstruction fetch
```
The listed continuation ADDR=LOADBI (000203) is simply the physically-next microstore
cell (a different instruction, "load bit"); NOOP is a single terminal cell and the
macroinstruction dispatch (TBC,NEXT) fetches the next instruction. LOADBI is NOT executed.

### Operands / datatypes
None.

### Result / side-effects
None. PC advances to the next instruction.

### Status flags
| Flag | Effect     |
|------|------------|
| K    | UNCHANGED  |
| Z    | UNCHANGED  |
| C    | UNCHANGED  |
| O    | UNCHANGED  |
| S    | UNCHANGED  |

No ST,SAVA; manual "Data status bits: Unaffected".

### Trap conditions
None.

### Citation
Microcode 000202 NOOP. Manual section 15.10 "No operation" (page 263/9683). Emulator
Noop.c matches (does nothing).

---

## SET1 - Set to one

Opcodes (one per datatype):
| Variant | Octal    | Hex    |
|---------|----------|--------|
| BI SET1 | 176206B  | 0FC86H |
| BY SET1 | 176207B  | 0FC87H |
| H  SET1 | 176210B  | 0FC88H |
| W  SET1 | 115B     | 04DH   |
| F  SET1 | 107B     | 047H   |
| D  SET1 | 176211B  | 0FC89H |

One operand: <operand /w/ t> (write, datatype = t).

Microcode entry: 000325 SET1 (integer path, TYP,DR) with ORCON datatype dispatch:
000325 (BI/BY/H/W) -> 000326 SET1F / 000327 (F, TYP,F) -> 000330 SET1D / 000331 (D, TYP,DF).

### Functional pseudocode
```
1. select variant cell by datatype (ORCON ORs the type into the microaddress)
2. ALU produces the constant 1 for the operand's datatype
3. write 1 -> <operand>   (WRITE, D,ALU,REG37)
4. ST,SAVA : save Z/C/O/S from the ALU result (result = 1)
```

### Operands / datatypes
- <operand> : write, datatype per opcode - BIT, BYTE, HALFWORD, WORD, single FLOAT, or
  double FLOAT. Result value is 1 in that type (integer 1, or floating 1.0).

### Result / side-effects
Destination operand replaced by 1. Memory or register written per operand specifier.

### Status flags
Result is +1 (positive, non-zero), and ST,SAVA writes the arithmetic status:
| Flag | Effect                     |
|------|----------------------------|
| K    | UNCHANGED (no K field)     |
| Z    | CLEARED (result != 0)      |
| C    | CLEARED                    |
| O    | CLEARED                    |
| S    | CLEARED (result positive)  |

Manual: "Data status bits: All cleared" - consistent with ST,SAVA on a +1 result plus
the rule that unmentioned bits are cleared.

### Trap conditions
- Addressing traps (constant destination is illegal for write instructions).

### Citation
Microcode 000325 SET1 -> 000326 SET1F -> 000327 -> 000330 SET1D -> 000331 (ORCON datatype
dispatch; every variant cell carries ST,SAVA + WRITE). Manual section 10.18 "Set to one"
(page 154/4857). Emulator Set1.c matches (writes 1, clears Z/S/C/O).

---

## SETE - Set bit in Own Trap Enable register

Opcode: 176471B / 0FD39H. One operand: <bit no /r/ BY> (byte).

Microcode entry: 000713 SETE -> 000714 -> 011110 SETE_01 -> 011120 SETE1 -> 011051 CED_TO_DIT.

### Functional pseudocode
```
1. read <bit no> (byte) from operand (READ, ADACT)                 ; 000713
2. bit_mask := A,BM05 combined for the addressed bit (ALU,AND)     ; 000714
3. gate against TEMM (Trap Enable Modification Mask):
      if TEMM bit for <bit no> = 0 -> raise IOV and abort          ; SETE_01, COND,MZRO
4. OTE[bit no] := 1        ; set the bit in Own Trap Enable         ; SETE1
5. propagate updated OTE to the Domain Information Table            ; 011051 CED_TO_DIT
```

### Operands / datatypes
- <bit no> : BYTE (r). Selects which OTE bit (0..63).

### Result / side-effects
Sets one bit of the Own Trap Enable register for the current domain (enabling that trap
type) and copies the state to the Domain Information Table.

### Status flags
| Flag | Effect     |
|------|------------|
| K    | UNCHANGED  |
| Z    | UNCHANGED  |
| C    | UNCHANGED  |
| O    | UNCHANGED  |
| S    | UNCHANGED  |

No ST,SAVA; manual "Data status bits: Unaffected".

### Trap conditions
- IOV (Illegal Operand Value) if the addressed bit is not modifiable in TEMM.
- Addressing traps on operand fetch.

### Citation
Microcode 000713 SETE -> 000714 -> 011110 SETE_01 -> 011120 SETE1 -> 011051 CED_TO_DIT.
Manual section 16.5 "Set bit in trap enable register" (page 293); TEMM rule page ~1905.
Emulator Sete.c matches (TEMM gating -> TRAP_IOV, sets OTE1/OTE2). SETE and CLTE share
the identical structure and both end in CED_TO_DIT; only step 4 (set vs clear) differs.

---

## SETK - Set flag (K)

Opcode: 177002B / 0FE02H. Zero operands.

Microcode entry: 000774 SETK. Single microcell.

### Functional pseudocode
```
1. K := 1   (microcell field K,ONE)     ; 000774
2. fall through to next macroinstruction fetch
```
The continuation ADDR=CLRK (000775) is the physically-next microcell (the separate
"clear K" instruction, K,ZRO); it is NOT executed - SETK is a single terminal cell.

### Operands / datatypes
None.

### Result / side-effects
Sets the K flag (destination-full / general flag) of the status register to 1.

### Status flags
| Flag | Effect     |
|------|------------|
| K    | SET (= 1)  |
| Z    | UNCHANGED  |
| C    | UNCHANGED  |
| O    | UNCHANGED  |
| S    | UNCHANGED  |

Manual "Data status bits: Unaffected" (K is the flag, not a data-status bit; only K is
touched).

### Trap conditions
None.

### Citation
Microcode 000774 SETK (K,ONE). Manual section 15.11 "Set flag" (page 278/9707).
Emulator Setk.c matches (ST1 |= K).

---

## SOLO - Disable process switch

Opcode: 177000B / 0FE00H. Zero operands.

Microcode entry: 000711 SOLO -> 004524 SOLO_0 -> 000104 DUMMY_2 -> 000105 DUMMY_1 (RETURN).

### Functional pseudocode
```
1. compute PSD-set value (ALU,A-1 on a scratch reg)                ; SOLO_0 004524
2. write the MIC micro-status / modus register (D,MIC,MISTS) to
   set the Process-Switch-Disable (PSD) status bit                 ; SOLO_0
3. T,PUSH / DUMMY subroutine, then RETURN to instruction fetch     ; DUMMY_2 -> DUMMY_1
```
Effect: subsequent instructions up to the next TUTTI run as an indivisible sequence.
A privileged process may stay in SOLO indefinitely; an unprivileged one is limited to
256 cycles (macroinstruction cycles on ND-5000, microcycles on 500/2), else a Disable
Process switch Timeout occurs.

### Operands / datatypes
None.

### Result / side-effects
Sets the Process-Switch-Disable status bit (PSD). Process switching is suppressed until
TUTTI (177001B) re-enables it, or a timeout/error trap fires.

### Status flags
| Flag | Effect     |
|------|------------|
| K    | UNCHANGED  |
| Z    | UNCHANGED  |
| C    | UNCHANGED  |
| O    | UNCHANGED  |
| S    | UNCHANGED  |

No ST,SAVA; manual "Data status bits: Unaffected". (PSD is a machine/trap-status bit,
not a data-status flag.)

### Trap conditions
- DT (Disable process switch Timeout) if disabled longer than 256 cycles (unprivileged).
- DE (Disable process switch Error) if a non-ignorable trap (e.g. page fault) occurs
  while the process switch is disabled.

### Citation
Microcode 000711 SOLO -> 004524 SOLO_0 (D,MIC,MISTS sets PSD) -> 000104 DUMMY_2 ->
000105 DUMMY_1 RETURN. Manual section 16.1 "Disable process switch" (page 289/10018),
trap discussion page ~2265-2273.

Note: the manual mnemonic terminating SOLO is TUTTI (16.2, 177001B); the emulator source
comments describe "conditional branch" termination, which is not what the manual states.
The emulator Solo.c is a logging stub (its PSD effect is a TODO); the mechanism above is
from microcode/manual, not the C stub.

---

## TSET - Test and set

Opcode: 176500B / 0FD40H. One operand: BY TSET <operand /rwl/ BY> (byte, read-write-locked).

Microcode entry: 000757 TSET (TYP,BY) -> 000760 -> 004540 TSET_1 -> 004541 -> 004542 ->
004543 -> 004544 -> 004545 -> 004546 (D,SPEC,MOD) -> DUMMY.

### Functional pseudocode
```
1. lock the memory system (locked swap access; TSET always reads main memory,
   bypassing cache, and updates cache for later loads)
2. old := <operand>            ; byte read (TYP,BY), effective addr via LADDR/EA1SAVE ; 000757/000760
3. set status from old value (COND,MZRO save, ADIRC/CSAVE):
        Z := (old == 0)
        S := (old < 0)         ; sign of the byte read
4. <operand> := all ones (0FFH for a byte)     ; write via DAC path
5. update MODUS register / release lock (D,SPEC,MOD)   ; 004546
6. unlock
```
Steps 1-6 are one uninterruptible read-modify-write (no other processor or channel can
interleave) on MPM-IV and later memory.

### Operands / datatypes
- <operand> : BYTE, read-write, locked. Register and constant operands are ILLEGAL
  (cause IOS). The manual example uses BY4 TSET RESERVE.

### Result / side-effects
Operand is atomically read then overwritten with all-ones (byte -> 0FFH). Cache line
updated. Z/S reflect the value before the store.

### Status flags
| Flag | Effect                                   |
|------|------------------------------------------|
| K    | UNCHANGED                                |
| Z    | CONDITIONAL - SET if old operand == 0    |
| C    | CLEARED (not mentioned -> reset rule)    |
| O    | CLEARED (not mentioned -> reset rule)    |
| S    | CONDITIONAL - SET if old operand < 0     |

Manual lists only "operand was zero -> Z" and "operand was negative -> S"; by the
unmentioned-bits-cleared rule (page 4040), C and O are cleared, K unchanged.

### Trap conditions
- Addressing traps.
- IOS (Illegal Operand Specifier) for register or constant operands.

### Citation
Microcode 000757 TSET -> 000760 -> 004540 TSET_1 -> 004541 (status save) -> 004542 ->
004543 (ADIRC/CSAVE) -> 004544 -> 004545 -> 004546 (D,SPEC,MOD, release) -> DUMMY.
Manual section 16.3 "Test and set" (page 291/10090); IOS rule pages ~2215, ~3630, ~3708;
locked-swap note page ~4003.

DISCREPANCY (emulator vs manual): Tset.c sets only the Z flag and does NOT set S for a
negative old value, and it writes the operand using fi->data_type (byte -> 0FF) rather
than the manual's fixed BYTE form. The manual and microcode both set S from the sign of
the pre-store value; the emulator's S handling is missing.

---

## Summary table

| Instr | Octal   | Operands            | K   | Z    | C   | O   | S    | Traps            |
|-------|---------|---------------------|-----|------|-----|-----|------|------------------|
| BP    | 002B    | none                | -   | -    | -   | -   | -    | BPT / IIC        |
| CLTE  | 176472B | bit no (BY)         | -   | -    | -   | -   | -    | IOV, addressing  |
| INIT  | 334B    | 3 x W               | -   | -    | -   | -   | -    | STO, addressing  |
| NOOP  | 003B    | none                | -   | -    | -   | -   | -    | none             |
| SET1  | *       | operand (w/t)       | -   | clr  | clr | clr | clr  | addressing       |
| SETE  | 176471B | bit no (BY)         | -   | -    | -   | -   | -    | IOV, addressing  |
| SETK  | 177002B | none                | SET | -    | -   | -   | -    | none             |
| SOLO  | 177000B | none                | -   | -    | -   | -   | -    | DT, DE           |
| TSET  | 176500B | operand (rwl/BY)    | -   | cond | clr | clr | cond | IOS, addressing  |

("-" = unchanged; "clr" = cleared; "cond" = conditionally set. SET1 octal is per datatype:
BI 176206B, BY 176207B, H 176210B, W 115B, F 107B, D 176211B.)

## Still-UNKNOWN after this trace (need deeper microtrace)

1. TSET C-flag and exact byte-vs-word status width: manual mentions only Z and S. The
   reset-rule fixes C/O to cleared, but the individual microcells 004541-004545 use
   TYP,HW / ADIRC / SARG masks whose bit-level effect on the saved condition stack was
   not decoded to the bit. The Z/S outcome is confirmed; C/O are inferred from the rule,
   not read out of the status-save microcells.
2. SOLO exact PSD bit position and the privileged/unprivileged 256-cycle counter logic:
   SOLO_0 writes D,MIC,MISTS, but the specific bit and the timeout counter are in the
   MIC status hardware, not decoded here.
3. BP trap-vector selection (which THA entry BPT vs IIC dispatch to): the BP_1/BP_2
   cells set the trap in MIC status; the vector index resolution lives in the shared
   trap-dispatch microcode not traced in this pass.
