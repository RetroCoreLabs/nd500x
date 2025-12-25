# Analysis: ND-500 Reference Manual Chapter 3 - STATIC DATA, STACK AND HEAP

## Key Findings from Chapter 3

### 3.2 Stack Allocation (Page 32)

**TOS Register Dual Purpose:**
> "When a stack is initialized, the TOS register is loaded with the address of the first free location beyond the stack's maximum extent. **TOS serves to prevent the stack from growing too large, and as a pointer to the variables describing the heap.**"

**Key Points:**
- INIT or ENTM initializes the stack
- TOS = address of first free location beyond stack's maximum extent
- TOS has TWO functions:
  1. Stack overflow protection (prevents stack from growing too large)
  2. Pointer to heap variables structure

### 3.3 Heap Allocation (Pages 34-35)

**Heap Variables Structure (Figure 4):**

```
TOS ->    | MAXL      | Max log size of elements allowed
          | STAH      | Start of heap (not used by heap instructions)
          | ENDH      | End of heap (not used by heap instructions)
          | FLOG0     | Head pointers for freelists of elements
          | FLOG1     | of the different log sizes.
          | FLOG2     | The freelist pointers have the value
          | FLOG3     | 0 if no element of the log size
          | .         | is available.
          | .         |
          | FLOG<MAXL>|
```

**CRITICAL REQUIREMENT (Page 35, line 1170):**
> **"The heap variables must be initialized by the user program and the user is responsible for building the lists."**

**STAH and ENDH:**
> "The STAH and ENDH variables are not used by the heap instructions, but are available for a heap administration routine implemented as a trap handler for the stack overflow trap."

**GETB Behavior (Page 35, lines 1184-1186):**
> "If a block of the requested size is available, it is unlinked from the list. If the list head is zero, indicating that the list is empty, lists representing larger blocks are examined. If a larger block is available, it is split in halves and one half is left in the appropriate freelist. The block may have to be split several times before an element of the requested size can be given to the program. **If no larger element is available, or if the requested size is larger than the MAXL value, a stack overflow trap condition occurs.**"

**Important Note (Page 35, line 1194):**
> "Be aware that initializing a new stack by INIT or ENTM will change TOS, thus another set of heap variables will be used by the buddy instructions. **The new heap variables may be initialized to the values of the old ones or to new values.**"

## Analysis: What This Means for Our Emulator

### Current Problem

1. **TOS is correctly set** to `0x801D538` (points to heap variables structure)
2. **MAXL is correctly set** to `0x17` (23) at `TOS+0`
3. **STAH and ENDH** are `0x00000000` (unused by heap instructions, OK)
4. **ALL FLOG[] arrays are empty** (all zeros) - THIS IS THE PROBLEM

### What Should Happen

According to the manual:
- **"The heap variables must be initialized by the user program"** - This means SINTRAN (the OS) should initialize them
- **"The user is responsible for building the lists"** - SINTRAN must allocate memory and link free blocks into FLOG[]

### What SINTRAN Should Do

When SINTRAN initializes a domain, it should:

1. **Allocate physical memory** for the heap pool (between STAH and ENDH addresses)
2. **Initialize MAXL** in heap variables (already done in DOM file)
3. **Create initial free blocks** and link them into FLOG[] arrays:
   - At minimum: Add one large block (size 2^MAXL words) to FLOG[MAXL]
   - This allows GETB to split it down to smaller sizes as needed
4. **Link blocks into freelists**:
   - Each free block's first word contains address of next free block
   - Last block in list has 0 in first word
   - FLOG[k] points to first block in list for size 2^k words

### What Our DOM Loader Should Do

Since we're emulating SINTRAN's initialization:

**Option 1: Initialize heap in DOM loader**
- Allocate a heap memory pool (e.g., 1MB starting after program/data segments)
- Set STAH and ENDH to heap pool boundaries
- Create one large free block covering entire heap pool
- Link it into FLOG[MAXL] (where MAXL=23, so block size = 2^23 = 8M words = 32MB)
- Actually, we should create a smaller initial block, maybe FLOG[15] = 32K words = 128KB

**Option 2: Wait for SINTRAN MON call**
- Some MON call (e.g., MON 422B GSWSP) might initialize the heap
- But we already implemented MON 422B and it doesn't initialize freelists

### Recommended Fix

**In `ndlib_dom_load_to_machine()`:**

1. After loading segments, allocate heap memory pool:
   ```c
   uint32_t heap_start = (phys_prog_base + total_prog_size + 0x7FF) & ~0x7FFu;  // Page-aligned
   uint32_t heap_size = 0x100000;  // 1MB heap pool
   uint32_t heap_end = heap_start + heap_size;
   ```

2. Find heap variables structure address (from TOS or DOM common part):
   - TOS might be set later by INIT/ENTM
   - Or we can read it from DOM file if available
   - Or allocate it in DATA segment

3. Initialize heap variables:
   ```c
   uint32_t heap_vars_addr = ...;  // Where TOS will point
   nd500_write_memory_32(cpu, heap_vars_addr + 0, max_log);  // MAXL
   nd500_write_memory_32(cpu, heap_vars_addr + 4, heap_start);  // STAH
   nd500_write_memory_32(cpu, heap_vars_addr + 8, heap_end);   // ENDH
   // Clear all FLOG[] arrays (already zeros)
   ```

4. Create initial free block and link into FLOG[15] (32K words = 128KB):
   ```c
   uint32_t initial_block_addr = heap_start;
   uint32_t initial_block_size = 0x8000;  // 32K words = 128KB
   uint32_t flog15_addr = heap_vars_addr + 12 + (15 * 4);
   
   // Link block into FLOG[15]: first word = 0 (end of list), FLOG[15] = block_addr
   nd500_write_memory_32(cpu, initial_block_addr, 0);  // Next pointer = 0 (end of list)
   nd500_write_memory_32(cpu, flog15_addr, initial_block_addr);  // FLOG[15] = block_addr
   ```

5. When TOS is set (by INIT/ENTM), it should point to `heap_vars_addr`

## Summary

**The Real Problem:**
- TOS address is correct ✓
- Heap variables structure exists ✓
- MAXL is valid ✓
- **Freelists are empty** ✗ - This is why GETB fails

**The Solution:**
- DOM loader must allocate heap memory pool
- DOM loader must initialize at least one free block in FLOG[]
- This mimics what SINTRAN would do during domain initialization

**Reference:**
- ND-500 Reference Manual §3.3 (Pages 34-35)
- "The heap variables must be initialized by the user program and the user is responsible for building the lists."


