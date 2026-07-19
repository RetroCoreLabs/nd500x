# SYNC: Float-arithmetic cluster + LOOP rebased from IEEE-reinterpret to ND-500 NATIVE (bias-256)

Full path: /home/ronny/repos/nd500x/docs/SYNC-FLOAT-NATIVE-REBASE.md

Supersedes the "RAW IEEE-754" assumption documented in
/home/ronny/repos/nd500x/docs/SYNC-FLOAT-ARITHMETIC-FIXES.md for the instructions
listed below. Everything else in that earlier doc still stands.

## Ground truth

ND-500 Reference Manual sections 2.5.1.4 (single, F) and 2.5.3.6 (double, D):
ND-500 floating point is a NATIVE bias-256 format, NOT IEEE-754.

- Single (F, 32-bit): sign(1) | exponent(9, bias 256) | mantissa(22, hidden leading 1 => 23 significant)
- Double (D, 64-bit): sign(1) | exponent(9, bias 256) | mantissa(54)
- Mantissa M is in [0.5, 1); value = (-1)^s * M * 2^(exp - 256)
- exponent field == 0 means the value is EXACTLY ZERO (all bits zero)

The bulk of the float instructions (Add/Sub/Mul/Div single-operand register forms,
CONR/CONV, trig, REM, ...) were already correct: they read the operand's NATIVE
bits and CONVERT to IEEE for host arithmetic via nd500_float_to_ieee754 /
nd500_double_to_ieee754 (C) and ND500Float.ToIeee754Single /
ND500Double.ToIeee754Double (C#), and store results back with the *_from_ieee754 /
FromIeee754* converters.

A subset (this rebase) was WRONG: it REINTERPRETED the raw operand/register bits as
IEEE-754. That subset is now converted to native, matching the hardware and the rest
of the emulator.

## Instructions rebased (both emulators)

ADD3, SUB2, SUB3, MUL2, MUL3, MULAD, DIV2, DIV3 (ARITHMETIC),
SET1 F/D (CONTROL), LOOP, LOOPI, LOOPD F/D (BRANCH).

These are exactly the callers of the shared float operand helpers, so the fix was
made in the HELPERS (no per-instruction duplication).

## nd500x (C) changes

File: /home/ronny/repos/nd500x/src/cpu/instruction_helpers.c

1. `nd500_read_operand_as_ieee_float()` -- was: reinterpret raw bits as IEEE.
   Now: read native bits then `nd500_float_to_ieee754` / `nd500_double_to_ieee754`.

2. `nd500_write_operand_from_ieee_float()` -- was: store IEEE bits directly.
   Now: `nd500_float_from_ieee754` / `nd500_double_from_ieee754` then store native bits.

3. `nd500_float_finish()` -- UNCHANGED in code (it already produced native result bits
   via *_from_ieee754 and set Z/S from those). It is now CONSISTENT with the storage
   helper (previously the value stored by the instruction was IEEE while the flags were
   computed from native bits -- an internal contradiction, now resolved).

File: /home/ronny/repos/nd500x/src/cpu/instructions/ARITHMETIC/Mulad.c
   The float path read Rn via a raw `union` reinterpret. Now reads Rn via
   `nd500_float_to_ieee754(nd500_read_float_register(...))` /
   `nd500_double_to_ieee754(nd500_read_double_register(...))`. x, y already went
   through the (now-native) `nd500_read_operand_as_ieee_float`.

No changes needed in Add3.c/Sub2.c/Sub3.c/Mul2.c/Mul3.c/Div2.c/Div3.c/Set1.c/
Loop.c/Loopi.c/Loopd.c -- they only call the helpers.

## RetroCore (C#) changes

File: /mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/InstructionHelpers.cs

- `ReadOperandAsIeeeFloat`  : BitConverter reinterpret -> `((ND500Float)bits).ToIeee754Single()` / `((ND500Double)bits).ToIeee754Double()`
- `WriteOperandAsIeeeFloat` : BitConverter reinterpret -> `ND500Float.FromIeee754Single((float)value)` / `ND500Double.FromIeee754Double(value)`
- `ReadRegisterAsIeeeFloat` : same native conversion (used by MULAD)
- `FloatFinish`            : result bits via `ND500Float.FromIeee754Single` / `ND500Double.FromIeee754Double`; Z/S taken from those native bits (bit masks unchanged: native zero == all-zero, native sign == top bit)

File: /mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Mulad.cs
   Comment corrected (Rn/x/y are native); code already used the helpers above.

The instruction .cs files (Add3/Sub2/.../Loop/Set1) are UNCHANGED -- helper-layer fix.
Portability boundary preserved: BitConverter/Math stay out of instruction files.

## Test generator changes (RetroCore/Emulated.Tests.ND500/Validation/Generators)

The fixtures encode the register/memory operand bits. Because the emulator now stores
native bits, the fixtures must encode native bits too (else every rebased case fails).

1. ComprehensiveArithmeticGenerator.cs
   - `FloatBitsRaw`/`DoubleBitsRaw` (BitConverter) -> `FloatBitsNative`/`DoubleBitsNative`
     using `ND500Float.FromIeee754Single` / `ND500Double.FromIeee754Double`.
   - MULAD x, y operands now native (were raw IEEE); Rn was already native.
   - `FloatFinishFlags` UNCHANGED (Z/S are representation-invariant for the test values).

2. ComprehensiveControlGenerator.cs -- SET1 F/D expected operand is now
   `ND500Float/ND500Double.FromIeee754(1.0)`, not the raw IEEE 0x3F800000 / 0x3FF0...

3. ComprehensiveBranchGenerator.cs -- LOOP/LOOPI/LOOPD float index (initial + expected)
   encoded native. Added `using Emulated.HW.ND.CPU.ND500;`.
   (ComprehensiveControlGenerator.cs got the same using.)

NOT changed: TEST (ComprehensiveCompareGenerator.cs). Float TEST reads the raw register
bits and sets Z/S from the sign bit / zero-ness -- identical bit positions in IEEE and
native, so it is representation-invariant and does not call the rebased helpers.

REM float (ComprehensiveArithmeticGenerator CreateFloatRemScenario) was ALREADY native.

## Known representation consequence (edge case, trap-guarded)

On floating underflow (nonzero result with |r| < 1e-38), the native store rounds the
value to native ZERO (exp field underflows), so the stored operand is all-zero and the
Z flag ends up set alongside FU. Under IEEE-reinterpret it was a nonzero denormal (Z
clear). This only occurs on the FU-trap path, where the validation runner skips
register/flag checks once the trap type matches, so it is not asserted. Storing zero on
underflow is consistent with `exp==0 => zero` in the native format.

## FOLLOW-UP FIX (same session): full-range codec + native-domain FU/FO detection

Regenerating after the mechanical rebase surfaced 22 FU/FO trap-fixture failures. Root
cause (byte-verified), NOT a rebase error but a pre-existing defect the rebase exposed:

1. The old IEEE-bitfield converters (nd500_float_to/from_ieee754,
   ND500Float.To/FromIeee754Single) only round-trip the IEEE single/double SUB-range and
   DISAGREED on denormals: C returned 0 for an IEEE-denormal input while C# returned a
   nonzero native value (exp 130). Once operands flowed THROUGH native encoding, this
   split (and the narrow range) broke every extreme fixture. Example: native single
   0x1F05B099 (= ~1e-40, a valid native number) decoded to 0.0 under the narrow C path,
   so DIV read its divisor as zero and raised a spurious DivisionByZero.
2. FU/FO were detected in the wrong domain: host-double isinf / |r|<1e-38 (the IEEE
   single denormal threshold) instead of the NATIVE bias-256 exponent range.

Manual ground truth (ND-05.009.4, lines 2052/2055/2514/2536): single AND double use a
9-bit exponent, bias 256, M in [0.5,1), e_field==0 => exactly zero; magnitude range
~[8.6e-78, 5.8e76]. FU = "negative exponent requires more than 9 bits" (e_field < 1;
store zero, keep sign, set Z). FO = signed exponent > 9 bits positive (e_field > 511;
store max magnitude, keep sign).

Fix (both emulators, kept bit-identical):
- New FULL-RANGE frexp-based codec carrying the value in a host DOUBLE (spans the whole
  native range), no IEEE bitfields, no denormal special-case:
  * C: nd500_native_single_to_double / _from_double, nd500_native_double_to_double /
    _from_double in src/cpu/instruction_helpers.c (+ prototypes in .h). The *_from_double
    functions report overflow/underflow via out-params by the native exponent range.
  * C#: ND500Float.NativeToDouble / FromDoubleNative and ND500Double.NativeToDouble /
    FromDoubleNative in ND500Float.cs (frexp emulated with Math.ILogB + Math.ScaleB).
- The cluster read/write helpers (nd500_read_operand_as_ieee_float /
  nd500_write_operand_from_ieee_float and the C# ReadOperandAsIeeeFloat /
  WriteOperandAsIeeeFloat / ReadRegisterAsIeeeFloat), Mulad's Rn read, and
  nd500_float_finish / FloatFinish now use this codec. float_finish/FloatFinish take
  FU/FO straight from the codec's overflow/underflow flags (inf/NaN still forced to FO).
- The legacy nd500_float_to/from_ieee754 and ND500Float.To/FromIeee754Single are LEFT AS
  IS -- they are still used by ~28 other float instructions whose tests pass with
  normal-range values. They carry the same latent narrow-range/denormal defect; converging
  them onto the full-range codec is a documented FOLLOW-UP (no failing test today).

Trap-fixture rework (ComprehensiveArithmeticGenerator.cs) -- the old seeds injected
values that are un-loadable in native (1e-40 is representable, not underflow; +inf clamps
to a finite max). New fixtures drive the exceptional RESULT through the arithmetic with
loadable operands:
- ADD3: 5e76 + 5e76 -> FO.   SUB2/SUB3: 5e76 - (-5e76) -> FO.
- MUL2/MUL3: 1e40*1e40 -> FO ; 1e-40*1e-40 -> FU.
- DIV2/DIV3: 1e40/1e-40 -> FO ; 1e-40/1e40 -> FU ; x/0 -> DZ.
- MULAD: rn*x with 1e40/1e-40 (y=0) -> FO / FU.
- ADD3/SUB get NO underflow negative: a sum/difference of loadable operands cannot fall
  below the native minimum except by fragile near-cancellation. FU coverage is via MUL/DIV.
  Fixture encoders switched to FromDoubleNative so operand bits match the emulator's codec.

Result: 40061/40061 pass (was 40045/40067), 3663 negatives correctly fail, nc gate 4/4.

## Validation (USER-gated: dotnet regen + make + test)

1. cd /mnt/e/Dev/Repos/Ronny/RetroCore && dotnet test Emulated.Tests.ND500 --filter "Generate_Master_JSON"
2. cp Emulated.Tests.ND500/bin/Debug/net9.0/nd500_tests.json /home/ronny/repos/nd500x/test/nd500_tests.json
3. cd /home/ronny/repos/nd500x && make
4. ./build/bin/test_instruction_validation --continue   (expect ALL PASSED)
5. cd build && ctest -R dom_nc_compile                   (nc gate, expect 4/4)
