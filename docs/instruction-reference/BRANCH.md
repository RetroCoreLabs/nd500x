# ND-500 BRANCH Instruction Behavior Reference

Authoritative behavior reference for the ND-500 CPU instruction category **BRANCH**,
built for validating the nd500x emulator. Every statement below is taken directly from
one of the two ground-truth sources; anything not confirmed by either source is marked
`UNKNOWN (needs verification)`.

## Sources

- PRIMARY spec (manual):
  `/home/ronny/repos/nd500x/docs/ND-05.009.4 EN ND-500 Reference Manual.md`
- GROUND-TRUTH micro-behavior (microcode):
  `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md`
- Micro-op field decode (mnemonics):
  `/mnt/e/Dev/Ronny/ND5000UC/manual/mnemonics.md`

Instruction set enumerated from:
`/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/*.c` (20 files).

## Flag model used here

The ND-500 data status bits relevant to this category are K, Z, C (carry), O (overflow),
S (sign). Manual references:

- Manual line 2024: "The Z, C, and S status bits have no corresponding trap conditions.
  They are only used for conditional jumps."
- Manual line 2020: "The majority of control and special instructions, including
  conditional jump instructions, leave the data status bits unaffected."

Microcode micro-op decode (mnemonics.md):

- `ST,SAVA` (mnemonics.md line 656) = "SAVE STATUS FROM ALU OPERATION" -> updates Z/C/O/S.
- `K,ONE` (653) set K=1; `K,ZRO` (654) clear K=0; `K,1IFZ` (655) set K=1 if ALU result 0.
- Condition selectors used by the jump decode (mnemonics.md lines 766-800):
  `COND,ZRO`=Z from status S1; `COND,SGN`=S from S1; `COND,CRY`=C from S1;
  `COND,K`=K from S1; `COND,SORZ`=OR of S and Z from S1; `COND,CNZ`=AND of C and NOT Z
  from S1.
- Branch/get polarity (mnemonics.md lines 843-845): `G,OOPS,T` = fetch (take branch) IF
  condition TRUE; `G,OOPS,F` = fetch (take branch) IF condition FALSE. `ABR,NPCREL`
  (line 817) = "CALCULATE JUMP TARGET ADDRESS".

**Absence rule:** For every instruction whose microcode decode word contains NO `ST,SAVA`
and NO `K,*` micro-op, Z/C/O/S/K are physically not written, i.e. UNCHANGED. This is used
below and is consistent with the manual "Data status bits: Unaffected" statements.

## Branch trap (BT), category-wide

Manual line 2082: "BT: Branch Trap condition occurs when the next instruction to be
executed is other than the one immediately following the last executed instruction; e.g.
after a GO, JUMPQ, RET, LOOP or conditional jump instruction. The trap condition does not
occur if the test in the conditional jump is false and no jump is made."

## Displacement semantics, category-wide

Manual line 3846: "The ND-500 instructions LOOP, LOOPI, LOOPD, GO and IF <rel> GO have
displacement (program relative) addressing. Each instruction has two instruction codes,
one for the byte displacement part and one for the halfword displacement part. GO is also
available with the word displacement part. The displacement is signed, and is the distance
from the first byte of the current instruction to the first byte of the addressed
instruction."

---

## 1. GO -- Unconditional relative jump

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Go.c`
- Manual section 13.1 (lines 7256-7288).
- Opcodes (manual line 7263-7265): GO:B = 0C0H / 300B; GO:H = 0C1H / 301B;
  GO:W = 0C2H / 302B.
- Operation (manual line 7268): `P + <<displacement>> -> P`.
- Operands: 1 direct signed displacement (byte / halfword / word), sign-extended.
- Trap conditions (manual line 7274): Addressing traps, Branch trap (BT).
- Microcode: no distinct labeled GO micro-routine located in
  `/mnt/e/Dev/Ronny/ND5000UC/microcode/MICRO-5800-A30.md` (searched; the relative
  unconditional jump is handled by the general NPCREL fetch path). No `ST,SAVA` path
  observed.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual line 7276 "Data status bits: Unaffected" |
| Z | UNCHANGED | Manual line 7276 |
| C | UNCHANGED | Manual line 7276 |
| O | UNCHANGED | Manual line 7276 |
| S | UNCHANGED | Manual line 7276 |

---

## 2. JUMPG -- Unconditional absolute jump

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Jumpg.c`
- Manual section 13.2 (lines 7294-7320).
- Opcode (manual line 7300): JUMPG = 0B4H / 264B.
- Operation (manual line 7302): `<address> -> P`.
- Operand: 1 general operand `<address/r/W>`. The `<address>` operand may NOT be prefixed
  by ALT (manual line 7306).
