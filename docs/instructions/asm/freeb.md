# FREEB - Free Buddy Element

## Overview

**Mnemonic:** `freeb`
**Function:** Release buddy element to heap
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `FREEB <log size/r/BY>,<element/s/W>`

---

## Description

Releases a previously allocated memory block back to the heap's buddy system freelist. The block is appended to the appropriate freelist based on its size (2^`<log size>` words).

**Key Characteristics:**
- Buddy system deallocation (power-of-2 size classes)
- No automatic coalescing (deferred to trap handlers)
- Supervisor privilege required (OS-level operation)
- Faster than GETB (simple freelist append)
- TOS register must point to heap structure
- Log size must match original allocation
- DESC prefix prevents index register update
- Essential for OS memory management

FREEB does not perform buddy coalescing automatically - blocks are simply returned to their size-specific freelist. Coalescing of adjacent free blocks into larger blocks may be performed by trap handlers (typically during stack overflow conditions when the heap needs to be compacted).

The heap administration is described in §3.3 of the Reference Manual. When executing FREEB, the TOS register must point to the variables describing the heap structure (same as GETB).

Write access to the element is required. If the element is addressed using a descriptor (DESC prefix), the index register is not updated during the operation.

This instruction requires supervisor privilege, making it a privileged operation typically used by operating system memory managers rather than user code.

**Operands:** 2
**Variants:** 1 opcode(s)

---

## Variants

Total variants: 1 (no register variants)

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFDB6 | FREEB |

---

## Operands

### Operand 1 (Log Size)

The base-2 logarithm of the block size being freed (same value used in GETB).

**Type:** Byte
**Access:** Read

**Supported modes:**
- **LOCAL** - Local variable containing log size
- **RECORD** - Record field containing log size
- **CONSTANT** - Immediate log size value
- **REGISTER** - Register containing log size
- **PRE_INDEXED** - Indexed addressing
- **ABSOLUTE** - Absolute memory address

**Note:** The log size must match the original allocation size. Freeing a block with incorrect size causes undefined behavior.

### Operand 2 (Element Address)

The address of the memory block to free (typically the value returned by GETB).

**Type:** Word
**Access:** Write (address validation)

**Supported modes:**
- **LOCAL** - Local variable containing element address
- **RECORD** - Record field containing element address
- **CONSTANT** - Immediate address (unusual)
- **REGISTER** - Register containing element address
- **PRE_INDEXED** - Indexed addressing
- **ABSOLUTE** - Absolute memory address

**Note:** When using DESC prefix for descriptor addressing, the index register is not updated.

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **Privilege violation:** Executed in user mode

---

## Data Status Bits

- **Z (Zero):** Not affected
- **S (Sign):** Not affected
- **O (Overflow):** Not affected
- **C (Carry):** Not affected

---

## Examples

### Example 1: Free allocated block

```assembly
        % Allocate 64-word block
        W3 GETB 6
        % Use the block
        % ...
        % Free the block back to heap
        FREEB 6, I3
```

### Example 2: Free descriptor-based block

```assembly
        % Free string buffer allocated via descriptor
        % LINE is a descriptor containing address and length
        % 128 bytes = 32 words = 2^5
        FREEB 5, IND(LINE)
        % Descriptor index register not modified
```

### Example 3: Free block from table

```assembly
        % Free all blocks in allocation table
        W1 := 0
FREE_LOOP:
        % Get block address from table
        W2 := B.ALLOC_TABLE(I1)
        % Skip if null
        W TEST I2
        IF=GO NEXT_BLOCK
        % Free the block (all are 16-word blocks)
        FREEB 4, I2
        % Clear table entry
        W MOVE 0, B.ALLOC_TABLE(I1)
NEXT_BLOCK:
        W INCR I1
        W COMP I1, B.TABLE_SIZE
        IF<GO FREE_LOOP
```

### Example 4: Conditional free with error handling

```assembly
        % Free block only if valid
        W1 := B.BLOCK_ADDR
        W TEST I1
        IF=GO NO_FREE
        % Validate block is in heap range
        W COMP I1, B.HEAP_BASE
        IF<GO INVALID_ADDR
        W COMP I1, B.HEAP_LIMIT
        IF>GO INVALID_ADDR
        % Free the block
        FREEB B.BLOCK_SIZE, I1
        GO FREE_OK

INVALID_ADDR:
        CALL HEAP_ERROR_HANDLER
NO_FREE:
FREE_OK:
```

### Example 5: Memory manager deallocation routine

```assembly
        % Memory manager routine to free with size tracking
        % Input: W1 = block address
        % B.SIZE_TABLE indexed by block address contains log size
MM_FREE:
        % Validate address is not null
        W TEST I1
        IF=GO MM_FREE_DONE
        % Calculate size table index from address
        W2 := I1
        W SHR I2, 5          % Divide by 32 (assuming 32-word minimum)
        % Get log size from table
        BY1 := B.SIZE_TABLE(I2)
        % Free the block
        FREEB I1, I1
        % Clear size table entry
        BY MOVE 0, B.SIZE_TABLE(I2)
MM_FREE_DONE:
        RET
```

---

## Performance Notes

- **Typical cycles:** 8-12 cycles depending on heap state
- **Best case:** 8 cycles (simple freelist append)
- **Worst case:** 12+ cycles (descriptor validation, complex addressing)

**Note:** FREEB is faster than GETB because it only appends to a freelist rather than searching and potentially splitting blocks. Actual heap compaction/coalescing is deferred to trap handlers.

**Privilege requirement:** This instruction requires supervisor mode. User code cannot directly manage heap freelists, preventing corruption of system memory structures.

---

## Reference Manual

**Section:** §15.14
**Title:** Free buddy element

---

## See Also

- [GETB](getb.md) - Allocate buddy element from heap
- [Privilege Levels](../ND500_PRIVILEGE_MODES.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
- [Heap Management](../ND500_HEAP_MANAGEMENT.md)
