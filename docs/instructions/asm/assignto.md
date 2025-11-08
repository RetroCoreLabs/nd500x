# := - Load (Assign To)

## Overview

**Mnemonic:** `:=`
**Function:** AssignTo
**Class:** MOVE
**Privilege:** user

**Format:** `{prefix}{register} := <source>`

---

## Description

Load a value from memory or another register into the specified destination register. This is the fundamental data movement instruction in the ND-500 architecture, equivalent to an assignment operation in high-level languages.

**Operation:**
```
destination_register ← source_operand
```

The `:=` instruction supports all data type prefixes (BI, BY, H, W, F, D) and all addressing modes, making it the most flexible load operation.

**Operands:** 1
**Variants:** 24 opcode(s)

---

## Variants

Total variants: 24

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/24 | 0xFC04 | BI | 1 | ALL |
| 2/24 | 0xFC05 | BI | 2 | ALL |
| 3/24 | 0xFC06 | BI | 3 | ALL |
| 4/24 | 0xFC07 | BI | 4 | ALL |
| 5/24 | 0xFC08 | BY | 1 | ALL |
| 6/24 | 0xFC09 | BY | 2 | ALL |
| 7/24 | 0xFC0A | BY | 3 | ALL |
| 8/24 | 0xFC0B | BY | 4 | ALL |
| 9/24 | 0xFC0C | H | 1 | ALL |
| 10/24 | 0xFC0D | H | 2 | ALL |
| 11/24 | 0xFC0E | H | 3 | ALL |
| 12/24 | 0xFC0F | H | 4 | ALL |
| 13/24 | 0xFC10 | W | 1 | ALL |
| 14/24 | 0xFC11 | W | 2 | ALL |
| 15/24 | 0xFC12 | W | 3 | ALL |
| 16/24 | 0xFC13 | W | 4 | ALL |
| 17/24 | 0xFC14 | F | 1 | ALL |
| 18/24 | 0xFC15 | F | 2 | ALL |
| 19/24 | 0xFC16 | F | 3 | ALL |
| 20/24 | 0xFC17 | F | 4 | ALL |
| 21/24 | 0xFC18 | D | 1 | ALL |
| 22/24 | 0xFC19 | D | 2 | ALL |
| 23/24 | 0xFC1A | D | 3 | ALL |
| 24/24 | 0xFC1B | D | 4 | ALL |

---

## Operands

### Operand 1: Source Value
The value to load into the destination register.

**Supported modes (ALL):**
- **CONSTANT:** `value` - Immediate constant value
- **CONSTANT_SHORT:** `value:S` - Short-form constant
- **REGISTER:** `Rn` - Copy from another register
- **LOCAL:** `B.displacement` - Load from local variable
- **LOCAL_SHORT:** `B.disp:S` - Short-form local access
- **LOCAL_INDIRECT:** `IND(B.ptr)` - Indirect through local pointer
- **RECORD:** `R.displacement` - Load from record field
- **RECORD_SHORT:** `R.disp:S` - Short-form record access
- **RECORD_INDIRECT:** `IND(R.ptr)` - Indirect through record pointer
- **ABSOLUTE:** `address` - Load from absolute address
- **ABSOLUTE_INDIRECT:** `IND(address)` - Indirect through absolute address
- **PRE_INDEXED:** `B.array(Rn)` - Pre-indexed array access
- **POST_INDEXED:** `B.base(Rn)` - Post-indexed access
- **DESCRIPTOR:** `DESC(operand)(Rn)` - Descriptor-based access

---

## Trap Conditions

- **OPERAND_ERROR (Bit 5):** Invalid addressing mode or alignment
- **MEMORY_PROTECT (Bit 6):** Source address violates memory protection
- **PAGE_FAULT (Bit 7):** Source page not present

---

## Data Status Bits

- **Z (Zero):** Set if loaded value is zero
- **S (Sign):** Set if loaded value is negative (sign bit set)
- **O (Overflow):** Cleared
- **K (Flag):** Preserved

---

## Examples

### Example 1: Load Immediate Constant

```assembly
        W1 := 42                ; Load constant 42 into W1
        H2 := 0x1000            ; Load hex constant into H2
        F3 := 3.14159           ; Load float constant into F3
```

**Explanation:** Most basic form - loading immediate constants into registers.

### Example 2: Load from Memory

```assembly
        W1 := B.COUNTER         ; Load local variable COUNTER into W1
        H2 := R.AGE             ; Load record field AGE into H2
        W3 := 0x2000            ; Load from absolute address 0x2000
```

**Explanation:** Loading values from various memory locations.

### Example 3: Register Copy

```assembly
        W2 := W1                ; Copy W1 to W2
        F4 := F1                ; Copy F1 to F4
```

**Explanation:** Copying values between registers.

### Example 4: Array Access

```assembly
        W1 := 5                 ; Index value
        W2 := B.ARRAY(W1)       ; Load ARRAY[5] into W2
        H3 := R.TABLE(W1)       ; Load TABLE[5] into H3
```

**Explanation:** Pre-indexed array access using another register as index.

### Example 5: Indirect Access

```assembly
        W1 := B.PTR             ; Load pointer value
        W2 := IND(B.PTR)        ; Load value pointed to by PTR
```

**Explanation:** Dereferencing a pointer stored in local variable.

---

## Performance Notes

- **Typical cycles:** 2-8 cycles (varies by addressing mode)
- **Best case:** 2 cycles (immediate constant to register)
- **Worst case:** 8+ cycles (complex indirect addressing or cache miss)

**Cycle counts by addressing mode:**
- CONSTANT: 2 cycles
- REGISTER: 2 cycles
- LOCAL/RECORD: 3-4 cycles
- ABSOLUTE: 4-5 cycles
- INDIRECT modes: 5-8 cycles
- DESCRIPTOR: 6-10 cycles

**Optimization tips:**
- Use CONSTANT_SHORT when possible (faster encoding)
- Keep frequently-accessed values in registers
- Minimize indirect addressing in tight loops

---

## Reference Manual

**Section:** §10.1
**Title:** Load
**Page:** 137

---

## See Also

- [=: instruction](assignfrom.md) - Store (reverse operation)
- [MOVE instruction](move.md) - Explicit move operation
- [CLR instruction](clr.md) - Clear register to zero
- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
