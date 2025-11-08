# ND-500 Instruction Documentation TODO

## Overview

This file tracks progress for the ND-500 instruction documentation automation project.

**Total Instructions to Document:** 241 unique instructions (1,078 variants)

**Target Directory:** `/docs/instructions/asm/`

**Generation Status:** Not started

---

## Progress Summary

| Category | Total | Completed | Remaining | Progress |
|----------|-------|-----------|-----------|----------|
| **Preparation** | 7 | 0 | 7 | 0% |
| **Generation** | 241 | 0 | 241 | 0% |
| **Validation** | 241 | 0 | 241 | 0% |

---

## Phase Status

- [x] **Phase 1: Analysis** - ✅ COMPLETED
- [x] **Phase 1.5: Pre-Approval Fixes** - ✅ COMPLETED
- [ ] **Phase 2: Planning** - ⏳ READY TO START
- [ ] **Phase 3: Generation** - ⏳ WAITING

---

## Preparation Tasks

### Repository Structure
- [x] Create `/docs/instructions/asm/` directory
- [x] Create `/docs/instructions/TODO.md` file
- [x] Validate all YAML files against schema (sampled - structurally valid)
- [x] Create instruction-to-manual-section mapping (43 instructions mapped)
- [x] Fix ASSEMBLY_EXAMPLES_CIND.md (added F and D variants → 20 total)
- [x] Normalize documentation standards (DOCUMENTATION_STANDARDS.md created)
- [x] Verify UTF-8 encoding (all files UTF-8 or US-ASCII)

---

## Generation Tasks (241 Instructions)

Instructions will be listed here once Phase 3 begins, grouped by class:

### ARITHMETIC (TBD)
### MOVE (TBD)
### FLOAT_MATH (TBD)
### SYSTEM (TBD)
### STRING (TBD)
### LOGICAL (TBD)
### BRANCH (TBD)
### BITFIELD (TBD)
### COMPARE (TBD)
### CALL (TBD)
### CONTROL (TBD)
### SHIFT (TBD)
### IO (TBD)

---

## Validation Tasks

### Cross-Check Criteria
- [ ] All opcodes verified against Reference Manual
- [ ] All addressing modes documented with examples
- [ ] All prefixes tested and verified
- [ ] Assembly examples compile/assemble successfully
- [ ] No broken cross-references
- [ ] All trap conditions documented
- [ ] Performance notes added where applicable

---

## Outstanding Issues

### From Phase 1 Analysis

1. **ASSEMBLY_EXAMPLES_CIND.md**
   - Status: ⚠️ INCOMPLETE
   - Issue: Missing F and D variants (8 variants)
   - Action: Add Float and Double CIND variants
   - Priority: HIGH

2. **Manual Section Mapping**
   - Status: ❌ MISSING
   - Issue: No mnemonic → §X.Y mapping exists
   - Action: Extract section numbers from Reference Manual
   - Priority: MEDIUM

3. **Documentation Style Inconsistencies**
   - Status: ⚠️ MIXED
   - Issue: Hex format (0xXXXX vs 0XXXXH), mnemonic format varies
   - Action: Standardize to 0x prefix, consistent mnemonic format
   - Priority: MEDIUM

---

## Notes

- Generation order: By instruction class (MOVE first, then ARITHMETIC, etc.)
- Each `.md` file named after mnemonic (lowercase, special chars replaced)
- Template based on corrected ASSEMBLY_EXAMPLES_CIND.md
- Manual validation required for all generated examples

---

**Last Updated:** 2025-11-08
**Status:** Phase 1.5 - Pre-Approval Fixes In Progress
