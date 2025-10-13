# Disassembly Fixes - Final Summary

## Date: October 12, 2025

---

## Issues Fixed

### 1. ✅ Signed Branch Displacements  
**Problem:** `go $-31` displayed as `$3478734049` or other large positive numbers

**Root Causes:**
- Branch size selection was wrong (always selecting 4 bytes based on template priority)
- Displacements printed as unsigned instead of signed

**Solution:**
- Check if mnemonic is `go` or `if*` to use variant-based size (lines 226-233 in `cpu_instr.c`)
- Print displacements with signed format `$%d` for branches (lines 48-56 in `debug_api.c`)

**Result:** ✅ `00000098: C0 E1     go $-31`

---

### 2. ✅ Call Instruction Decoding
**Problem:** `call` instruction lost sync, showing only 4 bytes instead of 7

**Root Cause:**
- `call` has `O_DIR` bit set like branches, but doesn't use variant for size
- Code treated it like a PC-relative branch, reading only 1 byte for address

**Solution:**
- Distinguish between PC-relative branches (`go`, `if*go`) and other direct operands (`call`)
- PC-relative branches use variant for size (0=byte, 1=halfword, 2=word)
- `call` uses template bits (`O_WS` = 4 bytes) (lines 226-242 in `cpu_instr.c`)

**Result:** ✅ `call $0,$0,$32` reads all 7 bytes correctly

---

### 3. ✅ Branch Target Symbol Lookup
**Problem:** All branch targets showed `<_add>` symbol (address 0)

**Root Cause:**
- Code was trying to extract absolute address from operand data
- Branches use PC-relative displacements, not absolute addresses

**Solution:**
- Calculate absolute target: `target = PC + instruction_length + signed_displacement`
- Only show symbol comment for branch instructions using this calculation (lines 192-218 in `debug_api.c`)

**Result:** ✅ Branch targets now show correct symbols

---

### 4. ✅ Symbol Labels at Addresses
**Problem:** Symbols not displayed in disassembly

**Solution:**
- Implemented symbol caching system in `ndlib_symbols.c`
- Display symbol on separate line before instruction (lines 108-114 in `debug_api.c`)

**Result:**
```
00000026:                                   _main:
00000026: B8 CF 68 00 00 00             ents         $104
```

---

### 5. ✅ Unresolved Externals Header
**Problem:** No indication of unresolved external symbols

**Solution:**
- Added `ndlib_symbols_list_unresolved()` function
- Integrated into header output

**Result:**
```
; Unresolved Externals: 1
;   - _write
```

---

## Files Modified

### Core Decoder
**`src/cpu/cpu_instr.c`** (~30 lines changed)
- Lines 218-242: Fixed direct operand size detection
  - PC-relative branches (`go`, `if*go`) use variant
  - `call` and others use template bits

### Disassembly Output  
**`src/machine/debug_api.c`** (~40 lines changed)
- Lines 48-56: Signed displacement printing
- Lines 108-114: Symbol label display
- Lines 192-218: Correct branch target calculation

### Symbol System
**`src/ndlib/ndlib_symbols.c`** (rewritten, ~130 lines)
- Symbol caching with address lookup
- Unresolved externals listing

**`src/ndlib/ndlib_aout.c`** (~20 lines)
- Fancy header format
- Integrated unresolved externals

**`src/ndlib/ndlib.h`** (~2 lines)
- New function declarations

---

## Test Results

### Test 1: PC-Relative Branch
```bash
Input:  C0 E1 at 0x98
Before: go $5850337
After:  go $-31
✅ PASS
```

### Test 2: Call Instruction
```bash
Input:  C3 00 00 00 00 00 20
Before: call $0,$0,$0  (4 bytes, lost sync)
After:  call $0,$0,$32 (7 bytes, correct)
✅ PASS
```

### Test 3: Branch Target Symbols
```bash
Before: go $-31 ; <_add>  (wrong - address 0)
After:  go $-31            (no symbol if target has none)
        go $91  ; <_main>  (correct symbol)
✅ PASS
```

### Test 4: Symbol Labels
```bash
Before: 00000013: B8 CF 1C 00 00 00             ents         $28
After:  00000013:                                   _sub:
        00000013: B8 CF 1C 00 00 00             ents         $28
✅ PASS
```

---

## Technical Details

### Branch Size Selection Logic
```c
if (is_pc_relative_branch) {
    // go/if*go: variant determines size
    disp_len = (variant == 0) ? 1 : (variant == 1) ? 2 : 4;
} else {
    // call: check template bits
    if (tmpl & 0x08) disp_len = 4;  // O_WS
}
```

### Branch Target Calculation
```c
// PC-relative: target = PC + len + disp
int32_t displacement = (int8_t)data[0];  // signed!
uint32_t target = pc + total_len + displacement;
```

### Operand Templates
- `O_DIR` (0x20000) - Direct operand, no address code
- `O_WS` (0x08) - Word size (4 bytes)
- `O_HS` (0x04) - Halfword size (2 bytes)
- `O_BS` (0x01) - Byte size (1 byte)

---

## Comparison with nd500-dis

### Before
```
00000098: C0 E1 44 59 CF          go           $3478734049
000000CD: C3 00 00 00             call         $0,$0,$0
000000D1: 00                       ??? ; opcode 0x0000
```

### After (nd500x)
```
00000098: C0 E1                   go           $-31
000000CD: C3 00 00 00 00 00 20    call         $0,$0,$32
```

### Reference (nd500-dis)
```
00000098: C0 E1                   go           $-31
000000CD: C3 00 00 00 00 00 20    call         $0,$0,$32
```

✅ **Perfect Match!**

---

## Summary

All critical bugs have been fixed:
- ✅ Signed branch displacements work correctly
- ✅ Call instruction reads all operands without losing sync
- ✅ Branch target symbols calculated correctly (PC-relative)
- ✅ Symbol labels displayed at addresses
- ✅ Unresolved externals listed in header
- ✅ Fancy nd500-dis compatible output format

The nd500x disassembler now produces accurate output matching nd500-dis.


