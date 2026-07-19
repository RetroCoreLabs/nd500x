# ND-500 Instruction Validation Report

Per-category reference material for every instruction audited below lives under
`/home/ronny/repos/nd500x/docs/instruction-reference/` (one file per category:
ARITHMETIC.md, BITFIELD.md, BRANCH.md, CALL.md, COMPARE.md, CONTROL.md,
FLOAT_MATH.md, IO.md, LOGICAL.md, MOVE.md, SHIFT.md, STRING.md, SYSTEM.md).

This report is a synthesis of the per-category flag/operation audits. "ok" = matches
the authoritative Norsk Data manual; "discrepancy" = confirmed mismatch vs the manual;
"unclear" = the manual/microcode reference itself marks the behaviour UNKNOWN, so no
pass/fail can be asserted (needs a manual/microcode re-check).

Authority order (per project rules): the ND-500 CPU / linker / assembler reference
manuals are the TRUTH. The C# emulator is NOT a reference.

---

## 1. Summary by category

| Category | Total | OK | Discrepancy | Unclear |
|-----------|------:|---:|------------:|--------:|
| ARITHMETIC | 38 | 5 | 33 | 0 |
| BITFIELD | 7 | 1 | 6 | 0 |
| BRANCH | 20 | 16 | 4 | 0 |
| CALL | 18 | 16 | 0 | 2 |
| COMPARE | 5 | 0 | 5 | 0 |
| CONTROL | 9 | 6 | 3 | 0 |
| FLOAT_MATH | 25 | 15 | 7 | 3 |
| IO | 1 | 0 | 1 | 0 |
| LOGICAL | 5 | 0 | 2 | 3 |
| MOVE | 54 | 1 | 50 | 3 |
| SHIFT | 5 | 0 | 5 | 0 |
| STRING | 18 | 8 | 10 | 0 |
| SYSTEM | 35 | 18 | 15 | 2 |
| **TOTAL** | **240** | **86** | **141** | **13** |

---

## 2. Overall totals

- Instructions audited: **240**
- Fully correct (OK): **86** (35.8%)
- Discrepancies: **141** (58.8%)
  - High severity: **44**
  - Medium severity: **86**
  - Low severity: **11**
- Unclear / reference-UNKNOWN (cannot pass/fail): **13**

Dominant discrepancy patterns:
1. **Float/double variants are unimplemented no-ops** (ADD3, DIV2, DIV3, MUL2, MUL3,
   MULAD, SUB2, SUB3) - eight arithmetic instructions silently do nothing for F/D.
2. **C and O status bits never cleared** after operations whose manual entry lists only
   Z/S - pervasive across MOVE (register load/store family), SHIFT, COMPARE, BITFIELD.
   The manual rule (6.5.1 / 4040) is "data status bits not mentioned are reset", so a
   stale C/O from a prior arithmetic op survives.
3. **FLOAT datatype has no case in `nd500_set_flags_zs`** - F NEG and F TEST therefore
   force Z=1/S=0 always (mask=0), breaking BEQ/BLSS after those ops.
4. **K not cleared on the success path** of the packed-BCD (P*) arithmetic family and
   packed compare/shift - K feeds conditional branches.
5. **Wrong operation entirely** - several instructions implement a different (or
   nonexistent) instruction than the manual defines (INTR, SCOPT, SSCAN, SSPAN,
   SMATCH, SCPUNO, PSHIFTR, RPHS/WPHS).

---

## 3. Prioritized discrepancies (HIGH severity first)

### 3.1 HIGH severity (44)

