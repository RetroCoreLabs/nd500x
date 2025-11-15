# ND500X Instruction Migration TODO

**Status as of 2025-11-15**

**Progress**: 39/242 (16.1%) - 203 stubs remaining

## Legend

- ✅ **DONE** - Fully implemented and tested
- 🚧 **IN_PROGRESS** - Currently being implemented
- ⏸️ **DEFERRED** - Requires infrastructure not yet available
- 📋 **TODO** - Not started
- ⚠️ **ISSUE** - Bug found in C# or spec discrepancy

**Priority**: 🔴 HIGH | 🟡 MEDIUM | 🟢 LOW

**Complexity**: S (Simple) | M (Medium) | C (Complex)

---

## Category Summary

| Category | Total | Done | TODO | Deferred | Progress |
|----------|-------|------|------|----------|----------|
| ARITHMETIC | 38 | 5 | 28 | 5 | 13.2% |
| BITFIELD | 7 | 0 | 7 | 0 | 0.0% |
| BRANCH | 20 | 7 | 13 | 0 | 35.0% |
| CALL | 18 | 6 | 12 | 0 | 33.3% |
| COMPARE | 5 | 3 | 0 | 2 | 60.0% |
| CONTROL | 9 | 1 | 8 | 0 | 11.1% |
| FLOAT_MATH | 25 | 0 | 0 | 25 | 0.0% |
| IO | 1 | 0 | 1 | 0 | 0.0% |
| **LOGICAL** | **5** | **5** | **0** | **0** | **100%** ✅ |
| MOVE | 56 | 4 | 52 | 0 | 7.1% |
| SHIFT | 5 | 4 | 0 | 1 | 80.0% |
| STRING | 18 | 0 | 0 | 18 | 0.0% |
| SYSTEM | 35 | 4 | 27 | 4 | 11.4% |
| **TOTAL** | **242** | **39** | **148** | **55** | **16.1%** |

---

## 🔴 HIGH PRIORITY CATEGORIES

### ARITHMETIC (38 total, 5 done, 33 remaining)

#### ✅ Already Implemented
- [x] Add (Add.c) - Priority register addition
- [x] Divide (Divide.c) - Basic division
- [x] Multiply (Multiply.c) - WN* instruction
- [x] Sub (Sub.c) - Priority register subtraction
- [x] Subtract (Subtract.c) - Basic subtraction

#### 📋 Simple Operations (5 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Neg | Neg.c | 🔴 HIGH | S | 📋 TODO | Two's complement negation |
| Abs | Abs.c | 🔴 HIGH | S | 📋 TODO | Absolute value |
| Add2 | Add2.c | 🔴 HIGH | S | 📋 TODO | Two-operand add |
| Sub2 | Sub2.c | 🔴 HIGH | S | 📋 TODO | Two-operand subtract |
| Div2 | Div2.c | 🔴 HIGH | M | 📋 TODO | Two-operand divide |

#### 📋 Three-Operand Operations (6 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Add3 | Add3.c | 🔴 HIGH | S | 📋 TODO | Three-operand add: op1 + op2 → op3 |
| Sub3 | Sub3.c | 🔴 HIGH | S | 📋 TODO | Three-operand subtract: op1 - op2 → op3 |
| Mul2 | Mul2.c | 🔴 HIGH | M | 📋 TODO | Two-operand multiply |
| Mul3 | Mul3.c | 🔴 HIGH | M | 📋 TODO | Three-operand multiply |
| Div3 | Div3.c | 🔴 HIGH | M | 📋 TODO | Three-operand divide |
| Div4 | Div4.c | 🔴 HIGH | M | 📋 TODO | Four-operand divide (quotient + remainder) |

#### 📋 Carry/Unsigned Operations (4 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Addc | Addc.c | 🔴 HIGH | M | 📋 TODO | Add with carry |
| Subc | Subc.c | 🔴 HIGH | M | 📋 TODO | Subtract with carry |
| Udiv | Udiv.c | 🔴 HIGH | M | 📋 TODO | Unsigned division |
| Umul | Umul.c | 🔴 HIGH | M | 📋 TODO | Unsigned multiplication |

