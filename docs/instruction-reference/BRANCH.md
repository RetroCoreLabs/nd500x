# ND-500 Instruction Reference - BRANCH class (functional, microcode-traced)

Source of truth for this document:

- Microcode (ground-truth mechanism):
  `$ND5000UC/microcode/MICRO-5800-A30.md`
- Microcode field mnemonics:
  `$ND5000UC/manual/mnemonics.md`
- Reference Manual (documented intent):
  `docs/ND-05.009.4 EN ND-500 Reference Manual.md`
- Emulator C sources traced for opcode/operand shape:
  `src/cpu/instructions/BRANCH/*.c`

This file was rebuilt by tracing each instruction's microcode routine. Where the
microcode, the manual, and the committed C implementation disagree, the
disagreement is called out explicitly under DISCREPANCY. The prior flags-only
version of this file is preserved in git history.

## How the ND-500 conditional-branch microcode works (shared mechanism)

All twelve arithmetic/flag conditional jumps are ONE microinstruction each, laid
out consecutively at octal 000544..000557. Each cell has the identical shape
(example is IFEQL / IF = GO at octal 000544):

```
ALU,FZRO SLOW2  A,BM00 B,X1  T,JMP  COND,ZRO  ABR,NPCREL  TBC,NEXT  G,OOPS,T
```

Field decode (from mnemonics.md):

- `ALU,FZRO`   - ALU output forced to zero; the cell performs NO data computation.
- `COND,<x>`   - selects which standing status bit(s) to test, read from status
                 register S1 (the flags left by the previous data instruction):
                 `ZRO`=Z, `SGN`=S, `SORZ`=S OR Z, `K`=K, `CRY`=C,
                 `CNZ`=C AND (NOT Z).
- `ABR,NPCREL` - "CALCULATE JUMP TARGET ADDRESS", target = instruction-P + displ.
- `G,OOPS,T`   - "GET NEXT INSTRUCTION ... IF TRUE": fetch from the branch target
                 when the tested condition is TRUE.
- `G,OOPS,F`   - same but branch when the condition is FALSE.

Because the cell never asserts `ST,SAVA` and never asserts any `K,*` field, NO
status bit is written. Manual section 13.3 confirms: "Data status bits:
Unaffected." So every conditional branch below leaves K,Z,C,O,S UNCHANGED.