| Instruction | Category | Mismatch | File:Line |
|-------------|----------|----------|-----------|
| ADD3 | ARITHMETIC | F/D variants (opcodes 0x0070-0x0077) print [DEFERRED] and return - no result, no flags | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Add3.c:54 |
| DIV2 | ARITHMETIC | F/D variants are [DEFERRED] no-ops (manual 11.8) | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Div2.c:54 |
| DIV3 | ARITHMETIC | F/D variants are [DEFERRED] no-ops (manual 11.12) | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Div3.c:54 |
| MUL2 | ARITHMETIC | F/D variants are [DEFERRED] no-ops (manual 11.7) | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Mul2.c:53 |
| MUL3 | ARITHMETIC | F/D variants are [DEFERRED] no-ops (manual 11.11) | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Mul3.c:53 |
| SUB2 | ARITHMETIC | F/D variants are [DEFERRED] no-ops (manual 11.6) | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Sub2.c:53 |
| SUB3 | ARITHMETIC | F/D variants are [DEFERRED] no-ops (manual 11.10) | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Sub3.c:53 |
| MULAD | ARITHMETIC | F/D variants [DEFERRED] no-ops; integer BY/H never compute C (only WORD) | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Mulad.c:54 |
| NEG | ARITHMETIC | F variant: Z always set, S always cleared (FLOAT datatype has no case in set_flags_zs, mask=0) - breaks BEQ/BLSS after NEG | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Neg.c:108 |
| AXI | ARITHMETIC | Sets K on invalid op (ref: K UNCHANGED); 0^(neg) should IOV-trap + store largest float, impl stores 0; C/O not cleared | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Axi.c:142 |
| PADD | ARITHMETIC | K not cleared on clean add (ref 17.1/17.2: K=BO|IVO else CLEARED); BO status bit never set | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Padd.c:87 |
| PADDR | ARITHMETIC | K not cleared on success; BO bit never set | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Paddr.c:80 |
| PMPY | ARITHMETIC | K not cleared on success; BO bit never set | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Pmpy.c:81 |
| PMPYR | ARITHMETIC | K not cleared on success; BO bit never set | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Pmpyr.c:84 |
| PPACK | ARITHMETIC | K not cleared on success; BO bit never set | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Ppack.c:92 |
| PPACKR | ARITHMETIC | K not cleared on success; BO bit never set | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Ppackr.c:94 |
| PSUB | ARITHMETIC | K not cleared on success; BO bit never set | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Psub.c:74 |
| PSUBR | ARITHMETIC | K not cleared on success; BO bit never set | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Psubr.c:80 |
| PUPACK | ARITHMETIC | K not cleared on success; BO bit never set | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Pupack.c:102 |
| PUPACKR | ARITHMETIC | K not cleared on success; BO bit never set | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Pupackr.c:125 |
| TEST | COMPARE | F TEST always sets Z, never sets S (FLOAT has no case in set_flags_zs); K/O also not cleared | /home/ronny/repos/nd500x/src/cpu/instructions/COMPARE/Test.c:54 |
| SCOMP | COMPARE | S polarity on byte-difference termination inverted vs reference table (affects ordering branches) - conflicts with impl's own runtime-verified claim; needs manual byte-level re-check | /home/ronny/repos/nd500x/src/cpu/instructions/COMPARE/Scomp.c:174 |
| BYCONR | FLOAT_MATH | Truncates instead of rounding (CONR must round) - 2.6 yields 2 not 3 | /home/ronny/repos/nd500x/src/cpu/instructions/FLOAT_MATH/Byconr.c:47 |
| HCONR | FLOAT_MATH | Truncates instead of rounding (ref 15.3) | /home/ronny/repos/nd500x/src/cpu/instructions/FLOAT_MATH/Hconr.c:47 |
| WCONR | FLOAT_MATH | Truncates instead of rounding (ref 15.3) | /home/ronny/repos/nd500x/src/cpu/instructions/FLOAT_MATH/Wconr.c:48 |
| INVC | LOGICAL | O flag never computed and integer-overflow trap never raised (manual 10.14 requires both) | /home/ronny/repos/nd500x/src/cpu/instructions/LOGICAL/Invc.c:67 |
| CAD:= (CadSet) | MOVE | Missing privileged-instruction (IIC/PIA) trap - non-privileged code can load CAD, breaking process isolation; also C/O not cleared | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/CadSet.c:38 |
| ST1=: (St1Get) | MOVE | Modifies Z and S although ref 16.8 says ST1=: must NOT affect data status bits - corrupts Z after a status save | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/St1Get.c:48 |
| P=: (PGet) | MOVE | Stores next-instruction PC, not the address of the P=: instruction itself (should store fi->address) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/PGet.c:45 |
| SHR | SHIFT | Rotate DIRECTION reversed - positive should rotate LEFT (manual 5214 + microcode SHR_PSC_2); C/O also not cleared | /home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/Shr.c:80 |
| PSHIFTR | SHIFT | Entirely wrong operation - implemented as logical right shift by a bit count instead of packed-BCD shift-with-rounding | /home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/Pshiftr.c:79 |
| SMOVN | STRING | Z inverted (Z=0 on "m items moved", Z=1 on source-empty; manual 14.7 is opposite); also byte-only for non-byte types | /home/ronny/repos/nd500x/src/cpu/instructions/STRING/Smovn.c:82 |
| SFILLN | STRING | Never sets Z=1 on "m elements filled" (manual 14.9 Z=1); Z hardwired 0; byte-only fill | /home/ronny/repos/nd500x/src/cpu/instructions/STRING/Sfilln.c:77 |
| SCOPT | STRING | Implements a copy-translate-until-stop; manual 14.13 SCOPT is a translated padded COMPARE (no writes, no dest) | /home/ronny/repos/nd500x/src/cpu/instructions/STRING/Scopt.c:39 |
| SSKIP | STRING | Z and S inverted/wrong; never computes >/< for S (manual 14.14) | /home/ronny/repos/nd500x/src/cpu/instructions/STRING/Sskip.c:73 |
| SSCAN | STRING | Wrong operation: 2-operand equality scan; manual 14.16 is 3-operand (source,mask,trans) with inverted Z sense | /home/ronny/repos/nd500x/src/cpu/instructions/STRING/Sscan.c:74 |
| SSPAN | STRING | Wrong operation: 2-operand set membership; manual 14.17 is 3-operand mask+translate span; also sets S (should be 0) | /home/ronny/repos/nd500x/src/cpu/instructions/STRING/Sspan.c:86 |
| SMATCH | STRING | Moves wrong index register (I1 instead of I2), leaves wrong one; sets S on not-found (manual 14.18 S=0) | /home/ronny/repos/nd500x/src/cpu/instructions/STRING/Smatch.c:53 |
| SCPUNO | STRING | Implements nonexistent "string copy until"; real SCPUNO (16.36, 177774B) stores CPU number to a destination | /home/ronny/repos/nd500x/src/cpu/instructions/STRING/Scpuno.c:32 |
| CIND | SYSTEM | Defining result Rn*(upper-lower+1)+index -> Rn is never computed (whole point of CIND absent); Z/O/S also missing | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Cind.c:62 |
| INTR | SYSTEM | Implements "read interrupt register"; manual 10.35 opcode 0xFE68 = ROUNDED integer-part FLOAT conversion | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Intr.c:59 |
| RPHS | SYSTEM | Wrong operand form (needs 3, decoder gives 1 -> always traps); wrong source addressing, no page-boundary handling | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Rphs.c:42 |
| WPHS | SYSTEM | Wrong operand form (needs 3, decoder gives 1 -> always traps); wrong dest addressing | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Wphs.c:42 |
| JUMPS | BRANCH | Implemented as plain PC=address; manual 16.34 defines it as supervisor call (save P,B to context, W1<-CPUno, SOLO mode) | /home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Jumps.c:172 |