- Behavior note (manual line 7308): "If a descriptor range trap occurs, the next
  instruction to be executed is the one following the JUMPG instruction (fall through)."
- Trap conditions (manual line 7310): Addressing traps, Branch trap (BT), Illegal operand
  specifier (IOS).
- Microcode: `JUMPG` at octal 000542 (MICRO-5800-A30.md line 368) and
  `JUMPG_1`/`JUMPG_2` at 003112/003114 (lines 1624-1626); the IRALT check at 003112
  (`COND,IRALT`) implements the ALT-prefix rejection. No `ST,SAVA` in the JUMPG path.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual line 7312 "Data status bits: Unaffected"; microcode no ST,SAVA |
| Z | UNCHANGED | Manual line 7312 |
| C | UNCHANGED | Manual line 7312 |
| O | UNCHANGED | Manual line 7312 |
| S | UNCHANGED | Manual line 7312 |

---

## 3. JUMPS -- Call supervisor ('87 extension)

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Jumps.c`
- Manual section 16.34 (lines 11521-11543). NOTE: the manual documents JUMPS as
  "call supervisor", NOT as a "jump short". See DISCREPANCY note below.
- Opcode (manual line 11527): JUMPS = B9H / 271B.
- Operation (manual lines 11529-11533):
  ```
  P -> context.P
  B -> context.B
  <address> -> P
  <cpuno> -> W1
  ```
- Description (manual lines 11535-11539): "Save P and B register in context block.
  Execution is started in <address>. The instruction implies SOLO mode. W1 returns the
  ND-500/ND-5000 CPU number."
- Operand: 1 general operand `<address/r/W>`.
- Trap conditions (manual line 11541): None.
- Microcode: `JUMPS` at octal 001045 (MICRO-5800-A30.md line 563) and `JUMPS_1` at
  011026 (line 4644). `JUMPS_1` contains `T,PUSH` and chains to `GET_CNTXT`, consistent
  with a context save / supervisor entry (not a plain PC load). No `ST,SAVA` observed.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNKNOWN (needs verification) | Manual 16.34 lists no "Data status bits" line; microcode shows no ST,SAVA/K,* (absence => inferred UNCHANGED, not documented) |
| Z | UNKNOWN (needs verification) | as above |
| C | UNKNOWN (needs verification) | as above |
| O | UNKNOWN (needs verification) | as above |
| S | UNKNOWN (needs verification) | as above |

Side effect (documented, not a flag): W1 <- CPU number.

**DISCREPANCY:** Emulator source `Jumps.c` documents JUMPS as "Jump Short" performing
`PC = address` only. The manual (ND-05.009.4, section 16.34) and microcode (context PUSH /
GET_CNTXT) define JUMPS as a supervisor call that saves P and B to a context block, sets
W1 to the CPU number, and implies SOLO mode. Needs emulator verification.

---

## Conditional jumps IF <rel> GO -- shared behavior

- Manual section 13.3 (lines 7329-7423).
- Format (manual line 7334): `IF <rel> GO <<displacement>>` and, for status-bit tests,
  `IF <rel> GO <bit No./r/BY>, <<displacement>>`.
- Operation (manual lines 7342-7346): `if <rel> then (P)+<<(displacement)>> -> P endif`.
  The sign-extended byte or halfword displacement is added to P when the condition is
  true (manual line 7352).
- Trap conditions (manual line 7358): Addressing traps, Branch trap (BT), Illegal operand
  value (IOV). BT does not occur when the test is false (manual line 2082).
- Data status bits (manual line 7360): **Unaffected** -- applies to ALL IF <rel> GO
  variants below.
- Microcode: the conditional-jump opcode decode chain lives at octal 000544-000567
  (MICRO-5800-A30.md lines 370-388). Every entry uses `ALU,FZRO` (force-zero ALU output),
  `ABR,NPCREL` (compute relative target) and `G,OOPS,T`/`G,OOPS,F` (take branch on
  true/false). NONE contains `ST,SAVA` or any `K,*` micro-op => K/Z/C/O/S all UNCHANGED.

For every IF <rel> GO instruction (sections 4-15), the status-flag table is identical:

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual line 7360 "Data status bits: Unaffected"; microcode decode has no ST,SAVA/K,* |
| Z | UNCHANGED | as above |
| C | UNCHANGED | as above |
| O | UNCHANGED | as above |
| S | UNCHANGED | as above |

Only the branch CONDITION differs per instruction, listed below.

---

## 4. IF = GO -- conditional jump if equal

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfEqualGo.c`
- Opcodes (manual line 7376-7377): IF = GO / IF Z GO = 0C4H / 304B (:B),
  0C5H / 305B (:H).
