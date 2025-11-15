# ND-500 Kernel Bootstrap - Stack Initialization Fix

## Problem with Original kernel.s

The original `kernel.s` starts with:

```assembly
_start:
    ents    $LFU1    # ← WRONG! ENTS requires CALL or INIT first
    ...
```

This violates the ND-500 architecture specification:

> **ND-500 Reference Manual, Page 230:**
> "Execution of an entry point instruction (except ENTT) **not resulting from a subroutine call** will cause an **instruction sequence error trap condition**."

### Why It Fails

1. **CPU Reset State**: After reset, `B=0` and `TOS=0`
2. **ENTS Operation**: Reads `newB` from `B+8` (address 8)
3. **Uninitialized Memory**: Address 8 contains garbage (or 0)
4. **Stack Overflow Check**: `newB + demand >= TOS` → `0 + 96 >= 0` → **TRAP!**

## Solution: INIT Instruction

The **INIT** instruction (opcode 0x0DC, page 229) properly initializes the stack:

```assembly
INIT <bottom of stack>, <main stack demand>, <total stack demand>
```

### What INIT Does

```
<bottom of stack> → B
<bottom of stack> + <total stack demand> → TOS
<bottom of stack> + <main stack demand> → B.SP
0 → B.PREVB
0 → B.RETA → L
```

### Example

```assembly
_bootstrap:
    init    stack_bottom, 0x1000, 0x10000
    call    _start, 0
    go      _halt
```

This creates:
- **B = 0x00010000** (stack starts at 64KB)
- **TOS = 0x00020000** (stack ends at 128KB, total 64KB space)
- **B.SP = 0x00011000** (first frame at bottom + 4KB)
- **B.PREVB = 0** (no previous frame, RET will trap)
- **B.RETA = 0** (no return address)

## Fixed Kernel Structure

```
bootstrap.s:
    - _bootstrap (entry point)
    - INIT stack
    - CALL _start

kernel.s:
    - _start (called from bootstrap)
    - ENTS $LFU1 (now legal!)
    - Normal kernel code
```

## How to Use

### Option 1: Compile with Bootstrap

```bash
# Assemble both files
nd500-as bootstrap.s -o bootstrap.o
nd500-as kernel.s -o kernel.o

# Link with bootstrap first (sets entry point)
nd500-ld bootstrap.o kernel.o -o kernel.out
```

### Option 2: Manual Debugger Setup

If you can't modify the build:

```
(debugger) set B 0x00010000
(debugger) set TOS 0x00020000
(debugger) set PC <address of _start>
(debugger) run
```

But this is hacky - proper bootstrap is better!

## Stack Memory Layout

```
Address Range        | Purpose
---------------------|----------------------------------
0x00000000-0x0000FFFF | Low memory (code, data)
0x00010000-0x0001FFFF | Stack area (64KB)
  0x00010000         |   ← B (stack bottom)
  0x00010000-0x00010013 |   Initial stack frame (20 bytes)
  0x00011000         |   ← B.SP (first function frame)
  ...                |   Stack grows upward
  0x0001FFFF         |   ← TOS-1 (last usable byte)
  0x00020000         |   ← TOS (overflow boundary)
0x00020000+          | Heap or other data
```

## Stack Overflow Protection

With `TOS = 0x00020000`, the stack overflow check in ENTS/ENTSN is:

```c
if (newB + stack_demand >= TOS) {
    trap_stack_overflow();
}
```

This prevents stack from growing beyond 64KB.

## References

- **ND-500 Reference Manual, Page 229**: INIT instruction
- **ND-500 Reference Manual, Page 230**: Entry point instructions
- **ND-500 Reference Manual, Chapter 3**: Local data area organization
- **RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Ents.cs**: Reference implementation

## Architecture Notes

1. **INIT vs ENTM**:
   - `INIT`: Used for main program initialization
   - `ENTM`: Used for module entry (can cross domain boundaries)

2. **Stack Frame Structure** (created by ENTS):
   ```
   B+0  (PREVB)  Previous B register value
   B+4  (RETA)   Return address
   B+8  (SP)     Stack pointer (B + stack_demand)
   B+12 (AUX)    Auxiliary field
   B+16 (N)      Argument count
   B+20+        Argument addresses
   ```

3. **Why PREVB=0 matters**: When the kernel does `RET`, it checks `PREVB`:
   ```c
   if (prev_b == 0) {
       trap_stack_underflow();  // Returning past outermost frame
   }
   ```

## Summary

✅ **Always use INIT** to set up the initial stack before any CALL/ENTS
✅ **bootstrap.s** provides the correct entry point
✅ **kernel.s** can now use ENTS safely
✅ **Stack overflow protection** works correctly