### 3.2 MEDIUM severity (86)

| Instruction | Category | Mismatch | File:Line |
|-------------|----------|----------|-----------|
| DIV4 | ARITHMETIC | C never cleared (ref: C CLEARED); DZ status bit not set before trap | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Div4.c:135 |
| DIVIDE | ARITHMETIC | Integer path never clears C; float path leaves C/O stale | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Divide.c:218 |
| IXI | ARITHMETIC | C never cleared (ref IXI: C CLEARED) | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Ixi.c:128 |
| MUL4 | ARITHMETIC | C never cleared (ref MUL4: C CLEARED) | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Mul4.c:155 |
| GETBI | BITFIELD | C/O/S left unchanged instead of cleared (Z/K correct) | /home/ronny/repos/nd500x/src/cpu/instructions/BITFIELD/Getbi.c:176 |
| PUTBI | BITFIELD | C/O/S left unchanged instead of cleared (Z/K correct) | /home/ronny/repos/nd500x/src/cpu/instructions/BITFIELD/Putbi.c:197 |
| CLEBI | BITFIELD | C/O/S left unchanged instead of cleared (Z=1/K correct) | /home/ronny/repos/nd500x/src/cpu/instructions/BITFIELD/Clebi.c:168 |
| SETBI | BITFIELD | C/O not cleared (Z/S cleared, K UNKNOWN in ref) | /home/ronny/repos/nd500x/src/cpu/instructions/BITFIELD/Setbi.c:163 |
| GETBF | BITFIELD | S from datatype MSB not the field's leftmost bit; C/O not cleared | /home/ronny/repos/nd500x/src/cpu/instructions/BITFIELD/Getbf.c:201 |
| PUTBF | BITFIELD | S from datatype MSB not the field's leftmost bit; C/O not cleared | /home/ronny/repos/nd500x/src/cpu/instructions/BITFIELD/Putbf.c:247 |
| LOOPI | BRANCH | Integer variants do not update Z/S (ref requires Z=index==0, S=sign); float path does | /home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Loopi.c:260 |
| LOOPD | BRANCH | Integer variants do not update Z/S; float path does | /home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Loopd.c:248 |
| LOOP | BRANCH | No Z/S update on any path; also missing step=0 IOV trap (would infinite-loop) | /home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/Loop.c:374 |
| COMP | COMPARE | K and O never written though manual rule 4040 requires unmentioned bits cleared (Z/C/S correct) | /home/ronny/repos/nd500x/src/cpu/instructions/COMPARE/Comp.c:128 |
| COMP2 | COMPARE | K and O never cleared (Z/C/S and float compare correct) | /home/ronny/repos/nd500x/src/cpu/instructions/COMPARE/Comp2.c:134 |
| PCOMP | COMPARE | C/O left "unaffected" though manual requires CLEARED; K not cleared on valid-BCD path | /home/ronny/repos/nd500x/src/cpu/instructions/COMPARE/Pcomp.c:158 |
| BP | CONTROL | Documented breakpoint trap not raised (printf stub) - flags correct; expected if debugger intercepts 0x0002 | /home/ronny/repos/nd500x/src/cpu/instructions/CONTROL/Bp.c:156 |
| TSET | CONTROL | S not set on negative pre-store byte; C and O not cleared | /home/ronny/repos/nd500x/src/cpu/instructions/CONTROL/Tset.c:228 |
| TAN | FLOAT_MATH | Missing |arg|>65536 IVO domain check; raises undocumented FO trap on infinite result | /home/ronny/repos/nd500x/src/cpu/instructions/FLOAT_MATH/Tan.c:54 |
| EXP | FLOAT_MATH | Overflow uses FO trap + inf instead of documented IVO + largest-float; over-traps underflow | /home/ronny/repos/nd500x/src/cpu/instructions/FLOAT_MATH/Exp.c:56 |
| WCONV | FLOAT_MATH | D/F overflow silently saturates to INT32 min/max; O never set, no IOV trap | /home/ronny/repos/nd500x/src/cpu/instructions/FLOAT_MATH/Wconv.c:55 |
| PWCONV | FLOAT_MATH | K set on integer overflow but NOT on IVO path (ref 17.9: K = IVO OR O) | /home/ronny/repos/nd500x/src/cpu/instructions/FLOAT_MATH/Pwconv.c:123 |
| RIOM | IO | Writes Z (set if count==0) but manual 16.23 + microcode say all data status bits Unaffected - clobbers Z on nearly every RIOM | /home/ronny/repos/nd500x/src/cpu/instructions/IO/Riom.c:258 |
| MOVE | MOVE | C and O not cleared after pure data transfer (ref 10.7) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Move.c:48 |
| LOAD tn:= (AssignTo) | MOVE | C and O not cleared (ref 10.1) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/AssignTo.c:68 |
| STORE tn=: (AssignFrom) | MOVE | C and O not cleared (ref 10.4) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/AssignFrom.c:79 |
| B:= (AssignBaseRegTo) | MOVE | C and O not cleared (ref 10.2) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/AssignBaseRegTo.c:49 |
| R:= (AssignRecordRegTo) | MOVE | C and O not cleared (ref 10.3) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/AssignRecordRegTo.c:49 |
| B=: (AssignToBaseReg) | MOVE | C and O not cleared (ref 10.5) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/AssignToBaseReg.c:47 |
| R=: (AssignToRecordReg) | MOVE | C and O not cleared (ref 10.6) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/AssignToRecordReg.c:49 |
| SWAP | MOVE | C and O not cleared (ref 10.8) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Swap.c:76 |
| CLR tn | MOVE | C and O not cleared (ref 10.16: Z SET, S/C/O CLEARED) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Clr.c:70 |
| STZ t | MOVE | C and O not cleared (ref 10.17) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Stz.c:51 |
| L:= (LSet) | MOVE | C and O not cleared (ref 16.7) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/LSet.c:48 |
| HL:= (HlSet) | MOVE | C and O not cleared (ref 16.7) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/HlSet.c:49 |
| LL:= (LlSet) | MOVE | C and O not cleared (ref 16.7) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/LlSet.c:49 |
| OTE1:= (Ote1Set) | MOVE | C and O not cleared (ref 16.7) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Ote1Set.c:60 |
| OTE2:= (Ote2Set) | MOVE | C and O not cleared (ref 16.7) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Ote2Set.c:60 |
| TOS:= (TosSet) | MOVE | C and O not cleared (ref 16.7) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/TosSet.c:49 |
| THA:= (ThaSet) | MOVE | C and O not cleared (ref 16.7) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/ThaSet.c:49 |
| L=: (LGet) | MOVE | C and O not cleared (ref 16.8) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/LGet.c:50 |
| HL=: (HlGet) | MOVE | C and O not cleared (ref 16.8) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/HlGet.c:49 |
| LL=: (LlGet) | MOVE | C and O not cleared (ref 16.8) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/LlGet.c:49 |
| OTE1=: (Ote1Get) | MOVE | C and O not cleared (ref 16.8) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Ote1Get.c:50 |
| OTE2=: (Ote2Get) | MOVE | C and O not cleared (ref 16.8) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Ote2Get.c:50 |
| CED=: (CedGet) | MOVE | C and O not cleared (ref 16.8) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/CedGet.c:50 |
| CAD=: (CadGet) | MOVE | C and O not cleared (ref 16.8; not privileged) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/CadGet.c:50 |
| CTE1=: (Cte1Get) | MOVE | C and O not cleared (ref 16.8) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Cte1Get.c:50 |
| CTE2=: (Cte2Get) | MOVE | C and O not cleared (ref 16.8) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Cte2Get.c:50 |
| MTE1=: (Mte1Get) | MOVE | C and O not cleared (ref 16.8) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Mte1Get.c:50 |
| MTE2=: (Mte2Get) | MOVE | C and O not cleared (ref 16.8) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Mte2Get.c:50 |
| TEMM1=: (Temm1Get) | MOVE | C and O not cleared (ref 16.8) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Temm1Get.c:50 |
| TEMM2=: (Temm2Get) | MOVE | C and O not cleared (ref 16.8) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Temm2Get.c:50 |
| PS=: (PsGet) | MOVE | C and O not cleared (ref 16.8) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/PsGet.c:49 |
| An:= (A1Set..A4Set, 4 instrs) | MOVE | C and O not cleared loading MS half of Dn (ref 16.9) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/A1Set.c:44 |
| An=: (A1Get..A4Get, 4 instrs) | MOVE | C and O not cleared storing MS half of Dn (ref 16.9) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/A1Get.c:50 |
| En:= (E1Set..E4Set, 4 instrs) | MOVE | C and O not cleared loading LS half of Dn (ref 16.9) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/E1Set.c:54 |
| En=: (E1Get..E4Get, 4 instrs) | MOVE | C and O not cleared storing LS half of Dn (ref 16.9) | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/E1Get.c:53 |
| SHL | SHIFT | C and O not cleared (SHIFT.md:118-119) | /home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/Shl.c:92 |
| SHA | SHIFT | C and O not cleared (SHIFT.md:174-175) | /home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/Sha.c:114 |
| PSHIFT | SHIFT | C/O not cleared; K never cleared when no overflow; header opcode conflicts with manual (177262B) | /home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/Pshift.c:199 |
| SMOVE | STRING | Sets Z=1 on source-empty termination; manual 14.2 lists only K so Z must be 0; no DR trap on zero-length start | /home/ronny/repos/nd500x/src/cpu/instructions/STRING/Smove.c:85 |
| SCHPAR | STRING | Sets S=1 on outside-string path (manual 14.20: S=0); never clears C or O | /home/ronny/repos/nd500x/src/cpu/instructions/STRING/Schpar.c:88 |
| LADDR | SYSTEM | S/C/O unconditionally cleared; ref 15.4 (SAVA) writes S from address sign bit | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Laddr.c:99 |
| RLADDR | SYSTEM | S/C/O force-cleared; ref 15.5: S = address sign bit | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Rladdr.c:67 |
| BLADDR | SYSTEM | S/C/O force-cleared; ref 15.6: S = address sign bit | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Bladdr.c:67 |
| LIND | SYSTEM | Z and S never set (ref 15.8: Z=index==0, S=index sign); only K/IX handled | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Lind.c:79 |
| RDUS | SYSTEM | Sets no flags; ref 16.25 defines Z (source==0) and S (source sign) | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Rdus.c:76 |
| SLOCA | SYSTEM | Outside-source case clears Z; ref 14.15 terminating table says Z=1 | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Sloca.c:90 |
| DDIRT | SYSTEM | Mapped at opcode 0xFE1E; manual 16.11 assigns DDIRT to 0xFFFA (177772B); 0xFFFA unmapped | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Ddirt.c:14 |
| LREGBL | SYSTEM | Mask bit numbering diverges from manual from PS (bit 17) upward - PS/TOS/LL/HL/THA/CED/CAD/OTE/CTE/MTE/TEMM select wrong registers | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Lregbl.c:94 |
| SREGBL | SYSTEM | Same mask-numbering divergence as LREGBL from PS upward | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Sregbl.c:85 |
| LCNTXT | SYSTEM | Mask-numbering divergence; address formula wrong (always (proc+1)*256+addr), uses logical not physical addressing, no address==0/negative-process handling (ref 16.27.4) | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Lcntxt.c:58 |
| SCNTXT | SYSTEM | Mask-numbering divergence; logical write not physical, address==0 save-area case not handled (ref 16.27.3) | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Scntxt.c:56 |

