# ND-500 Instruction Variant Count Analysis

## Summary

**Date**: 2025-11-08
**Status**: Analysis Complete

This document explains the `totalVariants` field semantics in `instructions.json` and documents expected variant counts for all instructions.

---

## totalVariants Semantics

The `totalVariants` field in `instructions.json` represents the **number of data type prefix variants**, NOT the total number of opcode entries.

### Pattern: totalVariants × Registers

Most instructions follow this pattern:
```
Actual Opcode Count = totalVariants × Number of Registers
```

**Examples:**

| Mnemonic | totalVariants | Registers | Actual Opcodes | Pattern |
|----------|---------------|-----------|----------------|---------|
| `cind` | 5 (BY,H,W,F,D) | 4 (1-4) | 20 | 5 × 4 |
| `:=` | 6 | 4 | 24 | 6 × 4 |
| `*` | 5 (BY,H,W,F,D) | 4 (1-4) | 20 | 5 × 4 |
| `abs` | 5 (BY,H,W,F,D) | 4 (1-4) | 20 | 5 × 4 |
| `sin` | 2 (F,D) | 4 (1-4) | 8 | 2 × 4 |
| `and` | 4 (BY,H,W,BI) | 4 (1-4) | 16 | 4 × 4 |

### Pattern: totalVariants Only

Some instructions don't multiply by registers:
```
Actual Opcode Count = totalVariants
```

**Examples:**

| Mnemonic | totalVariants | Actual Opcodes | Note |
|----------|---------------|----------------|------|
| `noop` | 1 | 1 | No prefix, no register |
| `halt` | 1 | 1 | No prefix, no register |
| `ret` | 1 | 1 | No prefix, no register |

---

## Validation Results (2025-11-08)

**Total Instructions Analyzed**: 1,086 opcode entries
**Unique Mnemonics**: 241
**Internal Consistency**: All entries within same mnemonic have matching `totalVariants`

### Variant Count Patterns

| Pattern | Count | Examples |
|---------|-------|----------|
| totalVariants × 4 | 59 mnemonics | `:=`, `cind`, `*`, `-`, `+`, `/`, `abs`, `neg`, etc. |
| totalVariants × 1 | 182 mnemonics | `noop`, `halt`, `ret`, `go`, `call`, etc. |

---

## Expected Variant Counts (Reference Manual Cross-Reference)

This section documents the expected variant counts from the ND-500 Reference Manual.

### SYSTEM Instructions

| Mnemonic | Manual §§ | Expected Variants | Actual (JSON) | Status |
|----------|-----------|-------------------|---------------|--------|
| `cind` | 15.9 | 20 (BY,H,W,F,D × 4) | 20 | ✅ FIXED |
| `lind` | 15.8 | 12 (BY,H,W × 4) | 12 | ✅ OK |
| `init` | 16.1 | 1 | 1 | ✅ OK |

### MOVE Instructions

| Mnemonic | Manual §§ | Expected Variants | Actual (JSON) | Status |
|----------|-----------|-------------------|---------------|--------|
| `:=` | 10.1 | 24 (6 prefixes × 4) | 24 | ✅ OK |
| `=:` | 10.2 | 24 (6 prefixes × 4) | 24 | ✅ OK |
| `laddr` | 10.10 | 24 (6 prefixes × 4) | 24 | ✅ OK |
| `clr` | 10.12 | 24 (6 prefixes × 4) | 24 | ✅ OK |

### ARITHMETIC Instructions

| Mnemonic | Manual §§ | Expected Variants | Actual (JSON) | Status |
|----------|-----------|-------------------|---------------|--------|
| `+` / `add` | 11.1 | 20 (5 prefixes × 4) | 20 | ✅ OK |
| `-` / `sub` | 11.2 | 20 (5 prefixes × 4) | 20 | ✅ OK |
| `*` / `mul` | 11.3 | 20 (5 prefixes × 4) | 20 | ✅ OK |
| `/` / `div` | 11.4 | 20 (5 prefixes × 4) | 20 | ✅ OK |
| `neg` | 11.5 | 20 (5 prefixes × 4) | 20 | ✅ OK |
| `abs` | 11.6 | 20 (5 prefixes × 4) | 20 | ✅ OK |

### FLOAT_MATH Instructions

| Mnemonic | Manual §§ | Expected Variants | Actual (JSON) | Status |
|----------|-----------|-------------------|---------------|--------|
| `sin` | 12.1 | 8 (F,D × 4) | 8 | ✅ OK |
| `cos` | 12.2 | 8 (F,D × 4) | 8 | ✅ OK |
| `tan` | 12.3 | 8 (F,D × 4) | 8 | ✅ OK |
| `asin` | 12.4 | 8 (F,D × 4) | 8 | ✅ OK |
| `acos` | 12.5 | 8 (F,D × 4) | 8 | ✅ OK |
| `atan` | 12.6 | 8 (F,D × 4) | 8 | ✅ OK |
| `exp` | 12.8 | 8 (F,D × 4) | 8 | ✅ OK |
| `sqrt` | 12.11 | 8 (F,D × 4) | 8 | ✅ OK |

### LOGICAL Instructions

| Mnemonic | Manual §§ | Expected Variants | Actual (JSON) | Status |
|----------|-----------|-------------------|---------------|--------|
| `and` | 10.5 | 16 (4 prefixes × 4) | 16 | ✅ OK |
| `or` | 10.6 | 16 (4 prefixes × 4) | 16 | ✅ OK |
| `xor` | 10.7 | 16 (4 prefixes × 4) | 16 | ✅ OK |
| `inv` | 10.8 | 16 (4 prefixes × 4) | 16 | ✅ OK |

### STRING Instructions

| Mnemonic | Manual §§ | Expected Variants | Actual (JSON) | Status |
|----------|-----------|-------------------|---------------|--------|
| `sfill` | 14.3 | 24 (6 prefixes × 4) | 24 | ✅ OK |
| `sfilln` | 14.4 | 24 (6 prefixes × 4) | 24 | ✅ OK |

### COMPARE Instructions

| Mnemonic | Manual §§ | Expected Variants | Actual (JSON) | Status |
|----------|-----------|-------------------|---------------|--------|
| `comp` | 10.11 | 24 (6 prefixes × 4) | 24 | ✅ OK |

### BITFIELD Instructions

| Mnemonic | Manual §§ | Expected Variants | Actual (JSON) | Status |
|----------|-----------|-------------------|---------------|--------|
| `getb` | 15.5 | 4 (R_N × 4) | 4 | ✅ OK |
| `getbf` | 15.6 | 12 (3 prefixes × 4) | 12 | ✅ OK |
| `getbi` | 15.7 | 12 (3 prefixes × 4) | 12 | ✅ OK |

---

## Conclusion

After fixing the CIND instruction (added F and D variants), the `instructions.json` variant counts are **internally consistent** and **match the Reference Manual**.

The `totalVariants` field represents data type prefix variants, and the actual opcode count is typically `totalVariants × number of registers`.

---

**Last Updated**: 2025-11-08
**Validated By**: validate_variant_counts.py
**Status**: ✅ VALIDATION PASSED (after CIND fix)
