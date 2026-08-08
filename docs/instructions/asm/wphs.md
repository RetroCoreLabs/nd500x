# WPHS - Write to Physical Segment

## Overview

**Mnemonic:** `wphs`
**Function:** Copy bytes from domain to physical segment
**Class:** SYSTEM
**Privilege:** supervisor
**Extension:** '87 architecture extension

**Format:** `WPHS <domain number>`

---

## Description

Copies a block of bytes from a logical address in a specified domain to a logical address on a physical segment. This is the write counterpart to RPHS and enables inter-process communication by allowing data transfer from one process's domain to another process's physical segment.

**Operation:**
```
while I1 > 0 do:
    S(<domain number>.I2) → D([I4,I3])
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
- **I2** - Source logical address in domain (incremented)
- **I3** - Destination address on physical segment (incremented)
- **I4** - Physical segment number
- **Operand** - Source domain number

**Key Characteristics:**
- Privileged instruction (supervisor mode only)
- '87 architecture extension (may not exist on earlier models)
- Stops at page boundary even if bytes remain
- Updates I1, I2, I3 automatically during copy
- Companion write instruction to RPHS
- Enables safe inter-process data transfer

**Common Use Cases:**
- Inter-process communication (IPC) - send data
- Message passing to other processes
- Writing to shared memory segments
- Operating system data transfer across process boundaries
- Response data in client-server architectures
- Protected data exchange in multi-process systems

**Operands:** 1 (source domain number)
**Variants:** 1 opcode

---

## Variants

Total variants: 1

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFFF4 | WPHS |

---

## Operands

### Operand 1 (Domain Number)

**Type:** Word
**Access:** Read

Specifies the source domain number from which bytes will be copied.

**Supported modes:**
- **LOCAL** - Domain number in local variable
- **RECORD** - Domain number in record field
- **CONSTANT** - Immediate domain number
- **REGISTER** - Domain number in register
- **PRE_INDEXED** - Indexed domain table
- **ABSOLUTE** - Absolute address containing domain number

---

## Register Setup

Before executing WPHS, registers must be initialized:

| Register | Purpose | Direction |
|----------|---------|-----------|
| **I1** | Byte count | Input/Output (decremented) |
| **I2** | Source address (domain) | Input/Output (incremented) |
| **I3** | Destination address (phys seg) | Input/Output (incremented) |
| **I4** | Physical segment number | Input |

**After execution:**
- I1 = remaining bytes (0 if all copied, >0 if page boundary hit)
- I2 = next source address
- I3 = next destination address
- I4 = unchanged

---

## Trap Conditions

- **Privilege violation:** If executed in user mode
- **Illegal instruction code (IIC):** Not supported on pre-'87 systems
- **Addressing traps:** Invalid domain number or physical segment
- **Page fault:** If source domain page not present
- **Protection violation:** Domain access rights violation

---

## Data Status Bits

- **Z (Zero):** Set if I1 = 0 (all bytes copied), cleared if I1 > 0 (page boundary hit)
- **Other flags:** Unaffected

---

## Examples

### Example 1: Send 100 bytes to physical segment

```assembly
        % Send 100 bytes from domain 7 to physical segment
        W1 := 100                   % Byte count
        W2 := SRC_ADDR              % Source in domain
        W3 := DEST_ADDR             % Destination on physical segment
        W4 := 3                     % Physical segment number
        WPHS 7                      % From domain 7
        % Check Z flag to see if all copied
```

**Explanation:** Basic inter-domain write operation. Z flag indicates completion status.

### Example 2: IPC response - send data back

```assembly
        % Send response message to client's physical segment
        W1 := RESPONSE_SIZE
        W2 := RESPONSE_BUFFER
        W3 := CLIENT_PHYS_OFFSET
        W4 := CLIENT_PHYS_SEG
        WPHS CURRENT_DOMAIN
```

**Explanation:** Server sending response data to client's physical segment for IPC.

### Example 3: Multi-page copy with loop

```assembly
        % Copy large block, handling page boundaries
        W1 := TOTAL_BYTES
        W2 := SRC_ADDR
        W3 := DEST_ADDR
        W4 := TARGET_PHYS_SEG
COPY_LOOP:
        WPHS SOURCE_DOMAIN
        IF=GO COPY_DONE             % Z set, all done
        % Page boundary hit, continue
        GO COPY_LOOP
COPY_DONE:
```

**Explanation:** Loop to handle copies larger than one physical page.

### Example 4: Verify completion

```assembly
        % Copy and check if interrupted
        W1 := BLOCK_SIZE
        W2 := SRC
        W3 := DEST
        W4 := PHYS_SEG_NUM
        WPHS DOMAIN_NUM
        W TEST I1                   % Check remaining bytes
        IF><GO INCOMPLETE           % Not all copied
        % Complete copy
INCOMPLETE:
        % Handle partial copy
```

**Explanation:** Check I1 register to verify if page boundary interrupted the copy.

### Example 5: Message queue write

```assembly
        % Write message to shared queue
        W1 := MSG_LENGTH
        W2 := MSG_BUFFER
        W3 := QUEUE_TAIL_OFFSET
        W4 := SHARED_PHYS_SEG
        WPHS MSG_DOMAIN
        % Update queue tail pointer
```

**Explanation:** Writing message to shared message queue via physical segment.

### Example 6: Bidirectional IPC with RPHS/WPHS

```assembly
        % Client: Send request, receive response
        % Send request
        W1 := REQ_SIZE
        W2 := REQUEST_DATA
        W3 := SERVER_REQ_OFFSET
        W4 := SERVER_PHYS_SEG
        WPHS CLIENT_DOMAIN

        % Wait for server processing...

        % Receive response
        W1 := RESP_SIZE
        W2 := RESPONSE_BUFFER
        W3 := SERVER_RESP_OFFSET
        W4 := SERVER_PHYS_SEG
        RPHS CLIENT_DOMAIN
```

**Explanation:** Full bidirectional IPC: send request with WPHS, receive response with RPHS.

### Example 7: Indexed domain write

```assembly
        % Write to domain selected from table
        W1 := DATA_SIZE
        W2 := DATA_BUFFER
        W3 := PHYS_OFFSET
        W4 := PHYS_SEG_NUM
        W5 := CLIENT_INDEX
        WPHS DOMAIN_TABLE(W5)
```

**Explanation:** Use indexed addressing to select target domain from table.

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
- Pair with RPHS for bidirectional IPC
- Essential for protected message passing

**Comparison with similar instructions:**
- `WPHS` vs `RPHS`: WPHS writes to physical segment, RPHS reads
- `WPHS` vs `BMOVE`: WPHS crosses domain boundaries, BMOVE is single-domain
- `WPHS` is '87 extension, may not exist on all ND-500 systems

---

## Reference Manual

**Section:** §16.32
**Title:** WPHS - Write to physical segment ('87 extension)

---

## See Also

- [RPHS](rphs.md) - Read from physical segment (companion instruction)
- [BMOVE](bmove.md) - Block move within same domain
- [RPGU](rpgu.md) - Read Page Used table
- [ZPGU](zpgu.md) - Clear Page Used bit
- [SYSTEM instruction reference](../../instruction-reference/SYSTEM.md)