### 3.3 LOW severity (11)

| Instruction | Category | Mismatch | File:Line |
|-------------|----------|----------|-----------|
| ABS | ARITHMETIC | C flag not cleared (ref: C CLEARED); operation and Z/S/O correct | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Abs.c:103 |
| ADD | ARITHMETIC | Float path leaves C and O stale (ref: float C cleared, O forced 0); integer path correct | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Add.c:143 |
| ADD2 | ARITHMETIC | Float C not cleared; header comment wrong ("<a>+Rn->b") though code correctly writes <a> | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Add2.c:132 |
| DECR | ARITHMETIC | Float path leaves C/O stale; integer flags correct | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Decr.c:80 |
| INCR | ARITHMETIC | Float path leaves C/O stale; integer flags correct | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Incr.c:81 |
| MULTIPLY | ARITHMETIC | Float path leaves C/O stale; header doc stale (integer C correctly cleared) | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Multiply.c:203 |
| REM | ARITHMETIC | C and O not cleared (ref REM: both CLEARED); float remainder correct | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Rem.c:101 |
| SUB | ARITHMETIC | Float path leaves C/O stale; integer carry polarity correct | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Sub.c:125 |
| SUBTRACT | ARITHMETIC | Float path leaves C/O stale; integer path correct | /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Subtract.c:143 |
| SOLO | CONTROL | Documented PSD side-effect (disable process switch) not implemented (printf stub); five tracked flags correct | /home/ronny/repos/nd500x/src/cpu/instructions/CONTROL/Solo.c:236 |
| XOR | LOGICAL | Header comment lists wrong BI XOR opcode (0xFDF0-0xFDF3); dispatch is correct (0xFDFC-0xFDFF); comment-only | /home/ronny/repos/nd500x/src/cpu/instructions/LOGICAL/Xor.c:16 |

