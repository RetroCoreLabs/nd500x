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
- [x] **Phase 2: Planning** - ✅ COMPLETED
- [ ] **Phase 3: Generation** - ⏳ AWAITING APPROVAL

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
