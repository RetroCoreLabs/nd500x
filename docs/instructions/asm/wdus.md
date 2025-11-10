# WDUS - Write Bypassing Cache

## Overview

**Mnemonic:** `wdus`
**Function:** Store data bypassing cache
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `tn WDUS <dest>`

---

## Description

Stores the contents of a register to main memory, bypassing the cache entirely. This instruction is the write counterpart to RDUS and is used when direct memory writes are required, such as for DMA buffers, memory-mapped I/O, or shared memory in multiprocessor systems.

**Operation:**
```
Rn → <dest> (bypass cache)
Memory updated directly
Cache invalidated for target address
```

**Key Characteristics:**
- Privileged instruction (supervisor mode only)
- Bypasses cache on write
- Writes directly to main memory
- Invalidates corresponding cache line
- Ensures immediate visibility to DMA controllers and other processors

**Common Use Cases:**
- Setting up DMA buffers for I/O operations
- Writing to memory-mapped I/O registers
- Inter-processor communication in multiprocessor systems
- Writing to shared memory segments
- System-level memory management tasks

**Important:** If the shared segment bit in the capability table is set, the cache is never used for that segment. In such cases, WDUS behaves identically to the ordinary store (`=:`) instruction.

**Operands:** 1 (destination in memory)
**Variants:** 12 opcodes (3 data types × 4 registers)

---

## Variants

Total variants: 12 (3 data types × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly |
|---------|--------|-----------|----------|----------|
| 1-4 | 0xFEC0-0xFEC3 | BY | I1-I4 | BYn WDUS |
| 5-8 | 0xFEC4-0xFEC7 | H | I1-I4 | Hn WDUS |
| 9-12 | 0xFEC8-0xFECB | W | I1-I4 | Wn WDUS |

---

## Operands

### Operand 1 (Destination)

The memory location to write to, bypassing cache.

**Type:** Byte, Halfword, or Word (matching register type)
**Access:** Write (to main memory, not cache)

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

**Illegal modes:**
- **CONSTANT** - Illegal (constants are read-only)
- **REGISTER** - Ambiguous (use register move instead)

**Note:** Only memory addressing modes are valid for destination.

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **Privilege violation:** If executed in user mode
- **Write protection:** Attempt to write to read-only memory

---

## Data Status Bits

No flags affected (unlike RDUS, WDUS does not set Z or S flags).

---

## Examples

### Example 1: Prepare DMA output buffer

```assembly
        % Write data to DMA buffer, bypassing cache
        W1 := OUTPUT_DATA
        W1 WDUS DMA_OUT_BUFFER
        % Now DMA controller can read the data
```

**Explanation:** Before initiating DMA, write data directly to memory so the DMA controller sees it immediately.

### Example 2: Write to memory-mapped I/O register

```assembly
        % Send command to hardware device
        W2 := COMMAND_CODE
        W2 WDUS IO_COMMAND_REG
```

**Explanation:** Memory-mapped I/O registers must be written directly to hardware, not to cache.

### Example 3: Update shared memory flag

```assembly
        % Signal other processor via shared memory
        W3 := STATUS_READY
        W3 WDUS SHARED_STATUS
```

**Explanation:** In multiprocessor systems, shared flags must bypass cache for immediate visibility to other CPUs.

### Example 4: Write array element for DMA

```assembly
        % Prepare indexed array element for DMA
        W1 := ELEMENT_INDEX
        W2 := DATA_VALUE
        W2 WDUS DMA_ARRAY(W1)
```

**Explanation:** When preparing array data for DMA transfer, use WDUS with indexed addressing.

### Example 5: Halfword write to hardware register

```assembly
        % Set halfword control value
        H4 := CONTROL_VALUE
        H4 WDUS HW_CONTROL_REG
```

**Explanation:** Hardware control registers often require halfword values written directly to hardware.

### Example 6: Byte write to device status

```assembly
        % Send byte command to device
        BY1 := DEVICE_CMD
        BY1 WDUS DEVICE_CMD_REG
```

**Explanation:** Some I/O devices use byte-sized command registers.

### Example 7: Write local variable for inter-process communication

```assembly
        % Write IPC message to shared local variable
        W4 := MESSAGE_ID
        W4 WDUS B.IPC_MESSAGE
        W4 := MESSAGE_DATA
        W4 WDUS B.IPC_DATA
```

**Explanation:** When using shared memory segments for IPC, WDUS ensures immediate visibility across processes.

---

## Performance Notes

- **Execution:** 3-5 cycles (slower than normal store due to cache bypass)
  - Memory write: 3 cycles (minimum)
  - Cache invalidation: +1 cycle
  - Page fault: +100+ cycles
- **Cache behavior:** Always writes to main memory, invalidates cache line
- **No cache:** If no cache is present, WDUS is equivalent to normal store (`=:`)

**Usage recommendations:**
- Use sparingly - only when cache bypass is essential
- Normal stores (`=:`) are faster and sufficient for most cases
- Use before initiating DMA transfers or when writing memory-mapped I/O
- If segment is marked as shared in capability table, normal stores bypass cache anyway

**Comparison with similar instructions:**
- `WDUS` vs `RDUS`: WDUS writes, RDUS reads (both bypass cache)
- `WDUS` vs `=:`: WDUS bypasses cache, `=:` uses cache
- `WDUS` + `RDUS`: Used together for cache-coherent DMA operations

---

## Reference Manual

**Section:** §16.26
**Title:** Operating Systems Support Instructions

---

## See Also

- [RDUS](rdus.md) - Read bypassing cache (load with cache bypass)
- [=:](assignfrom.md) - Normal store operation (uses cache)
- [WIOM](wiom.md) - Write I/O memory
- [Cache System](../ND500_CACHE_SYSTEM.md) - ND-500 cache architecture
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md) - Trap handling
