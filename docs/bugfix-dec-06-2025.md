# ND-500 Emulator Bug Fixes - December 6, 2025

This document summarizes all bug fixes made to the nd500x C emulator.
Use this as a reference for validating and fixing the C# RetroCore ND-500 implementation.

## Summary of Bugs Fixed

| Bug | Severity | File(s) | Commit |
|-----|----------|---------|--------|
| Register indexing off-by-one | Critical | cpu_instr.c | `99986e0` |
| Disassembly operand formatting | Medium | debug_api.c | `069b07c` |
| IF -K GO condition inverted | Critical | IfKeyGo.c | `bc4074f` |
| COMP2 carry flag convention | Critical | Comp2.c | `8a3a520` |
| SHL IOV trap should be NOOP | Medium | Shl.c | `813078c` |

---

## Bug 1: Register Indexing Off-by-One (CRITICAL)

### Commit
```
99986e0 fix(cpu): Correct register indexing for PREINDEXED and REGISTER modes
```

### Problem
The `op->reg` field is 1-indexed (values 1-4 for I1-I4), but the code was using it directly as a 0-indexed array access into `cpu->I[]`.

This caused:
- BMOVE writing to wrong memory addresses
- All PREINDEXED and REGISTER mode instructions using wrong registers
- Post-indexed modes using wrong index registers

### File
`/home/ronny/repos/nd500x/src/cpu/cpu_instr.c`

### C# Equivalent
Check `/Emulated.HW/ND/CPU/ND500/` for similar register access patterns.

### Fix Details

#### Fix 1: PREINDEXED effective address calculation
```diff
 case ND500_ADDR_PREINDEXED:
     /* Pre-indexed - I[n] + displacement */
-    if (op->reg < 4) {
-        address = (uint32_t)((int32_t)cpu->I[op->reg] + displacement);
+    /* op->reg is 1-4, cpu->I[] is 0-indexed (I[0]=I1, I[1]=I2, etc.) */
+    if (op->reg >= 1 && op->reg <= 4) {
+        address = (uint32_t)((int32_t)cpu->I[op->reg - 1] + displacement);
     }
```

#### Fix 2: POST-INDEXED modes
For post-indexed modes, derive register directly from `address_code` bits 0-1:
```diff
 case ND500_ADDR_LOCAL_PI:
 case ND500_ADDR_LOCAL_IND_PI:
-case ND500_ADDR_ABSOLUTE_PI:
+case ND500_ADDR_ABSOLUTE_PI: {
     /* Post-indexed - add I[reg] value */
-    if (op->reg < 4) {
-        address = (uint32_t)((int32_t)address + (int32_t)cpu->I[op->reg]);
-    }
+    /* reg from address_code: bits 0-1 give 0-3, maps to I1-I4 */
+    uint8_t pi_reg = op->address_code & 0x03;
+    address = (uint32_t)((int32_t)address + (int32_t)cpu->I[pi_reg]);
     break;
+}
```

#### Fix 3: REGISTER mode read
```diff
 case ND500_ADDR_REGISTER:
-    if (op->reg < 4) return cpu->I[op->reg];
+    /* op->reg is 1-4, cpu->I[] is 0-indexed */
+    if (op->reg >= 1 && op->reg <= 4) return cpu->I[op->reg - 1];
     return 0;
```

#### Fix 4: REGISTER mode write
```diff
 case ND500_ADDR_REGISTER:
-    if (op->reg < 4) cpu->I[op->reg] = value;
+    /* op->reg is 1-4, cpu->I[] is 0-indexed */
+    if (op->reg >= 1 && op->reg <= 4) cpu->I[op->reg - 1] = value;
     break;
```

#### Fix 5: Reset extra_operand_count
```diff
+/* Always reset extra_operand_count - prevents stale operands from previous CALL/CALLG */
+if (m->cpu) {
+    m->cpu->extra_operand_count = 0;
+}
```

### C# Action Required
Search for all uses of register arrays like `I[reg]` or similar and verify:
1. If `reg` comes from operand decoding, it's likely 1-indexed
2. Array access should use `reg - 1`
3. Bounds check should be `reg >= 1 && reg <= 4`

---

## Bug 2: IF -K GO Condition Inverted (CRITICAL)

### Commit
```
bc4074f fix(cpu): Correct IF-KGO to branch when K flag is CLEAR (K=0)
```

### Problem
The instruction `IF -K GO` (opcodes 0x00D2, 0x00D3) was branching when K=1.
It should branch when K=0 (the `-K` means "NOT K").