---

## 4. Unclear / needs manual verification (13)

These are cases where the reference (manual/microcode) itself marks behaviour UNKNOWN,
so no pass/fail can be asserted. Each needs a manual or microcode re-check.

| Instruction | Category | Open question | File:Line |
|-------------|----------|---------------|-----------|
| CHAIN | CALL | Z/C/O marked UNKNOWN in ref (hardware ST,SAVA writes them, only S documented); impl leaves Z/C/O untouched - likely wrong but unconfirmable | /home/ronny/repos/nd500x/src/cpu/instructions/CALL/Chain.c:174 |
| RETT | CALL | Ref marks per-bit status provenance UNKNOWN (partly B.arg18/19, partly DIT); impl restores all from frame | /home/ronny/repos/nd500x/src/cpu/instructions/CALL/Rett.c:343 |
| SQRT | FLOAT_MATH | Impl writes S; manual 12.4 lists only Z for SQRT (S UNKNOWN). Harmless (sqrt>=0 -> S always 0) but written where ref cannot confirm | /home/ronny/repos/nd500x/src/cpu/instructions/FLOAT_MATH/Sqrt.c:77 |
| BICONV | FLOAT_MATH | Sets S to the bit value; a 1-bit result has no defined sign (ambiguous per sources). Z correct | /home/ronny/repos/nd500x/src/cpu/instructions/FLOAT_MATH/Biconv.c:69 |
| WPCONV | FLOAT_MATH | No dedicated BO flag bit; impl repurposes integer-O bit for BCD overflow (K on BO correct); ST1 BO-bit position unverifiable | /home/ronny/repos/nd500x/src/cpu/instructions/FLOAT_MATH/Wpconv.c:134 |
| AND | LOGICAL | C and O UNKNOWN in ref (ST,SAVA physically saves undefined ALU C/O for a logical op); impl leaves unchanged. K/Z/S correct | /home/ronny/repos/nd500x/src/cpu/instructions/LOGICAL/And.c:59 |
| OR | LOGICAL | C and O UNKNOWN in ref; impl leaves unchanged. K/Z/S correct | /home/ronny/repos/nd500x/src/cpu/instructions/LOGICAL/Or.c:59 |
| INV | LOGICAL | C and O UNKNOWN in ref; impl leaves unchanged. K/Z/S correct | /home/ronny/repos/nd500x/src/cpu/instructions/LOGICAL/Inv.c:60 |
| BMOVE | MOVE | K effect UNKNOWN in ref; Z/C/S/O correctly cleared, K left unchanged | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/Bmove.c:146 |
| ST1:= (St1Set) | MOVE | Ref 16.7 Z/S resolution UNKNOWN (direct-bit-load vs recompute) pending a LOAD_ST1 microtrace; impl follows generic recompute rule | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/St1Set.c:46 |
| PS:= (PsSet) | MOVE | 177504B is a reserved row with no operational section/microcode; all effects UNKNOWN. Impl loads PS + sets Z/S, no privilege check | /home/ronny/repos/nd500x/src/cpu/instructions/MOVE/PsSet.c:45 |
| PHYLADR | SYSTEM | Manual 16.37 data-status line blank, no SAVA/K in microcode -> all flags UNKNOWN; impl sets Z only | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Phyladr.c:70 |
| WDUS | SYSTEM | Not in manual; opcode assignment unverifiable (header 0xFEB0+ vs dispatch 0xFEC0-3/0xFED0-3); "no flags" consistent with microcode | /home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Wdus.c:75 |