- Condition (manual line 7376): **Z = 1** ("equal").
- Operation: if Z=1 then `P + displacement -> P`.
- Microcode: `IFEQL` at octal 000544 (MICRO-5800-A30.md line 370):
  `COND,ZRO ... G,OOPS,T` -- take branch when Z true.
- Flags: see shared IF <rel> GO table (all UNCHANGED).
- Traps: Addressing traps, Branch trap (BT), Illegal operand value (IOV).

## 5. IF <> GO -- conditional jump if unequal

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfNotEqualGo.c`
- Opcodes (manual line 7380-7381): IF <> GO / IF -Z GO = 0C6H / 306B (:B),
  0C7H / 307B (:H).
- Condition (manual line 7380): **Z = 0** ("unequal").
- Microcode: `IFUEQ` at octal 000545 (line 371): `COND,ZRO ... G,OOPS,F` -- take branch
  when Z false.
- Flags: shared IF <rel> GO table (all UNCHANGED).
- Traps: Addressing traps, BT, IOV.

## 6. IF > GO -- conditional jump if greater (signed)

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfGreaterThanGo.c`
- Opcodes (manual line 7384-7386): 0C8H / 310B (:B), 0C9H / 311B (:H).
- Condition (manual line 7384): **S = 0 and Z = 0** ("greater signed").
- Microcode: `IFGR` at octal 000546 (line 372): `COND,SORZ ... G,OOPS,F` -- take branch
  when (S OR Z) is FALSE, i.e. S=0 and Z=0.
- Flags: shared IF <rel> GO table (all UNCHANGED).
- Traps: Addressing traps, BT, IOV.

## 7. IF < GO -- conditional jump if less (signed)

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfLessThanGo.c`
- Opcodes (manual line 7387-7390): 0CAH / 312B (:B), 0CBH / 313B (:H).
- Condition (manual line 7387): **S = 1** ("less signed").
- Microcode: `IFLS` at octal 000547 (line 373): `COND,SGN ... G,OOPS,T` -- take branch
  when S true.
- Flags: shared IF <rel> GO table (all UNCHANGED).
- Traps: Addressing traps, BT, IOV.

## 8. IF >= GO -- conditional jump if greater or equal (signed)

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfGreaterEqualGo.c`
- Opcodes (manual line 7391-7394): IF >= GO / IF -S GO = 0CCH / 314B (:B),
  0CDH / 315B (:H).
- Condition (manual line 7391): **S = 0** ("greater or equal signed").
- Microcode: `IFGRE` at octal 000550 (line 374): `COND,SGN ... G,OOPS,F` -- take branch
  when S false, i.e. S=0.
- Flags: shared IF <rel> GO table (all UNCHANGED).
- Traps: Addressing traps, BT, IOV.

