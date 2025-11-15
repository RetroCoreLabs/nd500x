# ND500 COMPREHENSIVE TEST COVERAGE - MASTER PLAN

**Project:** Complete test coverage for all 241 ND500 instruction families
**Goal:** Generate valid assembly and JSON tests for 100% of instruction variants
**Last Updated:** 2025-11-10 13:45:00
**Current Coverage:** 140/241 families (58.1%), 5,121 tests generated

---

## TABLE OF CONTENTS

1. [Current Status](#current-status)
2. [Architecture Overview](#architecture-overview)
3. [Completed Phases](#completed-phases)
4. [Remaining Work](#remaining-work)
5. [Phase-by-Phase Implementation Plan](#phase-by-phase-implementation-plan)
6. [File Locations](#file-locations)
7. [Assembly Documentation Reference](#assembly-documentation-reference)
8. [Testing and Validation](#testing-and-validation)
9. [Known Issues and Blockers](#known-issues-and-blockers)
10. [CONTINUATION PROMPT](#continuation-prompt)

---

## CURRENT STATUS

### Coverage Metrics
- **Total instruction families:** 241
- **Families generating tests:** 140 (58.1%)
- **Families blocked:** 101 (41.9%)
- **Total tests generated:** 5,121
- **Assembly documentation:** 150/150 families (100% - all needed docs exist!)

### Critical Finding
**Assembly documentation is 100% COMPLETE!** All blocked families have documentation.
**The blocker is missing generator patterns in the C# test generator code.**

### Breakdown by Status

#### ✅ WORKING (140 families)
- MOVE: move, swap, clr (3/6 families)
- BITFIELD: getbi, putbi, clebi, setbi, getbf, putbf (6/7 families)
- ARITHMETIC: neg, abs, incr, decr, +, -, *, /, add2, sub2, mul2, div2, add3, sub3, mul3, div3, mul4, div4, umul, udiv, addc, subc, mulad, psum, ixi (25/37 families)
- LOGICAL: and, or, xor, inv, invc (5/5 families - 100%)
- COMPARE: comp, comp2, test, pcomp, scomp (5/5 families - 100%)
- SHIFT: shl, shr, sha, pshift, pshiftr (5/5 families - 100%)
- BRANCH: go, ifkgo, jumps (3/8 families)
- CALL: ret, retk, retd, rett, retb, retbk, ifkret (7/18 families)
- CONTROL: noop, setk, bp, solo (4/9 families)
- STRING: scpuno (1/18 families)
- SYSTEM: ddirt, dcc, pcc, dmon, pmon, dmof, pmof, dctsb, pctsb, cpgu, tutti (11/35 families)

#### ⚠️ BLOCKED (101 families)
- ARITHMETIC: 12 families (rem, axi, padd, paddr, pmpy, pmpyr, ppack, ppackr, psub, psubr, pupack, pupackr)
- STRING: 17 families (smove, smovn, smvtr, smvtu, smvun, smvwh, sfill, sfilln, scotr, scopa, scopt, sskip, sscan, sspan, smatch, sspar, schpar)
- SYSTEM: 24 families (int, intr, laddr, rladdr, bladdr, lind, cind, freeb, sloca, rwip, zwip, rpgu, zpgu, svers, phyladr, lregbl, sregbl, lcntxt, scntxt, rdus, wdus, rphs, wphs, cwip)
- CALL: 11 families (call, callg, chain, entm, entd, ents, entsn, entf, entfn, entt, entb)
- BRANCH: 5 families (jumpg, loop, loopi, loopd, ifstgo)
- CONTROL: 5 families (set1, sete, clte, tset, init)
- MOVE: 3 families (stz, bmove, clrk)
- BITFIELD: 1 family (getb)
- IO: 1 family (riom)

---

## ARCHITECTURE OVERVIEW

### Test Generation Pipeline

```
instructions.json
       ↓
TestND500_GenerateComprehensiveTests.cs
       ↓
   (For each instruction family)
       ↓
GenerateComprehensiveTestsForInstruction()
       ↓
   (For each variant)
       ↓
Generate{N}OperandAssembly() methods
       ↓
ND500Assembler.Assemble()
       ↓
JSON test file created
       ↓
TestND500_ExecuteComprehensiveTests.cs
       ↓
Unit tests execute all JSON tests
```

### Key Files

**Test Generator:**
- `E:\Dev\Repos\Ronny\RetroCore\Emulated.Tests.ND500\Instructions\TestND500_GenerateComprehensiveTests.cs`
  - Main entry point: `Generate_Comprehensive_Tests_For_All_Instructions()`
  - Assembly generation methods: `GenerateSingleOperandAssembly()`, `GenerateTwoOperandAssembly()`, `GenerateThreeOperandAssembly()`
  - **MISSING:** `GenerateFourOperandAssembly()` - needed for loop, smvun, smvwh, scopt

**Test Executor:**
- `E:\Dev\Repos\Ronny\RetroCore\Emulated.Tests.ND500\Instructions\TestND500_ExecuteComprehensiveTests.cs`
  - Executes all JSON tests

**Metadata:**
- `\\wsl.localhost\Ubuntu\home\ronny\repos\nd500x\docs\instructions\instructions.json`
  - Source of truth for all 1078 instruction variants (241 families)

**Assembly Documentation:**
- `\\wsl.localhost\Ubuntu\home\ronny\repos\nd500x\docs\instructions\asm\*.md`
  - 150 instruction families documented (100% of what test generator needs)

**Generated Tests:**
- `E:\Dev\Repos\Ronny\RetroCore\Emulated.Tests.ND500\bin\Debug\net9.0\Instructions\JsonTests\Comprehensive\*.json`

---

## COMPLETED PHASES

### Phase 0: Infrastructure ✅ COMPLETE
**Completed:** Before this session
**Achievement:** Test generation framework fully operational

- ✅ Metadata-driven test generator framework
- ✅ JSON test file generation
- ✅ Comprehensive addressing mode coverage
- ✅ Unit test executor for all JSON tests
- ✅ 100% assembly documentation available

### Phase 1: MOVE/BITFIELD Operations ✅ COMPLETE
**Completed:** 2025-11-10
**Achievement:** +8 families, 125/241 (51.9% coverage)

**What was fixed:**
1. **MOVE operations (2 families):**
   - `move`: Fixed to use PREFIX notation `W MOVE I1, I2`
   - `swap`: Fixed to use PREFIX notation `W SWAP I1, I2`
   - Applied fix across all addressing modes (CONSTANT, REGISTER, ABSOLUTE, LOCAL, RECORD)
   - File: `TestND500_GenerateComprehensiveTests.cs` lines 656-659, 463-464, 522, 560

2. **BITFIELD 2-operand operations (4 families):**
   - `getbi`, `putbi`, `clebi`, `setbi`: Fixed to use register-embedded prefix
   - Pattern: `W1 GETBI $value, $bitpos` (not `w1 getbi w1, $bitpos`)
   - Applied fix across CONSTANT (line 645), REGISTER (line 694), ABSOLUTE (line 470), LOCAL (line 739), RECORD (line 754)
   - File: `TestND500_GenerateComprehensiveTests.cs`

3. **BITFIELD 3-operand operations (2 families):**
   - `getbf`, `putbf`: Already working with correct pattern
   - Pattern: `W1 GETBF <operand>, <bit#>, <size>`

**Tests generated:**
- MOVE_move.json: 81 tests
- MOVE_swap.json: 81 tests
- BITFIELD_getbi.json: 108 tests
- BITFIELD_putbi.json: 81 tests
- BITFIELD_clebi.json: 81 tests
- BITFIELD_setbi.json: 81 tests
- BITFIELD_getbf.json: 135 tests (already working)
- BITFIELD_putbf.json: 108 tests (already working)

**Key Learning:**
- PREFIX notation: `DT MNEMONIC operands` (e.g., `W MOVE I1, I2`)
- Register-embedded prefix: `DTN MNEMONIC` (e.g., `W1 GETBI`)
- Packed operations: No prefix (e.g., `PSHIFT B.0, B.4`)

### Phase 2: ARITHMETIC 2-Operand Operations ✅ COMPLETE
**Completed:** 2025-11-10 12:30:00
**Achievement:** +9 families, 132/241 (54.8% coverage), +832 tests

**What was fixed:**
1. **2-operand arithmetic (9 families working, 1 blocked):**
   - ✅ `add2`, `sub2`, `mul2`, `div2`: Basic arithmetic with PREFIX notation `W ADD2 I1, I2`
   - ✅ `addc`, `subc`: Arithmetic with carry, PREFIX notation
   - ✅ `mulad`, `psum`, `ixi`: Extended arithmetic operations
   - ⚠️ `axi`: Blocked (F/D types only - metadata issue)
   - Applied fix across all addressing modes (CONSTANT, REGISTER, ABSOLUTE, LOCAL, RECORD)
   - File: `TestND500_GenerateComprehensiveTests.cs` lines 623-632, 658-674, 708-720, 736-748, 769-779

**Tests generated:**
- ARITHMETIC_add2.json: 108 tests
- ARITHMETIC_sub2.json: 108 tests
- ARITHMETIC_mul2.json: 108 tests
- ARITHMETIC_div2.json: 108 tests
- ARITHMETIC_addc.json: 36 tests
- ARITHMETIC_subc.json: 36 tests
- ARITHMETIC_mulad.json: 135 tests
- ARITHMETIC_psum.json: 135 tests
- ARITHMETIC_ixi.json: 130 tests

**Key Pattern:**
- PREFIX notation for 2-operand: `{DT} {MNEMONIC} I1, I2`
- Example: `W ADD2 I1, I2` (destination I1, source I2)

### Phase 3: ARITHMETIC 3-Operand Operations ✅ COMPLETE
**Completed:** 2025-11-10 13:45:00
**Achievement:** +8 families, 140/241 (58.1% coverage), +810 tests

**What was fixed:**
1. **Regular 3-operand arithmetic (5 families):**
   - ✅ `add3`, `sub3`, `mul3`, `div3`: Non-destructive arithmetic with PREFIX notation `W ADD3 I1, I2, I3`
   - ⚠️ `rem`: Blocked (F/D/R types only - unusual metadata)
   - Pattern: Result in I3, sources in I1 and I2
   - File: `TestND500_GenerateComprehensiveTests.cs` lines 820-838

2. **Register-embedded 4-operand arithmetic (2 families):**
   - ✅ `mul4`, `div4`: Full double-length results, upper half in register Rn
   - Pattern: `W1 MUL4 I1, I2, I3` (lower half in I3, upper half in R1)
   - File: `TestND500_GenerateComprehensiveTests.cs` lines 840-858

3. **Unsigned register-embedded arithmetic (2 families):**
   - ✅ `umul`, `udiv`: Unsigned operations, W type only
   - Pattern: `W1 UMUL I1, I2, I3` (unsigned multiply with upper half in R1)
   - File: `TestND500_GenerateComprehensiveTests.cs` lines 860-878

**Tests generated:**
- ARITHMETIC_add3.json: 135 tests
- ARITHMETIC_sub3.json: 135 tests
- ARITHMETIC_mul3.json: 135 tests
- ARITHMETIC_div3.json: 90 tests
- ARITHMETIC_mul4.json: 135 tests
- ARITHMETIC_div4.json: 90 tests
- ARITHMETIC_umul.json: 45 tests
- ARITHMETIC_udiv.json: 45 tests

**Key Patterns:**
- Regular 3-operand: `{DT} {MNEMONIC} I1, I2, I3` (non-destructive)
- Register-embedded: `{DT}{N} {MNEMONIC} I1, I2, I3` (result split: I3 + Rn)
- Unsigned: `W{N} {MNEMONIC} I1, I2, I3` (word type only)

---

## REMAINING WORK

### By Priority

#### 🔴 HIGH PRIORITY (Phase 4-7): 24 families, ~2-3 weeks
Essential operations for core instruction coverage

**Phase 2:** ✅ COMPLETE - ARITHMETIC 2-operand (9/10 families) - 132/241 (54.8%)
**Phase 3:** ✅ COMPLETE - ARITHMETIC 3-operand (8/9 families) - 140/241 (58.1%)
**Phase 4:** CALL operations (11 families) - 1 day
**Phase 5:** BRANCH loops (5 families) - 4-6 hours
**Phase 6:** CONTROL operations (5 families) - 4-6 hours
**Phase 7:** MOVE remaining (3 families) - 2-3 hours

#### 🟡 MEDIUM PRIORITY (Phase 8-9): 50 families, ~3-4 weeks
Extended operations and string processing

**Phase 8:** STRING operations (17 families) - 1-2 days
**Phase 9:** Extended ARITHMETIC (10 families) - 2-3 days
**Phase 10:** Packed ARITHMETIC (10 families) - 1-2 days

#### 🟢 LOW PRIORITY (Phase 11): 24 families, ~2-3 weeks
System/supervisor mode instructions

**Phase 11:** SYSTEM operations (24 families) - 2-3 days

---

## PHASE-BY-PHASE IMPLEMENTATION PLAN

### Phase 2: ARITHMETIC 2-Operand Operations ✅ COMPLETE

**Status:** ✅ COMPLETE (2025-11-10 12:30:00)
**Achievement:** +9 families, 132/241 (54.8% coverage), +832 tests
**Details:** See [Completed Phases](#completed-phases) section above

---

### Phase 3: ARITHMETIC 3-Operand Operations ✅ COMPLETE

**Status:** ✅ COMPLETE (2025-11-10 13:45:00)
**Achievement:** +8 families, 140/241 (58.1% coverage), +810 tests
**Details:** See [Completed Phases](#completed-phases) section above

---

### Phase 4: CALL Operations 🔴 NEXT

**Status:** NOT STARTED
**Priority:** HIGH
**Estimated Time:** 1 day
**Target Coverage:** 151/241 (62.7%)

#### Families to Implement (11)
1. `call` (2 operands)
2. `callg` (2 operands)
3. `chain` (3 operands)
4. `entm` (3 operands)
5. `entd` (1 operand)
6. `ents` (1 operand)
7. `entsn` (2 operands)
8. `entf` (1 operand)
9. `entfn` (2 operands)
10. `entt` (2 operands)
11. `entb` (1 operand)

#### Research Required
Read call.md, callg.md, chain.md, entm.md, entd.md, ents.md, entsn.md, entf.md, entfn.md, entt.md, entb.md

Determine:
- Assembly format for CALL instructions
- How operands are specified
- Whether they use PREFIX notation or special syntax

#### Implementation Location
- 1-operand ENT: `GenerateSingleOperandAssembly()`
- 2-operand CALL/ENT: `GenerateTwoOperandAssembly()`
- 3-operand: `GenerateThreeOperandAssembly()`

---

### Phase 5: BRANCH Loop Operations 🔴

**Status:** NOT STARTED
**Priority:** HIGH
**Estimated Time:** 4-6 hours
**Target Coverage:** 160/241 (66.4%)

#### Families to Implement (5)
1. `jumpg` (1 operand) - doc: `jumpg.md`
2. `loop` (4 operands) - doc: `loop.md`
3. `loopi` (3 operands) - doc: `loopi.md`
4. `loopd` (3 operands) - doc: `loopd.md`
5. `ifstgo` (2 operands) - doc: `ifstgo.md`

#### Special Notes
- `loop` requires 4 operands - need to create `GenerateFourOperandAssembly()` method
- Branch instructions may use NONE addressing mode
- Check if displacement is embedded in opcode

---

### Phase 6: CONTROL Operations 🔴

**Status:** NOT STARTED
**Priority:** HIGH
**Estimated Time:** 4-6 hours
**Target Coverage:** 165/241 (68.5%)

#### Families to Implement (5)
1. `set1` (1 operand) - doc: `set1.md`
2. `sete` (1 operand) - doc: `sete.md`
3. `clte` (1 operand) - doc: `clte.md`
4. `tset` (1 operand) - doc: `tset.md`
5. `init` (3 operands) - doc: `init.md`

---

### Phase 7: MOVE Remaining 🔴

**Status:** NOT STARTED
**Priority:** HIGH
**Estimated Time:** 2-3 hours
**Target Coverage:** 168/241 (69.7%)

#### Families to Implement (3)
1. `stz` (1 operand) - doc: `stz.md` - Store Zero
2. `bmove` (3 operands) - doc: `bmove.md` - Block Move
3. `clrk` (1 operand) - doc: `clrk.md` - Clear something

---

### Phase 8: STRING Operations 🟡

**Status:** NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 1-2 days
**Target Coverage:** 185/241 (76.8%)

#### Families to Implement (17)
All have docs in asm/ folder. Need to research syntax pattern.

Common pattern likely: `SMOVE count` or `SMOVE src, dest`

1. `smove` (2 operands)
2. `smovn` (3 operands)
3. `smvtr` (3 operands)
4. `smvtu` (3 operands)
5. `smvun` (4 operands)
6. `smvwh` (4 operands)
7. `sfill` (1 operand)
8. `sfilln` (2 operands)
9. `scotr` (3 operands)
10. `scopa` (3 operands)
11. `scopt` (4 operands)
12. `sskip` (2 operands)
13. `sscan` (3 operands)
14. `sspan` (3 operands)
15. `smatch` (2 operands)
16. `sspar` (2 operands)
17. `schpar` (2 operands)

---

### Phase 9: Extended ARITHMETIC 🟡

**Status:** NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 2-3 days
**Target Coverage:** 195/241 (80.9%)

#### Packed BCD Operations (10 families)
All 3-operand packed operations. Documentation exists for all.

1. `padd` (3 operands)
2. `paddr` (3 operands)
3. `pmpy` (3 operands)
4. `pmpyr` (3 operands)
5. `psub` (3 operands)
6. `psubr` (3 operands)
7. `ppack` (2 operands)
8. `ppackr` (2 operands)
9. `pupack` (2 operands)
10. `pupackr` (2 operands)

---

### Phase 10: BITFIELD/IO Remaining 🟡

**Status:** NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 2-3 hours
**Target Coverage:** 197/241 (81.7%)

#### Families (2)
1. `getb` (1 operand) - doc: `getb.md`
2. `riom` (3 operands) - doc: `riom.md` - metadata says 3 operands but may be 0

---

### Phase 11: SYSTEM Operations 🟢

**Status:** NOT STARTED
**Priority:** LOW (supervisor mode)
**Estimated Time:** 2-3 days
**Target Coverage:** 221/241 (91.7%)

#### Families (24)
All are privileged/system mode instructions. Documentation exists for all.

1-operand: int, intr, laddr, rladdr, bladdr, phyladr, rdus, rpgu, rphs, rwip, svers, wdus, wphs, zpgu, zwip
2-operand: freeb, lregbl, scntxt, sloca, sregbl
3-operand: cind, lcntxt, lind

---

## FILE LOCATIONS

### Source Code (Windows paths)
```
Test Generator:
E:\Dev\Repos\Ronny\RetroCore\Emulated.Tests.ND500\Instructions\TestND500_GenerateComprehensiveTests.cs

Test Executor:
E:\Dev\Repos\Ronny\RetroCore\Emulated.Tests.ND500\Instructions\TestND500_ExecuteComprehensiveTests.cs

Generated JSON Tests:
E:\Dev\Repos\Ronny\RetroCore\Emulated.Tests.ND500\bin\Debug\net9.0\Instructions\JsonTests\Comprehensive\

Analysis Scripts:
E:\Dev\Repos\Ronny\RetroCore\Emulated.Tests.ND500\analyze_coverage.py
E:\Dev\Repos\Ronny\RetroCore\Emulated.Tests.ND500\rescan_asm_docs.py
```

### Documentation (WSL paths)
```
Metadata:
\\wsl.localhost\Ubuntu\home\ronny\repos\nd500x\docs\instructions\instructions.json

Assembly Docs:
\\wsl.localhost\Ubuntu\home\ronny\repos\nd500x\docs\instructions\asm\*.md

Status Reports:
\\wsl.localhost\Ubuntu\home\ronny\repos\nd500x\docs\instructions\FUCK-ME.md
\\wsl.localhost\Ubuntu\home\ronny\repos\nd500x\docs\instructions\TODO-PLAN.MD
```

### Build Commands
```bash
# Build project
cd E:\Dev\Repos\Ronny\RetroCore\Emulated.Tests.ND500
dotnet build

# Run test generator
dotnet test --filter "FullyQualifiedName~Generate_Comprehensive_Tests_For_All_Instructions" --logger "console;verbosity=normal"

# Get summary (last 50 lines)
dotnet test --filter "FullyQualifiedName~Generate_Comprehensive_Tests_For_All_Instructions" --logger "console;verbosity=normal" 2>&1 | Select-Object -Last 50

# Run all comprehensive tests
dotnet test --filter "FullyQualifiedName~ExecuteComprehensiveJsonTests"
```

---

## ASSEMBLY DOCUMENTATION REFERENCE

### Notation Patterns

#### PREFIX Notation (most common)
```
Format: <datatype> <MNEMONIC> <operands>
Example: W MOVE I1, I2
Example: W ADD2 I1, I2
Example: W ADD3 I1, I2, I3
```

#### Register-Embedded Prefix (bitfield ops)
```
Format: <datatype><register> <MNEMONIC> <operands>
Example: W1 GETBI $value, $bitpos
Example: W1 GETBF R.16, 11, 8
```

#### No Prefix (packed ops)
```
Format: <MNEMONIC> <operands>
Example: PSHIFT B.0, B.4
Example: PCOMP B.0, B.100
```

### Data Types
- `BY` = Byte (8-bit)
- `H` = Half-word (16-bit)
- `W` = Word (32-bit)
- `F` = Float (32-bit)
- `D` = Double (64-bit)

### Register Names
- `I1`, `I2`, `I3` = Generic registers
- `BY1`, `BY2` = Byte registers
- `W1`, `W2` = Word registers

### Addressing Modes
- `CONSTANT`: `$value` or `$0x1234`
- `REGISTER`: `I1`, `I2`, `BY1`, `W2`
- `ABSOLUTE`: `B.0xFFFF` (16-bit address)
- `LOCAL`: `B.0xFF` (8-bit offset)
- `RECORD`: `R.0xFF` (record offset)
- `PRE_INDEXED`: Complex addressing
- `NONE`: Embedded in opcode

---

## TESTING AND VALIDATION

### Test Generation Validation
After implementing each phase:

1. **Build the project:**
   ```bash
   cd E:\Dev\Repos\Ronny\RetroCore\Emulated.Tests.ND500
   dotnet build
   ```

2. **Run test generator:**
   ```bash
   dotnet test --filter "FullyQualifiedName~Generate_Comprehensive_Tests_For_All_Instructions"
   ```

3. **Check for new JSON files:**
   ```bash
   ls bin\Debug\net9.0\Instructions\JsonTests\Comprehensive\
   ```

4. **Verify test counts:**
   Look for lines like:
   ```
   ✓ Generated 108 tests → ARITHMETIC_add2.json
   ```

5. **Check for errors:**
   Look for lines like:
   ```
   ⊘ No tests generated (unsupported pattern)
   ⚠ Generation failed for BYREGISTER: Assembly failed: Error: ...
   ```

### Coverage Tracking
After each phase, update this document with:
- New coverage percentage
- Number of tests generated
- List of newly working families
- Any issues encountered

---

## KNOWN ISSUES AND BLOCKERS

### Resolved Issues
1. ✅ **riom metadata mismatch** - User updated instructions.json
2. ✅ **BITFIELD lowercase postfix** - Fixed with register-embedded PREFIX (Phase 1)
3. ✅ **MOVE/SWAP wrong syntax** - Fixed with PREFIX notation (Phase 1)

### Current Blockers
1. ⚠️ **Missing GenerateFourOperandAssembly()** - Needed for loop, smvun, smvwh, scopt
2. ⚠️ **ARITHMETIC 2-op lowercase postfix** - Add2/sub2/mul2/div2 generate wrong syntax
3. ⚠️ **No pattern for 3-op arithmetic** - Need to add to GenerateThreeOperandAssembly

### Potential Issues
- Some instructions may use NONE addressing mode (branches)
- 4-operand instructions need new method
- System instructions may require special handling

---

## CONTINUATION PROMPT

**USE THIS PROMPT TO RESUME WORK IF THE SESSION DIES:**

```
I'm continuing the ND500 comprehensive test coverage project. Here's the current status:

PROJECT CONTEXT:
- Goal: Generate valid assembly and JSON tests for all 241 ND500 instruction families
- Current coverage: 125/241 families (51.9%), 3,479 tests generated
- Assembly documentation: 100% complete (all 150 needed families have .md files)
- The blocker is MISSING GENERATOR PATTERNS in C# code, NOT missing documentation

CRITICAL FILES:
- Test generator: E:\Dev\Repos\Ronny\RetroCore\Emulated.Tests.ND500\Instructions\TestND500_GenerateComprehensiveTests.cs
- Metadata: \\wsl.localhost\Ubuntu\home\ronny\repos\nd500x\docs\instructions\instructions.json
- Assembly docs: \\wsl.localhost\Ubuntu\home\ronny\repos\nd500x\docs\instructions\asm\*.md
- Master plan: \\wsl.localhost\Ubuntu\home\ronny\repos\nd500x\docs\instructions\TODO-PLAN.MD
- Status report: \\wsl.localhost\Ubuntu\home\ronny\repos\nd500x\docs\instructions\FUCK-ME.md

COMPLETED PHASES:
✅ Phase 0: Infrastructure complete
✅ Phase 1: MOVE/BITFIELD operations (move, swap, getbi, putbi, clebi, setbi, getbf, putbf)
   - Fixed PREFIX notation: W MOVE I1, I2
   - Fixed register-embedded prefix: W1 GETBI $value, $bitpos

NEXT PHASE TO IMPLEMENT:
🔴 Phase 2: ARITHMETIC 2-Operand Operations (add2, sub2, mul2, div2, addc, subc, mulad, psum, axi, ixi)
   - Status: NOT STARTED
   - Priority: CRITICAL
   - Time: 2-3 hours
   - Problem: Generating "w1 add2 w1, w2" (lowercase postfix - WRONG)
   - Solution: Use PREFIX notation "W ADD2 I1, I2" (uppercase - CORRECT)

INSTRUCTIONS:
1. Read the TODO-PLAN.MD file at \\wsl.localhost\Ubuntu\home\ronny\repos\nd500x\docs\instructions\TODO-PLAN.MD
2. Continue from Phase 2 (or whatever phase is marked as NEXT)
3. Follow the detailed implementation steps in the plan
4. After completing each phase:
   - Update TODO-PLAN.MD with completion status
   - Run test generator to validate
   - Update coverage metrics
   - Mark phase as complete
5. Keep TODO-PLAN.MD updated as you work
6. Always use this continuation prompt format for the next session

IMMEDIATE ACTION:
Start implementing Phase 2 by:
1. Reading assembly docs for add2, sub2, mul2, div2
2. Updating GenerateTwoOperandAssembly in TestND500_GenerateComprehensiveTests.cs
3. Adding PREFIX notation pattern: dt.ToUpper() + mnemonic.ToUpper() + "I1, I2"
4. Building and testing
5. Verifying 10 new JSON files are created
6. Updating TODO-PLAN.MD with results

Do you understand the project context and are you ready to continue from Phase 2?
```

---

**END OF PLAN DOCUMENT**

*Last updated: 2025-11-10 11:30:00*
*Next phase: Phase 2 - ARITHMETIC 2-Operand Operations*
*Estimated completion to 80% coverage: 4-6 weeks*
*Estimated completion to 95% coverage: 8-12 weeks*
