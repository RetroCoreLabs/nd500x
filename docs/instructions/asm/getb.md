# GETB - Get Buddy Element

## Overview

**Mnemonic:** `getb`
**Function:** Allocate buddy element from heap
**Class:** BITFIELD
**Privilege:** user

**Format:** `Wn GETB <log size/r/BY>`

---

## Description

Allocates an element of size 2^`<log size>` words from the heap using the buddy system memory allocator. Returns the address of the allocated element in the specified word register.

The buddy system maintains freelists of memory blocks organized by size (powers of 2). When a block of the requested size is not available, GETB splits larger blocks until a suitable block can be allocated. The unused halves from splitting are added to their respective freelists.

If no block of the requested size or larger is available on the heap, a stack overflow trap (STO) is triggered. This allows trap handlers to implement heap expansion or garbage collection strategies.

The heap administration is described in §3.3 of the Reference Manual. When executing GETB, the TOS register must point to the variables describing the heap structure.

This instruction is essential for dynamic memory allocation in high-level language runtimes, particularly for implementing heap-based data structures (lists, trees, objects) in languages compiled for the ND-500.

**Operands:** 1
**Variants:** 4 opcode(s)

---

## Variants

Total variants: 4 (word type × 4 registers)

| Variant | Opcode | Register | Assembly Notation |
|---------|--------|----------|-------------------|
| 1/4 | 0xFE4C | W1 | W1 GETB |
| 2/4 | 0xFE4D | W2 | W2 GETB |
| 3/4 | 0xFE4E | W3 | W3 GETB |
| 4/4 | 0xFE4F | W4 | W4 GETB |

---

## Operands

### Operand 1 (Log Size)

The base-2 logarithm of the desired block size in words. A value of N allocates 2^N words.

**Type:** Byte (unsigned)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local variable containing log size
- **RECORD** - Record field containing log size
- **CONSTANT** - Immediate log size value
- **REGISTER** - Register containing log size
- **PRE_INDEXED** - Indexed addressing
- **ABSOLUTE** - Absolute memory address

**Common values:**
- 3 = 8 words (32 bytes)
- 4 = 16 words (64 bytes)
- 5 = 32 words (128 bytes)
- 6 = 64 words (256 bytes)
- 7 = 128 words (512 bytes)

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **Stack overflow (STO):** No blocks of requested size or larger available on heap

---

## Data Status Bits

- **Z (Zero):** Not affected
- **S (Sign):** Not affected
- **O (Overflow):** Not affected
- **C (Carry):** Not affected

---

## Examples

### Example 1: Allocate 64-word block

```assembly
        % Allocate a 64 word data block from the heap
        % 2^6 = 64 words = 256 bytes
        W3 GETB 6
        % W3 now contains the address of the allocated block
```

### Example 2: Allocate string buffer

```assembly
        % Allocate 128-word buffer for string processing
        W1 GETB 7
        % W1 points to 512-byte buffer
        % Initialize buffer pointer
        R := I1
```

### Example 3: Dynamic allocation with size from variable

```assembly
        % Allocate block with size determined at runtime
        % B.REQ_SIZE contains log2 of required size
        W2 GETB B.REQ_SIZE
        % W2 contains address of allocated block
        W TEST I2
        IF=GO ALLOC_FAILED
```

### Example 4: Allocate array of structures

```assembly
        % Allocate array of 16 records, 4 words each = 64 words total
        W1 GETB 6
        % Store array base
        W MOVE I1, B.ARRAY_BASE
        % Initialize first element
        R := I1
        W MOVE 0, R.0
```

### Example 5: Heap allocation with trap handling

```assembly
        % Allocate block with heap overflow protection
        W1 GETB 5
        % If we get here, allocation succeeded
        % W1 contains block address
        GO ALLOC_OK

HEAP_TRAP:
        % Trap handler for stack overflow
        % Could trigger garbage collection here
        CALL GARBAGE_COLLECT
        % Retry allocation
        W1 GETB 5
ALLOC_OK:
        % Continue with allocated block in W1
```

---

## Performance Notes

- **Typical cycles:** 10-20 cycles depending on freelist state
- **Best case:** 10 cycles (exact size available on freelist)
- **Worst case:** 50+ cycles (block splitting required, multiple list operations)

**Note:** Performance degrades if the heap becomes fragmented. The buddy system helps mitigate fragmentation by maintaining power-of-2 sized blocks. Blocks that are split are recombined when freed.

**Heap organization:** TOS register must point to heap descriptor containing:
- Freelist heads for each block size
- Heap base and limit addresses
- Allocation statistics

---

## Reference Manual

**Section:** §15.13
**Title:** Get buddy element

---

## See Also

- [FREEB](freeb.md) - Free buddy element back to heap
- [GETBF](getbf.md) - Get bit field (different instruction)
- [PUTBF](putbf.md) - Put bit field
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