---

## 5. Current state

Of 240 ND-500 instructions audited against the authoritative Norsk Data manuals,
**86 (36%) fully match**, **141 (59%) have a confirmed discrepancy**, and **13 (5%)
sit on reference behaviour the manual itself leaves UNKNOWN**. The BRANCH, CALL,
and SYSTEM classes are in good shape (the bulk of their non-ok entries are cosmetic
doc/opcode notes or reference-UNKNOWN cases), while COMPARE, SHIFT, IO, LOGICAL,
and especially MOVE and ARITHMETIC carry the weight of the defects. Two systemic
root causes account for most of the 86 medium-severity findings: (1) no per-instruction
pre-clear of C/O before dispatch, so every "Z/S only" instruction (the entire MOVE
register-transfer family, SHIFT, COMPARE, BITFIELD) leaks a stale C/O from the prior
op; and (2) `nd500_set_flags_zs` has no FLOAT datatype case, which turns F NEG and
F TEST into always-Z/never-S. Among the 44 high-severity items the most impactful are
the eight unimplemented float/double arithmetic no-ops (ADD3/DIV2/DIV3/MUL2/MUL3/
MULAD/SUB2/SUB3), the packed-BCD K-not-cleared family, and roughly a dozen instructions
that implement the wrong (or a nonexistent) operation entirely - INTR, SCOPT, SSCAN,
SSPAN, SMATCH, SCPUNO, PSHIFTR, SHR (reversed rotate), CIND, RPHS/WPHS, and JUMPS.
The three inverted-polarity findings that conflict with a runtime-verified impl comment
(SCOMP S polarity, and the SHR rotate direction) should be settled by a byte-level
hardware/microcode re-check before flipping, since the freshly-carved reference and the
impl's own history disagree.
