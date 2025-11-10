# RPHS - Read from Physical Segment

## Overview

**Mnemonic:** `rphs`
**Function:** Copy bytes from physical segment to domain
**Class:** SYSTEM
**Privilege:** supervisor
**Extension:** '87 architecture extension

**Format:** `RPHS <domain number>`

---

## Description

Copies a block of bytes from a logical address on a physical segment to a logical address in a specified domain. This privileged instruction enables inter-process communication by allowing data transfer from one process's physical segment to another process's domain address space.

**Operation:**
```
while I1 > 0 do:
    S([I4,I3]) → D(<domain number>.I2)
    I3 + 1 → I3
    I2 + 1 → I2
    I1 - 1 → I1
    if page_boundary_reached: break
endwhile

I1 = 0 → Z flag (if all bytes copied)
I1 > 0 → Z flag clear (if page boundary hit first)
```

**Register Usage:**
- **I1** - Number of bytes to move (decremented, 0 when complete)
- **I2** - Logical address in destination domain (incremented)
- **I3** - Source address on physical segment (incremented)
- **I4** - Physical segment number
- **Operand** - Destination domain number

**Key Characteristics:**
- Privileged instruction (supervisor mode only)
- '87 architecture extension (may not exist on earlier models)
- Stops at page boundary even if bytes remain
- Updates I1, I2, I3 automatically during copy
- Uses physical segment table pointer for address translation
- Enables safe inter-process data transfer

**Common Use Cases:**
- Inter-process communication (IPC)
- Message passing between processes
- Shared memory segment access
- Operating system data transfer across process boundaries
- Copying data from one domain to another
- Protected data exchange in multi-process systems

**Operands:** 1 (destination domain number)
**Variants:** 1 opcode

---

## Variants

Total variants: 1

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFFF5 | RPHS |

---

## Operands

### Operand 1 (Domain Number)

**Type:** Word
**Access:** Read

Specifies the destination domain number where bytes will be copied.

**Supported modes:**
- **LOCAL** - Domain number in local variable
- **RECORD** - Domain number in record field
- **CONSTANT** - Immediate domain number
- **REGISTER** - Domain number in register
- **PRE_INDEXED** - Indexed domain table
- **ABSOLUTE** - Absolute address containing domain number

---

## Register Setup

Before executing RPHS, registers must be initialized:

| Register | Purpose | Direction |
|----------|---------|-----------|
| **I1** | Byte count | Input/Output (decremented) |
| **I2** | Destination address (domain) | Input/Output (incremented) |
| **I3** | Source address (phys segment) | Input/Output (incremented) |
| **I4** | Physical segment number | Input |

**After execution:**
- I1 = remaining bytes (0 if all copied, >0 if page boundary hit)
- I2 = next destination address
- I3 = next source address
- I4 = unchanged

---

## Trap Conditions

- **Privilege violation:** If executed in user mode
- **Illegal instruction code (IIC):** Not supported on pre-'87 systems
- **Addressing traps:** Invalid domain number or physical segment
- **Page fault:** If destination domain page not present
- **Protection violation:** Domain access rights violation

---

## Data Status Bits

- **Z (Zero):** Set if I1 = 0 (all bytes copied), cleared if I1 > 0 (page boundary hit)
- **Other flags:** Unaffected

---

## Examples

### Example 1: Copy 100 bytes between domains

```assembly
        % Copy 100 bytes from phys seg 5 to domain 3
        W1 := 100                   % Byte count
        W2 := DEST_ADDR             % Destination in domain
        W3 := SRC_ADDR              % Source on physical segment
        W4 := 5                     % Physical segment number
        RPHS 3                      % Domain 3
        % Check Z flag to see if all copied
```

**Explanation:** Basic inter-domain copy operation. Z flag indicates if page boundary interrupted the copy.

### Example 2: Multi-page copy with loop

