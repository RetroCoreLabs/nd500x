# ND-500 Instruction Validation Report (FUNCTIONAL)

This is the **functional** validation audit (operation result + status flags + traps).
It **supersedes** the prior flags-only report. Every claim below is anchored to a
source file and line and to the per-category reference documents.

Per-category reference documents live under:
`docs/instruction-reference/`

| Category | Reference |
|----------|-----------|
| ARITHMETIC | `docs/instruction-reference/ARITHMETIC.md` |
| BITFIELD | `docs/instruction-reference/BITFIELD.md` |
| BRANCH | `docs/instruction-reference/BRANCH.md` |
| CALL | `docs/instruction-reference/CALL.md` |
| COMPARE | `docs/instruction-reference/COMPARE.md` |
| CONTROL | `docs/instruction-reference/CONTROL.md` |
| FLOAT_MATH | `docs/instruction-reference/FLOAT_MATH.md` |
| IO | `docs/instruction-reference/IO.md` |
| LOGICAL | `docs/instruction-reference/LOGICAL.md` |
| MOVE | `docs/instruction-reference/MOVE.md` |
| SHIFT | `docs/instruction-reference/SHIFT.md` |
| STRING | `docs/instruction-reference/STRING.md` |
| SYSTEM | `docs/instruction-reference/SYSTEM.md` |

Instruction implementation files are under:
`src/cpu/instructions/<CATEGORY>/`

---

## 1. Summary by category

`ok` = fully correct (result, flags, and traps all match reference).
`disc` = at least one confirmed discrepancy.
`unclear` = cannot be validated against ground truth (reference itself marks the
microcode/behaviour UNKNOWN).

| Category    | Total | OK | Disc | Unclear |
|-------------|------:|---:|-----:|--------:|
| ARITHMETIC  |    38 | 24 |   14 |       0 |
| BITFIELD    |     7 |  1 |    6 |       0 |
| BRANCH      |    21 | 13 |    7 |       1 |
| CALL        |    18 | 14 |    4 |       0 |
| COMPARE     |     5 |  0 |    4 |       1 |
| CONTROL     |     9 |  5 |    4 |       0 |
| FLOAT_MATH  |    25 |  2 |   23 |       0 |
| IO          |     1 |  0 |    1 |       0 |
| LOGICAL     |     5 |  0 |    5 |       0 |
| MOVE        |    39 |  2 |   35 |       2 |
| SHIFT       |     5 |  0 |    4 |       1 |
| STRING      |    18 |  6 |   12 |       0 |
| SYSTEM      |    35 | 10 |   25 |       0 |
| **TOTAL**   | **226** | **77** | **143** | **6** |

(Unclear instructions are counted inside the Disc column of the totals only where the
reference marks the behaviour unverifiable; they are broken out in the Unclear column
and listed in section 5.)

---

## 2. Overall totals split by KIND

The single most important distinction: **does the instruction compute the WRONG
RESULT / take the WRONG CONTROL FLOW (operation-level)**, versus only mis-setting a
**secondary status bit or trap (flags/trap-level)**. Operation-level defects are what
actually corrupt guest computation.

| Defect class | Approx. count | Meaning |
|--------------|--------------:|---------|
| **Operation-level** (wrong result / wrong control flow / wrong operand / wrong register) | **~59** | Guest gets a wrong value or jumps to the wrong place |
| **Flag-level only** (result correct, secondary status bit stale/wrong) | **~70** | Overwhelmingly "C and O not cleared" on move/logical/float ops |
| **Trap-level only** (result & flags correct, trap type or presence wrong) | **~14** | Wrong trap vector (IVO vs IOV/IOS) or missing/extra privilege trap |
| **Unclear** (reference marks ground truth UNKNOWN) | **6** | Cannot validate |

The dominant *systemic* flag defect: handlers that call `nd500_set_flags_zs` /
`nd500_set_flags_zsc` (in
`src/cpu/instructions/instruction_helpers.c:460`) write only
Z/S and **leave C and O holding the previous instruction's values**. The reference
"unlisted-bit" rule (rule 4040 / `ST,SAVA`) requires every data-status bit not named by
the instruction to be CLEARED. There is no central per-instruction flag reset (verified
`src/cpu/cpu.c` clears FLAGS only at reset), so stale C/O survive and can misfire a
later conditional branch.