#### 📋 Special Operations (4 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Rem | Rem.c | 🔴 HIGH | M | 📋 TODO | Remainder (modulo) |
| Mulad | Mulad.c | 🔴 HIGH | M | 📋 TODO | Multiply and add |
| Axi | Axi.c | 🔴 HIGH | C | 📋 TODO | Index arithmetic |
| Ixi | Ixi.c | 🔴 HIGH | C | 📋 TODO | Index increment |

#### ⏸️ Packed Operations (14 deferred - need helpers)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Padd | Padd.c | 🟡 MEDIUM | C | ⏸️ DEFERRED | Requires nd500_packed_add() helper |
| Paddr | Paddr.c | 🟡 MEDIUM | C | ⏸️ DEFERRED | Packed add with rounding |
| Psub | Psub.c | 🟡 MEDIUM | C | ⏸️ DEFERRED | Requires nd500_packed_sub() helper |
| Psubr | Psubr.c | 🟡 MEDIUM | C | ⏸️ DEFERRED | Packed subtract with rounding |
| Pmpy | Pmpy.c | 🟡 MEDIUM | C | ⏸️ DEFERRED | Requires nd500_packed_mul() helper |
| Pmpyr | Pmpyr.c | 🟡 MEDIUM | C | ⏸️ DEFERRED | Packed multiply with rounding |
| Ppack | Ppack.c | 🟡 MEDIUM | C | ⏸️ DEFERRED | Pack values |
| Ppackr | Ppackr.c | 🟡 MEDIUM | C | ⏸️ DEFERRED | Pack with rounding |
| Pupack | Pupack.c | 🟡 MEDIUM | C | ⏸️ DEFERRED | Unpack values |
| Pupackr | Pupackr.c | 🟡 MEDIUM | C | ⏸️ DEFERRED | Unpack with rounding |
| Psum | Psum.c | 🟡 MEDIUM | C | ⏸️ DEFERRED | Sum packed values |

---

### MOVE (56 total, 4 done, 52 remaining)

#### ✅ Already Implemented
- [x] AssignFrom (AssignFrom.c) - WN:= instruction
- [x] AssignTo (AssignTo.c) - :=WN instruction
- [x] Move (Move.c) - Basic move operation
- [x] Stz (Stz.c) - Store zero

#### 📋 A-Register Get/Set (8 remaining) - Float Registers
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| A1Get | A1Get.c | 🔴 HIGH | S | 📋 TODO | A1 → operand |
| A1Set | A1Set.c | 🔴 HIGH | S | 📋 TODO | operand → A1 |
| A2Get | A2Get.c | 🔴 HIGH | S | 📋 TODO | A2 → operand |
| A2Set | A2Set.c | 🔴 HIGH | S | 📋 TODO | operand → A2 |
| A3Get | A3Get.c | 🔴 HIGH | S | 📋 TODO | A3 → operand |
| A3Set | A3Set.c | 🔴 HIGH | S | 📋 TODO | operand → A3 |
| A4Get | A4Get.c | 🔴 HIGH | S | 📋 TODO | A4 → operand |
| A4Set | A4Set.c | 🔴 HIGH | S | 📋 TODO | operand → A4 |

#### 📋 E-Register Get/Set (8 remaining) - Extended Precision
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| E1Get | E1Get.c | 🔴 HIGH | S | 📋 TODO | E1 → operand |
| E1Set | E1Set.c | 🔴 HIGH | S | 📋 TODO | operand → E1 |
| E2Get | E2Get.c | 🔴 HIGH | S | 📋 TODO | E2 → operand |
| E2Set | E2Set.c | 🔴 HIGH | S | 📋 TODO | operand → E2 |
| E3Get | E3Get.c | 🔴 HIGH | S | 📋 TODO | E3 → operand |
| E3Set | E3Set.c | 🔴 HIGH | S | 📋 TODO | operand → E3 |
| E4Get | E4Get.c | 🔴 HIGH | S | 📋 TODO | E4 → operand |
| E4Set | E4Set.c | 🔴 HIGH | S | 📋 TODO | operand → E4 |

