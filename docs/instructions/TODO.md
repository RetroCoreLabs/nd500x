# ND-500 Instruction Documentation TODO

## Overview

This file tracks progress for the ND-500 instruction documentation automation project.

**Total Instructions to Document:** 241 unique instructions (1,086 variants)

**Target Directory:** `/docs/instructions/asm/`

**Generation Status:** Phase 2.5 complete, ready for Phase 3 generation

---

## Progress Summary

| Category | Total | Completed | Remaining | Progress |
|----------|-------|-----------|-----------|----------|
| **Preparation** | 10 | 10 | 0 | 100% |
| **Generation** | 241 | 3 | 238 | 1% |
| **Validation** | 241 | 0 | 241 | 0% |

---

## Phase Status

- [x] **Phase 1: Analysis** - ✅ COMPLETED (2025-11-08)
- [x] **Phase 1.5: Pre-Approval Fixes** - ✅ COMPLETED (2025-11-08)
- [x] **Phase 2: Planning** - ✅ COMPLETED (2025-11-08)
- [x] **Phase 2.5: Pre-Phase 3 Approval Fixes** - ✅ COMPLETED (2025-11-08)
- [ ] **Phase 3: Generation** - ⏳ READY FOR APPROVAL

---

## Preparation Tasks

### Phase 1.5 Tasks (Pre-Approval)
- [x] Create `/docs/instructions/asm/` directory
- [x] Create `/docs/instructions/TODO.md` file
- [x] Fix ASSEMBLY_EXAMPLES_CIND.md (added F and D variants → 20 total)
- [x] Normalize documentation standards (DOCUMENTATION_STANDARDS.md created)
- [x] Create instruction-to-manual-section mapping (43 instructions mapped → 18%)

### Phase 2.5 Tasks (Pre-Phase 3 Approval)
- [x] Fix CIND variants in instructions.json (added F1-F4, D1-D4 → 1,086 total variants)
- [x] Implement variant count validation script (validate_variant_counts.py)
- [x] Expand manual section mapping to 80% (195/241 = 81% coverage)
- [x] Execute schema validation (241/241 YAML files valid, JSON valid)
- [x] Implement quality metrics scoring system (quality_metrics.py)
- [x] Test template with 3 sample instructions (cind.md, assignto.md, add.md)
- [x] Verify UTF-8 encoding (all files UTF-8 compliant)

---

## Generation Tasks (241 Instructions)

**Status:** Not started (Phase 3 planning complete)

### MOVE (24 unique instructions) - 0% complete

- [ ] assignto.md (`:=`)
- [ ] assignfrom.md (`=:`)
- [ ] b_assignto.md (`b:=`)
- [ ] b_assignfrom.md (`b=:`)
- [ ] r_assignto.md (`r:=`)
- [ ] r_assignfrom.md (`r=:`)
- [ ] clr.md (`clr`)
- [ ] laddr.md (`laddr`)
- [ ] move.md (`move`)
- [ ] push.md (`push`)
- [ ] pop.md (`pop`)
- [ ] sar.md (`sar`)
- [ ] sll.md (`sll`)
- [ ] sra.md (`sra`)
- [ ] srl.md (`srl`)
- [ ] swap.md (`swap`)
- [ ] xchg.md (`xchg`)
- [ ] (17 more MOVE instructions to be listed)

### ARITHMETIC (37 unique instructions) - 0% complete

- [ ] add.md (`add`, `+`)
- [ ] sub.md (`sub`, `-`)
- [ ] mul.md (`mul`, `*`)
- [ ] div.md (`div`, `/`)
- [ ] neg.md (`neg`)
- [ ] abs.md (`abs`)
- [ ] add2.md (`add2`)
- [ ] add3.md (`add3`)
- [ ] addc.md (`addc`)
- [ ] axi.md (`axi`)
- [ ] decr.md (`decr`)
- [ ] div2.md (`div2`)
- [ ] div3.md (`div3`)
- [ ] div4.md (`div4`)
- [ ] incr.md (`incr`)
- [ ] ixi.md (`ixi`)
- [ ] mul2.md (`mul2`)
- [ ] mul3.md (`mul3`)
- [ ] mul4.md (`mul4`)
- [ ] mulad.md (`mulad`)
- [ ] padd.md (`padd`)
- [ ] paddr.md (`paddr`)
- [ ] pmpy.md (`pmpy`)
- [ ] pmpyr.md (`pmpyr`)
- [ ] ppack.md (`ppack`)
- [ ] ppackr.md (`ppackr`)
- [ ] psub.md (`psub`)
- [ ] psubr.md (`psubr`)
- [ ] psum.md (`psum`)
- [ ] pupack.md (`pupack`)
- [ ] pupackr.md (`pupackr`)
- [ ] rem.md (`rem`)
- [ ] sub2.md (`sub2`)
- [ ] sub3.md (`sub3`)
- [ ] subc.md (`subc`)
- [ ] udiv.md (`udiv`)
- [ ] umul.md (`umul`)

