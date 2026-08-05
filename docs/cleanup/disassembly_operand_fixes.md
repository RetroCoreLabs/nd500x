# Disassembly and Operand Handling Fixes

This document describes the fixes required for correct operand formatting in disassembly and execution of instructions like BMOVE. These fixes apply to:
- **nd500x** (C emulator)
- **nd500-dis** (standalone disassembler)
- **C# RetroCore ND500 CPU**

---

## Understanding the Disassembly Output

Before diving into the fixes, here's what the disassembly syntax means:

### Example: `w bmove $0xDEADBEEF,r1.(0x0),r2`

| Part | What It Means |
|------|---------------|
| `w` | **Word** operation - each element is 4 bytes |
| `bmove` | Block move instruction (like memcpy/memset) |
| `$0xDEADBEEF` | **Constant value** - the literal number 0xDEADBEEF |
| `r1.(0x0)` | **Pre-indexed address** - memory at (I1 + 0) |
| `r2` | **Register** - the value stored in I2 |

### What the Addressing Modes Mean

| Syntax | Mode | Meaning | Example |
|--------|------|---------|---------|
| `$value` | CONSTANT | Use this exact value | `$0xFF` = the number 255 |
| `rN` | REGISTER | Use value in register IN | `r2` = value in I2 |
| `rN.(offset)` | PRE-INDEXED | Memory address = IN + offset | `r1.(0x10)` = memory at I1+16 |
| `$addr` | ABSOLUTE | Read from memory at address | `$0x1000` = read from addr 0x1000 |

### Registers

The ND-500 has 4 integer registers (I1-I4):
- In assembly: written as `r1`, `r2`, `r3`, `r4`
- In C code: stored as `cpu->I[0]`, `cpu->I[1]`, `cpu->I[2]`, `cpu->I[3]`

**This indexing difference (r1 = I[0]) was the source of a critical bug.**

---

## Summary of Issues

Three categories of bugs were identified:

1. **Disassembly output format** - Operands displayed incorrectly
2. **Register index off-by-one** - Array indexing used 1-based values in 0-based arrays
3. **Operand addressing mode confusion** - `$value` syntax interpretation

---

## 1. Disassembly Output Format Fixes

### File: `src/machine/debug_api.c` (nd500x debugger)

The `format_operand_impl` function had incorrect output for several addressing modes:

### 1.1 CONSTANT Mode (address_code 0xCC-0xCF)

**Bug:** Used `#` prefix
**Fix:** Use `$` prefix to match nd500-dis/nd500-as syntax

```c
case ND500_ADDR_CONSTANT: {
    /* Extended constant: $value (same as nd500-dis uses) */
    if (p < e) *p++ = '$';
    int n = fmt_unsigned(p, (size_t)(e-p), val);
    p += (n>0 && n < (e-p)? n : (e-p));
    break;
}
```

### 1.2 REGISTER Mode (address_code 0xD0-0xD3)

**Bug:** Used internal `op->reg` field directly
**Fix:** Derive register number from `address_code & 0x03` and use `rN` format

```c
case ND500_ADDR_REGISTER: {
    /* Register: r1-r4 derived from address_code 0xD0-0xD3 */
    int regnum = (op->address_code & 0x03) + 1;  /* 0xD0=r1, 0xD1=r2, etc. */
    int n = snprintf(p, (size_t)(e-p), "r%d", regnum);
    p += (n>0 && n < (e-p)? n : (e-p));
    break;
}
```

### 1.3 PREINDEXED Mode (address_code 0xF4-0xFF)

**Bug:** Used wrong syntax `disp(IN)` and wrong register
**Fix:** Use `rN.(disp)` format to match nd500-as syntax

```c
case ND500_ADDR_PREINDEXED: {
    /* Pre-indexed: rN.(disp) - matches nd500-dis/nd500-as syntax */
    int regnum = (op->address_code & 0x03) + 1;  /* low 2 bits = register 1-4 */
    char vbuf[32];
    fmt_signed(vbuf, sizeof(vbuf), sval);
    int n = snprintf(p, (size_t)(e-p), "r%d.(%s)", regnum, vbuf);
    p += (n>0 && n < (e-p)? n : (e-p));
    break;
}
```

### Address Code to Register Mapping

| Address Code | Mode | Register |
|--------------|------|----------|
| 0xD0 | REGISTER | r1 (I1) |
| 0xD1 | REGISTER | r2 (I2) |
| 0xD2 | REGISTER | r3 (I3) |
| 0xD3 | REGISTER | r4 (I4) |
| 0xF4-0xF7 | PREINDEXED (byte disp) | r1-r4 |
| 0xF8-0xFB | PREINDEXED (half disp) | r1-r4 |
| 0xFC-0xFF | PREINDEXED (word disp) | r1-r4 |

---

## 2. Register Index Off-by-One Bug

### File: `src/cpu/cpu_instr.c`

The `op->reg` field is set to values 1-4 (from `(address_code & 0x03) + 1`), but the `cpu->I[]` array is 0-indexed (I[0]=I1, I[1]=I2, etc.).

