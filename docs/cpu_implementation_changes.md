# ND500X CPU Implementation Changes

This document describes corrections and changes made to the nd500x CPU emulator based on validation testing against the C# reference implementation.

## Test Results Summary

- **Tests Passed**: 21,145 of 21,305 (99.2%)
- **Test Framework**: `test/test_instruction_validation.c` using `test/nd500_tests.json`
- **Known Test Data Issues**: ~70 tests for branch instructions have buggy expected PC values
- **Remaining Failures**: ~90 SET1 instruction failures (separate issue from CONSTANT_SHORT)

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

## Test Data Issues (Known Bugs)

The following test failures are due to **buggy test data**, not implementation issues:

### GO Instruction Tests (~12 failures)

All GO displacement tests (Go_Offset0, Go_Offset2, Go_Offset4, Go_Offset10, Go_Offset20, Go_Offset50) expect the same final PC value (0x1002) regardless of displacement. This is clearly wrong for a relative branch instruction.

**Evidence**:
- `go $0` expects PC=0x1002 (CORRECT - no displacement from end of instruction)
- `go $2` expects PC=0x1002 (WRONG - should be 0x1004)
- `go $4` expects PC=0x1002 (WRONG - should be 0x1006)

Our implementation correctly calculates PC-relative branches.

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

## 12. SHR (Circular Shift) Direction and Large Shift Count

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

## Not Yet Implemented

### Float/Double Operations

Float and double arithmetic operations are stubbed out. Integer variants work correctly.

---

## Instruction Metadata Reference

From `instructions.json`, key metadata fields:
- `operandCount`: Number of operands
- `variantNumber`: Variant index within instruction family (NOT directly data type for 6-variant instructions)
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
1. Loads `nd500_tests.json` (21,305 test cases)
2. For each test: sets up CPU/memory state, executes instruction, validates results
3. Supports both stop-on-fail and continue modes
4. Skips status register validation (pending updated test data)
5. Shows failure summary by instruction type when using `--continue`

### Current Results (2024-11-30)

```
Results: 21,145 passed, 160 failed, 0 skipped (99.2% pass rate)

Failures by instruction:
  W1 (SET1)           : 90 failures (SET1 instruction issue, not AssignTo)
  Branch instructions : ~70 failures (test data PC issues)
```