The dominant *systemic* trap defect: many handlers raise `TRAP_IVO` (Invalid Operation)
via `trap_invalid_operation` where the reference specifies `IOV`/`IOS` (Illegal Operand
Value / Specifier) - a different trap vector.

---

## 3. Prioritized discrepancy list (operation-level first)

### 3A. HIGH severity - WRONG RESULT / WRONG CONTROL FLOW

| Instruction | Category | Kind | Mismatch | File:line |
|-------------|----------|------|----------|-----------|
| ADD3 F/D | ARITHMETIC | operation | Float/Double variant is a `[DEFERRED]` no-op - reads nothing, writes nothing (silent NOP) | `src/cpu/instructions/ARITHMETIC/Add3.c:54` |
| SUB2 F/D | ARITHMETIC | operation | Float/Double variant is a print+return NOP | `src/cpu/instructions/ARITHMETIC/Sub2.c:54` |
| SUB3 F/D | ARITHMETIC | operation | Float/Double variant is a print+return NOP | `src/cpu/instructions/ARITHMETIC/Sub3.c:54` |
| MUL2 F/D | ARITHMETIC | operation | Float/Double variant is a print+return NOP | `src/cpu/instructions/ARITHMETIC/Mul2.c:54` |
| MUL3 F/D | ARITHMETIC | operation | Float/Double variant is a print+return NOP | `src/cpu/instructions/ARITHMETIC/Mul3.c:54` |
| MULAD F/D | ARITHMETIC | operation | Float/Double variant is a print+return NOP | `src/cpu/instructions/ARITHMETIC/Mulad.c:55` |
| DIV2 F/D | ARITHMETIC | operation | Float/Double variant is a print+return NOP (int path also skips DZ flag) | `src/cpu/instructions/ARITHMETIC/Div2.c:55` |
| DIV3 F/D | ARITHMETIC | operation | Float/Double variant is a print+return NOP (int path also skips DZ flag) | `src/cpu/instructions/ARITHMETIC/Div3.c:54` |
| JUMPS | BRANCH | operation | Modeled as plain absolute jump; JUMPS is "call supervisor" - omits P/B context save, W1<-cpuno, SOLO entry | `src/cpu/instructions/BRANCH/Jumps.c:158` |
| IF ST GO | BRANCH | operation | Inverted condition: branches when bit CLEAR, must branch when SET | `src/cpu/instructions/BRANCH/IfStackGo.c:152` |
| IF -ST GO | BRANCH | operation | Inverted condition: branches when bit SET, must branch when CLEAR | `src/cpu/instructions/BRANCH/Ifstgo.c:172` |
| LOOP (step==0) | BRANCH | trap/operation | Missing step==0 -> IOV trap; a zero step produces an INFINITE LOOP | `src/cpu/instructions/BRANCH/Loop.c:303` |
| F TEST | COMPARE | operation | Single-float TEST: Z ALWAYS set, S NEVER set (helper has no FLOAT case) | `src/cpu/instructions/COMPARE/Test.c:54` |
| SET1 F/D | CONTROL | operation | F/D write integer `1` (denormal garbage) instead of 1.0; F also lands in the I bank not A | `src/cpu/instructions/CONTROL/Set1.c:61` |
| BP | CONTROL | trap | Stub: only prints, never raises BPT/IIC - the instruction's sole effect is absent | `src/cpu/instructions/CONTROL/Bp.c:156` |
| BYCONR | FLOAT_MATH | operation | "Rounded" convert actually TRUNCATES toward zero (2.6->2) - the whole point of *CONR | `src/cpu/instructions/FLOAT_MATH/Byconr.c:47` |
| HCONR | FLOAT_MATH | operation | Rounded convert truncates instead of round-to-nearest | `src/cpu/instructions/FLOAT_MATH/Hconr.c:47` |
| WCONR | FLOAT_MATH | operation | Rounded convert truncates; float>2^31 no range check/IOV | `src/cpu/instructions/FLOAT_MATH/Wconr.c:48` |
| ~~RIOM~~ | IO | operand | **RESOLVED (2026-07-20)** - dest buffer now uses `fi->operands[1].effective_address`; ND-100 source read as full W (`ND500_DTYPE_WORD`); privileged check via `nd500_require_privilege()` (IIC); and the bogus Z-on-count==0 removed (manual sec 16.23: "Data status bits: Unaffected"). Covered by `test/test_riom.c`. | `src/cpu/instructions/IO/Riom.c:177,186,191` |
| OR (BI) | LOGICAL | operation | BI datatype: upper 31 bits not zero-filled -> wrong register result | `src/cpu/instructions/LOGICAL/Or.c:53` |
| XOR (BI) | LOGICAL | operation | BI datatype: upper 31 bits not zero-filled -> wrong register result | `src/cpu/instructions/LOGICAL/Xor.c:53` |
| INV (BI) | LOGICAL | operation | BI datatype: full 32-bit complement, upper bits not cleared | `src/cpu/instructions/LOGICAL/Inv.c:54` |
| E1Set..E4Set | MOVE | operation | `en:=` writes A (low 32) instead of En - functionally identical to `an:=` | `src/cpu/instructions/MOVE/E1Set.c:48` |
| E1Get..E4Get | MOVE | operation | `en=:` reads A (low 32) instead of En | `src/cpu/instructions/MOVE/E1Get.c:46` |
| SHR (rotate) | SHIFT | operation | Rotate direction REVERSED vs microcode+manual (pos should rotate LEFT, impl rotates RIGHT) | `src/cpu/instructions/SHIFT/Shr.c:80` |
| PSHIFTR | SHIFT | operation | Wrong instruction: does a binary logical right shift; real op is packed-DECIMAL rounded rescale | `src/cpu/instructions/SHIFT/Pshiftr.c:79` |
| SCPUNO | STRING | operation | Wrong instruction entirely: implements "copy-until" instead of Store CPU Number | `src/cpu/instructions/STRING/Scpuno.c:32` |
| SCOPT | STRING | operation | Wrong op: "copy-translate-until-stop" instead of String Compare Translated With Pad | `src/cpu/instructions/STRING/Scopt.c:27` |
| SSCAN | STRING | operation | Wrong op & operand count: plain equal-byte scan vs masked+translated; Z inverted | `src/cpu/instructions/STRING/Sscan.c:36` |
| SSPAN | STRING | operation | Wrong op & operand count: char-set membership vs masked+translated span | `src/cpu/instructions/STRING/Sspan.c:34` |
| SSKIP | STRING | flags | Z inverted (primary result flag); S greater/less never computed | `src/cpu/instructions/STRING/Sskip.c:73` |
| SMATCH | STRING | operation | Operand roles swapped; updates wrong index register (I1 vs I2) | `src/cpu/instructions/STRING/Smatch.c:52` |
| SMOVN | STRING | operation | Byte-only copy ignores datatype (H/W/F/D/BI move wrong size); Z termination wrong | `src/cpu/instructions/STRING/Smovn.c:61` |
| SFILLN | STRING | operation | Byte-only fill ignores element size; Z never set on m-filled | `src/cpu/instructions/STRING/Sfilln.c:59` |
| SMVWH | STRING | flags | Condition-broken case sets Z=0; reference requires Z=1 | `src/cpu/instructions/STRING/Smvwh.c:87` |
| INTR | SYSTEM | operation | Wrong instruction: reads a phantom "interrupt register" returning 0 instead of round(x) into Fn/Dn | `src/cpu/instructions/SYSTEM/Intr.c:34` |
| CIND | SYSTEM | operation | Omits the entire index computation (In := In*range+index); only does bounds check | `src/cpu/instructions/SYSTEM/Cind.c:42` |