#### 📋 Special Register Operations (18 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| LGet | LGet.c | 🔴 HIGH | S | 📋 TODO | L register → operand |
| LSet | LSet.c | 🔴 HIGH | S | 📋 TODO | operand → L register |
| TosGet | TosGet.c | 🔴 HIGH | S | 📋 TODO | TOS → operand |
| TosSet | TosSet.c | 🔴 HIGH | S | 📋 TODO | operand → TOS |
| ThaGet | ThaGet.c | 🔴 HIGH | S | 📋 TODO | THA → operand |
| ThaSet | ThaSet.c | 🔴 HIGH | S | 📋 TODO | operand → THA |
| HlGet | HlGet.c | 🔴 HIGH | S | 📋 TODO | HL → operand |
| HlSet | HlSet.c | 🔴 HIGH | S | 📋 TODO | operand → HL |
| LlGet | LlGet.c | 🔴 HIGH | S | 📋 TODO | LL → operand |
| LlSet | LlSet.c | 🔴 HIGH | S | 📋 TODO | operand → LL |
| St1Get | St1Get.c | 🔴 HIGH | S | 📋 TODO | ST1 → operand |
| St1Set | St1Set.c | 🔴 HIGH | S | 📋 TODO | operand → ST1 |
| PGet | PGet.c | 🔴 HIGH | S | 📋 TODO | P register → operand |
| PsGet | PsGet.c | 🔴 HIGH | S | 📋 TODO | PS register → operand |
| PsSet | PsSet.c | 🔴 HIGH | S | 📋 TODO | operand → PS register |
| CadGet | CadGet.c | 🟡 MEDIUM | M | 📋 TODO | CAD → operand |
| CadSet | CadSet.c | 🟡 MEDIUM | M | 📋 TODO | operand → CAD |
| CedGet | CedGet.c | 🟡 MEDIUM | M | 📋 TODO | CED → operand |

#### 📋 Table Entry Operations (8 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Cte1Get | Cte1Get.c | 🟡 MEDIUM | M | 📋 TODO | Context table entry 1 → operand |
| Cte2Get | Cte2Get.c | 🟡 MEDIUM | M | 📋 TODO | Context table entry 2 → operand |
| Mte1Get | Mte1Get.c | 🟡 MEDIUM | M | 📋 TODO | MMU table entry 1 → operand |
| Mte2Get | Mte2Get.c | 🟡 MEDIUM | M | 📋 TODO | MMU table entry 2 → operand |
| Ote1Get | Ote1Get.c | 🟡 MEDIUM | M | 📋 TODO | Object table entry 1 → operand |
| Ote1Set | Ote1Set.c | 🟡 MEDIUM | M | 📋 TODO | operand → Object table entry 1 |
| Ote2Get | Ote2Get.c | 🟡 MEDIUM | M | 📋 TODO | Object table entry 2 → operand |
| Ote2Set | Ote2Set.c | 🟡 MEDIUM | M | 📋 TODO | operand → Object table entry 2 |

#### 📋 Memory Operations (6 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Temm1Get | Temm1Get.c | 🟡 MEDIUM | M | 📋 TODO | TEMM1 → operand |
| Temm2Get | Temm2Get.c | 🟡 MEDIUM | M | 📋 TODO | TEMM2 → operand |
| Bmove | Bmove.c | 🔴 HIGH | M | 📋 TODO | Block move (memcpy-like) |
| Clr | Clr.c | 🔴 HIGH | S | 📋 TODO | Clear memory/register |
| Clrk | Clrk.c | 🔴 HIGH | S | 📋 TODO | Clear with key |
| Swap | Swap.c | 🔴 HIGH | S | 📋 TODO | Exchange two operands |

#### 📋 Base/Record Register Operations (4 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| AssignBaseRegTo | AssignBaseRegTo.c | 🔴 HIGH | M | 📋 TODO | B → operand |
| AssignToBaseReg | AssignToBaseReg.c | 🔴 HIGH | M | 📋 TODO | operand → B |
| AssignRecordRegTo | AssignRecordRegTo.c | 🔴 HIGH | M | 📋 TODO | R → operand |
| AssignToRecordReg | AssignToRecordReg.c | 🔴 HIGH | M | 📋 TODO | operand → R |

---

### BRANCH (20 total, 7 done, 13 remaining)

#### ✅ Already Implemented
- [x] IfEqualGo (IfEqualGo.c)
- [x] IfGreaterEqualGo (IfGreaterEqualGo.c)
- [x] IfGreaterGo (IfGreaterGo.c)
- [x] IfLessEqualGo (IfLessEqualGo.c)
- [x] IfLessGo (IfLessGo.c)
- [x] IfNotEqualGo (IfNotEqualGo.c)
- [x] Ret (Ret.c)

