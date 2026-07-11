# ND500X CPU Implementation Changes

This document describes corrections and changes made to the nd500x CPU emulator based on validation testing against the C# reference implementation.

## Test Results Summary

- **Tests Passed**: 20,902 of 20,902 (100.0%)
- **Test Framework**: `test/test_instruction_validation.c` using `test/nd500_tests.json`
- **Status Register**: Full validation enabled (fixed in Sections 20-23)

---

## Bug Categories for Assembler Review

### Likely NOT Assembler Issues (CPU Implementation Bugs)

These were implementation bugs in the C emulator that don't affect how the assembler generates code:

1. **ST1 vs FLAGS register** (Section 14) - Internal CPU register confusion
2. **PC advancement timing** (Section 15) - CPU execution model
3. **32-bit RAM value byte order** (Section 16) - Test harness issue
4. **RETB/RETBK TOS check** (Section 17) - Heap operation condition

### Potentially Assembler-Related Issues

These fixes involve byte order and addressing that the assembler generates:

1. **Big-endian operand value reading** (Section 5) - Assembler must emit big-endian
2. **Big-endian displacement in compute_effective_address** (Section 18) - 2-byte displacements
3. **Branch PC calculation** (Section 12) - Relative to instruction START (first byte)

### Variant/Datatype Mapping Issues (Dispatch Table)

These are issues with how instruction variants map to data types. May affect:
- Instruction opcode generation
- Data type prefix selection

See Section 19 for details on the variant inconsistencies.

---

## 1. Data Type Detection from Dispatch Table

### File: `src/cpu/cpu_instr.c`

**Problem**: Data type detection used hardcoded opcode ranges which missed many instructions (e.g., DECR at 0xFC8C).

**Solution**: Use the `variant` field from the dispatch table directly:

```c
/* Variant: 0=BYTE, 1=HALFWORD, 2=WORD, 3=FLOAT, 4=DOUBLE */
uint8_t variant = instr_meta->variant;
switch (variant) {
    case 0: out->data_type = ND500_DTYPE_BYTE; break;
    case 1: out->data_type = ND500_DTYPE_HALFWORD; break;
    case 2: out->data_type = ND500_DTYPE_WORD; break;
    case 3: out->data_type = ND500_DTYPE_WORD; out->uses_float_registers = true; break;
    case 4: out->data_type = ND500_DTYPE_DOUBLEWORD; out->uses_float_registers = true; break;
}
```

**Note**: FLOAT uses `ND500_DTYPE_WORD` (32-bit) with `uses_float_registers=true`.

---

## 2. AssignTo/AssignFrom 6-Variant Data Type Detection

### File: `src/cpu/cpu_instr.c`

**Problem**: Instructions with 6 variants (BI, BY, H, W, F, D) have variant numbers that don't directly map to data types. For example, AssignTo (`:=`) opcode 0x000C has variant=3 in the dispatch table, but variant 3 should map to WORD not FLOAT.

**Root Cause**: For 6-variant instructions:
- variant 0 = BI (bit)
- variant 1 = BY (byte)
- variant 2 = H (halfword)
- variant 3 = W (word) <- NOT float!
- variant 4 = F (float)
- variant 5 = D (double)

**Solution**: Add special handling for opcodes 0x0004-0x002B based on opcode pattern:

```c
/* AssignTo (0x0004-0x0017): BY=0x04-07, H=0x08-0B, W=0x0C-0F, F=0x10-13, D=0x14-17 */
if (opcode >= 0x0004 && opcode <= 0x0017) {
    uint8_t type_offset = ((opcode - 0x0004) >> 2);
    switch (type_offset) {
        case 0: out->data_type = ND500_DTYPE_BYTE; break;       /* 0x04-07: BY */
        case 1: out->data_type = ND500_DTYPE_HALFWORD; break;   /* 0x08-0B: H */
        case 2: out->data_type = ND500_DTYPE_WORD; break;       /* 0x0C-0F: W */
        case 3: out->data_type = ND500_DTYPE_WORD;              /* 0x10-13: F */
                out->uses_float_registers = true; break;
        case 4: out->data_type = ND500_DTYPE_DOUBLEWORD;        /* 0x14-17: D */
                out->uses_float_registers = true; break;
    }
}
/* AssignFrom (0x0018-0x002B): same pattern */
```

---

## 3. Operand Ordering for Extended Arithmetic Instructions (*2)

### Files: `src/cpu/instructions/ARITHMETIC/Add2.c`, `Sub2.c`, `Mul2.c`, `Div2.c`

**Problem**: These instructions read/write operands in wrong order.

**Correct operand semantics** (per instruction metadata):
- `operands[0]` = **Destination** (read initial value, write result)
- `operands[1]` = **Source** (read-only)

**Assembly format**: `<instr> <dest>, <src>` (e.g., `BY ADD2 I1,I2`)

**Corrected pattern**:
```c
/* Read both values */
destValue = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
srcValue = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

/* Perform operation: dest OP src -> dest */
result = destValue + srcValue;  /* or -, *, / */

/* Write result to destination */
nd500_write_operand_value(cpu, &fi->operands[0], result, fi->data_type);
```

---

## 4. CONSTANT_SHORT Operand Value Reading

### File: `src/cpu/instruction_helpers.c`

**Problem**: CONSTANT_SHORT values (address codes 0x00-0x3F) were read from `op->data[0]` instead of from the address code itself.

**Solution**:
```c
uint64_t nd500_read_operand_value(...) {
    /* CONSTANT_SHORT: value embedded in address code (low 6 bits) */
    if (op->mode == ND500_ADDR_CONSTANT_SHORT) {
        return (uint64_t)(op->address_code & 0x3F);
    }
    ...
}
```

---

## 5. Big-Endian Constant Decoding

### File: `src/cpu/instruction_helpers.c`

**Problem**: Multi-byte constants were decoded as little-endian.

**Solution**: ND-500 uses big-endian byte order:
```c
case 2: return (uint16_t)((op->data[0] << 8) | op->data[1]);
case 4: return (uint32_t)((op->data[0] << 24) | (op->data[1] << 16) |
                          (op->data[2] << 8) | op->data[3]);
```