Displacement is a signed direct operand embedded in the instruction stream (1
byte for :B opcode, 2 bytes for :H opcode, and 4 bytes for GO:W). It is
sign-extended and added to the address of the FIRST byte of the branch
instruction (manual 13.9 line: "the distance from the first byte of the current
instruction to the first byte of the addressed instruction"), NOT to the address
of the next instruction.

Branch Trap (BT): manual (Traps chapter) states BT "occurs when the next
instruction to be executed is other than the one immediately following the last
executed instruction; e.g. after a GO, JUMPG, RET, LOOP or conditional jump
instruction. The trap condition does not occur if the test in the conditional
jump is false and no jump is made." BT is an ignorable trap CONDITION bit, not
one of the K/Z/C/O/S data-status bits. It is raised by the branch mechanism only
when a branch is actually TAKEN.

ND-500 carry convention (from the manual conditional-jump table, cross-checked
against the microcode `CRY`/`CNZ` tests): after a compare/subtract of A and B,
`C = 1` means "no borrow" i.e. A >= B unsigned; `C = 0` means "borrow" i.e.
A < B unsigned. Load instructions reset carry, so magnitude tests are only
meaningful after compare/subtract.

---

## GO - Unconditional relative jump

- Opcodes (octal / hex): GO:B 300B / 0xC0, GO:H 301B / 0xC1, GO:W 302B / 0xC2
- Operands: 1 direct signed displacement (BY / H / W by variant)
- Microcode: JUMPEND, octal 000541 (the sole unconditional NPCREL cell in the
  store). C source: `.../BRANCH/Go.c`

FUNCTIONAL PSEUDOCODE:
```
1. displ  <- signextend(direct operand, {BY|H|W})   ; 1, 2 or 4 stream bytes
2. P      <- address_of(this instruction) + displ    ; ABR,NPCREL
3. fetch next instruction from new P                 ; G,OOPS (unconditional)
```

Microcell (000541):
`ALU,FZRO A,BM00 B,X1 ... TBC,NPCREL G,OOPS` - unconditional (no ,T/,F), NPCREL
target, no ALU result, no status write.

- RESULT / side effects: P updated. No memory written.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: Addressing traps (operand fetch), Branch trap (BT, always, since the
  jump is unconditional).
- CITATION: microcode octal 000541 (JUMPEND); manual 13.1, "Data status bits:
  Unaffected".

---

## JUMPG - Unconditional absolute jump

- Opcode: 264B / 0xB4
- Operands: 1 general operand `<address/r/W>` (32-bit absolute target)
- Microcode: JUMPG octal 000542-000543, continuing at JUMPG_1 octal 003112,
  JUMPG_2 octal 003114. C source: `.../BRANCH/Jumpg.c`

FUNCTIONAL PSEUDOCODE:
```
1. SC4 <- read operand value (word) via general addressing   ; 000542, READ ADACT
2. if operand used the ALT prefix:                           ; JUMPG_1, COND,IRALT
       trap Illegal Operand Specifier (IOS)                  ; -> ILL_OP_SPEC
3. P   <- SC4                                                 ; JUMPG_2, D,IAC,P
4. fetch next instruction from new P                         ; -> GET_NEXT
   ; if a descriptor-range trap occurred while evaluating the
   ; operand, control "falls through" to the next instruction
   ; (JUMPG_DR_1 / SOUR_RANGE path, octal 003110/003117)
```

- OPERANDS + datatype: address, word (W), general addressing mode; ALT prefix
  forbidden.
- RESULT / side effects: P set to absolute address. No memory written.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- TRAPS: Addressing traps, Branch trap (BT), Illegal Operand Specifier (IOS) if
  ALT prefix used, descriptor-range trap (falls through to next instruction).
- CITATION: microcode octal 000542 (JUMPG) -> 003112 (JUMPG_1, the COND,IRALT
  ALT-prefix check) -> 003114 (JUMPG_2, P<-SC4); manual 13.2, "Data status bits:
  Unaffected".

---

## JUMPS - Call supervisor ('87 extension)   [NOT a plain jump]

- Opcode: 271B / 0xB9
- Operands: 1 general operand `<address/r/W>`
- Microcode: JUMPS octal 001045-001046, continuing at JUMPS_1 octal 011026.
  C source: `.../BRANCH/Jumps.c`

FUNCTIONAL PSEUDOCODE (manual 16.34):
```
1. context.P <- P            ; save current program counter
2. context.B <- B            ; save current base register
3. P         <- <address>    ; start execution at operand address
4. W1        <- <cpuno>      ; ND-500/ND-5000 CPU number returned in W1
   ; the instruction implies SOLO mode
```

Microcode evidence: JUMPS_1 (octal 011026) does `A,SRF11 ... T,PUSH -> GET_CNTXT`
and the following cells manipulate the context/DAC data path (011027 DAC,DPA,
011030 SPEC,LA, 011031 DAC,B ...) - i.e. a context save, NOT a bare `P<-address`.

- OPERANDS + datatype: address, word (W).
- RESULT / side effects: P and B saved into the context block; P set to
  `<address>`; W1 loaded with the CPU number; SOLO mode entered.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

(manual lists no data-status effect; the visible register effect is W1 <- cpuno)

- TRAPS: None (manual 16.34 "Trap Conditions: None").
- CITATION: microcode octal 001045 (JUMPS) -> 011026 (JUMPS_1 -> GET_CNTXT);
  manual 16.34.
- DISCREPANCY: the committed C implementation `Jumps.c` models JUMPS as an
  absolute jump identical to JUMPG (`PC = address`, nothing else). This is wrong:
  it omits the P/B context save, the `W1 <- cpuno` result, and SOLO-mode entry.
  Its header comment ("Jump Short, functionally identical to JUMPG") is
  incorrect - JUMPS is "call supervisor".

---

## Conditional relative jumps IF <rel> GO

All twelve share the single-microcell mechanism described at the top of this
file. For every one: STATUS FLAGS K,Z,C,O,S are UNCHANGED (manual 13.3 "Data
status bits: Unaffected"; microcode cells assert `ALU,FZRO` with no `ST,SAVA`).
Operand: 1 direct signed displacement (:B = 1 byte, :H = 2 bytes),
sign-extended, added to the address of the first byte of the instruction.
TRAPS for all: Addressing traps, Branch trap (BT) when the jump is taken.

| Mnemonic | Branch taken when | Opcode :B (oct/hex) | :H (oct/hex) | Microcode | C source |
|----------|-------------------|---------------------|--------------|-----------|----------|
| IF = GO   (equal)               | Z = 1            | 304B / 0xC4 | 305B / 0xC5 | IFEQL  000544 (COND,ZRO ,T)  | IfEqualGo.c |
| IF <> GO  (unequal)             | Z = 0            | 306B / 0xC6 | 307B / 0xC7 | IFUEQ  000545 (COND,ZRO ,F)  | IfNotEqualGo.c |
| IF > GO   (greater, signed)     | S = 0 AND Z = 0  | 310B / 0xC8 | 311B / 0xC9 | IFGR   000546 (COND,SORZ ,F) | IfGreaterThanGo.c |
| IF < GO   (less, signed)        | S = 1            | 312B / 0xCA | 313B / 0xCB | IFLS   000547 (COND,SGN ,T)  | IfLessThanGo.c |
| IF >= GO  (greater/eq, signed)  | S = 0            | 314B / 0xCC | 315B / 0xCD | IFGRE  000550 (COND,SGN ,F)  | IfGreaterEqualGo.c |
| IF <= GO  (less/eq, signed)     | S = 1 OR Z = 1   | 316B / 0xCE | 317B / 0xCF | IFLSE  000551 (COND,SORZ ,T) | IfLessEqualGo.c |
| IF K GO   (flag set)            | K = 1            | 320B / 0xD0 | 321B / 0xD1 | IFK    000552 (COND,K ,T)    | Ifkgo.c |
| IF -K GO  (flag reset)          | K = 0            | 322B / 0xD2 | 323B / 0xD3 | IFNK   000553 (COND,K ,F)    | IfKeyGo.c |
| IF >> GO  (greater, unsigned)   | C = 1 AND Z = 0  | 324B / 0xD4 | 325B / 0xD5 | IFGRM  000554 (COND,CNZ ,T)  | IfUnsignedGreaterGo.c |
| IF >= GO / IF C GO (ge, uns.)   | C = 1            | 326B / 0xD6 | 327B / 0xD7 | IFC    000555 (COND,CRY ,T)  | IfUnsignedGreaterEqualGo.c |
| IF << GO  (less, unsigned)      | C = 0            | 330B / 0xD8 | 331B / 0xD9 | IFNC   000556 (COND,CRY ,F)  | IfUnsignedLessGo.c |
| IF <= GO  (less/eq, unsigned)   | C = 0 OR Z = 1   | 332B / 0xDA | 333B / 0xDB | IFLSEM 000557 (COND,CNZ ,F)  | IfUnsignedLessEqualGo.c |

FUNCTIONAL PSEUDOCODE (generic, substitute the condition from the table):
```
1. cond <- test( selected status bits of S1 )    ; e.g. Z, S, C, K, S|Z, C&!Z
2. if cond == <polarity>:                         ; polarity = ,T (true) or ,F
       P <- address_of(this instruction) + signextend(displacement)
   else:
       P <- next sequential instruction
   ; no status bit written
```

Notes verified against microcode/manual:

- IF > GO (greater, signed) tests `SORZ` with FALSE polarity: branch when
  NOT(S OR Z) = S=0 AND Z=0. Matches manual "S=0 and Z=0".
- IF <= GO (signed) tests `SORZ` TRUE: branch when S=1 OR Z=1.
- IF >> GO (unsigned greater) tests `CNZ` = "C AND NOT Z", TRUE polarity: branch
  when C=1 AND Z=0. Matches manual "C=1 and Z=0".
- IF <= GO (unsigned) tests `CNZ` FALSE: branch when NOT(C AND !Z) = (C=0 OR Z=1).
  Matches manual "C=0 or Z=1".
- The executable logic in all twelve committed C files matches the microcode and
  manual. (Several C files contain self-contradictory DOC COMMENTS about the
  carry polarity - e.g. IfUnsignedGreaterEqualGo.c's prose says both "C=0" and
  "C=1" - but the compiled `if()` tests the correct bit. Trust the code and this
  table, not those comments.)

STATUS FLAGS (all twelve identical):

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

- CITATION: microcode octal 000544-000557; manual 13.3 conditional-jump table
  (page 220), "Data status bits: Unaffected".

---

## IF ST GO / IF -ST GO - Conditional jump on a specified status-register bit

- IF ST GO   (jump if bit SET):     :B 176173B / 0xFC7B, :H 176544B / 0xFD64
- IF -ST GO  (jump if bit NOT set):  :B 176545B / 0xFD65, :H 176204B / 0xFC84
- Operands: 2 - `<bit No./r/BY>` (0..29) and `<<displacement>>` (:B/:H)
- Microcode: entry cells IFSTB octal 000560, IFSTH 000562, IFNSTB 000564,
  IFNSTH 000566; bodies IFSTB_0 octal 003650.., IFST_CHK_B octal 003644..,
  range check at octal 003651 (`A-B, MARG=035` i.e. 29 - bitno).
  C sources: `.../BRANCH/IfStackGo.c` and `.../BRANCH/Ifstgo.c`

FUNCTIONAL PSEUDOCODE:
```
1. bitno <- read operand[0] as BY                  ; 000560, D,LC (loop counter)
2. if (29 - bitno) < 0   (i.e. bitno > 29):        ; 003651 A-B MARG=035, COND,MSGN
       trap Illegal Operand Value (IOV)            ; IFST_CHK path
3. mask <- 1 << bitno                              ; BMLC (bit mask from LC)
4. bit  <- (S1_status AND mask) != 0               ; 003652 AND, 003653 COND,MZRO
5. for IF -ST GO  (microcell branches when bit CLEAR):
       if bit == 0: P <- addr(this instr) + signextend(displacement)
6. for IF ST GO   (microcell branches when bit SET):
       if bit == 1: P <- addr(this instr) + signextend(displacement)
   else fall through. No status bit written.
```

Microcode detail: the IFSTB body (003652-003654) computes `mask AND status`, then
`COND,MZRO` (true when the masked result is zero, i.e. the bit is CLEAR), pushes
it with `CSAVE`, and at 003654 does `COND,SAVC1 ... G,OOPS,T` - branch when that
saved "bit is clear" condition is TRUE. So the IFSTB microcell branches when the
bit is CLEAR (= the IF -ST GO semantics). The parallel IFNSTB body branches when
the bit is SET (= the IF ST GO semantics). The two available behaviours
(jump-if-set, jump-if-clear) match the manual's two instructions.

- OPERANDS + datatype: bit number (BY, 0-29); displacement (BY or H direct).
- RESULT / side effects: P updated on a taken branch. No memory written.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED | UNCHANGED |

(manual 13.3 "Data status bits: Unaffected"; the IFST microcells assert no
`ST,SAVA` and no `K,*`.)

- TRAPS: Addressing traps, Branch trap (BT) on a taken branch, Illegal Operand
  Value (IOV) when bit number > 29. Manual note on out-of-range bit number: the
  jump is NOT performed for IF ST GO and IS performed for IF -ST GO (out-of-range
  bit is treated as "not set").
- CITATION: microcode octal 000560/000564 (IFSTB/IFNSTB) + octal 003644-003665
  (IFST_CHK_B, range check MARG=035=29, mask/test); manual 13.3 IF ST GO / IF -ST
  GO rows.
- DISCREPANCY: both committed C files invert the branch condition relative to the
  manual and microcode:
  - `IfStackGo.c` carries the IF ST GO opcodes (0xFC7B/0xFD64) but branches when
    `!bit_is_set` - it should branch when the bit IS set.
  - `Ifstgo.c` carries the IF -ST GO opcodes (0xFD65/0xFC84) but branches when
    `bit_is_set` - it should branch when the bit is NOT set.
  Their header comments are also swapped/garbled. The IOV range check (bitno>29)
  in both C files is correct and matches the microcode `MARG=035` test.

---

## LOOPI - Loop with increment

- Opcodes (octal / hex): BY 176336B/0xFCDE (:B), 176436B/0xFD1E (:H);
  H 176337B/0xFCDF, 176437B/0xFD1F; W 277B/0xBF, 341B/0xE1;
  F 176434B/0xFD1C, 176441B/0xFD21; D 176435B/0xFD1D, 176442B/0xFD22
- Operands: 3 - `<index/rw/t>`, `<limit/r/t>`, `<<displacement>>` (:B/:H).
  Data type t may be BY, H, W, F, D.
- Microcode: LOOPIB octal 000570-000573, LOOPIH 000574-000577; FP variants
  FLOOPIB 000600, DLOOPIB 000604, etc. C source: `.../BRANCH/Loopi.c`

FUNCTIONAL PSEUDOCODE (manual 13.4):
```
1. new <- index + 1                          ; 000570 ALU,A CRY,ONE (A+1) -> SC5
2. cmp <- limit - new                         ; 000571 ALU,B-A CRY,ONE
3. index <- new   (write back, full width)    ; 000573 D,ALU,REG37 WRITE
4. set data status from 'new' (ST,SAVA):      ; 000573 ST,SAVA
       Z <- (new == 0);  S <- signbit(new);  C <- 0;  O <- 0
5. if new <= limit  (signed, via MSORZ on cmp):   ; branch taken
       P <- address_of(this instruction) + signextend(displacement)
   else:
       P <- next sequential instruction
```

- OPERANDS + datatypes: index (rw, type t), limit (r, type t), displacement
  (direct BY or H). For F/D the increment is +1.0 and the compare is
  floating-point.
- RESULT / side effects: index incremented in place (written back); P updated on
  loop-back.
- STATUS FLAGS (manual 13.4 lists Z and S; unlisted bits cleared per manual rule
  4040; microcode `ST,SAVA` on the pass-through of `new` gives C=O=0; no `K,*`):

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL: 1 if modified index = 0, else 0 | CLEARED | CLEARED | CONDITIONAL: 1 if modified index sign bit set |

- TRAPS: Addressing traps, Branch trap (BT) on a taken loop-back.
- CITATION: microcode octal 000570-000577 (note `ST,SAVA` at 000573/000577);
  manual 13.4, Data-status table (modified index = 0 -> Z; modified index.signbit
  -> S); manual rule 4040 (unmentioned data-status bits cleared).
- DISCREPANCY: the committed `Loopi.c` integer path explicitly does NOT write
  Z/S/C/O ("LOOPI does NOT modify status flags"). That contradicts both the
  manual (Z and S ARE set from the modified index) and the microcode (`ST,SAVA`
  is asserted). Only the float/double path in `Loopi.c` sets Z/S. The integer
  path should set Z/S from the modified index (and clear C/O).

---

## LOOPD - Loop with decrement

- Opcodes (octal / hex): BY 176443B/0xFD23 (:B), 176450B/0xFD28 (:H);
  H 176444B/0xFD24, 176451B/0xFD29; W 176445B/0xFD25, 176452B/0xFD2A;
  F 176446B/0xFD26, 176453B/0xFD2B; D 176447B/0xFD27, 176454B/0xFD2C
- Operands: 3 - `<index/rw/t>`, `<limit/r/t>`, `<<displacement>>` (:B/:H)
- Microcode: LOOPDB octal 000612-000615, LOOPDH 000616-000621; FP variants
  FLOOPDB 000622, DLOOPDB 000626, etc. C source: `.../BRANCH/Loopd.c`

FUNCTIONAL PSEUDOCODE (manual 13.5):
```
1. new <- index - 1                          ; 000612 ALU,A-1 -> SC5
2. cmp <- limit - new                         ; 000613 ALU,B-A CRY,ONE
3. index <- new   (write back, full width)    ; 000615 D,ALU,REG37 WRITE
4. set data status from 'new' (ST,SAVA):      ; 000615 ST,SAVA
       Z <- (new == 0);  S <- signbit(new);  C <- 0;  O <- 0
5. if new >= limit  (signed, via MSGN on cmp):    ; branch taken
       P <- address_of(this instruction) + signextend(displacement)
   else:
       P <- next sequential instruction
```

- OPERANDS + datatypes: index (rw, type t), limit (r, type t), displacement
  (direct BY or H). For F/D the decrement is -1.0 and the compare is
  floating-point.
- RESULT / side effects: index decremented in place; P updated on loop-back.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL: 1 if modified index = 0, else 0 | CLEARED | CLEARED | CONDITIONAL: 1 if modified index sign bit set |

- TRAPS: Addressing traps, Branch trap (BT) on a taken loop-back.
- CITATION: microcode octal 000612-000621 (`ST,SAVA` at 000615/000621; the
  loop-back test is `COND,MSGN` at 000614/000620); manual 13.5 Data-status table;
  rule 4040.
- DISCREPANCY: `Loopd.c` integer path does not write Z/S/C/O (its comment claims
  "Data status bits: Unaffected"). This contradicts manual 13.5 and the microcode
  `ST,SAVA`. The float/double path does set Z/S. Integer path should set Z/S from
  the modified index and clear C/O.

---

## LOOP - Loop general (configurable step)

- Opcodes (hex, from committed C source; octal computed):
  BY 0xFD2D/176455B (:B), 0xFD32/176462B (:H);
  H 0xFD2E/176456B, 0xFD33/176463B; W 0xFD2F/176457B, 0xFD34/176464B;
  F 0xFD30/176460B, 0xFD35/176465B; D 0xFD31/176461B, 0xFD36/176466B
  (The manual's :B opcode column for section 13.6 is OCR-garbled - most rows
  print 0FD32H - so the :B codes above come from the committed dispatch table and
  the microcode ordering, not the manual table. The manual's :H column matches.)
- Operands: 4 - `<index/rw/t>`, `<step/r/t>`, `<limit/r/t>`, `<<displacement>>`
- Microcode: LOOPB octal 000634-000637, LOOPH 000640-000643;
  LOOPB_1/2/3 octal 003706-003710; LOOP_IOV_B octal 003674; FP variants
  FLOOPB 001543-ish (label FLOOPB), etc. C source: `.../BRANCH/Loop.c`

FUNCTIONAL PSEUDOCODE (manual 13.6):
```
1. read index -> SC5, read step -> SC6                 ; 000634-000635
2. if step == 0:                                        ; tested via COND,MZRO
       trap Illegal Operand Value (IOV); continue at next instruction
                                                        ; 000637 -> LOOP_IOV_B
3. new <- index + step                                  ; LOOPB_1 003706 ALU,A+B
4. if signbit(step)  (step < 0):                        ; LOOPB_1 COND,MSGN
       use decrement-style compare (LOOPB_3 -> LOOPDB_0): loop if new >= limit
   else (step >= 0):
       use increment-style compare (LOOPB_2 -> LOOPIB_0): loop if new <= limit
5. index <- new  (write back);  set data status from 'new' (ST,SAVA):
       Z <- (new == 0);  S <- signbit(new);  C <- 0;  O <- 0
       ; shared LOOPIB_0/LOOPDB_0 tail cells (000573 / 000615) assert ST,SAVA
6. if loop condition holds:
       P <- address_of(this instruction) + signextend(displacement)
   else:
       P <- next sequential instruction
```

Manual 13.6 phrasing of the exit test: exit (fall through) when the sign of
`(index - limit)` equals the sign of `step`; otherwise take the branch. This is
equivalent to: with step>0 exit if new>limit; with step<0 exit if new<limit -
matching the microcode's step-sign dispatch to the increment vs decrement compare
tail, and matching the committed `Loop.c` integer exit logic.

- OPERANDS + datatypes: index (rw, t), step (r, t), limit (r, t), displacement
  (direct BY or H). Types BY/H/W use signed integer compare; F/D use FP compare.
- RESULT / side effects: index updated in place; P updated on loop-back.
- STATUS FLAGS:

| K | Z | C | O | S |
|---|---|---|---|---|
| UNCHANGED | CONDITIONAL: 1 if modified index = 0, else 0 | CLEARED | CLEARED | CONDITIONAL: 1 if modified index sign bit set |

- TRAPS: Addressing traps, Branch trap (BT) on a taken loop-back, Illegal Operand
  Value (IOV) when step == 0 (then execution falls through to the next
  instruction).
- CITATION: microcode octal 000634-000643 (LOOPB/LOOPH) with step-sign dispatch
  at LOOPB_1 octal 003706 and IOV path LOOP_IOV_B octal 003674; the ST,SAVA is in
  the shared LOOPIB_0/LOOPDB_0 tail (octal 000573/000615). Manual 13.6, Data
  status table (modified index = 0 -> Z; modified index.signbit -> S), and the
  step==0 IOV rule; rule 4040.
- DISCREPANCY: `Loop.c` (integer path) does NOT set Z/S/C/O and does NOT
  implement the step==0 -> IOV trap (a zero step silently produces an infinite
  loop). Both contradict manual 13.6 and the microcode: LOOP must set Z/S from the
  modified index (clear C/O) and must raise IOV on step==0.

---

## Summary of C-implementation vs microcode/manual disagreements

1. JUMPS (Jumps.c) - modelled as a plain absolute jump; it is actually "call
   supervisor" (save P/B to context, W1<-cpuno, SOLO mode). Wrong behaviour.
2. IF ST GO / IF -ST GO (IfStackGo.c, Ifstgo.c) - both branch on the INVERTED
   bit condition relative to manual 13.3 and the microcode.
3. LOOPI / LOOPD / LOOP integer paths (Loopi.c, Loopd.c, Loop.c) - fail to set
   the Z and S data-status bits from the modified index (manual 13.4/13.5/13.6
   require it; microcode asserts ST,SAVA). LOOP additionally omits the step==0
   IOV trap.

These are emulator issues, not documentation gaps - the microcode + manual agree
with each other in every case above.