#### 📋 Unsigned Comparisons (6 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| IfUnsignedGreaterEqualGo | IfUnsignedGreaterEqualGo.c | 🔴 HIGH | S | 📋 TODO | Branch if unsigned >= |
| IfUnsignedGreaterGo | IfUnsignedGreaterGo.c | 🔴 HIGH | S | 📋 TODO | Branch if unsigned > |
| IfUnsignedLessEqualGo | IfUnsignedLessEqualGo.c | 🔴 HIGH | S | 📋 TODO | Branch if unsigned <= |
| IfUnsignedLessGo | IfUnsignedLessGo.c | 🔴 HIGH | S | 📋 TODO | Branch if unsigned < |
| Ifkgo | Ifkgo.c | 🔴 HIGH | M | 📋 TODO | Branch if key condition |
| Ifstgo | Ifstgo.c | 🔴 HIGH | M | 📋 TODO | Branch if stack condition |

#### 📋 Special Branches (2 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| IfKeyGo | IfKeyGo.c | 🔴 HIGH | M | 📋 TODO | Branch on key flag |
| IfStackGo | IfStackGo.c | 🔴 HIGH | M | 📋 TODO | Branch on stack condition |

#### 📋 Jump Operations (2 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Jumpg | Jumpg.c | 🔴 HIGH | M | 📋 TODO | Computed jump (jump table) |
| Jumps | Jumps.c | 🔴 HIGH | M | 📋 TODO | Simple jump |

#### 📋 Loop Operations (3 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Loop | Loop.c | 🔴 HIGH | M | 📋 TODO | Decrement and branch if != 0 |
| Loopd | Loopd.c | 🔴 HIGH | M | 📋 TODO | Loop doubleword counter |
| Loopi | Loopi.c | 🔴 HIGH | M | 📋 TODO | Loop with index |

---

### CALL (18 total, 6 done, 12 remaining)

#### ✅ Already Implemented
- [x] Call (Call.c)
- [x] Ents (Ents.c)
- [x] Entw (Entw.c)
- [x] Jump (Jump.c)
- [x] Mcall (Mcall.c)
- [x] Retr (Retr.c)

#### 📋 Entry Points (6 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Entb | Entb.c | 🔴 HIGH | C | 📋 TODO | Enter buddy subroutine |
| Entf | Entf.c | 🔴 HIGH | C | 📋 TODO | Enter function |
| Entfn | Entfn.c | 🔴 HIGH | C | 📋 TODO | Enter function (no args) |
| Entm | Entm.c | 🔴 HIGH | C | 📋 TODO | Enter main program |
| Entt | Entt.c | 🔴 HIGH | C | 📋 TODO | Enter task |

#### 📋 Return Operations (4 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Retb | Retb.c | 🔴 HIGH | C | 📋 TODO | Return from buddy |
| Retbk | Retbk.c | 🔴 HIGH | C | 📋 TODO | Return from buddy with key |
| Retk | Retk.c | 🔴 HIGH | M | 📋 TODO | Return with key |
| Rett | Rett.c | 🔴 HIGH | C | 📋 TODO | Return from task |

#### 📋 Special Calls (2 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Callg | Callg.c | 🔴 HIGH | M | 📋 TODO | Call via gate |
| Chain | Chain.c | 🔴 HIGH | M | 📋 TODO | Chain to new program |
| Ifkret | Ifkret.c | 🔴 HIGH | M | 📋 TODO | Conditional return on key |

---

## 🟡 MEDIUM PRIORITY CATEGORIES

### BITFIELD (7 total, 0 done, 7 remaining)

| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Clebi | Clebi.c | 🟡 MEDIUM | M | 📋 TODO | Clear bit (indexed) |
| Getb | Getb.c | 🟡 MEDIUM | M | 📋 TODO | Get byte from bitfield |
| Getbf | Getbf.c | 🟡 MEDIUM | M | 📋 TODO | Get bitfield |
| Getbi | Getbi.c | 🟡 MEDIUM | M | 📋 TODO | Get bit (indexed) |
| Putbf | Putbf.c | 🟡 MEDIUM | M | 📋 TODO | Put bitfield |
| Putbi | Putbi.c | 🟡 MEDIUM | M | 📋 TODO | Put bit (indexed) |
| Setbi | Setbi.c | 🟡 MEDIUM | M | 📋 TODO | Set bit (indexed) |

**Dependencies**: Implement bitfield helper functions first