### 2.1 PREINDEXED Effective Address Calculation

**Bug:**
```c
address = cpu->I[op->reg];  // Wrong: reg=1 accesses I[1]=I2
```

**Fix:**
```c
if (op->reg >= 1 && op->reg <= 4) {
    address = (uint32_t)((int32_t)cpu->I[op->reg - 1] + displacement);
}
```

### 2.2 REGISTER Mode Read

**Bug:**
```c
if (op->reg < 4) return cpu->I[op->reg];  // Wrong index and check
```

**Fix:**
```c
if (op->reg >= 1 && op->reg <= 4) return cpu->I[op->reg - 1];
```

### 2.3 REGISTER Mode Write

**Bug:**
```c
if (op->reg < 4) cpu->I[op->reg] = value;  // Wrong index
```

**Fix:**
```c
if (op->reg >= 1 && op->reg <= 4) cpu->I[op->reg - 1] = value;
```

### 2.4 Post-Indexed Modes

For LOCAL_PI, LOCAL_IND_PI, ABSOLUTE_PI modes, the register is derived directly from `address_code & 0x03` (giving 0-3), which correctly indexes into `cpu->I[]`:

```c
case ND500_ADDR_LOCAL_PI:
case ND500_ADDR_LOCAL_IND_PI:
case ND500_ADDR_ABSOLUTE_PI: {
    uint8_t pi_reg = op->address_code & 0x03;  // 0-3 maps to I1-I4
    address = (uint32_t)((int32_t)address + (int32_t)cpu->I[pi_reg]);
    break;
}
```

---

## 3. Addressing Mode Clarification

### $value vs Constants

In ND-500 assembly, `$value` syntax encodes differently depending on context:

| Address Code | Mode | Meaning |
|--------------|------|---------|
| 0xC4-0xC7 | ABSOLUTE | Memory address (read value from address) |
| 0xCC-0xCF | CONSTANT | Immediate value (use value directly) |

For BMOVE's count operand with `$0x4`:
- Encodes as 0xC4 (ABSOLUTE with word address)
- Reads count from memory address 0x4 (not the constant 4!)

To use a literal count in BMOVE:
- Use a register: `w bmove $0xDEADBEEF,r1.(0),r2`
- Use short constant (0-63): `w bmove $0xDEADBEEF,r1.(0),4`

---

## 4. C# RetroCore Fixes Required

### 4.1 Disassembly Formatting

Update `FormatOperand` method to match the fixes above:
- Use `$` prefix for CONSTANT mode
- Use `rN` for REGISTER mode (not `IN` or `WN`)
- Use `rN.(disp)` for PREINDEXED mode

### 4.2 Register Index Fix

In `ComputeEffectiveAddress` or equivalent:

```csharp
// Before (WRONG):
if (Operand.Mode == AddressMode.PreIndexed)
{
    address = (uint)((int)Registers.I[operand.Reg] + displacement);
}

// After (CORRECT):
if (Operand.Mode == AddressMode.PreIndexed && operand.Reg >= 1 && operand.Reg <= 4)
{
    address = (uint)((int)Registers.I[operand.Reg - 1] + displacement);
}
```

### 4.3 BMOVE Implementation

The C# BMOVE uses `EffectiveAddress` for source in copy mode:
```csharp
uint srcAddr = fi.Operands[0].EffectiveAddress + (uint)(i * fi.DataWidth);
```

This is correct for memory-to-memory copy. For fill mode (when source is CONSTANT or REGISTER), use:
```csharp
ulong fillValue = ReadOperandValue(fi.Operands[0], fi.DataType);
```

---

## 5. Test Cases

### Correct BMOVE Disassembly

```
Input bytes: FE 79 CF DE AD BE EF F4 00 D1
Expected:    w bmove      $0xDEADBEEF,r1.(0x0),r2
```

### Register Mapping Test

| Instruction | Expected Disassembly |
|-------------|---------------------|
| `set I1 = 0x1000; w bmove $0xAB,r1.(0),r2` | Fill 4 words at 0x1000 with 0xAB |
| Register r1 accesses cpu->I[0] (I1) | |
| Register r2 accesses cpu->I[1] (I2) | |

### Round-Trip Test

```bash
# Assemble
echo 'w bmove $0xF0F0F0F0,r1.(0),r2' | nd500-as - -o test.o

# Verify bytes
# Expected: FE 79 CF F0 F0 F0 F0 F4 00 D1

# Disassemble
nd500-dis -a test.o
# Expected: w bmove $0xF0F0F0F0,r1.(0x0),r2
```

---

## Files Modified

| File | Changes |
|------|---------|
| `src/machine/debug_api.c` | Fixed CONSTANT, REGISTER, PREINDEXED formatting |
| `src/cpu/cpu_instr.c` | Fixed register indexing (4 locations) |

---

## Related Documentation

- ND-500 Reference Manual, Chapter 7: Data Types and Registers
- ND-500 Reference Manual, Chapter 8: Operand Specifiers and Addressing
- ND-500 Reference Manual, Section 15.1: Block Move and Fill (BMOVE)