**Assembler Impact**: The assembler MUST emit multi-byte values in big-endian order.

---

## 6. Register Operand Decoding

### File: `src/cpu/cpu_instr.c`

**Problem**: Address codes 0xD0-0xD3 were mapped to registers 0-3.

**Solution**: Map to registers 1-4 (I1-I4):
```c
_op->reg = (_ac & 0x03) + 1;  /* 0xD0->I1, 0xD1->I2, 0xD2->I3, 0xD3->I4 */
```

---

## 7. GETBF Bit Range Validation

### File: `src/cpu/instructions/BITFIELD/Getbf.c`

**Problem**: For register operands, bit position was validated against the data type size (e.g., 8 bits for BYTE), but registers are always 32-bit.

**Solution**: Use 32-bit range for register operands:
```c
bool is_register = (fi->operands[0].mode == ND500_ADDR_REGISTER);
uint32_t bits = is_register ? 32 :
                (fi->data_type == ND500_DTYPE_BYTE ? 8 :
                 fi->data_type == ND500_DTYPE_HALFWORD ? 16 : 32);
```

---

## 8. LADDR (Load Address) Implementation

### File: `src/cpu/instructions/SYSTEM/Laddr.c`

**Problem**: LADDR instruction was not implemented (stub only).

**Solution**: Implemented full LADDR functionality:
```c
/* For CONSTANT and CONSTANT_SHORT modes, the "address" is the constant value itself.
 * For memory addressing modes, use the computed effective_address */
if (op->mode == ND500_ADDR_CONSTANT_SHORT) {
    address = op->address_code & 0x3F;
} else if (op->mode == ND500_ADDR_CONSTANT) {
    /* Value in op->data (big-endian) */
    switch (op->data_len) {
        case 1: address = op->data[0]; break;
        case 2: address = (op->data[0] << 8) | op->data[1]; break;
        case 4: address = (op->data[0] << 24) | (op->data[1] << 16) |
                          (op->data[2] << 8) | op->data[3]; break;
    }
} else {
    address = op->effective_address;
}

nd500_write_integer_register(cpu, fi->target_register, address);
```

**Key insight**: LADDR with a constant operand (like `laddr $4096`) loads the constant value itself as the address into the target register.

---

## 9. SHA (Arithmetic Shift) Direction and Sign Extension

### File: `src/cpu/instructions/SHIFT/Sha.c`

**HISTORY NOTE (corrected)**: An earlier revision of this section claimed
"positive count = RIGHT shift" with sign taken from the full 32-bit register.
That change was driven by buggy generated test data and was REVERTED
(commit 0a886d5). The current, correct behavior matches the reference manual
section 10.25 and the C# reference (Sha.cs):

- Positive shift count = LEFT shift; negative = RIGHT shift (arithmetic,
  sign-extending). Manual 10.25: "Positive shiftcount implies left shift,
  negative shiftcount implies right shift."
- Sign for right shifts is taken from the OPERAND's own width (bit 7 for BY,
  bit 15 for H, bit 31 for W), not from the full 32-bit register. The manual
  describes the shift as operating on "the byte, halfword or word operand".
- A shift count with magnitude >= the operand size raises an illegal operand
  value trap (manual 10.25); count 0 is legal and leaves the operand
  unchanged.

**Test case**: `BY SHA I1,$-1` (right shift by 1) with I1=0xFF
- 0xFF as byte operand = -1; -1 >> 1 = -1 = 0xFF (sign preserved at bit 7)

---

## 10. Unsigned Branch Carry Flag Polarity

### Files: `src/cpu/instructions/BRANCH/IfUnsigned*.c`

**Problem**: Unsigned comparison branch instructions were checking carry flag with inverted polarity.