---

### CONTROL (9 total, 1 done, 8 remaining)

#### ✅ Already Implemented
- [x] Init (Init.c)

#### 📋 Remaining (8)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Bp | Bp.c | 🟡 MEDIUM | M | 📋 TODO | Breakpoint |
| Clte | Clte.c | 🟡 MEDIUM | M | 📋 TODO | Clear table entry |
| Noop | Noop.c | 🟡 MEDIUM | S | 📋 TODO | No operation |
| Set1 | Set1.c | 🟡 MEDIUM | S | 📋 TODO | Set bit 1 |
| Sete | Sete.c | 🟡 MEDIUM | M | 📋 TODO | Set entry |
| Setk | Setk.c | 🟡 MEDIUM | S | 📋 TODO | Set key |
| Solo | Solo.c | 🟡 MEDIUM | M | 📋 TODO | Solo instruction |
| Tset | Tset.c | 🟡 MEDIUM | M | 📋 TODO | Test and set |

---

### COMPARE (5 total, 3 done, 2 deferred)

#### ✅ Already Implemented
- [x] Comp (Comp.c)
- [x] Comp2 (Comp2.c)
- [x] Test (Test.c)

#### ⏸️ Deferred (2)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Pcomp | Pcomp.c | 🟡 MEDIUM | C | ⏸️ DEFERRED | Requires BCD infrastructure |
| Scomp | Scomp.c | 🟡 MEDIUM | C | ⏸️ DEFERRED | Requires string descriptors |

---

### SHIFT (5 total, 4 done, 1 deferred)

#### ✅ Already Implemented
- [x] Sha (Sha.c) - Arithmetic shift
- [x] Shl (Shl.c) - Logical shift
- [x] Shr (Shr.c) - Rotate shift
- [x] Pshiftr (Pshiftr.c) - Packed shift right

#### ⏸️ Deferred (1)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Pshift | Pshift.c | 🟡 MEDIUM | C | ⏸️ DEFERRED | Requires BCD infrastructure |

---

## 🟢 LOW PRIORITY / SPECIALIZED CATEGORIES

### FLOAT_MATH (25 total, 0 done, 25 deferred)

**All deferred pending ND-500 floating point format documentation**

| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Acos | Acos.c | 🟢 LOW | C | ⏸️ DEFERRED | Arc cosine |
| Alog | Alog.c | 🟢 LOW | C | ⏸️ DEFERRED | Natural logarithm |
| Alog10 | Alog10.c | 🟢 LOW | C | ⏸️ DEFERRED | Base-10 logarithm |
| Alog2 | Alog2.c | 🟢 LOW | C | ⏸️ DEFERRED | Base-2 logarithm |
| Asin | Asin.c | 🟢 LOW | C | ⏸️ DEFERRED | Arc sine |
| Atan | Atan.c | 🟢 LOW | C | ⏸️ DEFERRED | Arc tangent |
| Atan2 | Atan2.c | 🟢 LOW | C | ⏸️ DEFERRED | Arc tangent (two args) |
| Biconv | Biconv.c | 🟢 LOW | C | ⏸️ DEFERRED | BI → float conversion |
| Byconr | Byconr.c | 🟢 LOW | C | ⏸️ DEFERRED | Float → BY conversion (round) |
| Byconv | Byconv.c | 🟢 LOW | C | ⏸️ DEFERRED | Float → BY conversion |
| Cos | Cos.c | 🟢 LOW | C | ⏸️ DEFERRED | Cosine |
| Dconv | Dconv.c | 🟢 LOW | C | ⏸️ DEFERRED | Double conversion |
| Exp | Exp.c | 🟢 LOW | C | ⏸️ DEFERRED | Exponential |
| Fconr | Fconr.c | 🟢 LOW | C | ⏸️ DEFERRED | Float conversion (round) |
| Fconv | Fconv.c | 🟢 LOW | C | ⏸️ DEFERRED | Float conversion |
| Hconr | Hconr.c | 🟢 LOW | C | ⏸️ DEFERRED | H → float conversion (round) |
| Hconv | Hconv.c | 🟢 LOW | C | ⏸️ DEFERRED | H → float conversion |
| Poly | Poly.c | 🟢 LOW | C | ⏸️ DEFERRED | Polynomial evaluation |
| Pwconv | Pwconv.c | 🟢 LOW | C | ⏸️ DEFERRED | Packed word conversion |
| Sin | Sin.c | 🟢 LOW | C | ⏸️ DEFERRED | Sine |
| Sqrt | Sqrt.c | 🟢 LOW | C | ⏸️ DEFERRED | Square root |
| Tan | Tan.c | 🟢 LOW | C | ⏸️ DEFERRED | Tangent |
| Wconr | Wconr.c | 🟢 LOW | C | ⏸️ DEFERRED | W → float conversion (round) |
| Wconv | Wconv.c | 🟢 LOW | C | ⏸️ DEFERRED | W → float conversion |
| Wpconv | Wpconv.c | 🟢 LOW | C | ⏸️ DEFERRED | Word packed conversion |

