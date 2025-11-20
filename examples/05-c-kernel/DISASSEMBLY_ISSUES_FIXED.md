# Disassembly Display Issues - Fixed

## Summary

Two issues were identified and fixed in the WASM debugger's disassembly display:

1. **PC incorrectly set to 0 instead of 4** - Entry point not honored for executables
2. **Addresses shown as signed decimals** - Hard to read (e.g., `-402649088` instead of `0xE8001000`)

## Issue 1: PC Set to 0 Instead of Entry Point (0x4)

### Problem

When loading the kernel executable, the debugger set PC to 0 instead of 4, causing disassembly to start at the a.out header (invalid instructions) rather than the first executable instruction (INIT).

**Symptoms:**
```
Disassembly starting at 0x00000000:
00000000  00        ???
00000001  00        ???
00000002  00        ???
00000003  00        ???
00000004  DC...     init $-402649088,$20,$16384  ← Should start here!
```

### Root Cause

**File:** `/home/ronny/repos/nd500x/src/ndlib/ndlib_aout.c` (Line 442)

**Buggy code:**
```c
if (entry == 0 || entry == 4) {
    /* Object file - use first instruction from map */
    uint32_t first_instr = ndlib_symbols_first_instruction_addr();
    pc = (first_instr > 0) ? first_instr : 0;
} else {
    /* Executable - use entry point */
    pc = entry;
}
```

