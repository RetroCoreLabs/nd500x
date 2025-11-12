# RIOM - Read I/O Processor Memory

## Overview

**Mnemonic:** `riom`
**Function:** Read I/O processor memory to ND-500 memory
**Class:** IO
**Privilege:** supervisor
**Format:** `H RIOM <nd100-addr>, <buffer>, <count>`

---

## Description

Privileged instruction that copies data from the I/O processor (ND-100) memory to ND-500 memory through the ND-500 interface. This allows the ND-500 to access private ND-100 memory that is not directly addressable by the ND-500.

The ND-100 memory is accessed via DMA and does not interrupt ND-100 program execution, allowing efficient data transfer between the two processors.

**Operation:**
```
Copy <count> halfwords from ND-100 address to ND-500 buffer
ND-100 memory[nd100-addr..<nd100-addr+count>] → ND-500 memory[buffer..<buffer+count>]
```

**Key Characteristics:**
- Supervisor-only DMA transfer from I/O processor to ND-500 memory
- Accesses ND-100 private memory not directly addressable by ND-500
- Halfword (16-bit) transfers only
- IIC trap if not in supervisor mode
- IOV trap on invalid address or count
- DMA does not interrupt ND-100 execution
- ~(10 + 2×count) cycle execution time
- Essential for inter-processor communication
- Paired with WIOM for bidirectional data transfer

**Common Use Cases:**
- Accessing I/O processor private memory
- Inter-processor data transfer
- Reading I/O processor status/configuration
- Debugging I/O processor state
- Shared memory communication

**Operands:** 3 (source address, destination buffer, count)
**Variants:** 1 opcode (halfword only)

---

## Variants

| Variant | Opcode | Data Type | Assembly |
|---------|--------|-----------|----------|
| 1/1 | 0xFE76 | H | H RIOM |

**Note:** Only halfword (H) transfers are supported.

---

## Operands

**Operand 1** (ND-100 Address, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word (physical ND-100 address)
- **Role**: Source address in I/O processor memory
- **Note**: Usually private ND-100 memory, not directly ND-500 addressable

**Operand 2** (Buffer, Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Halfword
- **Role**: Destination buffer in ND-500 memory (logical address)

**Operand 3** (Count, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Halfword
- **Role**: Number of halfwords to transfer

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation
- **Illegal instruction code (IIC)**: Not in supervisor mode
- **Illegal operand value (IOV)**: Invalid count or address

---

## Data Status Bits

- **Z, S, C, O**: Unaffected

**Note:** RIOM does not modify any status flags.

---

## Examples

### Example 1: Copy one page from ND-100 memory
```assembly
        % Copy 1024 halfwords from ND-100 to array PG
        H RIOM 66000B:W, PG, 1024
```

### Example 2: Read I/O processor status
```assembly
        % Read IOP status into buffer
        H RIOM IOP_STATUS_ADDR:W, STATUS_BUFFER, 16
```

### Example 3: Variable-sized transfer
```assembly
        % Transfer with count in variable
        H RIOM SOURCE_ADDR, DEST_BUFFER, B.TRANSFER_SIZE
```

### Example 4: Read configuration data
```assembly
        % Read IOP configuration block
        H RIOM CONFIG_BASE:W, LOCAL_CONFIG, 256
```

### Example 5: Indexed transfer
```assembly
        % Read from indexed IOP location
        H RIOM IOP_BASE(W1), BUFFER, COUNT
```

### Example 6: Small transfer (single halfword)
```assembly
        % Read single halfword from IOP
        H RIOM IO_REGISTER:W, B.VALUE, 1
```

### Example 7: Read device registers
```assembly
        % Copy device register block
        H RIOM DEV_REGS:W, DEVICE_STATE, 32
```

### Example 8: Bulk data transfer
```assembly
        % Large block transfer
        H RIOM DMA_SOURCE:W, DATA_AREA, 4096
```

### Example 9: Read IOP variables
```assembly
        % Access IOP private variables
        W1 := IOP_VAR_BASE
        H RIOM I1, LOCAL_COPY, VAR_SIZE
```

### Example 10: Sequential reads
```assembly
        % Read multiple blocks
        W1 := IOP_ADDR
        W2 := 0
LOOP:
        H RIOM I1, BUFFER(I2), BLOCK_SIZE
        W1 ADD BLOCK_SIZE
        W2 ADD BLOCK_SIZE
        W2 COMP TOTAL_SIZE
        IF<GO LOOP
```

---

## Performance Notes

- **Execution**: Variable, depends on transfer size
  - Overhead: ~10 cycles
  - Transfer: ~2 cycles per halfword
  - Total: ~(10 + 2*count) cycles
- **DMA access**: Does not interrupt ND-100 execution
- **Memory access**: Direct hardware DMA transfer
- **Privileged**: Requires supervisor mode

**Transfer efficiency:**
```assembly
% Single halfword:
H RIOM ADDR, BUF, 1     % ~12 cycles overhead-dominated

% Large block:
H RIOM ADDR, BUF, 1000  % ~2010 cycles, efficient transfer
```

**Address space:**
- ND-100 addresses are physical addresses
- ND-500 buffer addresses are logical (virtual) addresses
- Hardware translates ND-500 addresses via MMU
- ND-100 addresses used directly by DMA controller

**Typical use pattern:**
```assembly
% Read IOP shared memory region:
H RIOM IOP_SHMEM_BASE:W, LOCAL_BUFFER, SHMEM_SIZE

% Process data in LOCAL_BUFFER

% Write back if needed (use WIOM instruction)
H WIOM LOCAL_BUFFER, IOP_SHMEM_BASE:W, SHMEM_SIZE
```

**Error handling:**
```assembly
% Check privilege level before attempting RIOM:
% If not supervisor, will trap with IIC

% Ensure addresses are valid:
% Invalid ND-100 address may cause IOV trap

% Verify buffer size:
% Buffer must have sufficient space for transfer
```

**Interaction with WIOM:**
```assembly
% Read-modify-write pattern:
H RIOM IOP_ADDR:W, BUFFER, SIZE    % Read from IOP
% Modify BUFFER contents
H WIOM BUFFER, IOP_ADDR:W, SIZE    % Write back to IOP
```

**DMA considerations:**
- Transfer occurs via hardware DMA
- ND-100 continues executing during transfer
- ND-500 blocks until transfer completes
- Efficient for bulk transfers
- Minimal impact on ND-100 performance

**Address calculation:**
```assembly
% ND-100 addresses often use octal constants:
H RIOM 77000B:W, BUFFER, 512    % Octal address

% Can compute addresses:
W1 := IOP_BASE
W1 ADD OFFSET
H RIOM I1, BUFFER, COUNT
```

---

## Reference Manual

**Section:** §16.23
**Title:** Read I/O processor memory

---

## See Also

- [WIOM](wiom.md) - Write I/O processor memory
- [RWIP](rwip.md) - Read/write I/O processor (if available)
- [ZWIP](zwip.md) - Zero I/O processor memory (if available)