### FLOAT_MATH (35 unique instructions) - 0% complete

- [ ] sin.md (`sin`)
- [ ] cos.md (`cos`)
- [ ] tan.md (`tan`)
- [ ] asin.md (`asin`)
- [ ] acos.md (`acos`)
- [ ] atan.md (`atan`)
- [ ] atan2.md (`atan2`)
- [ ] sinh.md (`sinh`)
- [ ] cosh.md (`cosh`)
- [ ] tanh.md (`tanh`)
- [ ] exp.md (`exp`)
- [ ] log.md (`log`, `ln`)
- [ ] log10.md (`log10`)
- [ ] log2.md (`log2`)
- [ ] alog.md (`alog`)
- [ ] alog10.md (`alog10`)
- [ ] alog2.md (`alog2`)
- [ ] sqrt.md (`sqrt`)
- [ ] power.md (`power`, `pow`)
- [ ] fabs.md (`fabs`)
- [ ] fint.md (`fint`)
- [ ] fneg.md (`fneg`)
- [ ] entier.md (`entier`)
- [ ] (12 more FLOAT_MATH instructions)

### SYSTEM (60+ unique instructions) - 0% complete

- [ ] cind.md (`cind`)
- [ ] lind.md (`lind`)
- [ ] init.md (`init`)
- [ ] rdus.md (`rdus`)
- [ ] wrus.md (`wrus`)
- [ ] ldpc.md (`ldpc`)
- [ ] stpc.md (`stpc`)
- [ ] getb.md (`getb`)
- [ ] putb.md (`putb`)
- [ ] (50+ more SYSTEM instructions)

### STRING (22 unique instructions) - 0% complete

- [ ] smove.md (`smove`)
- [ ] scomp.md (`scomp`)
- [ ] sfill.md (`sfill`)
- [ ] sfilln.md (`sfilln`)
- [ ] sscan.md (`sscan`)
- [ ] sskip.md (`sskip`)
- [ ] slocate.md (`slocate`)
- [ ] smatch.md (`smatch`)
- [ ] spack.md (`spack`)
- [ ] sunpack.md (`sunpack`)
- [ ] (12 more STRING instructions)

### LOGICAL (16 unique instructions) - 0% complete

- [ ] and.md (`and`)
- [ ] or.md (`or`)
- [ ] xor.md (`xor`)
- [ ] inv.md (`inv`, `not`)
- [ ] nand.md (`nand`)
- [ ] nor.md (`nor`)
- [ ] xnor.md (`xnor`)
- [ ] test.md (`test`)
- [ ] (8 more LOGICAL instructions)

### BRANCH (19 unique instructions) - 0% complete

- [ ] go.md (`go`, `jmp`)
- [ ] if_eq_go.md (`if=go`)
- [ ] if_ne_go.md (`if><go`)
- [ ] if_lt_go.md (`if<go`)
- [ ] if_le_go.md (`if<=go`)
- [ ] if_gt_go.md (`if>go`)
- [ ] if_ge_go.md (`if>=go`)
- [ ] ifkgo.md (`ifkgo`)
- [ ] ifstgo.md (`ifstgo`)
- [ ] jumpg.md (`jumpg`)
- [ ] jumps.md (`jumps`)
- [ ] loop.md (`loop`)
- [ ] loopd.md (`loopd`)
- [ ] loopi.md (`loopi`)
- [ ] (5 more BRANCH instructions)

### CALL (18 unique instructions) - 0% complete

- [ ] call.md (`call`)
- [ ] callg.md (`callg`)
- [ ] ret.md (`ret`)
- [ ] chain.md (`chain`)
- [ ] entb.md (`entb`)
- [ ] entd.md (`entd`)
- [ ] entf.md (`entf`)
- [ ] entfn.md (`entfn`)
- [ ] entm.md (`entm`)
- [ ] ents.md (`ents`)
- [ ] entsn.md (`entsn`)
- [ ] entt.md (`entt`)
- [ ] retb.md (`retb`)
- [ ] retd.md (`retd`)
- [ ] rett.md (`rett`)
- [ ] (3 more CALL instructions)

### BITFIELD (7 unique instructions) - 0% complete

- [ ] getb.md (`getb`)
- [ ] putb.md (`putb`)
- [ ] getbf.md (`getbf`)
- [ ] putbf.md (`putbf`)
- [ ] getbi.md (`getbi`)
- [ ] putbi.md (`putbi`)
- [ ] clebi.md (`clebi`)
- [ ] setbi.md (`setbi`)