**Infrastructure needed**: IEEE754 ↔ ND-500 float format conversion

---

### STRING (18 total, 0 done, 18 deferred)

**All deferred pending string descriptor infrastructure**

| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Schpar | Schpar.c | 🟢 LOW | C | ⏸️ DEFERRED | String character compare |
| Scopa | Scopa.c | 🟢 LOW | C | ⏸️ DEFERRED | String copy all |
| Scopt | Scopt.c | 🟢 LOW | C | ⏸️ DEFERRED | String copy until |
| Scotr | Scotr.c | 🟢 LOW | C | ⏸️ DEFERRED | String copy/translate |
| Scpuno | Scpuno.c | 🟢 LOW | C | ⏸️ DEFERRED | String copy until not |
| Sfill | Sfill.c | 🟢 LOW | M | ⏸️ DEFERRED | String fill |
| Sfilln | Sfilln.c | 🟢 LOW | M | ⏸️ DEFERRED | String fill N times |
| Smatch | Smatch.c | 🟢 LOW | C | ⏸️ DEFERRED | String match |
| Smove | Smove.c | 🟢 LOW | M | ⏸️ DEFERRED | String move |
| Smovn | Smovn.c | 🟢 LOW | M | ⏸️ DEFERRED | String move N bytes |
| Smvtr | Smvtr.c | 🟢 LOW | C | ⏸️ DEFERRED | String move/translate |
| Smvtu | Smvtu.c | 🟢 LOW | C | ⏸️ DEFERRED | String move until |
| Smvun | Smvun.c | 🟢 LOW | C | ⏸️ DEFERRED | String move until not |
| Smvwh | Smvwh.c | 🟢 LOW | C | ⏸️ DEFERRED | String move while |
| Sscan | Sscan.c | 🟢 LOW | C | ⏸️ DEFERRED | String scan |
| Sskip | Sskip.c | 🟢 LOW | C | ⏸️ DEFERRED | String skip |
| Sspan | Sspan.c | 🟢 LOW | C | ⏸️ DEFERRED | String span |
| Sspar | Sspar.c | 🟢 LOW | C | ⏸️ DEFERRED | String span reverse |

**Infrastructure needed**: String descriptor loading, I1/I2 index register usage

---

### SYSTEM (35 total, 4 done, 31 remaining)

#### ✅ Already Implemented
- [x] Mon1 (Mon1.c)
- [x] Mon503 (Mon503.c)
- [x] Dvinst (Dvinst.c)
- [x] Pviol (Pviol.c)

#### 📋 Simple Operations (4 remaining)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Int | Int.c | 🟡 MEDIUM | M | 📋 TODO | Interrupt |
| Intr | Intr.c | 🟡 MEDIUM | M | 📋 TODO | Interrupt return |
| Svers | Svers.c | 🟡 MEDIUM | S | 📋 TODO | Set version |
| Tutti | Tutti.c | 🟡 MEDIUM | M | 📋 TODO | Test instruction |