## 9. IF <= GO -- conditional jump if less or equal (signed)

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfLessEqualGo.c`
- Opcodes (manual line 7395-7397): 0CEH / 316B (:B), 0CFH / 317B (:H).
- Condition (manual line 7395): **S = 1 or Z = 1** ("less or equal signed").
- Microcode: `IFLSE` at octal 000551 (line 375): `COND,SORZ ... G,OOPS,T` -- take branch
  when (S OR Z) true.
- Flags: shared IF <rel> GO table (all UNCHANGED).
- Traps: Addressing traps, BT, IOV.

## 10. IF K GO -- conditional jump if flag K set

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Ifkgo.c`
- Opcodes (manual line 7398-7400): 0D0H / 320B (:B), 0D1H / 321B (:H).
- Condition (manual line 7398): **K = 1** ("flag set").
- Microcode: `IFK` at octal 000552 (line 376): `COND,K ... G,OOPS,T` -- take branch when
  K true.
- Flags: shared IF <rel> GO table (all UNCHANGED). (K is TESTED, not modified.)
- Traps: Addressing traps, BT, IOV.

## 11. IF -K GO -- conditional jump if flag K reset

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfKeyGo.c`
- Opcodes (manual line 7401-7403): 0D2H / 322B (:B), 0D3H / 323B (:H).
- Condition (manual line 7401): **K = 0** ("flag reset").
- Microcode: `IFNK` at octal 000553 (line 377): `COND,K ... G,OOPS,F` -- take branch when
  K false, i.e. K=0.
- Flags: shared IF <rel> GO table (all UNCHANGED). (K is TESTED, not modified.)
- Traps: Addressing traps, BT, IOV.

## 12. IF >> GO -- conditional jump if greater (magnitude / unsigned)

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfUnsignedGreaterGo.c`
- Opcodes (manual line 7404-7406): 0D4H / 324B (:B), 0D5H / 325B (:H).
- Condition (manual line 7404): **C = 1 and Z = 0** ("greater magnitude").
- Microcode: `IFGRM` at octal 000554 (line 378): `COND,CNZ ... G,OOPS,T` -- COND,CNZ =
  AND of C and NOT Z; take branch when (C and not Z) true.
- Flags: shared IF <rel> GO table (all UNCHANGED).
- Traps: Addressing traps, BT, IOV.
- Manual usage note (line 7356): magnitude tests are only meaningful after compare and
  subtract instructions, as carry is reset in load instructions.

## 13. IF >= GO (magnitude) / IF C GO -- conditional jump if greater or equal magnitude

- Source file:
  `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfUnsignedGreaterEqualGo.c`
- Opcodes (manual line 7407-7410): IF >= GO / IF C GO = 0D6H / 326B (:B),
  0D7H / 327B (:H).
- Condition (manual line 7407): **C = 1** ("greater or equal magnitude").
- Microcode: `IFC` at octal 000555 (line 379): `COND,CRY ... G,OOPS,T` -- take branch
  when C true, i.e. C=1.
- Flags: shared IF <rel> GO table (all UNCHANGED).
- Traps: Addressing traps, BT, IOV.
- NOTE: emulator `IfUnsignedGreaterEqualGo.c` header comment claims branch on C=0, but the
  emulator code body branches on `nd500_test_flag(cpu, ND500_FLAG_C)` (C=1), which matches
  the manual and microcode. The header comment is wrong; the code is correct.

## 14. IF << GO / IF -C GO -- conditional jump if less magnitude

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfUnsignedLessGo.c`
- Opcodes (manual line 7411-7414): IF << GO / IF -C GO = 0D8H / 330B (:B),
  0D9H / 331B (:H).
- Condition (manual line 7411): **C = 0** ("less magnitude").
- Microcode: `IFNC` at octal 000556 (line 380): `COND,CRY ... G,OOPS,F` -- take branch
  when C false, i.e. C=0.
- Flags: shared IF <rel> GO table (all UNCHANGED).
- Traps: Addressing traps, BT, IOV.

## 15. IF <= GO (magnitude) -- conditional jump if less or equal magnitude

- Source file:
  `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfUnsignedLessEqualGo.c`
- Opcodes (manual line 7415-7417): 0DAH / 332B (:B), 0DBH / 333B (:H).
- Condition (manual line 7415): **C = 0 or Z = 1** ("less or equal magnitude").
- Microcode: `IFLSEM` at octal 000557 (line 381): `COND,CNZ ... G,OOPS,F` -- take branch
  when (C and not Z) FALSE, i.e. C=0 or Z=1.
- Flags: shared IF <rel> GO table (all UNCHANGED).
- Traps: Addressing traps, BT, IOV.

---

## 16. IF ST GO -- conditional jump if specified status bit SET

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfStackGo.c`
  (opcode-verified: 0xFC7B :B, 0xFD64 :H = manual "IF ST GO").