### COMPARE (6 unique instructions) - 0% complete

- [ ] comp.md (`comp`)
- [ ] cmp.md (`cmp`)
- [ ] test.md (`test`)
- [ ] tst.md (`tst`)
- [ ] (2 more COMPARE instructions)

### CONTROL (13 unique instructions) - 0% complete

- [ ] noop.md (`noop`)
- [ ] halt.md (`halt`)
- [ ] wait.md (`wait`)
- [ ] setk.md (`setk`)
- [ ] clrk.md (`clrk`)
- [ ] monitor.md (`monitor`)
- [ ] rfi.md (`rfi`)
- [ ] (6 more CONTROL instructions)

### SHIFT (11 unique instructions) - 0% complete

- [ ] shl.md (`shl`)
- [ ] shr.md (`shr`)
- [ ] sal.md (`sal`)
- [ ] sar.md (`sar`)
- [ ] rol.md (`rol`)
- [ ] ror.md (`ror`)
- [ ] sll.md (`sll`)
- [ ] srl.md (`srl`)
- [ ] sra.md (`sra`)
- [ ] rcl.md (`rcl`)
- [ ] rcr.md (`rcr`)

### IO (1 unique instruction) - 0% complete

- [ ] io.md (`io`)

---

**Note:** Full detailed list of all 241 unique instructions will be generated during Phase 3 execution based on instructions.json analysis.

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

### From Phase 1 Analysis (All Resolved in Phase 2.5)

1. **ASSEMBLY_EXAMPLES_CIND.md**
   - Status: ✅ RESOLVED
   - Issue: Missing F and D variants (8 variants)
   - Resolution: Added F1-F4, D1-D4 variants in Phase 1.5
   - Verified: CIND example now shows all 20 variants

2. **Manual Section Mapping**
   - Status: ✅ RESOLVED
   - Issue: No mnemonic → §X.Y mapping exists
   - Resolution: Created comprehensive mapping in Phase 2.5
   - Coverage: 195/241 instructions (81%)

3. **Documentation Style Inconsistencies**
   - Status: ✅ RESOLVED
   - Issue: Hex format (0xXXXX vs 0XXXXH), mnemonic format varies
   - Resolution: Created DOCUMENTATION_STANDARDS.md in Phase 1.5
   - Standard: 0x prefix, consistent mnemonic format

### Low-Priority Items (Deferred to Phase 3)

1. **Unmapped Instructions (46 remaining)**
   - Specialized variants and '87 extensions
   - Can be generated using JSON data alone
   - Manual references will be marked "TBD"

2. **YAML/JSON Naming Consistency**
   - Minor casing differences
   - Impact: Minimal (parsers handle both)
   - Action: Document conventions during Phase 3

---

## Notes

- Generation order: By instruction class (MOVE first, then ARITHMETIC, etc.)
- Each `.md` file named after mnemonic (lowercase, special chars replaced)
- Template based on corrected ASSEMBLY_EXAMPLES_CIND.md
- Manual validation required for all generated examples

---

## Phase 2.5 Completion Summary

**Date:** 2025-11-08
**Status:** ✅ PHASE 2.5 COMPLETE

### Critical Fixes Completed
- ✅ CIND variants fixed (8 new variants added to instructions.json)
- ✅ Variant count validation script implemented and operational

### High-Priority Fixes Completed
- ✅ Manual section mapping expanded to 81% (195/241 instructions)
- ✅ Schema validation executed (100% pass rate)

### Medium-Priority Fixes Completed
- ✅ Quality metrics system implemented
- ✅ Template tested with 3 sample instruction files
- ✅ Documentation cross-links validated

### Files Created in Phase 2.5
1. `fix_cind_variants.py` - CIND variant addition script
2. `validate_variant_counts.py` - Variant validation script
3. `validate_schemas.py` - Schema validation script
4. `quality_metrics.py` - Quality scoring system
5. `VARIANT_COUNT_ANALYSIS.md` - Variant semantics documentation
6. `PHASE_2_5_COMPLETION_REPORT.md` - Comprehensive completion report
7. `asm/cind.md` - Sample SYSTEM instruction
8. `asm/assignto.md` - Sample MOVE instruction
9. `asm/add.md` - Sample ARITHMETIC instruction

### Files Modified in Phase 2.5
1. `instructions.json` - Added 8 CIND variants (1,078 → 1,086 total)
2. `MANUAL_SECTION_MAPPING.json` - Expanded to 195 mappings (81% coverage)

### Next Phase
**Phase 3: Instruction Documentation Generation**
- Generate 241 instruction documentation files
- Run quality validation on all generated files
- Create comprehensive index and cross-reference system

---

**Last Updated:** 2025-11-08
**Status:** Phase 2.5 Complete - Ready for Phase 3 Approval