### File
`/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/IfKeyGo.c`

### C# Equivalent
`/Emulated.HW/ND/CPU/ND500/Instructions/BRANCH/IfKeyGo.cs`

### Fix
```diff
-/* Check K flag condition */
-if (nd500_test_flag(cpu, ND500_FLAG_K)) {
+/* Check K flag condition - IF -K GO branches when K is NOT set */
+if (!nd500_test_flag(cpu, ND500_FLAG_K)) {
     /* Read displacement value */
     uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
     ...
     cpu->PC = (uint32_t)(fi->address + displacement);
 }
-/* else: K flag clear, fall through to next instruction */
+/* else: K flag SET, fall through to next instruction (no branch) */
```

### Mnemonic Clarification
- `IF -K GO` (0xD2/0xD3): Branch when K=0 (K is NOT set)
- `IF K GO` (0xD0/0xD1): Branch when K=1 (K IS set)

The `-` prefix means "NOT", so `IF -K GO` means "if NOT K, then GO".

### C# Action Required
Check the C# implementation of `IfKeyGo` - verify it uses `!K` (not K) for the branch condition.

---

## Bug 3: COMP2 Carry Flag Convention (CRITICAL)

### Commit
```
8a3a520 fix(cpu): Correct COMP2 carry flag to ARM/6502 convention
```

### Problem
The carry flag after COMP2 (compare) was set incorrectly.
- Old (wrong): C=1 if borrow occurred (op1 < op2)
- New (correct): C=1 if NO borrow (op1 >= op2)

The ND-500 uses the same carry convention as ARM and 6502 processors.

### File
`/home/ronny/repos/nd500x/src/cpu/instructions/COMPARE/Comp2.c`

### C# Equivalent
`/Emulated.HW/ND/CPU/ND500/Instructions/COMPARE/Comp2.cs`

### Fix
```diff
-/* Detect carry (borrow) (like C# line 56) */
-bool carry = (op1 < op2);
+/* Detect carry - ND-500 uses ARM/6502 convention: C=1 means NO borrow (op1 >= op2) */
+bool carry = (op1 >= op2);
```

### Carry Flag Convention
| Comparison | Old (Wrong) | New (Correct) |
|------------|-------------|---------------|
| op1 > op2  | C=0         | C=1           |
| op1 == op2 | C=0         | C=1           |
| op1 < op2  | C=1         | C=0           |

### C# Action Required
Check the C# COMP2 implementation. The carry flag should be set when `op1 >= op2` (no borrow), not when `op1 < op2`.

---

## Bug 4: SHL IOV Trap Should Be NOOP (MEDIUM)

### Commit
```
813078c fix(cpu): Change SHL IOV trap to NOOP per ND-500 manual
```

### Problem
When SHL (shift left) has a shift count >= bit width, it was triggering an IOV trap.
Per ND-500 Reference Manual section 6.5.3.1, IOV is an IGNORABLE trap.
When ignored, the instruction should act as a NOOP (not modify destination).

### File
`/home/ronny/repos/nd500x/src/cpu/instructions/SHIFT/Shl.c`

### C# Equivalent
`/Emulated.HW/ND/CPU/ND500/Instructions/SHIFT/Shl.cs`

### Fix
```diff
-/* Validate shift count (like C# lines 24-28) */
+/* Validate shift count - IOV is an IGNORABLE trap per ND-500 Reference Manual 6.5.3.1
+ * "If the IOV trap condition is ignored the instruction will be terminated (act as a NOOP)"
+ * "On the IOV trap condition the destination field is not changed" */
 if (abs_shift >= (int32_t)bits) {
-    printf("[TRAP] SHL at PC=0x%08X: Illegal shift count %d (>= %u bits)\n",
+    printf("[IOV] SHL at PC=0x%08X: Shift count %d >= %u bits (treating as NOOP)\n",
            fi->address, abs_shift, bits);
-    trap_invalid_operation(cpu, fi->address);  /* IOV trap */
+    /* Don't modify destination, just return (act as NOOP) */
     return;
 }
```

### C# Action Required
Check if the C# SHL triggers a hard trap on invalid shift count. If so, change to:
1. Log/warn about the condition
2. Return without modifying destination (NOOP behavior)
3. Do NOT trigger a hard trap

---

## Bug 5: Disassembly Operand Formatting (MEDIUM)

### Commit
```
069b07c fix(disasm): Correct operand formatting for CONSTANT, REGISTER, PREINDEXED
```