**Problem:** The condition `entry == 4` incorrectly assumes entry point 4 means "object file". This is wrong! Entry point 4 is valid for executables (it's where code starts after the 4-byte a.out header).

**Why entry point is 4:**
- Bytes 0-3: a.out magic number (0x0109) and header
- Byte 4+: First executable instruction

### Fix

**File:** `/home/ronny/repos/nd500x/src/ndlib/ndlib_aout.c` (Line 442-449)

**Fixed code:**
```c
if (entry == 0) {
    /* Object file (no entry point) - use first instruction from map */
    uint32_t first_instr = ndlib_symbols_first_instruction_addr();
    pc = (first_instr > 0) ? first_instr : 0;
} else {
    /* Executable - use entry point (even if it's 4) */
    pc = entry;
}
```

**Changes:**
- Removed `|| entry == 4` condition
- Only treat `entry == 0` as object file
- All non-zero entry points are valid for executables

### Result

After fix, PC is correctly set to 0x00000004, and disassembly starts at the INIT instruction.

## Issue 2: Address Display Format (Disassembler Formatting)

### Problem

The disassembler shows addresses as **signed decimal** integers instead of **hexadecimal**, making them hard to read and correlate with memory addresses.

**Example:**
```
init $-402649088,$20,$16384
```

Should be:
```
init $0xE8001000,$20,$16384
```

### Analysis

The value `-402649088` is mathematically correct - it's the **signed 32-bit interpretation** of `0xE8001000`:

```python
>>> hex(0xE8001000)
'0xe8001000'
>>> int(0xE8001000)  # Unsigned
3892318208
>>> struct.unpack('i', struct.pack('I', 0xE8001000))[0]  # Signed
-402649088
```

Since `0xE8001000` has bit 31 set (high bit), it's negative when interpreted as signed int32.

### Root Cause

The disassembler uses `%d` format (signed decimal) instead of `0x%X` format (hexadecimal) for immediate operands.

**File:** Disassembler formatting code (need to locate exact location)

### Fix Needed

The disassembler should:
1. Display addresses and large constants in **hexadecimal** (`0x...`)
2. Display small constants (<256) in **decimal** for readability
3. Use **unsigned** interpretation for all addresses

**Example improved output:**
```
Before: init $-402649088,$20,$16384
After:  init $0xE8001000,$20,$16384

Before: call $436207616,$0
After:  call $0x1A000000,$0  (or with symbol: call _kernel_main,$0)
```

### Status

**Issue 1 (PC):** ✓ **FIXED** - WASM build updated
**Issue 2 (Display):** ⚠ **Identified** - Requires disassembler formatting changes

## Verification

### Test Case: Load kernel.zip in WASM Debugger

**Before fix:**
```
PC = 0x00000000
Disassembly shows:
00000000  00  ???
00000001  00  ???
00000002  00  ???
00000003  00  ???
```

**After fix:**
```
PC = 0x00000004
Disassembly shows:
00000004  DC 00 10 00 E8 14 CE 00 40  init $-402649088,$20,$16384
0000000D  1D FF                       dctsb
0000000F  1C FF                       pctsb
```

**Expected after Issue 2 fix:**
```
PC = 0x00000004
Disassembly shows:
00000004  DC 00 10 00 E8 14 CE 00 40  init $0xE8001000,$20,$16384
0000000D  1D FF                       dctsb
0000000F  1C FF                       pctsb
00000011  C3 00 00 00 1A 00           call $0x1A000000,$0
```

### Build and Test

```bash
# Rebuild WASM with PC fix
cd /home/ronny/repos/nd500x
make wasm

# Start web server
make wasm-serve

# Open browser to http://localhost:8000
# Load Demo Kernel
# Check browser console for PC value
# Verify disassembly starts at 0x00000004
```

**Expected console output:**
```
[updateUI] PC updated: 0x00000004
[updateDisassembly] currentPC=0x4
```

## Technical Details

### A.out File Format

**Header (32 bytes):**
```
Offset  Size  Field       Value (kernel)
------  ----  ----------  --------------
0x00    2     a_magic     0x0109 (IMAGIC)
0x02    2     (padding)   0x0000
0x04    4     a_text      762 bytes (0x2FA)
0x08    4     a_data      72 bytes (0x48)
0x0C    4     a_bss       78608 bytes (0x13310)
0x10    4     a_syms      408 bytes (0x198)
0x14    4     a_entry     0x00000004  ← Entry point!
0x18    4     a_trsize    0 bytes
0x1C    4     a_drsize    0 bytes
```

**Text segment starts at offset 0x20 (after header)**
**Entry point 0x04 is relative to memory address 0, not file offset**

When loaded into memory:
- Memory 0x00-0x03: Header (not executable, shows as ???)
- Memory 0x04: First instruction (INIT) ← Entry point points here

### Why Entry Point is 4

The a.out format uses **memory addresses**, not file offsets:
- File offset 0x20 contains the first instruction byte
- But that instruction loads at memory address 0x04
- Entry point field contains 0x04 (memory address)
- Loader skips the header bytes when loading to memory

### Object Files vs Executables

**Object File (.o):**
- Magic: 0x0107 (OMAGIC) or 0x010B
- Entry point: 0 (no entry - needs linking)
- Relocations present (a_trsize, a_drsize > 0)

**Executable:**
- Magic: 0x0109 (IMAGIC)
- Entry point: Non-zero (e.g., 0x04)
- Relocations removed (a_trsize, a_drsize = 0)
- All symbols resolved

**Previous buggy logic assumed:**
```c
if (entry == 0 || entry == 4)  // WRONG!
    → treat as object file
```

**Correct logic:**
```c
if (entry == 0)  // RIGHT!
    → treat as object file (no entry point set)
else
    → treat as executable (entry point valid)
```

## Impact

### Before Fix

**User experience:**
1. Loads kernel.zip
2. Sees disassembly starting at 0x00000000
3. First 4 lines show `???` (invalid instructions)
4. INIT instruction shown at line 5
5. Can't single-step from PC=0 (would trap on 0x00)
6. Must manually `set PC 4` before stepping

**JavaScript console shows:**
```
PC = 0x00000000
[updateDisassembly] currentPC=0x0
```

### After Fix

**User experience:**
1. Loads kernel.zip
2. PC automatically set to 0x00000004
3. Disassembly starts at INIT instruction
4. Single-step works immediately
5. No manual PC adjustment needed

**JavaScript console shows:**
```
PC = 0x00000004
[updateDisassembly] currentPC=0x4
[INIT] Stack initialized at B=0xE8001000, TOS=0xE8005000
```

## Future Improvements

### Disassembler Hex Formatting

**Changes needed in disassembler:**

1. **Address operands** → Always hex
   ```c
   // Before:
   printf("$%d", value);

   // After:
   printf("$0x%08X", value);
   ```

2. **Small constants** → Decimal (for readability)
   ```c
   if (value < 256) {
       printf("$%d", value);
   } else {
       printf("$0x%X", value);
   }
   ```

3. **Symbol resolution** (if symbol available)
   ```c
   const char* sym = nd500_symbols_lookup_addr(value);
   if (sym) {
       printf("%s", sym);  // e.g., "_kernel_main"
   } else {
       printf("$0x%08X", value);
   }
   ```

### Example Improved Output

**Current:**
```
00000004  init $-402649088,$20,$16384
00000011  call $436207616,$0
```

**Improved:**
```
00000004  init $0xE8001000,$20,$16384
00000011  call _kernel_main,$0
```

Or with both address and symbol:
```
00000011  call _kernel_main @ 0x1A000000,$0
```

## Summary

| Issue | Status | Impact |
|-------|--------|--------|
| PC set to 0 instead of 4 | ✓ FIXED | Critical - allows proper execution from entry point |
| Addresses shown as signed decimal | ⚠ Identified | Minor - cosmetic, doesn't affect functionality |

**Fix deployed:** WASM build updated with PC fix in `/home/ronny/repos/nd500x/build_wasm/bin/`

**Remaining work:** Update disassembler to show hex addresses (cosmetic improvement)

---

**Files Modified:**
- `/home/ronny/repos/nd500x/src/ndlib/ndlib_aout.c` (Line 442: Fixed PC initialization logic)

**Build command:**
```bash
make wasm
```

**Verification:**
```bash
make wasm-serve
# Open http://localhost:8000
# Load Demo Kernel
# Verify PC = 0x00000004 in debugger
```