### 3B. MEDIUM severity - operation / operand defects

| Instruction | Category | Kind | Mismatch | File:line |
|-------------|----------|------|----------|-----------|
| DIV4 | ARITHMETIC | both | Result correct; DZ flag not set before trap; overflow raises wrong trap type | `src/cpu/instructions/ARITHMETIC/Div4.c:63` |
| UDIV | ARITHMETIC | both | Result correct; DZ flag not set before trap | `src/cpu/instructions/ARITHMETIC/Udiv.c:53` |
| GETBF | BITFIELD | both | Register operand widens range to 32 (skips IOV, reads beyond datatype); S uses data-type MSB not field MSB | `src/cpu/instructions/BITFIELD/Getbf.c:167` |
| ENTT | CALL | operation | Register-block save correct; B.AUX and L differ from reference in minor fields | `src/cpu/instructions/CALL/Entt.c:284` |
| TSET | CONTROL | both | Missing S (sign of old value); C/O not cleared; no IOS trap for register/constant operands | `src/cpu/instructions/CONTROL/Tset.c:222` |
| SOLO | CONTROL | operation | Stub: does not set the PSD status bit (no observable effect today, single-threaded) | `src/cpu/instructions/CONTROL/Solo.c:236` |
| TAN | FLOAT_MATH | both | Missing \|arg\|>65536 range guard (IVO); large args give garbage instead of 0.0 | `src/cpu/instructions/FLOAT_MATH/Tan.c:54` |
| EXP | FLOAT_MATH | both | Overflow detected at IEEE isinf (~e^709) not ND ~e^176.75; misses FO, mis-clamps | `src/cpu/instructions/FLOAT_MATH/Exp.c:57` |
| POLY | FLOAT_MATH | both | On overflow forces result 0.0 (wrong value + wrong Z); no IOS check on m | `src/cpu/instructions/FLOAT_MATH/Poly.c:112` |
| WCONV | FLOAT_MATH | both | Float/double out-of-range silently clamps, no IOV trap, no O flag | `src/cpu/instructions/FLOAT_MATH/Wconv.c:54` |
| HCONV | FLOAT_MATH | both | On overflow truncated result NOT written (ref says it is); O not set | `src/cpu/instructions/FLOAT_MATH/Hconv.c:74` |
| WPCONV | FLOAT_MATH | both | BCD overflow sets integer-O (bit 9) instead of BO (bit 15); no TRAP_BO | `src/cpu/instructions/FLOAT_MATH/Wpconv.c:132` |
| PGet | MOVE | operation | Stores already-advanced PC (next instruction) instead of the P=: instruction address | `src/cpu/instructions/MOVE/PGet.c:45` |
| St1Set | MOVE | both | No loadable-bit mask; overwrites just-loaded Z/S by recomputing them | `src/cpu/instructions/MOVE/St1Set.c:46` |
| RPHS | SYSTEM | both | Wrong operand/register contract; omits physical-segment addressing + page-boundary stop | `src/cpu/instructions/SYSTEM/Rphs.c:54` |
| WPHS | SYSTEM | both | Same contract/page-boundary omissions as RPHS (write direction) | `src/cpu/instructions/SYSTEM/Wphs.c:54` |
| LREGBL | SYSTEM | operand | reg_num*4 with 1-based reg_num vs 0-based (P=0) ref table -> every register read 4 bytes off | `src/cpu/instructions/SYSTEM/Lregbl.c:72` |
| SREGBL | SYSTEM | operand | Same off-by-one-slot store offset; plus UB shift for reg_num 33..37 | `src/cpu/instructions/SYSTEM/Sregbl.c:62` |
| LCNTXT | SYSTEM | both | Ignores address!=0 rule; packed layout vs base+regnum*4; logical not physical memory | `src/cpu/instructions/SYSTEM/Lcntxt.c:58` |
| SCNTXT | SYSTEM | both | Packed layout vs base+regnum*4; logical not physical; no context-area fallback | `src/cpu/instructions/SYSTEM/Scntxt.c:56` |
| SLOCA | SYSTEM | both | Z=0 on outside-source/not-found (ref wants Z=1); no DR trap | `src/cpu/instructions/SYSTEM/Sloca.c:90` |
| DDIRT | SYSTEM | operation | Decoded at 0xFE1E; manual gives octal 177772 = 0xFFFA (wrong dispatch slot) | `src/cpu/instructions/SYSTEM/Ddirt.c:14` |
| LIND | SYSTEM | both | Z/S not set from index; no Illegal-Index (IX) trap on out-of-range (only status bit) | `src/cpu/instructions/SYSTEM/Lind.c:74` |
| SMOVE | STRING | both | Forward-only copy (overlap corrupts); Z set where ref clears | `src/cpu/instructions/STRING/Smove.c:67` |
| SMVTR | STRING | operation | Forward-only translate-move (overlap); table-operand ILL_OP_SPEC not modelled | `src/cpu/instructions/STRING/Smvtr.c:68` |
| SFILL | STRING | operand | Fill value always from integer reg & 32-bit; F/D variants use wrong source, truncate | `src/cpu/instructions/STRING/Sfill.c:56` |