- Opcodes (manual line 7418-7420): IF ST GO = 0FC7BH / 176173B (:B),
  0FD64H / 176544B (:H).
- Format (manual line 7337): `IF ST GO <bit No./r/BY>, <<displacement>>`.
- Condition (manual line 7354): tests `<bit No.>` of the status register; **jump is
  performed if that bit is SET** ("specified bit in status register set"). `<bit No.>`
  range is 0..29 inclusive; other values cause an illegal operand value trap and no jump
  when <rel> is ST.
- Microcode: `IFSTB` at octal 000560 / `IFSTH` at 000562 (MICRO-5800-A30.md lines
  382,384); helper `IFST_CHK_B` at 003644 / `IFST_CHK_H` at 003646 (lines 1970,1972).
  No `ST,SAVA` in these paths.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual line 7360 "Data status bits: Unaffected"; microcode no ST,SAVA/K,* |
| Z | UNCHANGED | as above |
| C | UNCHANGED | as above |
| O | UNCHANGED | as above |
| S | UNCHANGED | as above |

- Trap conditions (manual line 7358): Addressing traps, Branch trap (BT),
  Illegal operand value (IOV) when `<bit No.>` > 29.

## 17. IF -ST GO -- conditional jump if specified status bit NOT set

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Ifstgo.c`
  (opcode-verified: 0xFD65 :B, 0xFC84 :H = manual "IF -ST GO").
- Opcodes (manual line 7421-7423): IF -ST GO = 0FD65H / 176545B (:B),
  0FC84H / 176204B (:H).
- Format (manual line 7337): `IF -ST GO <bit No./r/BY>, <<displacement>>`.
- Condition (manual line 7354): tests `<bit No.>` of the status register; **jump is
  performed if that bit is NOT set** ("specified bit in status register not set").
  `<bit No.>` range 0..29 inclusive; other values cause an illegal operand value trap.
  Manual line 7354 explicitly: "the jump is performed if <rel> is -ST" (for out-of-range
  bit numbers).
- Microcode: `IFNSTB` at octal 000564 / `IFNSTH` at 000566 (MICRO-5800-A30.md lines
  386,388). No `ST,SAVA` in these paths.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Manual line 7360 "Data status bits: Unaffected"; microcode no ST,SAVA/K,* |
| Z | UNCHANGED | as above |
| C | UNCHANGED | as above |
| O | UNCHANGED | as above |
| S | UNCHANGED | as above |

- Trap conditions (manual line 7358): Addressing traps, Branch trap (BT),
  Illegal operand value (IOV) when `<bit No.>` > 29.

---

## 18. LOOPI -- Loop with increment

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Loopi.c`
- Manual section 13.4 (lines 7434-7507).
- Opcodes (manual line 7441-7450):
  BY LOOPI:B = 0FCDEH / 176336B; BY LOOPI:H = 0FD1EH / 176436B;
  H LOOPI:B = 0FCDFH / 176337B; H LOOPI:H = 0FD1FH / 176437B;
  W LOOPI:B = 0FBFH / 277B; W LOOPI:H = 0E1H / 341B;
  F LOOPI:B = 0FD1CH / 176434B; F LOOPI:H = 0FD21H / 176441B;
  D LOOPI:B = 0FD1DH / 176435B; D LOOPI:H = 0FD22H / 176442B.
- Operands: `<index/rw/t>, <limit/r/t>, <<displacement>>`; t in {BY, H, W, F, D}.
- Operation (manual lines 7452-7458):
  ```
  if <index + 1> - <limit> > 0 then
      address of next instruction -> P     (fall through)
  else
      P + <<displacement>> -> P            (loop back)
  endif
  <index> + 1 -> <index>
  ```
  i.e. increment index by 1; branch (loop back) while modified index <= limit
  (signed comparison, manual line 7462-7464).
