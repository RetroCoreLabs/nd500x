# Sync Handoff: Float-Arithmetic Operation-Level Fixes (Section 3A)

Date: 2026-07-19
Repos to keep identical:
- nd500x (C):        `.`
- RetroCore (C#):    `$RETROCORE`

Ground truth: ND-5000 microcode `MICRO-5800-A30.md` + ND-500 Reference Manual
(`docs/ND-05.009.4 EN ND-500 Reference Manual.md`).
The C# emulator is NOT the reference; both emulators are corrected to the manual/microcode.

## Root cause

The float/double variants of the extended two-/three-address arithmetic instructions
were `[DEFERRED]` silent no-ops in nd500x: they read nothing, wrote nothing, set no
flags. Any guest floating-point code using ADD3/SUB2/SUB3/MUL2/MUL3/MULAD/DIV2/DIV3 in
single (F) or double (D) precision silently produced a stale/garbage destination.

Two more float operation-level defects in the same section:
- `SET1` F/D wrote the integer bit pattern `0x00000001` (a tiny denormal) instead of
  floating `1.0`.
- `F TEST` / `D TEST` ran the integer Z/S helper on raw float bits, so Z/S were wrong.

## Correct behavior (from microcode carve)

Float arithmetic status is written by microcode `ST,SAVF`: it sets **Z** (result==0),
**S** (sign of result), and conditionally **FU**/**FO** (floating under/overflow). The
Reference "unlisted-bit" rule (manual line 4040) requires every data-status bit the
instruction does not name to be CLEARED, so **C and O are cleared** on every float
arithmetic result. Traps: Floating overflow (FO), Floating underflow (FU); DIV also
Divide-by-zero (DZ).

| Instr | Microcode | Operation | Dest |
|-------|-----------|-----------|------|
| ADD3 F/D  | ADD3F @002177 / ADD3D @002221  | a + b       | operand[2] (`<c>`) |
| SUB2 F/D  | SUB2F @002234                  | a - b       | operand[0] (`<a>`) |
| SUB3 F/D  | SUB3F @002241                  | a - b       | operand[2] (`<c>`) |
| MUL2 F/D  | MUL2F @002360                  | a * b       | operand[0] (`<a>`) |
| MUL3 F/D  | MUL3F / MUL3D                  | a * b       | operand[2] (`<c>`) |
| MULAD F/D | MULADF @002633 / MULADD @002635| Rn * x + y  | register Rn |
| DIV2 F/D  | DIV2F / DIV2D                  | a / b       | operand[0] (`<a>`) |
| DIV3 F/D  | DIV3F / DIV3D                  | a / b       | operand[2] (`<c>`) |
| SET1 F/D  | SET1F @000326 / SET1D @000330  | 1.0         | operand[0] |
| F/D TEST  | F TEST @000252 / D TEST @000253| value vs 0.0| flags only (Z,S; no C for float) |

## nd500x changes (committed)

Shared helper added (avoids duplicating the flags/trap tail in 8 files):
- `src/cpu/instruction_helpers.c`
  `uint64_t nd500_float_finish(Nd500Cpu* cpu, uint32_t pc, double result, bool is_double)`
  Converts IEEE result to ND-500 bits, sets Z/S (via `nd500_set_flags_zs_float`),
  clears C/O, sets FU/FO conditionally, raises FO/FU trap, returns result bits.
  FO detect: `isinf||isnan`; FU detect: `result!=0 && fabs(result)<1e-38` (mirrors the
  existing Add.c float path).
- Declared in `src/cpu/instruction_helpers.h`.

Instruction files (each replaced the `[DEFERRED]` block with a real float/double path
using `nd500_read_operand_as_ieee_float` / `nd500_write_operand_from_ieee_float` /
`nd500_float_finish`):
- `src/cpu/instructions/ARITHMETIC/Add3.c`
- `src/cpu/instructions/ARITHMETIC/Sub2.c`
- `src/cpu/instructions/ARITHMETIC/Sub3.c`
- `src/cpu/instructions/ARITHMETIC/Mul2.c`
- `src/cpu/instructions/ARITHMETIC/Mul3.c`
- `src/cpu/instructions/ARITHMETIC/Mulad.c`   (register dest; reads x,y operands)
- `src/cpu/instructions/ARITHMETIC/Div2.c`    (adds DZ + trap_divide_by_zero on b==0.0)
- `src/cpu/instructions/ARITHMETIC/Div3.c`    (adds DZ + trap_divide_by_zero on b==0.0)
- `src/cpu/instructions/CONTROL/Set1.c`       (F opcode 0x0047 / D opcode 0xFC89 -> 1.0)
- `src/cpu/instructions/COMPARE/Test.c`       (float -> nd500_set_flags_zs_float; O cleared)

## Regression status (nd500x)

- nc compile gate (`dom_nc_compile_a/b` + smoke): 4/4 PASS.
- Existing instruction-validation integer cases for all 10 instructions: 100% pass,
  0 fail (Sub2 86, Mulad 71, Test 3648, etc.). The F/D variants have NO existing test
  coverage (the generator never emitted them) - see below.

## RetroCore (C#) emulator mirror - DONE (2026-07-19, not yet committed pending regen)

Applied the identical float paths to the C# emulator, keeping instruction files
PORTABLE (no C#-framework APIs in instruction files; BitConverter/Math confined to the
helper layer, mirroring the C `union`/`isinf`/`fabs` boundary).

- Helper added: `Emulated.HW/ND/CPU/ND500/Instructions/InstructionHelpers.cs`
  - `ulong FloatFinish(double result, bool isDouble)` - mirror of `nd500_float_finish`
    (Z,S from result; C,O cleared; FU/FO conditional + trap; returns raw IEEE bits).
  - `double ReadRegisterAsIeeeFloat(byte regNum, bool isDouble)` - register-as-IEEE read
    for MULAD (uses BitConverter in the helper layer only).
- Instruction files rewritten to call ONLY helpers
  (`ReadOperandAsIeeeFloat`/`WriteOperandAsIeeeFloat`/`FloatFinish`), dead
  `ND500Float`/`ND500Double` switch cases and float trap tails removed:
  ARITHMETIC/{Add3,Sub2,Sub3,Mul2,Mul3,Mulad,Div2,Div3}.cs, MOVE/Set1.cs, COMPARE/Test.cs.
- Verified: `grep` confirms no `BitConverter|Math.|IsInfinity|ND500Float|ND500Double`
  remain in any of the 10 instruction files.

KEY REPRESENTATION FACT (proven, do not regress): float operands/registers are stored as
RAW IEEE-754 bits, NOT ND-500 native bias-256. Evidence: validation case
`FloatAbs_F_1_40400000` stores `a1 = 0x40400000` = IEEE 3.0. Both emulators now use the
raw-IEEE path; the old C# `ND500Float.FromIeee754Single` native conversion was the bug.

## ST flag bit encoding (both emulators + test `st` field) - verified

`Registers.cs` ToStatusWord / FlagCalculator: Z=1<<5 (0x20), C=1<<6 (0x40), S=1<<7 (0x80),
O=1<<9 (0x200), DZ=1<<12 (0x1000), FU=1<<13 (0x2000), FO=1<<14 (0x4000). Identical to
nd500x `ND500_FLAG_*`.

## Generator extension spec (39k suite must exercise F/D + negatives)

Float register encoding in scenarios (pattern from ComprehensiveFloatGenerator.cs):
- F: `InitialRegisters["a1"] = (uint)ieeeBits`; assembly uses A registers, e.g.
  `F ADD3 A1,A2,A3`.
- D: split - `a1 = (uint)(bits & 0xFFFFFFFF)`, `e1 = (uint)(bits >> 32)` (low->An, high->En).
Expected result bits = `BitConverter.SingleToInt32Bits(aIeee OP bIeee)` (F) /
`DoubleToInt64Bits` (D). Expected `st` per FloatFinish: Z if result==0, S if signbit,
C=O=0, FU/FO per |r|<1e-38 / isinf-isnan; DIV DZ (0x1000) + ExpectedTrap DivideByZero
when divisor==0.
Negative/edge cases required (per user directive - exercise every fixed instruction):
FO (e.g. max_float * max_float -> inf, ExpectedTrap FloatingOverflow, st FO),
FU (tiny * tiny -> underflow, st FU), DZ (x / 0 -> ExpectedTrap DivideByZero, st DZ),
SET1 F/D produces 1.0 (a1 = 0x3F800000 for F; a1/e1 for D=1.0), F/D TEST of
+0.0/-0.0/positive/negative -> correct Z/S.
Files: ComprehensiveArithmeticGenerator.cs (Add3/Sub2/Sub3/Mul2/Mul3/Mulad/Div2/Div3 -
add float branch in each Create*Scenario keyed on dataType F/D), and the CONTROL/COMPARE
generators for SET1 and TEST. Ensure the orchestrator actually routes the F/D variants to
these generators (currently no float ADD3 cases are emitted at all).

## Generator extension - DONE (2026-07-19), verified, pending user regen

Extended and verified (spot-checked against actual source, not assumed):
- ComprehensiveArithmeticGenerator.cs: F/D routing added in GenerateForMnemonic (root
  cause of zero float coverage: the F/D branch only routed `+ - * / abs neg incr decr`,
  never the extended 2/3-address forms). New FloatFinishFlags/FloatBitsRaw/DoubleBitsRaw/
  ApplyFloatOp/GenerateFloatExtendedArithmeticScenarios/CreateFloatArithScenario/
  CreateFloatMuladScenario. FloatFinishFlags mirrors nd500_float_finish exactly.
- ComprehensiveControlGenerator.cs: GenerateForMnemonic override + CreateSet1FloatScenario
  (SET1 F/D -> 1.0).
- ComprehensiveCompareGenerator.cs: GenerateForMnemonic override + CreateFloatTestScenario
  (F/D TEST: +0.0/-0.0/positive/negative/C-preserved).
- ~96 new float scenarios (positive + FU/FO/DZ negatives); framework auto-adds wrong-flag
  negatives at 5% over the positive scenarios.
- Verified: TrapType enum has FloatException + DivisionByZero (NOT FloatingOverflow etc.);
  test_instruction_validation.c maps FO(bit14)+FU(bit13)->"FloatException", DZ(bit12)->
  "DivisionByZero". Generator uses TrapType.FloatException (FO/FU) and .DivisionByZero (DZ).
- Verified: ST flag constants in generator (Z=0x20,S=0x80,DZ=0x1000,FU=0x2000,FO=0x4000).

nd500x MULAD register-read bug fixed (commit c57935e): reads Rn as raw IEEE-754 (was
ND-native), now consistent with C# ReadRegisterAsIeeeFloat + the write path.

CAVEAT to watch during regen: the exporter's AssembleSingleStatement is the real gate for
the float operand spellings (esp. `F1 MULAD A2,A3` with A-register operands vs the integer
form's constant operands). If any float scenario fails to assemble, that mnemonic's operand
form needs adjusting. MULAD positive cases validate FLAGS ONLY by design (now that both
emulators read Rn as raw IEEE, they could be strengthened to validate the result register).

## Regen commands (USER runs - heavy dotnet step)
```
cd $RETROCORE
dotnet test Emulated.Tests.ND500 --filter "Generate_Master_JSON"
cp Emulated.Tests.ND500/bin/Debug/net9.0/nd500_tests.json test/nd500_tests.json
cd . && make
./build/bin/test_instruction_validation --continue
```
If green: commit RetroCore (emulator instruction files + InstructionHelpers.cs + 3 generators)
and the refreshed nd500x test/nd500_tests.json.

## RetroCore (C#) remaining TODO

1. Apply the same float/double paths to
   `$RETROCORE/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/`
   {Add3,Sub2,Sub3,Mul2,Mul3,Mulad,Div2,Div3}.cs, and CONTROL/Set1.cs, COMPARE/Test.cs.
   Same flag rule: Z,S set; C,O cleared; FU,FO conditional; DIV DZ on zero divisor.
2. Extend the test generators so the 39,598-case suite COVERS the F/D variants (it
   currently emits only BY/H/W for these) with MANUAL-derived expected results and flags,
   plus NEGATIVE tests (FO on overflow, FU on underflow, DZ on divide-by-zero, SET1 F/D
   produces 1.0, F/D TEST Z/S on +0.0/-0.0/positive/negative):
   `$RETROCORE/Emulated.Tests.ND500/Validation/Generators/ComprehensiveArithmeticGenerator.cs`
   (GenerateAdd3Scenarios etc.), and the CONTROL/COMPARE generators for SET1/TEST.
3. Regenerate + copy + validate (this runs `dotnet test`, a heavy step - run explicitly):
   ```
   cd $RETROCORE
   dotnet test Emulated.Tests.ND500 --filter "Generate_Master_JSON"
   cp Emulated.Tests.ND500/bin/Debug/net9.0/nd500_tests.json test/nd500_tests.json
   cd . && make
   ./build/bin/test_instruction_validation --continue
   ```