### Problem
The disassembler was producing incorrect output:
- CONSTANT: Using signed format instead of unsigned hex with `$` prefix
- REGISTER: Using `op->reg` directly instead of deriving from `address_code`
- PREINDEXED: Using `offset(IN)` format instead of `rN.(disp)` format

### File
`/home/ronny/repos/nd500x/src/machine/debug_api.c`

### Fix Details

#### CONSTANT mode
```diff
 case ND500_ADDR_CONSTANT: {
-    /* Extended constant: $value */
+    /* Extended constant: $value (same as nd500-dis uses) */
     if (p < e) *p++ = '$';
-    int n = fmt_signed(p, (size_t)(e-p), sval);
+    int n = fmt_unsigned(p, (size_t)(e-p), val);
     p += (n>0 && n < (e-p)? n : (e-p));
     break;
 }
```

#### REGISTER mode
```diff
 case ND500_ADDR_REGISTER: {
-    /* Register: r1-r4 (reg is 0-3) */
-    int n = snprintf(p, (size_t)(e-p), "r%d", (int)op->reg + 1);
+    /* Register: r1-r4 derived from address_code 0xD0-0xD3 */
+    int regnum = (op->address_code & 0x03) + 1;  /* 0xD0=r1, 0xD1=r2, 0xD2=r3, 0xD3=r4 */
+    int n = snprintf(p, (size_t)(e-p), "r%d", regnum);
     p += (n>0 && n < (e-p)? n : (e-p));
     break;
 }
```

#### PREINDEXED mode
```diff
 case ND500_ADDR_PREINDEXED: {
-    /* Pre-indexed: offset(In) */
+    /* Pre-indexed: rN.(disp) - matches nd500-dis/nd500-as syntax */
+    int regnum = (op->address_code & 0x03) + 1;  /* low 2 bits = register 1-4 */
     char vbuf[32];
     fmt_signed(vbuf, sizeof(vbuf), sval);
-    int n = snprintf(p, (size_t)(e-p), "%s(I%d)", vbuf, (int)op->reg + 1);
+    int n = snprintf(p, (size_t)(e-p), "r%d.(%s)", regnum, vbuf);
     p += (n>0 && n < (e-p)? n : (e-p));
     break;
 }
```

### Address Code to Register Mapping
| Address Code | Register |
|--------------|----------|
| 0xD0, 0xF4   | r1 (I1)  |
| 0xD1, 0xF5   | r2 (I2)  |
| 0xD2, 0xF6   | r3 (I3)  |
| 0xD3, 0xF7   | r4 (I4)  |

Formula: `regnum = (address_code & 0x03) + 1`

### C# Action Required
If C# has a disassembler, verify:
1. CONSTANT uses unsigned hex with `$` prefix
2. REGISTER derives register from `address_code & 0x03`
3. PREINDEXED uses `rN.(disp)` syntax

---

## Additional Commits (Non-Bug Fixes)

These commits add features or documentation but are not bug fixes:

| Commit | Description |
|--------|-------------|
| `6a144cd` | docs(bmove): Rewrite instruction documentation |
| `1f5298d` | docs(asm): Update bmove.md with verified examples |
| `b53f423` | docs: Add disassembly operand fix documentation |
| `acb4cfe` | feat(debugger): Add domverify command |
| `d554691` | feat(trace): Include FLAGS (ST1) in trace output |
| `46381e1` | feat(loader): Add DOM segment load logging |
| `5d9ddea` | feat(mon): Add debug logging to parameter writes |
| `3e968d0` | docs: Add ND-500 reference tables |
| `653f2e9` | test: Update test suite with expanded coverage |

---

## Validation Checklist for C#

- [ ] Fix register indexing: `I[reg]` -> `I[reg - 1]` for 1-indexed reg values
- [ ] Fix IF -K GO: Branch when K=0, not K=1
- [ ] Fix COMP2 carry: C=1 when op1 >= op2 (no borrow), not op1 < op2
- [ ] Fix SHL IOV: Treat as NOOP when shift count >= bit width
- [ ] Fix disassembly formatting (if applicable)

---

## Git Commands to View Changes

```bash
# View specific commit
git show <commit-hash>

# View diff for a commit
git diff <commit-hash>^..<commit-hash>

# View all commits from this session
git log --oneline 99986e0^..653f2e9
```

---

## Test Validation

After applying fixes, run the test suite:
```bash
./build/bin/test_instruction_validation
```

Current status: 21,545 test cases, 100% pass rate.