- Trap conditions (manual line 7486-7487): Addressing traps, Branch trap (BT).
- Microcode: `LOOPIB` at octal 000570 / `LOOPIH` at 000574 (MICRO-5800-A30.md lines
  390,394). The write-back micro-instructions at 000573 and 000577 contain **`ST,SAVA`**
  (save status from ALU operation) => Z/C/O/S written from the (index+1) ALU result.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Microcode LOOPI path has no K,* micro-op |
| Z | CONDITIONAL: SET if modified index = 0, else CLEARED | Manual line 7493 "modified index = 0 -> Z"; microcode ST,SAVA at 000573/000577 |
| S | CONDITIONAL: SET if modified index sign bit = 1, else CLEARED | Manual line 7494 "modified index.signbit -> S"; microcode ST,SAVA |
| C | Written by ST,SAVA (carry of the index+1 ALU op) but NOT documented in manual | Microcode ST,SAVA at 000573/000577; manual data-status table lists only Z and S. Value/semantics UNKNOWN (needs verification) |
| O | Written by ST,SAVA (overflow of the index+1 ALU op) but NOT documented in manual | Microcode ST,SAVA; manual silent. UNKNOWN (needs verification) |

**DISCREPANCY:** manual + microcode show LOOPI updates at least Z and S; emulator source
`Loopi.c` header comments do not reflect Z/S being set. Needs emulator verification.