### 3C. MEDIUM/LOW - flag-level only (result correct)

These do not corrupt the computed value; they leave a secondary status bit wrong.
The overwhelming majority are the **"C and O not cleared"** systemic issue.

- ARITHMETIC (secondary flags): Add, Subtract, Sub, Multiply, Abs, Addc, Subc, Psum, Rem
  - float paths do not clear C/O and do not set FU/FO (`Add.c:117`, `Subtract.c:143`, `Multiply.c:203`, etc.).
- BITFIELD: GETBI, PUTBI, SETBI, CLEBI, PUTBF - operation OK, but C/O/S not cleared and range trap raises IVO instead of IOV (`Getbi.c:176`, `Putbi.c:197`, `Setbi.c:163`, `Clebi.c:168`, `Putbf.c:247`).
- BRANCH: LOOPI, LOOPD, LOOP(flags) - integer path sets no Z/S and does not clear C/O from the modified index (`Loopi.c:260`, `Loopd.c:248`, `Loop.c:374`).
- COMPARE: COMP, COMP2 (O/K never cleared; COMP2 float wrongly SETS C), SCOMP header doc inverted (`Comp.c:96`, `Comp2.c:67`, `Scomp.c:48`).
- FLOAT_MATH: Biconv (S wrongly set), Byconv (O not set on overflow), and the trig/log/sqrt/conv family (Sin, Cos, Asin, Acos, Atan, Atan2, Alog, Alog2, Alog10, Sqrt, Fconv, Dconv) all leave C/O unaffected instead of cleared (`Sin.c:103` and siblings).
- LOGICAL: AND (C/O not cleared), INVC (O never set, integer-overflow trap never raised) (`And.c:59`, `Invc.c:67`).
- MOVE: the entire assign/register-transfer family (Move, Swap, Stz, Clr, AssignTo/From, A1..A4, L, HL, LL, B, R, OTE, TOS, THA, CAD/CED get, CTE, MTE, TEMM, PS get, St1Get) - result and Z/S correct, C and O not cleared (`Move.c:48` and ~30 siblings). St1Get additionally must NOT touch Z/S at all.
- SHIFT: SHL, SHA - result correct, K/C/O not cleared per rule 4040 (`Shl.c:92`, `Sha.c:114`).
- SYSTEM (flag/low): Phyladr (must clear K, doesn't; sets Z instead), Rdus/Wdus (no Z/S from value), Laddr/Rladdr/Bladdr (S cleared instead of from bit31; constant operand not rejected), Zpgu/Zwip (K,1IFZ not modelled), Rpgu/Rwip (stub returns 0), Svers (version constant differs), Dcc/Pcc/Tutti (extra IIC privilege trap not in reference).

### 3D. Trap-type-only discrepancies (result & flags correct)

- ADD2 - integer overflow raises `trap_invalid_operation` unconditionally (wrong type + ignores trap-enable); float overflow trapped twice (`Add2.c:202`).
- MUL4, UMUL - overflow raises IVO instead of Integer-Overflow trap, unconditionally (`Mul4.c:177`, `Umul.c:93`).
- AXI - 0**negative sets K and continues instead of raising IVO trap; corrupts architectural K bit (`Axi.c:142`).
- CALL, CALLG - argument IOS check rejects only constants, not register operands (`Call.c:83`, `Callg.c:217`).
- CHAIN - raises IOS where reference requires IOV (`Chain.c:232`).
- SCOMP - missing Descriptor-Range (DR) trap when a pointer starts outside its string (`Scomp.c:113`).

---

## 4. Notable correct implementations (spot-check)

Categories with strong functional correctness: **CALL** (14/18 - all ENT*/RET* frame
building matches), **BRANCH** conditional IF..GO family (13/21), **ARITHMETIC** integer
paths and packed-BCD add/sub/mul (24/38), **SYSTEM** MMU on/off + cache/TSB no-ops
(10/35 including DMON/PMON/DMOF/PMOF/PCTSB/DCTSB/CPGU/CWIP/INT/FREEB).

---

## 5. Unclear / needs deeper microtrace

These cannot be confirmed or refuted because the reference itself marks the ground-truth
microcode/behaviour UNKNOWN:

| Instruction | Category | Why unclear | File:line |
|-------------|----------|-------------|-----------|
| JUMPG | BRANCH | ALT->IOS trap and descriptor-range fall-through deferred to operand decoder, unverified from this file | `src/cpu/instructions/BRANCH/Jumpg.c:157` |
| PCOMP (precision/scale) | COMPARE | BCD compared via double subtraction; >2^53 precision loss and decimal-scale alignment not verifiable | `src/cpu/instructions/COMPARE/Pcomp.c:140` |
| CadSet (`cad :=`) | MOVE | Bare assign; ADOM/capability install missing; ref flag effect after domain-install UNKNOWN | `src/cpu/instructions/MOVE/CadSet.c:46` |
| PsSet (`ps :=`) | MOVE | Opcode 0xFF44 UNRESOLVED in reference - no located microcode handler | `src/cpu/instructions/MOVE/PsSet.c:46` |
| PSHIFT | SHIFT | Approach matches manual intent, but exact microcode routine + sign-per-dest-bit-26 UNKNOWN | `src/cpu/instructions/SHIFT/Pshift.c:161` |
| RIOM (IOV thresholds) | IO | The specific 16-bit/22-bit IOV range thresholds are inferred/UNKNOWN in the reference | `src/cpu/instructions/IO/Riom.c:182` |

---

## 6. Current state - conclusion

**Functional correctness is much stronger than flag correctness, but there is a hard
core of wrong-result instructions that must be fixed before the emulator can be trusted
for real workloads.**

- **77 of 226 instructions (34%) are fully correct** (result + flags + traps).
- **~59 instructions have operation-level defects** - they produce a wrong value, take
  the wrong branch, touch the wrong register, or dereference the wrong operand. The most
  dangerous cluster:
  - **All F/D variants of ADD3/SUB2/SUB3/MUL2/MUL3/MULAD/DIV2/DIV3 are silent NOPs** -
    floating-point three-address and two-address arithmetic simply does not execute.
  - **STRING is a minefield**: SCPUNO, SCOPT, SSCAN, SSPAN, SMATCH implement a
    *different instruction* than their opcode; SMOVN/SFILLN ignore datatype.
  - **Convert-rounded (BYCONR/HCONR/WCONR) truncate** - the rounding is entirely absent.
  - **Inverted control flow** in IF ST GO / IF -ST GO, and **an infinite loop** on
    `LOOP` with step==0.
  - **SHR rotates the wrong way**; **PSHIFTR is the wrong algorithm**; **INTR/CIND/SET1
    F,D** produce wrong values.
  - **`en:=` / `en=:` never touch the E registers** (alias to A) - a pervasive latent
    bug for any double-precision code using the E half.
- **~70 instructions have flag-level-only defects**, and these are dominated by one
  systemic root cause: `nd500_set_flags_zs`/`_zsc` never clear C and O, and there is no
  central per-instruction status reset. A single fix (clear C/O in the shared flag
  helpers, or a dispatch-loop pre-clear of unlisted bits) would resolve the bulk of the
  MOVE, LOGICAL, SHIFT, FLOAT_MATH, and COMPARE flag findings at once.
- **~14 trap-type-only defects**, mostly `TRAP_IVO` raised where the reference wants
  `IOV`/`IOS`, plus a few unconditional-trap-on-overflow issues.
- **6 instructions cannot be validated** and need deeper microtrace of the ND-500
  microcode.

**Recommended fix order:** (1) the operation-level HIGH cluster in section 3A -
especially the float-arithmetic NOPs and the STRING wrong-instructions, because they
silently corrupt data; (2) the shared-helper C/O-clear fix, which retires most of
section 3C in one change; (3) the trap-vector corrections in 3D; (4) resolve the section
5 unclear items against the microcode.