```assembly
        % Copy large block across page boundaries
        W1 := TOTAL_BYTES
        W2 := DEST_ADDR
        W3 := SRC_ADDR
        W4 := PHYS_SEG
COPY_LOOP:
        RPHS TARGET_DOMAIN
        IF=GO COPY_DONE             % Z set, all done
        % Page boundary hit, continue
        GO COPY_LOOP
COPY_DONE:
```

**Explanation:** Loop to handle copies larger than one physical page. Continues until Z flag set.

### Example 3: IPC message transfer

```assembly
        % Transfer message from sender's segment to receiver
        W1 := MSG_SIZE
        W2 := RCV_BUFFER
        W3 := MSG_OFFSET
        W4 := SENDER_PHYS_SEG
        RPHS RECEIVER_DOMAIN
```

**Explanation:** Message passing: copy from sender's physical segment to receiver's domain.

### Example 4: Check for partial copy

```assembly
        % Copy and verify completion
        W1 := BLOCK_SIZE
        W2 := DEST
        W3 := SRC
        W4 := PHYS_SEG_NUM
        RPHS DOMAIN_NUM
        W TEST I1                   % Check remaining bytes
        IF><GO INCOMPLETE           % Not all copied
        % Complete copy
INCOMPLETE:
        % Handle partial copy
```

**Explanation:** Explicitly check I1 to determine if page boundary interrupted copy.

### Example 5: Indexed domain table

```assembly
        % Copy to domain from table
        W1 := BYTES
        W2 := DEST_ADDR
        W3 := SRC_ADDR
        W4 := PHYS_SEG
        W5 := PROCESS_INDEX
        RPHS DOMAIN_TABLE(W5)
```

**Explanation:** Use indexed addressing to select destination domain from table.

### Example 6: Safe IPC with retry

```assembly
        % Robust inter-process copy
        W1 := DATA_SIZE
        W2 := IPC_DEST
        W3 := IPC_SRC
        W4 := SRC_PHYS_SEG
RETRY:
        RPHS DST_DOMAIN
        W TEST I1
        IF=GO SUCCESS               % All copied
        % Page boundary, update and retry
        GO RETRY
SUCCESS:
```

**Explanation:** Retry pattern for large transfers across multiple pages.

### Example 7: Byte-by-byte fallback check

```assembly
        % Initial fast copy
        W1 := COUNT
        W2 := DST
        W3 := SRC
        W4 := SEG
        RPHS DOMAIN
        % Check if interrupted
        W1 TEST
        IF=GO DONE
        % Remaining bytes (rare edge case)
        % Handle remaining with byte moves
DONE:
```

**Explanation:** Use RPHS for bulk, handle edge case if page boundary interrupts.

---

## Performance Notes

- **Execution:** Variable, depends on byte count
  - Per-byte transfer: ~2-3 cycles/byte
  - Page boundary check: +2 cycles
  - Typical 100-byte transfer: ~250-300 cycles
- **Implementation:** Hardware block move with MMU translation
- **Page boundary:** Automatically stops to prevent crossing page

**Usage recommendations:**
- Use for inter-process communication in multiprocessing OS
- Check Z flag after execution to detect page boundary interruption
- Loop if copying more than one page worth of data
- Pair with WPHS for bidirectional IPC
- Essential for protected message passing

**Comparison with similar instructions:**
- `RPHS` vs `WPHS`: RPHS reads from physical segment, WPHS writes
- `RPHS` vs `BMOVE`: RPHS crosses domain boundaries, BMOVE is single-domain
- `RPHS` is '87 extension, may not exist on all ND-500 systems

---

## Reference Manual

**Section:** §16.31
**Title:** RPHS - Read from physical segment ('87 extension)

---

## See Also

- [WPHS](wphs.md) - Write to physical segment (companion instruction)
- [BMOVE](bmove.md) - Block move within same domain
- [RPGU](rpgu.md) - Read Page Used table
- [ZPGU](zpgu.md) - Clear Page Used bit
- [MMU Documentation](../ND500_MMU.md) - Memory management unit
- [Virtual Memory](../ND500_VIRTUAL_MEMORY.md) - Physical segment addressing