#### ⏸️ MMU/Context Operations (27 deferred)
| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Bladdr | Bladdr.c | 🟢 LOW | C | ⏸️ DEFERRED | Block address translation |
| Cind | Cind.c | 🟢 LOW | C | ⏸️ DEFERRED | Context indirect |
| Cpgu | Cpgu.c | 🟢 LOW | C | ⏸️ DEFERRED | Copy page |
| Cwip | Cwip.c | 🟢 LOW | C | ⏸️ DEFERRED | Clear WIP |
| Dcc | Dcc.c | 🟢 LOW | C | ⏸️ DEFERRED | Data cache control |
| Dctsb | Dctsb.c | 🟢 LOW | C | ⏸️ DEFERRED | Data cache TSB |
| Ddirt | Ddirt.c | 🟢 LOW | C | ⏸️ DEFERRED | Data dirty |
| Freeb | Freeb.c | 🟢 LOW | C | ⏸️ DEFERRED | Free block |
| Laddr | Laddr.c | 🟢 LOW | C | ⏸️ DEFERRED | Load address |
| Lcntxt | Lcntxt.c | 🟢 LOW | C | ⏸️ DEFERRED | Load context |
| Lind | Lind.c | 🟢 LOW | C | ⏸️ DEFERRED | Load indirect |
| Lregbl | Lregbl.c | 🟢 LOW | C | ⏸️ DEFERRED | Load register block |
| Pcc | Pcc.c | 🟢 LOW | C | ⏸️ DEFERRED | Program cache control |
| Pctsb | Pctsb.c | 🟢 LOW | C | ⏸️ DEFERRED | Program cache TSB |
| Phyladr | Phyladr.c | 🟢 LOW | C | ⏸️ DEFERRED | Physical address |
| Rdus | Rdus.c | 🟢 LOW | C | ⏸️ DEFERRED | Read DUSIB |
| Rladdr | Rladdr.c | 🟢 LOW | C | ⏸️ DEFERRED | Release address |
| Rpgu | Rpgu.c | 🟢 LOW | C | ⏸️ DEFERRED | Release page |
| Rphs | Rphs.c | 🟢 LOW | C | ⏸️ DEFERRED | Release physical segment |
| Rwip | Rwip.c | 🟢 LOW | C | ⏸️ DEFERRED | Read WIP |
| Scntxt | Scntxt.c | 🟢 LOW | C | ⏸️ DEFERRED | Save context |
| Sloca | Sloca.c | 🟢 LOW | C | ⏸️ DEFERRED | Set local address |
| Sregbl | Sregbl.c | 🟢 LOW | C | ⏸️ DEFERRED | Save register block |
| Wdus | Wdus.c | 🟢 LOW | C | ⏸️ DEFERRED | Write DUSIB |
| Wphs | Wphs.c | 🟢 LOW | C | ⏸️ DEFERRED | Write physical segment |
| Zpgu | Zpgu.c | 🟢 LOW | C | ⏸️ DEFERRED | Zero page |
| Zwip | Zwip.c | 🟢 LOW | C | ⏸️ DEFERRED | Zero WIP |

**Infrastructure needed**: Full MMU implementation, context switching

---

### IO (1 total, 0 done, 1 remaining)

| Instruction | File | Priority | Complexity | Status | Notes |
|-------------|------|----------|------------|--------|-------|
| Riom | Riom.c | 🟢 LOW | C | ⏸️ DEFERRED | Reserved I/O monitor |

**Infrastructure needed**: I/O subsystem design

---

## Implementation Notes

### Helper Functions Status

**Existing helpers** (in instruction_helpers.{h,c}):
- ✅ Memory access (MMU-aware reads/writes)
- ✅ Operand access (unified operand reading/writing)
- ✅ Register access (integer, float, double)
- ✅ Flag manipulation (set/clear/test)
- ✅ Arithmetic helpers (sign extend, mask, overflow/carry detection)

**Needed helpers** (to be implemented):
- 🔧 Stack operations (push/pop word/doubleword)
- 🔧 Bitfield operations (extract/insert/set/clear/test bit)
- 🔧 Loop helpers (decrement and test)
- 🔧 Packed arithmetic (SIMD-style add/sub/mul)
- ⏸️ Float conversion (IEEE754 ↔ ND-500 format) - DEFERRED
- ⏸️ String operations (descriptor-based) - DEFERRED
- ⏸️ MMU/Context operations - DEFERRED

### Verification Checklist

For each instruction implementation:
- [ ] Read C# source from RetroCore
- [ ] Check YAML spec (docs/instructions/yaml/)
- [ ] Check ASM docs (docs/asm/)
- [ ] Search ND-500 Reference Manual PDFs
- [ ] Document any discrepancies
- [ ] Implement in C with helpers
- [ ] Build and test (make -j4)
- [ ] Update this TODO with ✅

### Bug Tracking

Document any issues in `/home/ronny/repos/nd500x/docs/C#_BUGS_FOUND.md`

---

**Last Updated**: 2025-11-15
