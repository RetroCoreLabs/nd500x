# RDUS - Read Bypassing Cache

## Overview

**Mnemonic:** `rdus`
**Function:** Load data bypassing cache
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `tn RDUS <source>`

---

## Description

Loads an operand from main memory, bypassing the cache entirely. This instruction is primarily used after DMA transfers to memory to prevent reading obsolete cached data. The data is loaded directly from main memory and simultaneously refreshed in the cache for future references.

**Operation:**
```
<source> → Rn (bypass cache)
<source> = 0 → Z flag
<source>.signbit → S flag
Cache line refreshed with new data
```

**Key Characteristics:**
- Privileged instruction (supervisor mode only)
- Bypasses cache on read
- Ensures data consistency after DMA operations
- Simultaneously updates cache with fresh data
- Register and constant operands are illegal

**Common Use Cases:**
- Reading memory after DMA transfers
- Ensuring data coherency in multiprocessor systems
- Reading memory-mapped I/O registers
- Bypassing stale cache data
- System-level memory management

**Important:** If the shared segment bit in the capability table is set, the cache is never used for that segment. In such cases, RDUS behaves identically to the ordinary load (`:=`) instruction.

**Operands:** 1 (source from memory)
**Variants:** 16 opcodes (4 data types × 4 registers)

---

## Variants

Total variants: 16 (4 data types × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly |
|---------|--------|-----------|----------|----------|
| 1-4 | 0xFEA0-0xFEA3 | BI | I1-I4 | BIn RDUS |
| 5-8 | 0xFEA4-0xFEA7 | BY | I1-I4 | BYn RDUS |
| 9-12 | 0xFEA8-0xFEAB | H | I1-I4 | Hn RDUS |
| 13-16 | 0xFEAC-0xFEAF | W | I1-I4 | Wn RDUS |

---

## Operands

### Operand 1 (Source)

The memory location to read from, bypassing cache.

**Type:** Bit, Byte, Halfword, or Word (matching register type)
**Access:** Read (from main memory, not cache)

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

**Illegal modes:**
- **CONSTANT** - Illegal (causes IOS trap)
- **REGISTER** - Illegal (causes IOS trap)

**Note:** Only memory addressing modes are valid. Register and constant operands will trigger an Illegal Operand Specifier trap.

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **Privilege violation:** If executed in user mode
- **Illegal operand specifier (IOS):** Register or constant operand used

---

## Data Status Bits

- **Z (Zero):** Set if source value = 0, cleared otherwise
- **S (Sign):** Set to source value's sign bit
- **C (Carry):** Unaffected
- **V (Overflow):** Unaffected

---

## Examples

### Example 1: Read DMA buffer after transfer

```assembly
        % DMA transfer completed, read buffer bypassing cache
        W1 RDUS DMA_BUFFER
        W1 =: B.DATA
```

**Explanation:** After a DMA transfer, the cache may contain stale data. RDUS ensures we read the fresh data from main memory.

### Example 2: Read memory-mapped I/O register

```assembly
        % Read status register from I/O device
        W2 RDUS IO_STATUS_REG
        W2 AND 0x0001           % Check ready bit
        IF=GO NOT_READY
```

**Explanation:** Memory-mapped I/O registers must be read from the device, not from cache, to get current hardware status.

### Example 3: Read field in DMA'd record

```assembly
        % Read field from record after DMA transfer
        W3 RDUS R.STAT
        IF<GO ERROR_CONDITION
```

**Explanation:** This is the example from the reference manual. After DMA updates a record, RDUS ensures we read the current field value.

### Example 4: Read array element after DMA

```assembly
        % Read array element updated by DMA
        W1 := ARRAY_INDEX
        W2 RDUS DATA_ARRAY(W1)
        W2 =: B.RESULT
```

**Explanation:** When DMA updates array elements, use RDUS with indexed addressing to read the fresh values.

### Example 5: Halfword read from I/O port

```assembly
        % Read halfword from hardware register
        H4 RDUS HW_CONTROL_REG
        H4 OR 0x0010            % Set control bit
        H4 =: HW_CONTROL_REG
```

**Explanation:** Reading hardware registers requires bypassing cache to get real-time hardware state.

### Example 6: Byte read from shared memory

```assembly
        % Read byte flag from DMA-accessible memory
        BY1 RDUS SHARED_FLAG
        IF><GO FLAG_SET
        % Flag is clear
FLAG_SET:
        % Process flag value
```

**Explanation:** Shared memory flags updated by DMA or other processors must be read with RDUS to avoid cache staleness.

### Example 7: Read local variable after potential DMA

```assembly
        % Read local variable that may have been DMA'd
        W4 RDUS B.STATUS_WORD
        W4 TEST
        IF=GO ZERO_STATUS
        % Non-zero status processing
```

**Explanation:** If local stack variables can be DMA targets (rare but possible), RDUS ensures fresh data.

---

## Performance Notes

- **Execution:** 3-5 cycles (slower than normal load due to cache bypass)
  - Memory read: 3 cycles (minimum)
  - Cache refresh: +1-2 cycles
  - Page fault: +100+ cycles
- **Cache behavior:** Always reads from main memory, then updates cache
- **No cache:** If no cache is present, RDUS is equivalent to normal load (`:=`)

**Usage recommendations:**
- Use sparingly - only when cache consistency is critical
- Normal loads (`:=`) are faster and sufficient for most cases
- Use after DMA transfers or when reading memory-mapped I/O
- If segment is marked as shared in capability table, normal loads bypass cache anyway

**Comparison with similar instructions:**
- `RDUS` vs `WDUS`: RDUS reads, WDUS writes (both bypass cache)
- `RDUS` vs `:=`: RDUS bypasses cache, `:=` uses cache
- `RDUS` is slower but guarantees fresh data from main memory

---

## Reference Manual

**Section:** §16.25
**Title:** Load bypassing cache

---

## See Also

- [WDUS](wdus.md) - Write bypassing cache (store with cache bypass)
- [:=](assignto.md) - Normal load operation (uses cache)
- [RIOM](riom.md) - Read I/O memory
- [Cache System](../ND500_CACHE_SYSTEM.md) - ND-500 cache architecture
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md) - Trap handling
