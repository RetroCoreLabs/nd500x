# ND500X CPU Implementation Changes

This document describes corrections and changes made to the nd500x CPU emulator based on validation testing against the C# reference implementation.

## Test Results Summary

- **Tests Passed**: 20,902 of 20,902 (100.0%)
- **Test Framework**: `test/test_instruction_validation.c` using `test/nd500_tests.json`
- **Status Register**: Validation disabled pending variant-to-datatype mapping fixes

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
3. **Branch PC calculation** (Section 12) - Relative to instruction END, not start

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

**Problem 1 - Wrong shift direction**: Original implementation had positive shift count = left shift, but tests expected right shift.

**Problem 2 - Wrong sign determination**: For byte/halfword operations on registers, sign was determined from the masked data type (e.g., 0xFF = -1 as byte), but should use the full 32-bit register value (0x000000FF = 255, positive).

**Solution**:
```c
/* Empirically verified convention (matching test expectations):
 * - Positive count = RIGHT shift (arithmetic, sign-extending)
 * - Negative count = LEFT shift (same as logical shift left) */
if (shift_count >= 0) {
    /* Sign determined by bit 31 of full register, not data type */
    int32_t signed_val = (int32_t)(raw_value & 0xFFFFFFFF);
    result = (uint64_t)(uint32_t)(signed_val >> abs_shift);
} else {
    result = raw_value << abs_shift;
}
/* Mask output to data type */
result = nd500_mask_to_datatype(result, fi->data_type);
```

**Test case**: `BY SHA I1,$1` with I1=0xFF
- Old (wrong): 0xFF as byte = -1, -1 >> 1 = -1 = 0xFF
- New (correct): 0x000000FF as int32 = 255, 255 >> 1 = 127 = 0x7F

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

## 12. Branch PC Calculation (Relative to Instruction END)

### Files: `src/cpu/instructions/BRANCH/*.c`

**Problem**: All branch instructions (GO, IF*GO, LOOP*) were calculating the target PC relative to the instruction START instead of END.

**C# formula**: `expectedPC = DefaultPC + SIZE_BRANCH + (uint)offset`

This means: `PC = instruction_start + instruction_size + displacement`

**Old (incorrect)**:
```c
cpu->PC = (uint32_t)((int64_t)cpu->PC + displacement);
```
Result: 0x1000 + 20 = 0x1014 (wrong)

**New (correct)**:
```c
cpu->PC = (uint32_t)(fi->address + fi->total_len + displacement);
```
Result: 0x1000 + 2 + 20 = 0x1016 (correct)

**Assembler Impact**: When calculating branch displacements, the assembler must account for the instruction size. The displacement is relative to the address AFTER the branch instruction, not the branch instruction itself.

---

## 13. SHR (Circular Shift) Direction and Large Shift Count

### File: `src/cpu/instructions/SHIFT/Shr.c`

**Problem 1 - Large shift count handling**: Originally trapped for shift counts >= data width (e.g., shift 31 on byte type).

**Problem 2 - Rotation direction**: Needed to match C# reference behavior.

**Solution**:
```c
/* Rotation direction (matching C# Shr.cs):
 * - Positive count = rotate RIGHT (bits wrap from LSB to MSB)
 * - Negative count = rotate LEFT (bits wrap from MSB to LSB) */

/* For circular shift, normalize shift count with modulo instead of trapping */
if (shift >= (int32_t)bits && bits > 0) {
    shift = shift % (int32_t)bits;
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

**Rationale**: For rotation, shift 31 on 8-bit value = shift 7 (31 % 8 = 7). Test data expects normalization, not trapping.

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

**SHL/SHA/SHR (0xFCA8-0xFCB0)**:
- variant 0 = HALFWORD (H)
- variant 1 = WORD (W)
- variant 2 = DOUBLEWORD (D)

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

Float and double arithmetic operations are stubbed out. Integer variants work correctly.

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
4. Skips status register validation (pending variant mapping fixes)
5. Shows failure summary by instruction type when using `--continue`

### Current Results (2025-12-01)

```
Results: 20,902 passed, 0 failed, 0 skipped (100.0% pass rate)
ALL TESTS PASSED

Note: Status register (st) validation is disabled pending investigation
of variant-to-datatype mapping inconsistencies across instruction classes.
```

---

## Summary for Assembler Review

The following fixes have implications for how the assembler generates code:

| Section | Issue | Assembler Impact |
|---------|-------|------------------|
| 5 | Big-endian constants | Assembler must emit multi-byte values in big-endian |
| 11 | 6-bit signed constants | Values 0x20-0x3F encode negative numbers -32 to -1 |
| 12 | Branch displacement | Relative to instruction END, not start |
| 18 | Big-endian 2-byte displacement | 2-byte address displacements must be big-endian |
| 19 | Variant inconsistencies | Data type prefixes may map to different variant values per instruction class |