**ND-500 convention** (verified against C# reference):
- `IF U< GO`: Branch if C=0 (borrow occurred, A < B unsigned)
- `IF U>= GO`: Branch if C=1 (no borrow, A >= B unsigned)
- `IF U<= GO`: Branch if C=0 OR Z=1
- `IF U> GO`: Branch if C=1 AND Z=0

**Solution**: Fixed all unsigned comparison branch instructions to use correct polarity.

---

## 11. CONSTANT_SHORT 6-Bit Sign Extension

### File: `src/cpu/instruction_helpers.c`

**Problem**: CONSTANT_SHORT operands (address codes 0x00-0x3F) were read as unsigned 6-bit values. But the ND-500 treats them as **signed** 6-bit values where bit 5 is the sign bit.

**Example failure**:
```
Assembly: w1 := $4294967294    (which is -2)
Bytes: 0C 3E
Expected: I1 = 0xFFFFFFFE (-2)
Got:      I1 = 0x0000003E (62)
```

The assembler encodes -2 as 0x3E (binary 111110), which is a 6-bit signed value.

**Solution**: Added `nd500_sign_extend_6bit()` function and updated `nd500_read_operand_value()`:
```c
int32_t nd500_sign_extend_6bit(uint8_t value) {
    // Bit 5 is the sign bit (values 0x20-0x3F are negative: -32 to -1)
    if (value & 0x20) {
        return (int32_t)(value | 0xFFFFFFC0);  // Extend with 1s
    }
    return (int32_t)(value & 0x3F);
}

/* In nd500_read_operand_value(): */
if (op->mode == ND500_ADDR_CONSTANT_SHORT) {
    uint8_t raw_value = op->address_code & 0x3F;
    int32_t signed_value = nd500_sign_extend_6bit(raw_value);
    return (uint64_t)(uint32_t)signed_value;
}
```

**Value mapping**:
- 0x00-0x1F (0-31): Positive values 0 to 31
- 0x20-0x3F (32-63): Negative values -32 to -1

---

## 12. Branch PC Calculation (Relative to Instruction START)

### Files: `src/cpu/instructions/BRANCH/*.c`

**HISTORY NOTE (corrected)**: An earlier revision of this section claimed the
displacement was relative to the instruction END (start + size), based on the
C# test formula `expectedPC = DefaultPC + SIZE_BRANCH + offset`. That claim
came from BUGGY generated test data (see "Test Data Issues" below: all GO
tests carry offset-0 expected PCs) and was wrong. The change was reverted.

**Correct behavior** (manual section 8.16.1, confirmed against a real
SINTRAN-linked binary): the displacement is "the distance from the first byte
of the current instruction to the first byte of the addressed instruction".

```c
cpu->PC = (uint32_t)(fi->address + displacement);
```

**Real-binary verification** (nc-a06.dom, NC compiler, linker v97.251, 1990):

```
08000205: C5 00 86   IF = GO 0x86
  start-relative: 0x08000205 + 0x86 = 0x0800028B -> valid instruction boundary
  end-relative:   0x08000208 + 0x86 = 0x0800028E -> lands MID-instruction (wrong)

08000291: IF = GO 0x92 -> 0x08000291 + 0x92 = 0x08000323
0800029D: IF = GO 0x86 -> 0x0800029D + 0x86 = 0x08000323
  (two independent branches converge on one common exit only with the
   start-relative anchor)
```

Note: `cpu->PC` is advanced past the instruction BEFORE the handler runs
(cpu.c), so branch handlers must use `fi->address`, never `cpu->PC`, as the
base. IFKGO had exactly this bug (fixed).

**Assembler Impact**: branch displacements are computed from the first byte
of the branch instruction; the assembler must NOT add the instruction size.

---

## 13. SHR (Circular Shift) Direction and Large Shift Count

### File: `src/cpu/instructions/SHIFT/Shr.c`

**Problem 1 - Large shift count handling (RESOLVED: traps per manual)**: An
earlier revision normalized the count with modulo instead of trapping. That was
backed by NEITHER the manual NOR the C# reference and existed only to satisfy
buggy generated test data. The current code raises an IOV (Illegal Operand
Value) trap for counts >= operand width, matching manual section 10.26 and C#
Shr.cs.

**Problem 2 - Rotation direction**: Needed to match C# reference behavior.

**Current behavior** (`src/cpu/instructions/SHIFT/Shr.c`):
```c
/* Illegal operand value trap for out-of-range counts (manual 10.26, C# Shr.cs) */
if (shift >= (int32_t)bits) {
    raise_trap(cpu, TRAP_IOV, fi->address, 0);
    return;
}

if (shift_count >= 0) {
    /* Rotate right: (value >> shift) | (value << (bits - shift)) */
    result = (value >> shift) | (value << (bits - shift));
} else {
    /* Rotate left: (value << shift) | (value >> (bits - shift)) */
    result = (value << shift) | (value >> (bits - shift));
}
result = nd500_mask_to_datatype(result, fi->data_type);
```

**CAVEATS (after manual review)**:
- The rotate-RIGHT-on-positive-count direction contradicts the manual's
  "positive shiftcount implies left shift" (10.26). Both this emulator and
  the C# reference rotate right, justified only by empirical testing against
  nd500-as (see docs/instructions/asm/shr.md). Unresolved against real
  hardware.

---

## 14. ST1 vs FLAGS Register for Status Flags

### Files: `src/cpu/instructions/COMPARE/Test.c`, `Comp2.c`, `src/cpu/instructions/LOGICAL/Invc.c`

**Problem**: Some instructions directly accessed `cpu->FLAGS` instead of using the status register helpers which operate on `cpu->ST1`.

**Background**: The CPU structure has both:
- `ST1, ST2`: The actual 64-bit status register (split into two 32-bit parts)
- `FLAGS`: An unused "simplified status" field

The helper functions (`nd500_set_flag()`, `nd500_clear_flag()`, `nd500_test_flag()`) correctly operate on `cpu->ST1`, but some instructions bypassed them.

**Solution**: Updated all instructions to use helper functions:
```c
/* Before (wrong): */
cpu->FLAGS |= ND500_FLAG_C;

/* After (correct): */
nd500_set_flag(cpu, ND500_FLAG_C);
```

**Fixed instructions**:
- `Test.c`: C flag setting
- `Comp2.c`: Z, C, S flag setting/clearing
- `Invc.c`: C flag reading

---

## 15. PC Advancement Before Instruction Execution

### File: `src/cpu/cpu.c`

**Problem**: The CPU step function advanced PC AFTER instruction execution, but only if PC hadn't changed. This failed for "jump to self" cases where the instruction intentionally sets PC to its own address.

**Old behavior**:
```c
nd500_execute_decoded(cpu, &fi);
if (cpu->PC == old_pc) {
    cpu->PC = old_pc + fi.total_len;  // Only advance if PC unchanged
}
```

This couldn't distinguish between:
- "PC not modified by instruction" (should advance)
- "PC intentionally set to same value" (should not advance)

**C# behavior**: PC is advanced BEFORE execution, allowing branch instructions to overwrite it.

**Solution**:
```c
/* Advance PC BEFORE execution (like C# implementation)
 * Branch/jump instructions will overwrite PC as needed */
cpu->PC = old_pc + (fi.total_len ? fi.total_len : fi.opcode_len);

nd500_execute_decoded(cpu, &fi);
```

---

## 16. Test Harness: 32-bit RAM Values in Big-Endian

### File: `test/test_instruction_validation.c`

**Problem**: The test harness was writing initial RAM values as single bytes instead of 32-bit words in big-endian order.

**Old (incorrect)**:
```c
nd500_bus_write8(m, addr, (uint8_t)val);
```

**New (correct)**:
```c
/* Write 32-bit value in big-endian (ND-500 byte order) */
nd500_bus_write8(m, addr + 0, (uint8_t)((val >> 24) & 0xFF));
nd500_bus_write8(m, addr + 1, (uint8_t)((val >> 16) & 0xFF));
nd500_bus_write8(m, addr + 2, (uint8_t)((val >> 8) & 0xFF));
nd500_bus_write8(m, addr + 3, (uint8_t)(val & 0xFF));
```

This fixed RET/RETK/RETB/RETBK tests that were failing with "stack underflow" because the stack frame PREVB and RETA values weren't being read correctly.

---

## 17. RETB/RETBK: Skip Heap Operations if TOS=0

### Files: `src/cpu/instructions/CALL/Retb.c`, `Retbk.c`

**Problem**: RETB/RETBK trapped when TOS register was 0 (heap not initialized).

**C# behavior**: When TOS=0, the instructions skip heap operations but still perform the return.

**Solution**:
```c
/* STEP 5: Free block back to heap using buddy system (if heap is initialized) */
uint32_t heap_vars_addr = cpu->TOS;
if (heap_vars_addr != 0) {
    /* Read MAXL, validate log_size, link block to freelist... */
}
/* Note: If TOS=0, still perform the return but skip heap operations */

/* STEP 6: Restore CPU registers */
cpu->B = prev_b;
cpu->PC = ret_addr;
cpu->L = ret_addr;
```

---

## 18. Big-Endian 2-Byte Displacement Decoding

### File: `src/cpu/cpu_instr.c`

**Problem**: In `compute_effective_address()`, 2-byte displacements were being read in little-endian order.

**Old (incorrect)**:
```c
} else if (op->data_len == 2) {
    uint16_t raw = (uint16_t)op->data[0] | ((uint16_t)op->data[1] << 8);  /* Little-endian */
    displacement = (int16_t)raw;
}
```

**New (correct)**:
```c
} else if (op->data_len == 2) {
    uint16_t raw = ((uint16_t)op->data[0] << 8) | (uint16_t)op->data[1];  /* Big-endian */
    displacement = (int16_t)raw;
}
```

**Assembler Impact**: The assembler must emit 2-byte displacements in big-endian order.

---

## 19. Variant-to-Datatype Mapping Inconsistencies

### Issue: Dispatch Table Variant Field

The `variant` field in the dispatch table (`nd500_instructions.c`) has DIFFERENT meanings for different instruction classes:

**Default mapping (most instructions)**:
- variant 0 = BYTE
- variant 1 = HALFWORD
- variant 2 = WORD
- variant 3 = FLOAT
- variant 4 = DOUBLE

**AND/OR/XOR (0xFC90-0xFCA7)**:
- variant 1 = BYTE (BY)
- variant 2 = HALFWORD (H)

**SHL/SHA/SHR (0xFCA8-0xFCB0)** (corrected; an earlier revision wrongly said H/W/D):
- variant 0 = BYTE (BY)
- variant 1 = HALFWORD (H)
- variant 2 = WORD (W)
- (prefixes_mask 0x0E = BY|H|W; matches manual sections 10.24-10.26 octal
  codes 176250B-176260B, the C# generator, and the test JSON. Shifts have
  no BI, F or D variants.)

**6-variant instructions (AssignTo, AssignFrom, etc.)**:
- variant 0 = BI (bit)
- variant 1 = BY (byte)
- variant 2 = H (halfword)
- variant 3 = W (word)
- variant 4 = F (float)
- variant 5 = D (double)

**Impact**: Status register validation is currently disabled because flag setting depends on correct data type detection. The variant inconsistencies cause incorrect sign bit detection for Z/S flags.

**Potential fixes**:
1. Regenerate dispatch table with consistent variant values
2. Add per-opcode-range handling in the decoder
3. Store explicit data type in dispatch table instead of variant index

---

## Test Data Issues (Known Bugs)

The following test failures are due to **buggy test data**, not implementation issues:

### GO Instruction Tests (~15 failures)

All GO displacement tests expect the same final PC (0x1002) regardless of offset. Test data was generated with offset=0 for all tests.

**C# formula**: `expectedPC = DefaultPC + SIZE_BRANCH + (uint)offset`

**Verified correct behavior**:
- `go $0` expects PC=0x1002, we get 0x1002 (0x1000 + 2 + 0)
- `go $4` expects PC=0x1002, we get 0x1006 (0x1000 + 2 + 4) - test data bug
- `go $20` expects PC=0x1002, we get 0x1016 (0x1000 + 2 + 20) - test data bug

Our implementation is correct (see Section 12 for fix details).

### Conditional Branch Tests (~50 failures)

Same issue as GO - all conditional branches (if=go, if<go, if>go, etc.) have buggy expected PC values.

### SET1 Bit Tests (~90 failures)

Tests use opcode 0x004D (W SET1) with CONSTANT_SHORT operand (e.g., `W1 SET1 $0`), but expect I1 to have bit N set. This confuses two different instructions:

- **SET1** (opcode 0x004D): Sets the destination operand to the value 1. Format: `W SET1 <dest>`
- **SETBI** (opcode 0xFE80): Sets a specific bit in the destination. Format: `W SETBI <dest>,<bit>`

The test expects `W1 SET1 $0` to set bit 0 in register I1 (result: 0x00000001), but SET1 should write the value 1 TO the operand (which is a constant, so no effect).

**Conclusion**: Test data was likely generated with incorrect instruction semantics. The implementation is correct.

### Entry/Return Instructions (~17 failures)

Tests for ENTB, ENTF, ENTS, RET, RETB, RETK, RETBK, RETD execute without proper call stack setup. These instructions require a preceding CALL to set up the stack frame.

---

## Not Yet Implemented

### Float/Double Operations

(Updated) F/D arithmetic is implemented, not stubbed: the ND storage format
(sign + 9-bit exponent, bias 256, hidden-bit mantissa) is faithful, but all
arithmetic converts ND -> host IEEE 754 -> ND with truncation. The manual's
L/G/St guard/round/sticky rounding is NOT modeled; bit-exactness against real
hardware is unverified. Remaining literal stubs: none in ARITHMETIC after the
MUL4/REM F/D implementation. Known limitation: no native ND rounding (would
require a SoftFloat-style core).

---

## Instruction Metadata Reference

From `instructions.json`, key metadata fields:
- `operandCount`: Number of operands
- `variantNumber`: Variant index within instruction family (NOT directly data type for some instructions)
- `totalVariants`: How many variants this instruction has (5 or 6)
- `metadata[]`: Operand roles (`OperandRole.Destination`, `OperandRole.Source`)
- `operandTemplates[]`: Operand encoding flags

---

## Testing

Run validation tests:
```bash
cd build
make test_instruction_validation
cd test
../bin/test_instruction_validation
```

### Test Options:
- `--continue`: Run all tests instead of stopping on first failure
- `--filter <str>`: Only run tests containing string
- `--start <n>`: Start at test number n
- `--count <n>`: Run only n tests

The test framework:
1. Loads `nd500_tests.json` (20,902 test cases)
2. For each test: sets up CPU/memory state, executes instruction, validates results
3. Supports both stop-on-fail and continue modes
4. Full status register (ST1) validation enabled
5. Shows failure summary by instruction type when using `--continue`

### Current Results (2025-12-01)

```
Results: 20,902 passed, 0 failed, 0 skipped (100.0% pass rate)
ALL TESTS PASSED

Status register (ST1) validation: ENABLED (see Sections 20-23)
```

---

## Summary for Assembler Review

The following fixes have implications for how the assembler generates code:

| Section | Issue | Assembler Impact |
|---------|-------|------------------|
| 5 | Big-endian constants | Assembler must emit multi-byte values in big-endian |
| 11 | 6-bit signed constants | Values 0x20-0x3F encode negative numbers -32 to -1 |
| 12 | Branch displacement | Relative to instruction START (first byte), not end |
| 18 | Big-endian 2-byte displacement | 2-byte address displacements must be big-endian |
| 19 | Variant inconsistencies | Data type prefixes may map to different variant values per instruction class |

---

## 20. Unified Variant-to-Datatype Algorithm

### File: `src/cpu/cpu_instr.c`

**Problem**: The variant field in the dispatch table had different meanings for different instruction classes (see Section 19). This caused incorrect data type detection and subsequently wrong status flag calculations (Z and S flags depend on data type for sign bit position).

**Root Cause**: The dispatch table stores a `prefixes_mask` field that indicates which data type prefixes (BI, BY, H, W, F, D) are valid for each instruction. The variant number is an index into this prefix list, not a direct data type mapping.

**Solution**: Implemented unified algorithm that builds a type list from the prefixes_mask and indexes into it:

```c
/* Prefix mask constants (matching C# InstructionPrefixes enum) */
#define ND500_PREFIX_BI   0x01   /* Bit field */
#define ND500_PREFIX_BY   0x02   /* Byte (8-bit) */
#define ND500_PREFIX_H    0x04   /* Halfword (16-bit) */
#define ND500_PREFIX_W    0x08   /* Word (32-bit) */
#define ND500_PREFIX_F    0x10   /* Float (32-bit) */
#define ND500_PREFIX_D    0x20   /* Double (64-bit) */

static Nd500DataType determine_datatype_from_prefixes(
    uint8_t prefixes_mask, uint8_t variant, bool *uses_float) {
    Nd500DataType types[6];
    int count = 0;

    /* Build ordered type list: BI, BY, H, W, F, D */
    if (prefixes_mask & ND500_PREFIX_BI) types[count++] = ND500_DTYPE_BYTE;
    if (prefixes_mask & ND500_PREFIX_BY) types[count++] = ND500_DTYPE_BYTE;
    if (prefixes_mask & ND500_PREFIX_H)  types[count++] = ND500_DTYPE_HALFWORD;
    if (prefixes_mask & ND500_PREFIX_W)  types[count++] = ND500_DTYPE_WORD;
    if (prefixes_mask & ND500_PREFIX_F)  types[count++] = ND500_DTYPE_WORD;
    if (prefixes_mask & ND500_PREFIX_D)  types[count++] = ND500_DTYPE_DOUBLEWORD;

    if (count == 0) { *uses_float = false; return ND500_DTYPE_WORD; }

    int idx = variant % count;
    /* Determine if this is a float type (F or D) */
    int int_count = __builtin_popcount(prefixes_mask & 0x0F);
    *uses_float = (idx >= int_count) && (prefixes_mask & 0x30);

    return types[idx];
}
```

**Example mappings**:

| Instruction | Prefixes | Variant | Data Type |
|-------------|----------|---------|-----------|
| AND (0xFC90) | BI\|BY\|H\|W | 1 | BYTE (BY) |
| SHL (0xFCA8) | BY\|H\|W | 0 | BYTE (BY) |
| SHL (0xFCA9) | BY\|H\|W | 1 | HALFWORD (H) |

**Assembly Test Cases**:

```asm
; test_variant_mapping.asm - Validate data type detection
; Assemble: nd500-as test_variant_mapping.asm -o test_variant.bin

        .org    $1000

; Test AND variants (BI|BY|H|W prefixes)
        w1 := $FF           ; I1 = 0xFF
        w2 := $80           ; I2 = 0x80
        by i1 and i2        ; Byte AND: 0xFF & 0x80 = 0x80
        ; Expected: I1=0x80, ST1.S=1 (sign bit 7 set)

        w1 := $FFFF         ; I1 = 0xFFFF
        w2 := $8000         ; I2 = 0x8000
        h i1 and i2         ; Halfword AND: 0xFFFF & 0x8000 = 0x8000
        ; Expected: I1=0x8000, ST1.S=1 (sign bit 15 set)

; Test SHL variants (BY|H|W prefixes)
        w1 := $40           ; I1 = 0x40
        by shl i1,$1        ; Byte shift left: 0x40 << 1 = 0x80
        ; Expected: I1=0x80, ST1.S=1 (sign bit 7 set)

        w1 := $4000         ; I1 = 0x4000
        h shl i1,$1         ; Halfword shift left: 0x4000 << 1 = 0x8000
        ; Expected: I1=0x8000, ST1.S=1 (sign bit 15 set)

        bp                  ; Stop
```

---

## 21. MUL (Multiply) Carry Flag Fix

### File: `src/cpu/instructions/ARITHMETIC/Multiply.c`

**Problem**: The MUL instruction was setting the carry flag on overflow, but per ND-500 Reference Manual section 11.7, MUL should NOT set the carry flag.

**Test Case**:
- Instruction: `h1 * $4096` (halfword multiply)
- I1 initial value: 0x1000 (4096)
- Operation: 4096 * 4096 = 16,777,216 (overflows 16-bit)
- Expected ST1: 0x0220 (Z=1, O=1) - result truncated to 0x0000
- Actual ST1 before fix: 0x0260 (Z=1, O=1, C=1)
- Difference: 0x40 = Carry flag incorrectly set

**Solution**:

```c
/* Before (incorrect): */
nd500_set_flags_zsco(cpu, masked_result, fi->data_type, carry, overflow);

/* After (correct - MUL clears carry per ND-500 Manual 11.7): */
nd500_set_flags_zsco(cpu, masked_result, fi->data_type, false, overflow);
```

**Assembly Test Cases**:

```asm
; test_mul_flags.asm - Validate MUL does NOT set carry flag
; Assemble: nd500-as test_mul_flags.asm -o test_mul.bin

        .org    $1000

; Test 1: Byte multiply with overflow
        w1 := $10           ; I1 = 16
        by1 * $10           ; 16 * 16 = 256, overflows byte (result 0x00)
        ; Expected: I1=0x00, ST1: Z=1, S=0, C=0, O=1 (0x0220)

; Test 2: Halfword multiply with overflow
        w1 := $1000         ; I1 = 4096
        h1 * $4096          ; 4096 * 4096 = 16777216, overflows halfword
        ; Expected: I1=0x0000, ST1: Z=1, S=0, C=0, O=1 (0x0220)

; Test 3: Word multiply without overflow
        w1 := $100          ; I1 = 256
        w1 * $100           ; 256 * 256 = 65536 (fits in word)
        ; Expected: I1=0x00010000, ST1: Z=0, S=0, C=0, O=0 (0x0000)

; Test 4: Multiply resulting in zero (no overflow)
        w1 := $0            ; I1 = 0
        w1 * $1234          ; 0 * anything = 0
        ; Expected: I1=0, ST1: Z=1, S=0, C=0, O=0 (0x0020)

        bp                  ; Stop
```

---

## 22. ABS (Absolute Value) Overflow Detection Fix

### File: `src/cpu/instructions/ARITHMETIC/Abs.c`

**Problem**: The ABS instruction was missing overflow detection for "most negative" values. In two's complement, the most negative value (e.g., -128 for byte, -32768 for halfword) cannot be negated because there is no positive representation. When negated, the result stays the same negative value, which is an overflow condition.

**Most negative values by data type**:
- BYTE: 0x80 (-128)
- HALFWORD: 0x8000 (-32768)
- WORD: 0x80000000 (-2147483648)

**Test Case**:
- Instruction: `BY1 ABS` (byte absolute value)
- I1 initial value: 0x80 (-128 as signed byte)
- Expected result: 0x80 (unchanged - cannot negate)
- Expected ST1: 0x0280 (S=1, O=1) - overflow with sign bit still set
- Actual ST1 before fix: 0x0000 (no flags set)

**Solution**:

```c
bool isMinNegative = false;

switch (fi->data_type) {
    case ND500_DTYPE_BYTE:
        isMinNegative = ((value & 0xFF) == 0x80);
        break;
    case ND500_DTYPE_HALFWORD:
        isMinNegative = ((value & 0xFFFF) == 0x8000);
        break;
    case ND500_DTYPE_WORD:
        isMinNegative = ((value & 0xFFFFFFFF) == 0x80000000);
        break;
}

/* After negation, update flags for overflow case */
cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_O);
if (result == 0) cpu->ST1 |= ND500_FLAG_Z;
if (isMinNegative) {
    cpu->ST1 |= ND500_FLAG_S;  /* Sign flag set - result still negative */
    cpu->ST1 |= ND500_FLAG_O;  /* Overflow flag set */
}
```

**Assembly Test Cases**:

```asm
; test_abs_overflow.asm - Validate ABS overflow detection
; Assemble: nd500-as test_abs_overflow.asm -o test_abs.bin

        .org    $1000

; Test 1: Byte ABS with most negative value (overflow)
        w1 := $80           ; I1 = 0x80 (-128 as signed byte)
        by1 abs             ; Cannot negate -128
        ; Expected: I1=0x80, ST1: Z=0, S=1, O=1 (0x0280)

; Test 2: Halfword ABS with most negative value (overflow)
        w1 := $8000         ; I1 = 0x8000 (-32768 as signed halfword)
        h1 abs              ; Cannot negate -32768
        ; Expected: I1=0x8000, ST1: Z=0, S=1, O=1 (0x0280)

; Test 3: Word ABS with most negative value (overflow)
        w1 := $80000000     ; I1 = 0x80000000 (-2147483648)
        w1 abs              ; Cannot negate -2147483648
        ; Expected: I1=0x80000000, ST1: Z=0, S=1, O=1 (0x0280)

; Test 4: Byte ABS with normal negative value (no overflow)
        w1 := $FF           ; I1 = 0xFF (-1 as signed byte)
        by1 abs             ; |-1| = 1
        ; Expected: I1=0x01, ST1: Z=0, S=0, O=0 (0x0000)

; Test 5: ABS of positive value (no change)
        w1 := $7F           ; I1 = 0x7F (127)
        by1 abs             ; |127| = 127
        ; Expected: I1=0x7F, ST1: Z=0, S=0, O=0 (0x0000)

; Test 6: ABS of zero
        w1 := $0            ; I1 = 0
        by1 abs             ; |0| = 0
        ; Expected: I1=0x00, ST1: Z=1, S=0, O=0 (0x0020)

        bp                  ; Stop
```

---

## 23. GETBF (Get Bit Field) Sign Flag Fix

### File: `src/cpu/instructions/BITFIELD/Getbf.c`

**Problem**: The GETBF instruction was setting the S (sign) flag based on the MSB of the extracted bit field (using field_size), but should set it based on the MSB of the data type (byte=bit 7, halfword=bit 15, word=bit 31).

**Test Case**:
- Instruction: `BY GETBF I1,$0,$4` (byte, extract 4 bits starting at bit 0)
- I1 initial value: 0x78
- Extracted field: 0x08 (bits 3-0 of 0x78)
- Expected ST1: 0x0000 (Z=0, S=0 - byte bit 7 of 0x08 is 0)
- Actual ST1 before fix: 0x0080 (S=1 - field MSB bit 3 of 0x08 is 1)

**Why it was wrong**:
- Old code checked: `bit_field & (1 << (field_size-1))` = `0x08 & 0x08` = 1 (field MSB)
- Should check: `bit_field & 0x80` = `0x08 & 0x80` = 0 (byte MSB)

**Solution**:

```c
/* Before (incorrect - uses field MSB): */
if (field_size > 0 && (bit_field & (1U << (field_size - 1)))) {
    cpu->ST1 |= ND500_FLAG_S;
} else {
    cpu->ST1 &= ~ND500_FLAG_S;
}

/* After (correct - uses data_type MSB via helper): */
nd500_set_flags_zs(cpu, (uint64_t)bit_field, fi->data_type);
```

**Assembly Test Cases**:

```asm
; test_getbf_flags.asm - Validate GETBF S flag uses data type MSB
; Assemble: nd500-as test_getbf_flags.asm -o test_getbf.bin

        .org    $1000

; Test 1: Byte GETBF, field MSB=1 but byte MSB=0
        w1 := $78           ; I1 = 0x78 = 0111_1000
        by getbf i1,$0,$4   ; Extract bits 3-0: 0x08 = 0000_1000
        ; Field MSB (bit 3) = 1, but Byte MSB (bit 7) = 0
        ; Expected: I1=0x08, ST1: Z=0, S=0 (0x0000)

; Test 2: Byte GETBF, both MSBs set
        w1 := $F8           ; I1 = 0xF8 = 1111_1000
        by getbf i1,$0,$4   ; Extract bits 3-0: 0x08 = 0000_1000
        ; Field MSB (bit 3) = 1, Byte MSB (bit 7) = 0
        ; Expected: I1=0x08, ST1: Z=0, S=0 (0x0000)

; Test 3: Byte GETBF, byte result with byte MSB set
        w1 := $FF           ; I1 = 0xFF
        by getbf i1,$0,$8   ; Extract all 8 bits: 0xFF
        ; Byte MSB (bit 7) = 1
        ; Expected: I1=0xFF, ST1: Z=0, S=1 (0x0080)

; Test 4: Halfword GETBF, field MSB set but halfword MSB not
        w1 := $0800         ; I1 = 0x0800 = bit 11 set
        h getbf i1,$8,$4    ; Extract bits 11-8: 0x08
        ; Field MSB (bit 3) = 1, Halfword MSB (bit 15) = 0
        ; Expected: I1=0x08, ST1: Z=0, S=0 (0x0000)

; Test 5: Word GETBF, result zero
        w1 := $00           ; I1 = 0x00
        w getbf i1,$0,$4    ; Extract bits 3-0: 0x00
        ; Expected: I1=0x00, ST1: Z=1, S=0 (0x0020)

        bp                  ; Stop
```

---

## Status Flag Summary

After fixes in Sections 20-23, status register validation is now fully enabled:

| Instruction | Flag | Behavior |
|-------------|------|----------|
| **MUL** | C (Carry) | Always cleared (not set on overflow) |
| **MUL** | O (Overflow) | Set if result exceeds data type range |
| **MUL** | Z, S | Based on masked result and data type |
| **ABS** | O (Overflow) | Set for most negative values (0x80, 0x8000, etc.) |
| **ABS** | S (Sign) | Set if overflow (result unchanged, still negative) |
| **ABS** | Z | Set if result is zero |
| **GETBF** | S (Sign) | Based on data type MSB, NOT field MSB |
| **GETBF** | Z | Based on extracted field value |

### Current Test Results (2025-12-01)

```
Results: 20,902 passed, 0 failed, 0 skipped (100.0% pass rate)
ALL TESTS PASSED

Status register (ST1) validation: ENABLED
```

---

## 24. Disassembler (nd500-dis) Verification

### Analysis Date: 2025-12-01

**Question**: Do the bugs fixed in Sections 20-23 affect the disassembler (nd500-dis)?

**Answer**: No. The disassembler already uses the correct unified algorithm via `nd500_instr_dtype_prefix()`.

### Disassembler Architecture

The nd500x disassembler uses the same infrastructure as the CPU decoder:

1. **Dispatch Table Lookup**: `nd500_instr_lookup()` returns instruction metadata
2. **Data Type Detection**: `nd500_instr_dtype_prefix()` in `src/cpu/cpu_instr.c`
3. **Mnemonic Generation**: `nd500_disasm_instruction()` in `src/disasm/nd500_disasm.c`

The key function `nd500_instr_dtype_prefix()` already uses the `prefixes_mask` field:

```c
/* From src/cpu/cpu_instr.c, lines 263-281 */
const char* nd500_instr_dtype_prefix(const Nd500InstructionMeta* instr) {
    static const char* prefixes[] = {"bi", "by", "h", "w", "f", "d"};
    static const uint8_t bits[] = {
        ND500_PREFIX_BI, ND500_PREFIX_BY, ND500_PREFIX_H,
        ND500_PREFIX_W, ND500_PREFIX_F, ND500_PREFIX_D
    };

    int target = instr->variant;
    int count = 0;

    for (int i = 0; i < 6; i++) {
        if (instr->prefixes_mask & bits[i]) {
            if (count == target) return prefixes[i];
            count++;
        }
    }
    return "";  /* No prefix */
}
```

This correctly indexes through the available prefixes for each instruction class.

### Verification Test

Created test file `/home/ronny/repos/nd500x/test/disasm_mul_abs_test.s`:

```asm
.text
.org	4096

# Test MUL instruction data type prefixes
_test_mul:
	by1 := $10
	by1 * $2
	h1 := $100
	h1 * $3
	w1 := $1000
	w1 * $4

# Test ABS instruction data type prefixes
_test_abs:
	by1 := $80
	by1 abs
	h1 := $8000
	h1 abs
	w1 := $80000000
	w1 abs

_end:
	ret
```

### Assemble and Disassemble

```bash
$ nd500-as disasm_mul_abs_test.s -o disasm_mul_abs_test.bin
$ echo -e "load disasm_mul_abs_test.bin\nd 0x1000 50\nq" | nd500x --debug
```

### Disassembly Output (Verified Correct)

```
00001000:                                   _test_mul:
  00001000: 04 0A                   by1 :=       $10
  00001002: FC 44 02                by1 *        $2
  00001005: 08 CD 64                h1 :=        $100
  00001008: FC 48 03                h1 *         $3
  0000100B: 0C CE 03 E8             w1 :=        $1000
  0000100F: 6C 04                   w1 *         $4
00001011:                                   _test_abs:
  00001011: 04 CD 50                by1 :=       $80
  00001014: FF 00                   by1 abs
  00001016: 08 CE 1F 40             h1 :=        $8000
  0000101A: FF 04                   h1 abs
  0000101C: 0C CF 04 C4 B4 00       w1 :=        $80000000
  00001022: FF 08                   w1 abs
00001024:                                   _end:
  00001024: 80                      ret
```

All data type prefixes display correctly:
- **MUL**: `by1 *`, `h1 *`, `w1 *`
- **ABS**: `by1 abs`, `h1 abs`, `w1 abs`

### nd500-as Assembler Limitations

During testing, the following nd500-as bugs were identified:

1. **Shift instructions crash**: SHL, SHA, SHR cause assembler segfaults
2. **Bitfield instructions crash**: GETBI, PUTBI, CLEBI, SETBI, GETBF, PUTBF all segfault

These bugs are documented in `/home/ronny/repos/ragge/pcc-nd500/tests/asm_generated/phase2_intermediate/`.

Because of these assembler limitations, full verification of shift and bitfield instruction disassembly requires:
- Using the nd500x debug mode to manually enter opcodes, OR
- Creating raw binary test files with known byte sequences

### Conclusion

**No fixes required for nd500-dis**. The disassembler uses `nd500_instr_dtype_prefix()` which correctly implements the unified variant-to-datatype algorithm via the `prefixes_mask` field.

### Test Files Created

- `/home/ronny/repos/nd500x/test/disasm_mul_abs_test.s` - MUL and ABS data type prefix test
- `/home/ronny/repos/nd500x/test/disasm_mul_abs_test.bin` - Assembled binary

---

## 25. TEMM Enforcement for OTE / SETE / CLTE and LREGBL Privilege Fix

### Files: `src/cpu/instructions/MOVE/Ote1Set.c`, `Ote2Set.c`,
### `src/cpu/instructions/CONTROL/Sete.c`, `Clte.c`,
### `src/cpu/instructions/SYSTEM/Lregbl.c`, `src/cpu/cpu.c`,
### `src/cpu/instruction_helpers.{c,h}`

**Background**: The Own Trap Enable register (OTE) is writable, but each bit is
modifiable only if the corresponding bit in the Trap Enable Modification Mask
(TEMM) is set. The instruction pages are authoritative:

- SETE (manual 10.24, ref line 10177): "The specified bit in OTE is set. The
  <bit no> operand is compared with a modify mask (TEMM) ... An attempt to
  modify a non-modifiable bit will cause an illegal operand value trap."
- CLTE (manual 10.25, ref line 10209): same wording for clearing.
- OTE load (ref line 10253): "the operand is compared with a modify mask
  (TEMM) ... An attempt to modify a non-modifiable bit in the Own Trap Enable
  register will cause an illegal operand value trap."

(The general architecture text at line 1905 says such a change is "ignored";
the stricter instruction-specific pages govern these opcodes and are used here.)

**Problem**: OTE1:=, OTE2:=, SETE and CLTE wrote OTE unconditionally, ignoring
TEMM entirely. No illegal-operand-value trap was raised for a protected bit.

**Fix**: Added a shared helper `nd500_temm_allows_change(changed_bits, temm)`
(returns `(changed_bits & ~temm) == 0`) and applied it:
- OTE load: `changed = new ^ current`; if any changed bit is not modifiable,
  raise `TRAP_IOV` and leave OTE unchanged.
- SETE/CLTE: gate the single target bit against the relevant TEMM half
  (TEMM1 for bits 0-31, TEMM2 for bits 32-63).

**TEMM cold-boot default**: A real domain loads TEMM from its Domain
Information Table (ref line 8133). At cold boot / in a bare (root) context no
mother has restricted the process, so `nd500_cpu_init` now defaults
`TEMM1 = TEMM2 = 0xFFFFFFFF` (all bits modifiable). A zero default would have
trapped every OTE write. Enforcement therefore only bites once a domain
installs a restrictive mask.

### LREGBL privilege leak + undefined-shift fix (`Lregbl.c`)

**Problem 1 (privilege leak)**: The non-privileged mask reduction only cleared
ST2/PS/CED/CAD. CTE1 is register number 32 = mask bit 31, which is
representable in the 32-bit mask and therefore escaped the filter, letting
non-privileged code overwrite CTE1.

**Problem 2 (undefined behaviour)**: The load loop ran `reg_num` 1..37 and
computed `1u << (reg_num - 1)`. For reg_num > 32 the shift count is >= 32,
which is undefined in C (on x86 it aliases low mask bits, e.g. reg_num 33
tested mask bit 0).

**Fix**: The mask operand is a 32-bit word, so only reg_num 1..32 (up to CTE1)
are addressable; the loop is capped at 32 (removing the UB), and an in-loop
guard skips the privileged registers ST2(18), PS(19), CED(24), CAD(25) and
CTE1(32) when not privileged. CTE2/MTE/TEMM (reg 33-37) are unreachable via a
32-bit mask.

### Tests

`test/test_ote_instructions.c` gained TEMM and LREGBL coverage (42 checks total):
- ote1:= blocked by TEMM (IOV raised, OTE1 unchanged) and allowed when only
  modifiable bits change.
- SETE blocked / allowed by TEMM; CLTE blocked by TEMM.
- LREGBL cannot write CTE1 in non-privileged mode; can in privileged mode.

All 18 ctest targets and the 39,598-case validation suite remain green.
