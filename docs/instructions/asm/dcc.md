# DCC - Data Cache Clear

## Overview

**Mnemonic:** `dcc`
**Function:** Clear data cache
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `DCC`

---

## Description

Clears (invalidates) the data cache and writes back all dirty (modified) cache lines to main memory. This privileged instruction ensures cache coherence between the cache and main memory, critical for DMA operations, multiprocessor synchronization, and memory-mapped I/O access.

DCC performs two operations:
1. Writes all modified (dirty) data from cache back to main memory
2. Marks all cache entries as invalid

Following DCC execution, subsequent data accesses will reload from main memory, ensuring that externally modified data (via DMA, other processors, or I/O devices) is properly observed.

On systems without a data cache, DCC executes as a no-op, allowing code portability across different ND-500 configurations.

This instruction is essential before:
- DMA transfers that read main memory
- Sharing data with other processors
- Memory-mapped I/O device access
- Context switches requiring cache flush
- Debugging memory contents

**Operands:** None
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFF15 | DCC |

---

## Operands

None - operates on implicit data cache.

---

## Trap Conditions

None (when executed in supervisor mode)

---

## Data Status Bits

All status bits unaffected.

---

## Examples

### Example 1: Before DMA read operation

```assembly
% Prepare for DMA device to read buffer from memory
        DCC                     % Flush dirty data to memory
        % Configure DMA to read BUFFER
        CALL START_DMA_READ
        % DMA sees current buffer contents
```

### Example 2: After DMA write operation

```assembly
% After DMA writes to memory, invalidate cache
        CALL START_DMA_WRITE
        CALL WAIT_DMA_COMPLETE
        DCC                     % Invalidate cache
        % CPU reads will fetch DMA-written data
```

### Example 3: Multi-processor data sharing

```assembly
% Flush shared data before signaling other CPU
        W MOVE NEW_VALUE, SHARED_DATA
        DCC                     % Ensure write visible to others
        W MOVE 1, SIGNAL_FLAG   % Signal data ready
```

### Example 4: Memory-mapped I/O access

```assembly
% Ensure I/O register writes reach device
        W MOVE COMMAND, IO_REGISTER
        DCC                     % Flush to actual device
        % Device sees command immediately
```

### Example 5: Context switch cache flush

```assembly
% Save process context with cache flush
CONTEXT_SWITCH:
        DCC                     % Write back all dirty data
        CALL SAVE_REGISTERS
        CALL LOAD_NEW_CONTEXT
        RET
```

---

## Performance Notes

- **Write-Back**: All dirty cache lines written to memory
- **Invalidation**: All cache entries marked invalid
- **Performance Impact**: Subsequent accesses slower until cache reloads
- **Scope**: Affects entire data cache, not selective
- **DMA Critical**: Required for cache coherence with DMA
- **Typical Use**: DMA operations, multiprocessor sync, I/O access
- **Paired with DCTSB**: Often combined with translation buffer clear

---

## Reference Manual

**Section:** §16.10
**Title:** Data cache clear

---

## See Also

- [PCC](pcc.md) - Program cache clear
- [DCTSB](dctsb.md) - Data translation speedup buffer clear
- [PCTSB](pctsb.md) - Program translation speedup buffer clear