## 19. LOOPD -- Loop with decrement

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Loopd.c`
- Manual section 13.5 (lines 7516-7581).
- Opcodes (manual line 7523-7532):
  BY LOOPD:B = 0FD23H / 176443B; BY LOOPD:H = 0FD28H / 176450B;
  H LOOPD:B = 0FD24H / 176444B; H LOOPD:H = 0FD29H / 176451B;
  W LOOPD:B = 0FD25H / 176445B; W LOOPD:H = 0FD2AH / 176452B;
  F LOOPD:B = 0FD26H / 176446B; F LOOPD:H = 0FD2BH / 176453B;
  D LOOPD:B = 0FD27H / 176447B; D LOOPD:H = 0FD2CH / 176454B.
- Operands: `<index/rw/t>, <limit/r/t>, <<displacement>>`; t in {BY, H, W, F, D}.
- Operation (manual lines 7534-7540):
  ```
  <index> - 1 -> <index>
  if <index> - <limit> < 0 then
      address of next instruction -> P     (fall through)
  else
      P + <<displacement>> -> P            (loop back)
  endif
  ```
  i.e. decrement index by 1; branch (loop back) while modified index >= limit
  (signed comparison, manual line 7544).
- Trap conditions (manual line 7559-7561): Addressing traps, Branch trap (BT).
- Microcode: `LOOPDB` at octal 000612 / `LOOPDH` at 000616 (MICRO-5800-A30.md lines
  408,412). Write-back micro-instructions at 000615 and 000621 contain **`ST,SAVA`**
  => Z/C/O/S written from the (index-1) ALU result.

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Microcode LOOPD path has no K,* micro-op |
| Z | CONDITIONAL: SET if modified index = 0, else CLEARED | Manual line 7567 "modified index = 0 -> Z"; microcode ST,SAVA at 000615/000621 |
| S | CONDITIONAL: SET if modified index sign bit = 1, else CLEARED | Manual line 7568 "modified index.signbit -> S"; microcode ST,SAVA |
| C | Written by ST,SAVA (carry of the index-1 ALU op) but NOT documented in manual | Microcode ST,SAVA; manual data-status table lists only Z and S. UNKNOWN (needs verification) |
| O | Written by ST,SAVA (overflow of the index-1 ALU op) but NOT documented in manual | Microcode ST,SAVA; manual silent. UNKNOWN (needs verification) |

**DISCREPANCY:** manual + microcode show LOOPD updates at least Z and S; emulator source
`Loopd.c` header comments do not reflect Z/S being set. Needs emulator verification.

## 20. LOOP -- Loop general (with step)

- Source file: `/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Loop.c`
- Manual section 13.6 (lines 7591-7662).
- Opcodes (manual line 7598-7607; NOTE the manual table has several OCR-duplicated
  hex values in the :B column -- treat the octal codes / :H column with caution):
  F LOOP:B = 0FD30H / 176460B; F LOOP:H = 0FD35H / 176465B;
  D LOOP:B = 0FD31H / 176461B; D LOOP:H = 0FD36H / 176466B;
  BY/H/W LOOP:H = 0FD33H / 0FD34H family (see manual line 7598-7603).
  The emulator `Loop.c` assigns: BY LOOP:B 0xFD2D, BY LOOP:H 0xFD32, H LOOP:B 0xFD2E,
  H LOOP:H 0xFD33, W LOOP:B 0xFD2F, W LOOP:H 0xFD34, F LOOP:B 0xFD30, F LOOP:H 0xFD35,
  D LOOP:B 0xFD31, D LOOP:H 0xFD36. The manual's :B integer codes (FD2D-FD2F) are
  UNKNOWN (needs verification) because the manual table prints 0FD32H for every :B row
  (apparent OCR corruption).
- Operands: `<index/rw/t>, <step/r/t>, <limit/r/t>, <<displacement>>`; t in
  {BY, H, W, F, D}.
- Operation (manual lines 7609-7620):
  ```
  <index> + <step> -> <index>
  if <step> > 0 and <index> - <limit> > 0
     or <step> < 0 and <index> - <limit> < 0 then
      address of next instruction -> P     (fall through)
  else
      P + <displacement> -> P              (loop back)
  endif
  if <step> = 0 then
      illegal operand value trap condition
  endif
  ```
  i.e. add step to index; loop back unless the sign of (index - limit) equals the sign of
  step (manual line 7624).
- Special: a `<step>` value of 0 causes an Illegal operand value (IOV) trap condition, and
  execution continues at the next instruction (manual lines 7618-7620, 7630).
- Trap conditions (manual line 7641-7642): Addressing traps, Branch trap (BT), Illegal
  operand value (IOV).
- Microcode: `LOOPB` at octal 000634 / `LOOPH` at 000640 (MICRO-5800-A30.md lines
  426,430). Both include an `ST,SAVA` micro-instruction (000637 for LOOPB, 000643 for
  LOOPH) => Z/C/O/S written from the ALU result. The step=0 IOV path is `LOOP_IOV_B` at
  003674 / `LOOP_IOV_H` at 003701 (lines 1994,1999), which also carries `ST,SAVA`
  (003676). Dispatch helpers `LOOPB_1`/`LOOPH_1` at 003706/003711 select the increment
  vs decrement sub-path based on step sign (`COND,MSGN`).

| Flag | Effect | Source |
|------|--------|--------|
| K | UNCHANGED | Microcode LOOP path has no K,* micro-op |
| Z | CONDITIONAL: SET if modified index = 0, else CLEARED | Manual line 7648 "modified index = 0 -> Z"; microcode ST,SAVA at 000637/000643 |
| S | CONDITIONAL: SET if modified index sign bit = 1, else CLEARED | Manual line 7649 "modified index.signbit -> S"; microcode ST,SAVA |
| C | Written by ST,SAVA (carry of the index+step ALU op) but NOT documented in manual | Microcode ST,SAVA; manual data-status table lists only Z and S. UNKNOWN (needs verification) |
| O | Written by ST,SAVA (overflow of the index+step ALU op) but NOT documented in manual | Microcode ST,SAVA; manual silent. UNKNOWN (needs verification) |

**DISCREPANCY:** manual + microcode show LOOP updates at least Z and S; emulator source
`Loop.c` header comments do not reflect Z/S being set. Needs emulator verification.

---

## Summary of open items / unresolved

1. JUMPS: manual + microcode define it as "call supervisor" (save P and B to context,
   W1 <- CPU number, SOLO mode), not a plain "jump short". Emulator implements
   `PC = address` only.
2. LOOP / LOOPI / LOOPD: manual data-status tables and microcode `ST,SAVA` show Z and S
   ARE updated (index=0 -> Z; index sign bit -> S). C and O are physically written by
   `ST,SAVA` but their LOOP semantics are undocumented in the manual (marked UNKNOWN).
3. LOOP :B integer opcodes (FD2D-FD2F range): the manual table is OCR-corrupted
   (prints 0FD32H for every :B row); authoritative :B codes UNKNOWN from the manual.
4. GO: no distinct labeled micro-routine located in the microcode file; flag behavior is
   taken from the manual ("Unaffected") only.
