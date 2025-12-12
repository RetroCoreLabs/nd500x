# ND-500 CPU Trap Reference

> Complete trap reference derived from ND-05.009.4 EN ND-500 Reference Manual

## Overview

The ND-500 has 64 status bits in the status register (ST1/ST2), of which 40 are currently defined as trap conditions. Traps are divided into three categories based on severity and handling requirements.

## Status Register Layout

```
ST1 (bits 0-31)                    ST2 (bits 32-63)
+--------------------------------+ +--------------------------------+
| Reserved  | Data | Trace | Ref | | Non-Ign | Fatal | System | Rsv |
| 0-4       | 5-15 | 16-20 | 21+ | | 30-36   | 37-38 | 39-41  | 42+ |
+--------------------------------+ +--------------------------------+
```

## Trap Categories Summary

| Category | Bits | Characteristics | Handling |
|----------|------|-----------------|----------|
| **Status bits (S)** | 1-8 | Not trap conditions | Used for conditionals, flags |
| **Ignorable (I)** | 9-29 | Can be disabled via OTE | Handler invoked at end of instruction |
| **Non-ignorable (N)** | 30-36 | Cannot be disabled | Immediate handler invocation |
| **Fatal (F)** | 37-41 | System-critical | I/O processor notified, system may halt |

## Trap Timing

| Timing | Description | PC on Handler Entry |
|--------|-------------|---------------------|
| **Before (B)** | Trap before instruction executes | Points to faulting instruction (retry) |
| **During (D)** | Trap during instruction, partial results | Points to faulting instruction (retry/resume) |
| **After (A)** | Trap after instruction completes | Points to next instruction (continue) |

---

## Complete Trap Table

| Bit | Code | Name | Type | Mod | Time | Description |
|-----|------|------|------|-----|------|-------------|
| 0 | - | Reserved | - | - | - | Reserved for future use |
| 1 | PIA | Privileged Instruction Allowed | S | - | - | Set when privileged instructions allowed |
| 2 | PD | Part Done | S | - | - | Long instruction partially complete |
| 3 | IR | Instruction Reference | S | - | - | Page fault on instruction vs data |
| 4 | PSD | Process Switch Disabled | S | - | - | Set by SOLO, cleared by TUTTI |
| 5 | Z | Zero | S | M | - | Result was zero |
| 6 | C | Carry | S | M | - | Carry/borrow occurred |
| 7 | S | Sign | S | M | - | Result sign bit |
| 8 | K | Flag | S | M | - | General purpose flag |
| 9 | O | Overflow | I | M | A | Integer overflow |
| 10 | - | Reserved | - | - | - | Reserved |
| 11 | IVO | Invalid Operation | I | M | A | Invalid math operation (e.g., sqrt(-1)) |
| 12 | DZ | Divide by Zero | I | M | A | Division by zero |
| 13 | FU | Floating Underflow | I | M | A | Float result too small |
| 14 | FO | Floating Overflow | I | M | A | Float result too large |
| 15 | BO | BCD Overflow | I | M | A | BCD arithmetic overflow |
| 16 | IOV | Illegal Operand Value | I | M | A | Operand value out of range |
| 17 | SIT | Single Instruction Trap | I | M | A | After each instruction (debug) |
| 18 | BT | Branch Trap | I | M | A | Branch instruction taken |
| 19 | CT | Call Trap | I | M | A | Call instruction executed |
| 20 | BPT | Breakpoint Trap | I | M | B | Breakpoint instruction executed |
| 21 | ATF | Address Trap Fetch | I | M | A | Instruction fetch from watched address |
| 22 | ATR | Address Trap Read | I | M | A | Data read from watched address |
| 23 | ATW | Address Trap Write | I | M | A | Data write to watched address |
| 24 | AZ | Address Zero Access | I | M | A | Access to address 0x00000000 |
| 25 | DR | Descriptor Range | I | M | D | Descriptor index out of bounds |
| 26 | IX | Illegal Index | I | M | D | Invalid index value in LIND/CIND |
| 27 | STO | Stack Overflow | I | M | D | TOS exceeded HL (high limit) |
| 28 | STU | Stack Underflow | I | M | D | TOS below LL (low limit) |
| 29 | PRT | Programmed Trap | I | M | B | TRAP instruction executed |
| 30 | DT | Disable Process Switch Timeout | N | - | A | PSD set for >256 microcycles |
| 31 | DE | Disable Process Switch Error | N | - | A | Non-ignorable trap with PSD set |
| 32 | XSE | Index Scaling Error | N | - | D | Index exceeds 32 bits after scaling |
| 33 | IIC | Illegal Instruction Code | N | - | D | Unknown/reserved opcode |
| 34 | IOS | Illegal Operand Specifier | N | - | D | Invalid addressing mode |
| 35 | ISE | Instruction Sequence Error | N | - | D | Invalid instruction sequence |
| 36 | PV | Protect Violation | N | - | D | Segment permission violation |
| 37 | THM | Trap Handler Missing | F | - | B | No handler for occurred trap |
| 38 | PGF | Page Fault | F | - | D | Virtual page not in memory |
| 39 | PWF | Power Failure | F | - | A | Power supply voltage drop |
| 40 | PRF | Processor Fault | F | - | D | CPU internal error (not on ND-5000) |
| 41 | HWF | Hardware Fault | F | - | D | External hardware error (not on ND-5000) |
| 42-63 | - | Reserved | - | - | - | Reserved for future use |

**Legend:**
- Type: S=Status, I=Ignorable, N=Non-ignorable, F=Fatal
- Mod: M=Modifiable by software
- Time: B=Before, D=During, A=After instruction

---

## Addressing Traps

The term **"Addressing traps"** is used in instruction documentation as a collective name for all traps that may occur during operand fetching or instruction addressing. These include:

| Trap | Bit | When It Occurs |
|------|-----|----------------|
| ATF | 21 | Instruction fetch from address between LL and HL |
| ATR | 22 | Data read from address between LL and HL |
| ATW | 23 | Data write to address between LL and HL |
| AZ | 24 | Access to address 0x00000000 |
| DR | 25 | Descriptor index >= descriptor length |
| IX | 26 | Invalid index in LIND/CIND |
| XSE | 32 | Post-indexed address calculation overflow |
| IOS | 34 | Invalid addressing mode for instruction |
| PV | 36 | Segment permission violation |
| PGF | 38 | Virtual page not present |

**Note:** Not all addressing traps apply to every instruction. The specific traps depend on the addressing modes used.

---

## Detailed Trap Descriptions

### Data Status Bits (5-8) - Not Trap Conditions

#### Z - Zero (Bit 5)
Set if operand/result is exactly zero. Floating underflow sets Z in most cases.

#### C - Carry (Bit 6)
Set on carry out of or borrow into MSB during integer arithmetic. Used by ADDC, SUBC, INVC.

#### S - Sign (Bit 7)
Holds sign bit of last operand/result.

#### K - Flag (Bit 8)
General purpose flag. Used by:
- Descriptor addressing: set when accessing last element
- LIND/CIND: set on illegal index
- String instructions: termination indication

---

### Data Trap Conditions (9-15)

#### O - Overflow (Bit 9) - Ignorable
**Triggers:** Integer arithmetic result too large for destination.
- Addition: sign bits of addends equal, result sign different
- Multiplication: product exceeds destination size
- Result stored as truncated value

#### IVO - Invalid Operation (Bit 11) - Ignorable
**Triggers:** Mathematically undefined operations.
- SQRT of negative number
- Invalid floating-point operation
- Destination unchanged on trap

#### DZ - Divide by Zero (Bit 12) - Ignorable
**Triggers:** Division with zero divisor.
- Result: largest possible value with sign of dividend
- 0/0 = 0

#### FU - Floating Underflow (Bit 13) - Ignorable
**Triggers:** Exponent requires >9 bits (negative).
- Result: 0.0 with appropriate sign
- Z bit NOT set (exception: POLY, IXI)

#### FO - Floating Overflow (Bit 14) - Ignorable
**Triggers:** Exponent requires >9 bits (positive).
- Result: largest float with appropriate sign

#### BO - BCD Overflow (Bit 15) - Ignorable
**Triggers:** BCD result exceeds destination field width.
- BCD arithmetic is a hardware option

---

### Tracing Status Bits (17-20) - Ignorable

All tracing bits are cleared at start of next instruction regardless of enable state.

#### SIT - Single Instruction Trap (Bit 17)
**Triggers:** After each instruction completes.
- Used for single-stepping in debuggers

#### BT - Branch Trap (Bit 18)

**IMPORTANT:** BT is "Branch Trap", NOT "Buddy Trap". There is no "Buddy Trap" in the ND-500 - buddy heap exhaustion triggers STO (Stack Overflow).

**Triggers:** When next instruction is not immediately following current instruction (branch taken).

**Instructions that trigger BT:**
- GO - Unconditional jump
- IF <condition> GO - Conditional jump (only if branch taken)
- JUMPQ - Quick jump
- JMAP - Jump map (computed goto)
- LOOP - Loop instruction
- RET, RETK, IF K RET - Return instructions (also trigger BT)

**NOT triggered by:**
- CALL, CALLG, CALLS - Use CT (Call Trap) instead
- Conditional branches when condition is false (no jump made)

#### CT - Call Trap (Bit 19)
**Triggers:** Call instruction executed.
- CALL, CALLG, CALLS

#### BPT - Breakpoint Trap (Bit 20)
**Triggers:** BP (breakpoint) instruction executed.
- If BPT disabled, BP causes IIC trap instead

**Priority order (highest first):** BPT > CT > BT > SIT

---

### Instruction/Operand Reference Traps (16, 21-28)

#### IOV - Illegal Operand Value (Bit 16) - Ignorable
**Triggers:** Operand value exceeds legal range.
- Bit field instructions with invalid bit number
- CALL with invalid argument count
- Destination unchanged; instruction acts as NOOP

#### ATF - Address Trap Fetch (Bit 21) - Ignorable
**Triggers:** Program counter within LL < addr <= HL.
- Instruction fetch to watched address
- Takes precedence over ATR/ATW

#### ATR - Address Trap Read (Bit 22) - Ignorable
**Triggers:** Data read address within LL < addr <= HL.
- Checked only if ATF disabled

#### ATW - Address Trap Write (Bit 23) - Ignorable
**Triggers:** Data write address within LL < addr <= HL.
- Store completes before trap handler

#### AZ - Address Zero Access (Bit 24) - Ignorable
**Triggers:** Any access to address 0x00000000.
- Detects null pointer dereferences
- INIT sets B.PREVB to 0 (triggers AZ if dereferenced)

#### DR - Descriptor Range (Bit 25) - Ignorable
**Triggers:** Descriptor addressing index out of bounds.
- Index < 0 or index >= descriptor length
- Empty string (length 0) in string/BCD instruction
- Index register still incremented

#### IX - Illegal Index (Bit 26) - Ignorable
**Triggers:** LIND/CIND bounds check failure.
- Index exceeds array dimensions

#### STO - Stack Overflow (Bit 27) - Ignorable

**IMPORTANT:** This is NOT "Buddy Trap" - there is no such trap in the ND-500. STO is used for BOTH stack overflow AND buddy heap exhaustion.

**Triggers:** When new stack pointer (B.SP) >= TOS register, OR when buddy heap cannot satisfy allocation.

**Instructions that set/reset STO:**
- ENTS, ENTSN - Stack subroutine entry
- ENTB - Buddy allocation subroutine entry
- INIT - Stack initialization
- ENTM - Module entry with new stack
- GETB - Get block from buddy heap

**Detailed trigger conditions:**

1. **Stack Subroutine Entry (ENTS/ENTSN):**
   - Condition: `B + <stack_demand> >= TOS`
   - The `<stack_demand>` includes 20 bytes for PREVB, RETA, SP, AUX, N

2. **Buddy Allocation (GETB/ENTB):**
   - All freelists for requested size and larger are empty
   - Requested `log_size` exceeds MAXL in heap variables
   - No blocks available to split down to requested size

3. **Stack Initialization (INIT/ENTM):**
   - `<stack_demand_of_main_program> >= <total_system_stack_demand>`

**Heap Administration:**
The STO trap handler is expected to handle heap exhaustion:
- STAH (start of heap) and ENDH (end of heap) in heap variables are available for trap handler
- These are NOT used by GETB/FREEB instructions themselves
- Trap handler can allocate more memory or coalesce freed blocks
- FREEB does NOT combine blocks - this is left to the STO trap handler

**Status bit behavior:** Set/reset for each ENTS, ENTSN, ENTB, INIT, ENTM, and GETB instruction.

#### STU - Stack Underflow (Bit 28) - Ignorable

**Triggers:** Return instruction with RETA=0 or PREVB=0 when no alternative domain exists.

**Instructions that set/reset STU:**
- RET - Return from subroutine
- RETK - Return with K flag set
- RETB - Return from buddy subroutine (releases heap block)
- RETBK - Return from buddy subroutine with K flag set
- IF K RET - Conditional return

**Detailed trigger conditions:**

1. **Domain Return Check:**
   - B.PREVB = 0 OR B.RETA = 0
   - AND CAD (Current Alternative Domain) = 0 OR CAD = CED (same domain)
   - Result: STU trap

2. **Domain Switch (no trap):**
   - B.PREVB = 0 OR B.RETA = 0
   - AND CAD != 0 AND CAD != CED
   - Result: Domain switch to CAD, registers loaded from DIT

**Common use cases:**
- Program termination: Main program returns with RETA=0, STU trap returns control to OS
- INIT sets B.PREVB and B.RETA to zero, creating deliberate stack bottom marker

**Status bit behavior:** Set/reset at each return from stack subroutine.

---

### Signaling/Synchronization Bits (29-31)

#### PRT - Programmed Trap (Bit 29) - Ignorable
**Triggers:** Software-initiated via TRAP instruction or monitor call.
- Process can trap itself by setting PRT bit
- Used for inter-process signaling

#### DT - Disable Process Switch Timeout (Bit 30) - Non-ignorable
**Triggers:** PSD (process switch disabled) set for >256 microcycles.
- Prevents monopolization of CPU

#### DE - Disable Process Switch Error (Bit 31) - Non-ignorable
**Triggers:** Non-ignorable trap (including PGF) with PSD set.
- Page fault during SOLO-TUTTI critical section

---

### Non-Ignorable Traps (32-36)

#### XSE - Index Scaling Error (Bit 32)
**Triggers:** Post-indexed address calculation produces >32-bit result.
- base + (scale * index) overflow

#### IIC - Illegal Instruction Code (Bit 33)
**Triggers:**
- Undefined opcode
- Privileged instruction with PIA=0
- BP instruction with BPT disabled

#### IOS - Illegal Operand Specifier (Bit 34)
**Triggers:**
- Constant operand as destination
- ALT prefix on routine argument
- Type conflict between instruction and operand
- Non-constant argument count in CALL/POLY
- Register/constant operand in TSET, RDUS

#### ISE - Instruction Sequence Error (Bit 35)
**Triggers:**
- Illegal subroutine entry point (CALL to non-ENTR)
- Illegal domain call nesting (double domain call)
- Entry point instruction without preceding CALL

#### PV - Protect Violation (Bit 36)
**Triggers:** Capability table access code violation.
- Write to read-only segment
- Execute from non-execute segment
- Privileged operation in user segment

---

### Fatal Traps (37-41)

#### THM - Trap Handler Missing (Bit 37)
**Triggers:** Required trap handler address is zero.
- Also if ENTT operands cause non-ignorable traps

#### PGF - Page Fault (Bit 38)
**Triggers:**
- PST entry is zero (no mapping)
- Index page entry is zero
- Page not present in memory

#### PWF - Power Failure (Bit 39)
**Triggers:** Power supply voltage drop detected.

#### PRF - Processor Fault (Bit 40)
**Triggers:** Microprogram sequencing failure.
- Not implemented on ND-5000

#### HWF - Hardware Fault (Bit 41)
**Triggers:**
- TSB parity error
- Cache data parity error
- Cache directory error
- Memory parity error
- Memory timeout error
- Not implemented on ND-5000

---

## Buddy System and Traps

### Overview

The ND-500 buddy system is a memory allocation mechanism that uses **power-of-2 sized blocks**. It is important to understand that:

1. **There is NO "Buddy Trap"** - The term does not exist in ND-500 documentation
2. **STO (Stack Overflow, Bit 27)** is used for buddy heap exhaustion
3. **BT (Bit 18) is "Branch Trap"**, not related to buddy allocation

### Heap Variables Structure

The TOS register points to heap administration variables:

| Offset | Name | Size | Description |
|--------|------|------|-------------|
| +0 | MAXL | 4 bytes | Maximum logarithmic block size |
| +4 | STAH | 4 bytes | Start of heap address (for trap handler) |
| +8 | ENDH | 4 bytes | End of heap address (for trap handler) |
| +12 | FLOG[0] | 4 bytes | Freelist head for 2^0 word blocks |
| +16 | FLOG[1] | 4 bytes | Freelist head for 2^1 word blocks |
| +20 | FLOG[2] | 4 bytes | Freelist head for 2^2 word blocks |
| ... | ... | ... | ... |
| +12+(k*4) | FLOG[k] | 4 bytes | Freelist head for 2^k word blocks |

**Note:** STAH and ENDH are NOT used by GETB/FREEB instructions - they are provided for the STO trap handler to use when expanding or reorganizing the heap.

### Buddy System Instructions and Their Traps

| Instruction | Description | Traps |
|-------------|-------------|-------|
| GETB | Allocate block from heap | Addressing traps, **STO** |
| FREEB | Free block to heap | Addressing traps only |
| ENTB | Enter subroutine with buddy allocation | Addressing traps, **STO**, ISE |
| RETB | Return from buddy subroutine | Addressing traps, **STU**, BT |
| RETBK | Return from buddy subroutine (set K) | Addressing traps, **STU**, BT |

### STO Trap Handler Responsibilities for Buddy System

When STO trap occurs during buddy allocation:

1. **Identify cause:** Check if stack overflow or heap exhaustion
2. **For heap exhaustion:**
   - Use STAH/ENDH to determine heap bounds
   - Allocate additional memory from system
   - OR coalesce freed blocks (FREEB does NOT coalesce)
   - Initialize new blocks in appropriate FLOG lists
3. **Return from trap:** RETT instruction, instruction will be retried

### Important Notes

- Changing TOS (via INIT or ENTM) changes which heap variables are used
- New heap variables may need initialization after stack change
- Block coalescing must be done by software (trap handler), not hardware

---

## Traps by Instruction Category

### GETB (Get Buddy Element from Heap)

**Documented Trap Conditions:**
1. **Addressing traps** - Any operand addressing issues
2. **STO (Stack Overflow, Bit 27)** - Primary trap for GETB

**STO occurs when:**
- No free blocks of requested size available
- No larger blocks available to split
- Requested log_size exceeds MAXL value in heap variables
- Heap not initialized (TOS = 0)

**Data status bits:** Unaffected

**GETB Trap Flow:**
```
GETB <log_size>
  |
  +---> Read log_size operand
  |       +---> [Addressing traps possible: ATR, AZ, PV, PGF]
  |
  +---> Validate log_size <= MAXL
  |       +---> If log_size > MAXL: STO trap
  |
  +---> Search FLOG[log_size]
  |       +---> [Memory reads: ATR, PV, PGF possible]
  |
  +---> If empty, search larger FLOGs
  |       +---> [Memory reads: ATR, PV, PGF possible]
  |
  +---> If no blocks found: STO trap
  |
  +---> Split larger blocks
  |       +---> [Memory writes: ATW, PV, PGF possible]
  |
  +---> Return block address
          +---> Success (no trap)
```

### FREEB (Free Buddy Element)

**Documented Trap Conditions:**
- Addressing traps

**FREEB does NOT trigger STO** - elements are not combined during free; combination may be done by STO trap handler.

---

## Complete Entry/Return Instruction Trap Reference

### Stack and Module Entry Instructions

| Instruction | Description | Trap Conditions | STO Condition |
|-------------|-------------|-----------------|---------------|
| **INIT** | Initialize stack | Addressing, STO | `stack_demand >= system_stack` |
| **ENTM** | Enter module (new stack) | Addressing, STO, ISE | `stack_demand >= system_stack` |
| **ENTS** | Enter stack subroutine | Addressing, STO, ISE | `B + stack_demand >= TOS` |
| **ENTSN** | Enter stack subroutine (max args) | Addressing, STO, ISE | `B + stack_demand >= TOS` |
| **ENTF** | Enter fixed data subroutine | Addressing, ISE | (none) |
| **ENTFN** | Enter fixed data subroutine (max args) | Addressing, ISE | (none) |
| **ENTB** | Enter buddy subroutine | Addressing, STO, ISE | No free blocks in heap |
| **ENTT** | Enter trap handler | Addressing, ISE | (none - OTE disabled) |
| **ENTD** | Enter direct subroutine | Addressing, ISE | (none) |

### Return Instructions

| Instruction | Description | Trap Conditions | STU Condition |
|-------------|-------------|-----------------|---------------|
| **RET** | Return from subroutine (clear K) | Addressing, STU, BT | RETA=0 or PREVB=0, no alt domain |
| **RETK** | Return from subroutine (set K) | Addressing, STU, BT | RETA=0 or PREVB=0, no alt domain |
| **IF K RET** | Conditional return | Addressing, STU, BT | RETA=0 or PREVB=0, no alt domain |
| **RETB** | Return from buddy subroutine | Addressing, STU, BT | RETA=0 or PREVB=0, no alt domain |
| **RETBK** | Return from buddy subroutine (set K) | Addressing, STU, BT | RETA=0 or PREVB=0, no alt domain |
| **RETD** | Return from direct subroutine | (none) | (none) |
| **RETT** | Return from trap handler | (none) | (none) |

### Entry/Return Pairing

| Entry | Must Return With | Notes |
|-------|------------------|-------|
| ENTS, ENTSN | RET, RETK, IF K RET | Stack subroutines |
| ENTF, ENTFN | RET, RETK, IF K RET | Fixed data area |
| ENTM | RET, RETK, IF K RET | Module entry |
| ENTB | RETB, RETBK | Buddy allocation - must free block |
| ENTD | RETD | Direct subroutine |
| ENTT | RETT | Trap handler |

**Warning:** Using wrong return instruction can corrupt stack or leak buddy blocks.

---

## Trap Handler Address Calculation

### The THA Register

The **THA (Trap Handler Address)** register points to the base of an array in memory containing the start addresses of trap handler routines.

### CRITICAL: Memory Space Distinction

**The ND-500 has separate program and data address spaces.** Understanding where each component resides is essential:

| Component | Memory Space | Description |
|-----------|--------------|-------------|
| **Trap vector table** | **DATA memory** | Array of 64 handler addresses pointed to by THA |
| **Local data field** | **DATA memory** | Follows vector table at THA + 256 |
| **Handler routines** | **PROGRAM memory** | Actual ENTT...RETT code |

From the ND-500 Reference Manual (Section 6.4):
> "The Trap Handler Address register (THA) points to the base of an array **in data memory**, containing the start addresses of the trap handler routines **in program memory**."

This distinction is important because:
1. The vector table must be readable/writable by the CPU to look up handler addresses
2. The handler code executes from program space (separate MMU translation)
3. The saved register block goes into the data-space local data field

### Memory Layout at THA (Data Memory)

```
DATA MEMORY                          PROGRAM MEMORY
============                         ==============

THA Register points here
          |
          v
+=========+==================+       +==================+
| Offset  | Contents         |       | Handler Code     |
+=========+==================+       +==================+
| +0      | addr trap 0      |------>| (trap 0 handler) |
| +4      | addr trap 1      |------>| (trap 1 handler) |
| +8      | addr trap 2      |       |                  |
| +12     | addr trap 3      |       |                  |
| ...     | ...              |       |                  |
| +44     | addr trap 11     |------>| IVO handler      |
| +48     | addr trap 12     |------>| DZ handler       |
| +52     | addr trap 13     |------>| FU handler       |
| ...     | ...              |       |                  |
| +108    | addr trap 27     |------>| STO handler      |
| +112    | addr trap 28     |------>| STU handler      |
| ...     | ...              |       |                  |
| +252    | addr trap 63     |       |                  |
+---------+------------------+       +------------------+
| +256    | Local data field |
| (400B)  | (B points here)  |
|         | on ENTT          |
+---------+------------------+
```

**Note:** 64 entries x 4 bytes = 256 bytes (0x100 or 400 octal)

### Handler Address Calculation Formula

When a trap occurs, the CPU calculates the handler address as follows:

```
handler_pointer = THA + (trap_number * 4)
handler_address = memory[handler_pointer]  ; Read 32-bit address from memory
```

**Example calculations:**

| Trap | Bit | Handler Pointer Formula | If THA = 0x1000 |
|------|-----|-------------------------|-----------------|
| IVO | 11 | THA + (11 * 4) = THA + 44 | 0x1000 + 0x2C = 0x102C |
| DZ | 12 | THA + (12 * 4) = THA + 48 | 0x1000 + 0x30 = 0x1030 |
| STO | 27 | THA + (27 * 4) = THA + 108 | 0x1000 + 0x6C = 0x106C |
| STU | 28 | THA + (28 * 4) = THA + 112 | 0x1000 + 0x70 = 0x1070 |
| PGF | 38 | THA + (38 * 4) = THA + 152 | 0x1000 + 0x98 = 0x1098 |
| THM | 37 | THA + (37 * 4) = THA + 148 | 0x1000 + 0x94 = 0x1094 |

### Trap Dispatch Process

```
1. Trap condition detected
   |
   v
2. Determine trap number (bit position in ST1/ST2)
   - Bits 0-31 are in ST1
   - Bits 32-63 are in ST2
   |
   v
3. Calculate handler pointer address
   handler_pointer = THA + (trap_number * 4)
   |
   v
4. Read handler address from memory
   handler_address = read_32bit(handler_pointer)
   |
   v
5. Check if handler exists
   if handler_address == 0:
       -> THM trap (Trap Handler Missing)
   |
   v
6. Verify ENTT instruction at handler_address
   - First 2 bytes must be 0x00BC (ENTT opcode)
   - If not ENTT: Invalid handler
   |
   v
7. Save state for RETT:
   - Save trapping PC (address of instruction that caused trap)
   - Save OTE1/OTE2 values
   - Save trap number
   |
   v
8. Clear OTE1/OTE2 (prevent recursive traps)
   |
   v
9. Set PC = handler_address
   |
   v
10. Execute ENTT instruction at handler start
    - B register = THA + 256 (local data field)
    - Initialize trap handler stack frame
    - Save register block to local data field
   |
   v
11. Handler executes...
   |
   v
12. RETT instruction returns:
    - Restore OTE1/OTE2
    - Clear trap status bit
    - Return to trapping PC or next instruction
```

---

## Register Saving During Trap Handling

This section documents exactly which registers are saved, where they are saved, and by what mechanism during trap handling. **No assumptions are made** - all information is derived directly from the ND-500 Reference Manual Chapter 2 (Table 5), Chapter 6, and the ENTT instruction description.

### Overview: Two Save Locations

When a trap occurs, registers are saved to **two distinct locations**:

| Location | Contents | Saved By | Purpose |
|----------|----------|----------|---------|
| **Domain Information Table (DIT)** | Status register, domain state | Hardware (automatic) | Preserves domain context for propagation |
| **Trap Handler Data Field** | Full register block (50 words) | ENTT instruction | Provides handler access to trapped state |

### Location 1: Domain Information Table (DIT)

The DIT is part of the Process Segment (PS) and contains per-domain state. Each domain has a 256-byte (400 octal) DIT entry.

**Registers saved to DIT during trap (automatic by hardware):**

| DIT Offset | Size | Register/Field | When Saved |
|------------|------|----------------|------------|
| 213B (139) | 1 | Trapped domain number | When trap propagates to mother |
| 214B (140) | 1 | Alternative of trapped domain | When trap propagates to mother |
| 216B (142) | 4 | Status register (ST1, ST2) | Always on trap |
| 273B (187) | 1 | Inside trap handler flag | Set to 1 on ENTT |

**Registers loaded from DIT on trap return (RETT):**

| DIT Offset | Size | Register | Description |
|------------|------|----------|-------------|
| 226B (150) | 4 | OTE (Own Trap Enable) | Restored on RETT |
| 256B (174) | 4 | TEMM (Trap Enable Mod Mask) | Loaded for handler |
| 266B (182) | 4 | THA (Trap Handler Address) | Used to find handler |
| 274B (188) | 4 | TOS (Top of Stack) | Domain's stack limit |
| 300B (192) | 4 | LL (Low Limit) | Address trap register |
| 304B (196) | 4 | HL (High Limit) | Address trap register |

From Reference Manual Section 6.4:
> "When a trap handler is invoked, the status register (ST) is saved in the domain information table of the domain where the trap occurred."

### Location 2: Trap Handler Data Field (ENTT saves here)

The ENTT instruction saves the complete register block to the trap handler local data field at THA + 256 bytes (THA + 400 octal).

From ENTT instruction description (Section 13.10):
> "The register block is stacked as shown in table 5 on page 12."

The trap handler data field uses the **standard local data area layout** (same as stack subroutines), with a 5-word header followed by the saved registers as "arguments":

```
Address         Field       Contents
-----------     -------     ------------------------------------------
THA + 256       B.PREVB     0 (no previous frame - trap handlers are
 = B + 0                    non-reentrant)

B + 4           B.RETA      0 (no return address - RETT handles return)

B + 8           B.SP        B + (trap handler main program stack demand)
                            Points to first free location for handler

B + 12          B.AUX       Protect violation information (if PV trap)

B + 16          B.N         50 (decimal) = 62 (octal)
                            Number of "arguments" (saved registers)

B + 20          B.arg1      Start of saved register block
   ...             ...      (see Table 5 below for register mapping)
B + 176         B.arg40     End of saved register block

B + 180         B.arg41     Start of program memory copy (10 words)
   ...             ...
B + 216         B.arg50     End of program memory copy

B + 220+        (free)      Local data area for handler use
```

**Figure source:** Reference Manual Figure 41 "Layout of data structure when entering ENTT"

### Table 5: Register Numbers (from Reference Manual Chapter 2)

Table 5 defines **which register is stored at which argument position**. ENTT uses this numbering when saving registers to B.arg1 through B.arg50.

| Arg | Register | Arg | Register | Arg | Register | Arg | Register |
|-----|----------|-----|----------|-----|----------|-----|----------|
| 1 | Trapping P | 11 | A2 | 21 | TOS | 31 | OTE1 |
| 2 | P | 12 | A3 | 22 | LL | 32 | OTE2 |
| 3 | L | 13 | A4 | 23 | HL | 33 | CTE1 |
| 4 | B | 14 | E1 | 24 | THA | 34 | CTE2 |
| 5 | R | 15 | E2 | 25 | CED | 35 | MTE1 |
| 6 | I1 | 16 | E3 | 26 | CAD | 36 | MTE2 |
| 7 | I2 | 17 | E4 | 27 | mic | 37 | TEMM1 |
| 8 | I3 | 18 | ST1 | 28 | mic | 38 | TEMM2 |
| 9 | I4 | 19 | ST2 | 29 | mic | 39 | mic |
| 10 | A1 | 20 | PS | 30 | mic | 40 | mic |
| | | | | | | 41-50 | Program memory copy |

**Note:** "mic" = microcode scratch registers (internal CPU state)

### Combined View: Memory Offset to Register Mapping

To access a saved register in the trap handler, use offset B + 16 + (arg# * 4):

| Offset | Arg # | Register | Description |
|--------|-------|----------|-------------|
| B+0 | - | PREVB | Header: Previous B (always 0) |
| B+4 | - | RETA | Header: Return address (always 0) |
| B+8 | - | SP | Header: Stack pointer |
| B+12 | - | AUX | Header: Protect violation info |
| B+16 | - | N | Header: Argument count (50) |
| B+20 | arg1 | Trapping P | PC of instruction that caused trap |
| B+24 | arg2 | P | Return PC (next instruction or same as arg1) |
| B+28 | arg3 | L | Link register |
| B+32 | arg4 | B | Base register |
| B+36 | arg5 | R | Record base register |
| B+40 | arg6 | I1 | Integer register 1 |
| B+44 | arg7 | I2 | Integer register 2 |
| B+48 | arg8 | I3 | Integer register 3 |
| B+52 | arg9 | I4 | Integer register 4 |
| B+56 | arg10 | A1 | Floating point accumulator 1 |
| B+60 | arg11 | A2 | Floating point accumulator 2 |
| B+64 | arg12 | A3 | Floating point accumulator 3 |
| B+68 | arg13 | A4 | Floating point accumulator 4 |
| B+72 | arg14 | E1 | Extension register 1 |
| B+76 | arg15 | E2 | Extension register 2 |
| B+80 | arg16 | E3 | Extension register 3 |
| B+84 | arg17 | E4 | Extension register 4 |
| B+88 | arg18 | ST1 | Status register (bits 0-31) |
| B+92 | arg19 | ST2 | Status register (bits 32-63) |
| B+96 | arg20 | PS | Process segment pointer |
| B+100 | arg21 | TOS | Top of stack |
| B+104 | arg22 | LL | Low limit register |
| B+108 | arg23 | HL | High limit register |
| B+112 | arg24 | THA | Trap handler address |
| B+116 | arg25 | CED | Current executing domain |
| B+120 | arg26 | CAD | Current alternative domain |
| B+124 | arg27 | mic | Microcode scratch |
| B+128 | arg28 | mic | Microcode scratch |
| B+132 | arg29 | mic | Microcode scratch |
| B+136 | arg30 | mic | Microcode scratch |
| B+140 | arg31 | OTE1 | Own trap enable (bits 0-31) |
| B+144 | arg32 | OTE2 | Own trap enable (bits 32-63) |
| B+148 | arg33 | CTE1 | Child trap enable (bits 0-31) |
| B+152 | arg34 | CTE2 | Child trap enable (bits 32-63) |
| B+156 | arg35 | MTE1 | Mother trap enable (bits 0-31) |
| B+160 | arg36 | MTE2 | Mother trap enable (bits 32-63) |
| B+164 | arg37 | TEMM1 | Trap enable mod mask (bits 0-31) |
| B+168 | arg38 | TEMM2 | Trap enable mod mask (bits 32-63) |
| B+172 | arg39 | mic | Microcode scratch |
| B+176 | arg40 | mic | Microcode scratch |
| B+180 | arg41-50 | - | Copy of 10 words of program memory |

### RETT: Loading Registers Back

From RETT instruction description (Section 13.11):
> "The register block is loaded from B.arg2..B.arg40."

**Note:** arg1 (Trapping P) is NOT restored - it was just informational. The P register is restored from arg2.

### Trapping P vs Saved P (arg1 vs arg2)

From Reference Manual Section 6.4:

> "The P register saved in B.ARG2 holds the address of the instruction to be executed when the trap condition has been taken care of. Trapping P and the saved P register will be equal if the trap is handled before the instruction is executed."

| Field | Offset | Value | Description |
|-------|--------|-------|-------------|
| arg1 (Trapping P) | B+20 | PC of faulting instruction | First byte of instruction that caused trap |
| arg2 (Saved P) | B+24 | Return PC | Next instruction (for "after" traps) or same as arg1 (for "before" traps) |

**Trap timing and PC values:**

| Trap Type | arg1 (Trapping P) | arg2 (Saved P) | On RETT |
|-----------|-------------------|----------------|---------|
| Before | Faulting instruction | Same as arg1 | Re-execute instruction |
| During | Faulting instruction | Same as arg1 | Re-execute instruction |
| After | Faulting instruction | Next instruction | Continue at next |

### Status Register Handling

The status register is saved in **two places** with different purposes:

| Location | Fields | Purpose |
|----------|--------|---------|
| DIT offset 216B | ST1, ST2 | Original status for merge on RETT |
| arg18, arg19 (B+88, B+92) | ST1, ST2 | Modifiable copy for handler |

From Reference Manual Section 6.4:
> "Modification of status bits is done by changing the status word in the saved register block. Upon trap handler return, this status word is 'merged' with the saved status word in the domain information table and loaded into the status register."

**Status merge rules on RETT:**

| Bit Type | From Register Block | From DIT | Result |
|----------|---------------------|----------|--------|
| Ignorable trap bits | Yes (modifiable) | Ignored | Handler's changes apply |
| Non-ignorable trap bits | No | Yes | Original value preserved |
| Fatal trap bits | No | Yes | Original value preserved |
| Data status bits (Z,C,S,K) | Yes | Ignored | Handler's changes apply |

### OTE Register Behavior

**On ENTT (entering trap handler):**
- OTE is **cleared to 0** by hardware
- This prevents recursive traps in the same domain
- Saved OTE is available in arg31, arg32 (B+140, B+144)

**On RETT (returning from trap handler):**
- OTE is **restored from DIT** (offset 226B), not from the saved register block
- The trap status bit that caused the trap is **cleared**

From Reference Manual Section 6.4:
> "The Own Trap Enable register (OTE) is therefore cleared, forcing propagation to the mother domain of any trap condition occurring during trap handler execution. The OTE register is reloaded from the domain information table on return from the trap handler."

### Domain Information Table Complete Layout

For reference, the complete DIT layout relevant to trap handling:

| Offset (Octal) | Offset (Dec) | Size | Field | Category |
|----------------|--------------|------|-------|----------|
| 200B | 128 | 1 | Calling domain | M |
| 201B | 129 | 1 | Alternative of calling domain | M |
| 203B | 131 | 4 | P of calling domain | M |
| 207B | 135 | 4 | B of calling domain | M |
| 213B | 139 | 1 | Trapped domain | T |
| 214B | 140 | 1 | Alternative of trapped domain | T |
| 216B | 142 | 4 | Status register save area | T |
| 226B | 150 | 4 | Own trap enable (OTE) | O/M |
| 236B | 158 | 4 | Child trap enable (CTE) | O |
| 246B | 166 | 4 | Mother trap enable (MTE) | O |
| 256B | 174 | 4 | Trap enable modification mask (TEMM) | O |
| 266B | 182 | 4 | Trap handler address (THA) | O/M |
| 272B | 186 | 1 | Mother domain number | O |
| 273B | 187 | 1 | Inside trap handler flag | T |
| 274B | 188 | 4 | Top of stack register (TOS) | O/M |
| 300B | 192 | 4 | Low limit register (LL) | O/M |
| 304B | 196 | 4 | High limit register (HL) | O/M |
| 310B | 200 | 1 | Domain status (PIA = bit 0) | O |

**Categories:**
- **O** = Owned by this domain (loaded when domain becomes active)
- **M** = Modified by domain calls/returns
- **T** = Trap handling fields

---

## ENTT Instruction - Enter Trap Handler

This section documents exactly what the ENTT instruction does, based on ND-500 Reference Manual Sections 6.4 and 13.10.

### ENTT Assembly Format

```
ENTT <trap handler main program stack demand>, <total trap handler stack demand>
```

| Field | Hex | Octal | Description |
|-------|-----|-------|-------------|
| Opcode | 0xBC | 274B | Enter trap handler |

### ENTT Operands

| Operand | Name | Description |
|---------|------|-------------|
| Operand 1 | Trap handler main program stack demand | Bytes needed for local variables in handler. Determines B.SP value. |
| Operand 2 | Total trap handler stack demand | Total bytes reserved for trap handler. Determines TOS value. |

### What Triggers ENTT Execution

ENTT is **NOT** called like a normal subroutine. The sequence is:

1. **Trap condition occurs** (e.g., divide by zero sets ST bit 12)
2. **Hardware checks OTE** - is trap enabled locally?
3. **Hardware looks up handler address** from THA vector: `handler_addr = memory[THA + (trap_number * 4)]`
4. **Hardware transfers control** to handler address
5. **ENTT instruction executes** at handler address (MUST be first instruction)

From Reference Manual Section 13.10:
> "Execution of an entry point instruction (except ENTT) not resulting from a subroutine call will cause an instruction sequence error trap condition. ENTT may only be executed as a result of a trap, and may not be used as an entry point by a CALL or CALLG."

### ENTT Step-by-Step Operation

When ENTT executes, the following operations occur **in this order**:

**Step 1: Set B Register to Local Data Field**
```
B := THA + 256 bytes (THA + 400 octal)
```
The B register now points to the trap handler's local data field, which immediately follows the 64-word (256-byte) trap vector table.

**Step 2: Initialize Stack Frame Header**
```
memory[B + 0]  := 0                      ; B.PREVB = 0 (no previous frame)
memory[B + 4]  := 0                      ; B.RETA = 0 (no return address)
memory[B + 8]  := B + operand1           ; B.SP = B + main program stack demand
memory[B + 12] := <protect violation info> ; B.AUX (if PV trap, else undefined)
memory[B + 16] := 50                     ; B.N = 50 (decimal) = 62 (octal)
```

**Step 3: Save Register Block to B.arg1 through B.arg40**

The complete CPU register block is saved starting at B + 20, using the Table 5 numbering:
```
memory[B + 20]  := Trapping_P   ; arg1 - PC of faulting instruction
memory[B + 24]  := P            ; arg2 - Return PC
memory[B + 28]  := L            ; arg3
memory[B + 32]  := B_old        ; arg4 - B value before ENTT
memory[B + 36]  := R            ; arg5
memory[B + 40]  := I1           ; arg6
memory[B + 44]  := I2           ; arg7
memory[B + 48]  := I3           ; arg8
memory[B + 52]  := I4           ; arg9
memory[B + 56]  := A1           ; arg10
memory[B + 60]  := A2           ; arg11
memory[B + 64]  := A3           ; arg12
memory[B + 68]  := A4           ; arg13
memory[B + 72]  := E1           ; arg14
memory[B + 76]  := E2           ; arg15
memory[B + 80]  := E3           ; arg16
memory[B + 84]  := E4           ; arg17
memory[B + 88]  := ST1          ; arg18 - Status register low
memory[B + 92]  := ST2          ; arg19 - Status register high
memory[B + 96]  := PS           ; arg20
memory[B + 100] := TOS_old      ; arg21 - TOS before ENTT
memory[B + 104] := LL           ; arg22
memory[B + 108] := HL           ; arg23
memory[B + 112] := THA          ; arg24
memory[B + 116] := CED          ; arg25
memory[B + 120] := CAD          ; arg26
memory[B + 124] := mic          ; arg27 - microcode scratch
memory[B + 128] := mic          ; arg28
memory[B + 132] := mic          ; arg29
memory[B + 136] := mic          ; arg30
memory[B + 140] := OTE1         ; arg31
memory[B + 144] := OTE2         ; arg32
memory[B + 148] := CTE1         ; arg33
memory[B + 152] := CTE2         ; arg34
memory[B + 156] := MTE1         ; arg35
memory[B + 160] := MTE2         ; arg36
memory[B + 164] := TEMM1        ; arg37
memory[B + 168] := TEMM2        ; arg38
memory[B + 172] := mic          ; arg39
memory[B + 176] := mic          ; arg40
```

**Step 4: Copy Program Memory (arg41-arg50)**
```
memory[B + 180] := program_memory[Trapping_P + 0]   ; arg41
memory[B + 184] := program_memory[Trapping_P + 4]   ; arg42
...
memory[B + 216] := program_memory[Trapping_P + 36]  ; arg50
```
10 words of program memory around the trapping instruction are copied for diagnostic purposes.

**Step 5: Set TOS Register**
```
TOS := B + operand2   ; B + total trap handler stack demand
```

**Step 6: Set L Register**
```
L := B.SP             ; L points to first free location (same as B.SP)
```

**Step 7: Clear OTE Register**
```
OTE := 0              ; Disable all local traps during handler execution
```
This prevents recursive traps in the same domain. Any trap during handler execution will propagate to the mother domain.

**Step 8: Set Inside Trap Handler Flag**
```
DIT[273B] := 1        ; Set "inside trap handler" flag in Domain Information Table
```

**Step 9: Clear Trap Status Bit**

The specific status bit that caused the trap is cleared before handler execution begins.

**Step 10: Continue Execution**

Execution continues at the instruction following ENTT (PC was already set to handler address by hardware).

### Memory Layout After ENTT

```
THA -->  +---------------------------+
         | Trap vector [0]           |  4 bytes - handler addr for trap 0
         | Trap vector [1]           |  4 bytes - handler addr for trap 1
         | ...                       |
         | Trap vector [63]          |  4 bytes - handler addr for trap 63
         +---------------------------+  THA + 256 (400B)
B -->    | PREVB = 0                 |  B + 0
         | RETA = 0                  |  B + 4
         | SP                        |  B + 8  (= B + operand1)
         | AUX                       |  B + 12
         | N = 50                    |  B + 16
         +---------------------------+  B + 20
         | arg1: Trapping P          |
         | arg2: P                   |
         | arg3: L                   |
         | ...                       |
         | arg40: mic                |  B + 176
         +---------------------------+  B + 180
         | arg41-50: program copy    |  40 bytes
         +---------------------------+  B + 220
B.SP --> | (free for handler use)   |
L -->    |                           |
         | ...                       |
         +---------------------------+
TOS -->  | (end of trap stack)      |  B + operand2
         +---------------------------+
```

### Why PREVB and RETA are Zero

From Reference Manual Section 6.4:
> "The trap handler data area is not re-entrant, due to the fixed location."

Setting PREVB = 0 and RETA = 0 means:
- If the handler tries to execute RET, it will cause STU (Stack Underflow) trap
- This forces proper return via RETT instruction
- Trap handlers cannot be recursive within the same domain

### ENTT vs Other Entry Instructions

| Instruction | B Register | PREVB | RETA | Stack Source |
|-------------|------------|-------|------|--------------|
| ENTS | B.SP (grows stack) | oldB | return addr | Existing stack |
| ENTF | Fixed address | oldB | return addr | None (fixed area) |
| ENTM | operand1 (new stack) | oldB | return addr | New stack |
| ENTB | Buddy block | oldB | return addr | Heap |
| **ENTT** | **THA + 256** | **0** | **0** | **Fixed trap area** |

ENTT is unique because:
- B is set to a fixed location (THA + 256), not derived from caller
- PREVB and RETA are always 0
- Full register block is saved (not just arguments)
- OTE is cleared
- Cannot be called via CALL/CALLG

---

## RETT Instruction - Return from Trap Handler

### RETT Assembly Format

```
RETT
```

| Field | Hex | Octal | Description |
|-------|-----|-------|-------------|
| Opcode | 0x83 | 203B | Return from trap handler |

RETT takes no operands.

### RETT Step-by-Step Operation

**Step 1: Load Register Block from B.arg2 through B.arg40**

From Reference Manual Section 13.11:
> "The register block is loaded from B.arg2..B.arg40."

**Note:** arg1 (Trapping P) is NOT loaded - it was informational only.

```
P    := memory[B + 24]   ; arg2 - determines where execution resumes
L    := memory[B + 28]   ; arg3
; B is loaded last (see step 5)
R    := memory[B + 36]   ; arg5
I1   := memory[B + 40]   ; arg6
I2   := memory[B + 44]   ; arg7
I3   := memory[B + 48]   ; arg8
I4   := memory[B + 52]   ; arg9
A1   := memory[B + 56]   ; arg10
A2   := memory[B + 60]   ; arg11
A3   := memory[B + 64]   ; arg12
A4   := memory[B + 68]   ; arg13
E1   := memory[B + 72]   ; arg14
E2   := memory[B + 76]   ; arg15
E3   := memory[B + 80]   ; arg16
E4   := memory[B + 84]   ; arg17
; ST1, ST2 handled specially (step 3)
PS   := memory[B + 96]   ; arg20
TOS  := memory[B + 100]  ; arg21
LL   := memory[B + 104]  ; arg22
HL   := memory[B + 108]  ; arg23
THA  := memory[B + 112]  ; arg24
CED  := memory[B + 116]  ; arg25
CAD  := memory[B + 120]  ; arg26
; mic registers (arg27-30, 39-40) - internal, restored automatically
; OTE, CTE, MTE, TEMM handled from DIT (step 2)
```

**Step 2: Load Control Registers from Domain Information Table**

From Reference Manual Section 13.11:
> "OTE, TEMM, CED and CAS are loaded from the domain information table."

```
OTE  := DIT[226B]        ; Restore trap enables from DIT
TEMM := DIT[256B]        ; Restore modification mask
; CED, CAS also from DIT
```

**Step 3: Merge Status Register**

From Reference Manual Section 13.11:
> "The status register is loaded partly from B.arg18..B.arg19 and partly from the domain information table."

The merge rules:
- Ignorable trap bits: from saved register block (handler can modify)
- Non-ignorable/Fatal bits: from DIT (handler cannot modify)
- Data status bits (Z, C, S, K, O): from saved register block

**Step 4: Clear Trap Status Bit**

The status bit that caused the original trap is cleared.

**Step 5: Restore B Register**

```
B := memory[B + 32]      ; arg4 - restore original B
```

**Step 6: Clear Inside Trap Handler Flag**

```
DIT[273B] := 0           ; Clear "inside trap handler" flag
```

**Step 7: Resume Execution**

Execution resumes at the address in P (loaded from arg2).

### Handler Can Modify Return Behavior

The trap handler can modify the saved register block before RETT:

| To Do This | Modify |
|------------|--------|
| Change return address | B.arg2 (P) |
| Return different value in I1 | B.arg6 (I1) |
| Set/clear flags | B.arg18, B.arg19 (ST1, ST2) |
| Skip faulting instruction | Set B.arg2 = B.arg1 + instruction_length |

### SREGBL/LREGBL Instructions (for reference)

The '87 extension instructions SREGBL/LREGBL use a mask to select which registers to save/load:

| Bit | Register | Bit | Register | Bit | Register | Bit | Register |
|-----|----------|-----|----------|-----|----------|-----|----------|
| 0 | P | 10 | A1 | 20 | STS | 30 | MIC |
| 1 | L | 11 | A2 | 21 | PS | 31 | OTE |
| 2 | B | 12 | A3 | 22 | TOS | 32 | CTE |
| 3 | R | 13 | A4 | 23 | LL | 33 | MTE |
| 4 | I1 | 14 | E1 | 24 | HL | 34 | TEMM |
| 5 | I2 | 15 | E2 | 25 | THA | 35 | (free) |
| 6 | I3 | 16 | E3 | 26 | CED | 36 | (free) |
| 7 | I4 | 17 | E4 | 27 | CAD | 37 | (free) |

**Note:** ENTT does NOT use this mask format - it saves all registers in the fixed Table 5 format.

### Assembly Example: Setting Up Trap Handlers

```asm
; =============================================================================
; TRAP HANDLER SETUP EXAMPLE
; =============================================================================
; NOTE: The trap vector table must be in DATA memory.
;       The trap handler code is in PROGRAM memory.
; =============================================================================

; -----------------------------------------------------------------------------
; DATA SEGMENT - Trap vector table and local data field
; -----------------------------------------------------------------------------
DATA SEGMENT

TRAP_VECTOR:
    ; 64 entries x 4 bytes = 256 bytes total
    W 0                     ; Trap 0 - reserved
    W 0                     ; Trap 1 - PIA (status bit, not trap)
    W 0                     ; Trap 2 - PD
    W 0                     ; Trap 3 - IR
    W 0                     ; Trap 4 - PSD
    W 0                     ; Trap 5-8 - Z, C, S, K (status bits)
    W 0
    W 0
    W 0
    W O_HANDLER             ; Trap 9 - Overflow
    W 0                     ; Trap 10 - reserved
    W IVO_HANDLER           ; Trap 11 - Invalid Operation
    W DZ_HANDLER            ; Trap 12 - Divide by Zero
    W FU_HANDLER            ; Trap 13 - Floating Underflow
    W FO_HANDLER            ; Trap 14 - Floating Overflow
    W 0                     ; Trap 15 - BCD Overflow
    W 0                     ; Trap 16 - IOV
    W SIT_HANDLER           ; Trap 17 - Single Instruction Trap
    W BT_HANDLER            ; Trap 18 - Branch Trap
    W CT_HANDLER            ; Trap 19 - Call Trap
    W BPT_HANDLER           ; Trap 20 - Breakpoint Trap
    W 0                     ; Trap 21-26 - ATF, ATR, ATW, AZ, DR, IX
    W 0
    W 0
    W 0
    W 0
    W 0
    W STO_HANDLER           ; Trap 27 - Stack Overflow / Buddy Heap
    W STU_HANDLER           ; Trap 28 - Stack Underflow
    W PRT_HANDLER           ; Trap 29 - Programmed Trap
    W 0                     ; Trap 30-36 - Non-ignorable
    W 0
    W 0
    W 0
    W 0
    W 0
    W 0
    W THM_HANDLER           ; Trap 37 - Trap Handler Missing (fatal)
    W PGF_HANDLER           ; Trap 38 - Page Fault (fatal)
    ; ... remaining entries to 63

; Local data field starts here at TRAP_VECTOR + 256
; ENTT will use this area for the handler's stack frame
TRAP_LOCAL_DATA:
    BLOCK 256               ; Reserve space for trap handler local data

DATA ENDS

; -----------------------------------------------------------------------------
; PROGRAM SEGMENT - Trap handler code
; -----------------------------------------------------------------------------
PROG SEGMENT

; Initialize trap system
INIT_TRAPS:
    ; Set THA to point to trap vector table (in data memory)
    THA:: TRAP_VECTOR

    ; Enable specific traps using SETE instruction
    SETE 9                  ; Enable Overflow trap
    SETE 12                 ; Enable Divide by Zero trap
    SETE 17                 ; Enable Single Instruction Trap
    SETE 27                 ; Enable Stack Overflow trap
    SETE 28                 ; Enable Stack Underflow trap

    RET

; -----------------------------------------------------------------------------
; DIVIDE BY ZERO HANDLER
; -----------------------------------------------------------------------------
DZ_HANDLER:
    ENTT 100, 200           ; stack_demand=100, max_args=200

    ; B now points to local data field (THA + 256)
    ; Access saved register block:
    W1 := B.20              ; arg1 = Trapping P (faulting instruction)
    W2 := B.24              ; arg2 = Saved P (return address)
    W3 := B.28              ; arg3 = Saved L
    W4 := B.32              ; arg4 = Saved B

    ; Access saved arithmetic registers
    ; arg6-9 are I1-I4, arg10-13 are A1-A4

    ; Log the error, substitute a result, etc.
    ; ...

    RETT                    ; Return from trap (continues at next instr)

; -----------------------------------------------------------------------------
; STACK OVERFLOW HANDLER (also handles buddy heap exhaustion)
; -----------------------------------------------------------------------------
STO_HANDLER:
    ENTT 100, 200

    ; Determine if this is stack overflow or buddy heap exhaustion
    ; by examining the trapping instruction

    W1 := B.20              ; Trapping P
    ; Read instruction at trapping P to determine cause

    ; For buddy heap exhaustion (GETB/ENTB):
    ;   - Use STAH/ENDH from heap variables to expand heap
    ;   - Or coalesce freed blocks

    ; For stack overflow (ENTS/INIT/ENTM):
    ;   - Allocate more stack space
    ;   - Or signal error to user

    RETT                    ; Retry the instruction

; -----------------------------------------------------------------------------
; STACK UNDERFLOW HANDLER
; -----------------------------------------------------------------------------
STU_HANDLER:
    ENTT 100, 200

    ; Stack underflow means program tried to return
    ; when RETA=0 or PREVB=0 with no alternative domain

    ; Typically indicates program termination
    ; Return control to operating system

    ; ...

    RETT

PROG ENDS
```

**Key Points:**
1. Trap vector table MUST be in DATA memory (not program memory)
2. Handler code is in PROGRAM memory (separate address space)
3. THA register points to the vector table base
4. ENTT sets B = THA + 256, pointing to the local data field
5. Register block is saved at B.20 through B.180+

### Important Notes

1. **Handler address of 0 = THM trap**: If the vector entry contains 0, the CPU raises THM (Trap Handler Missing, bit 37).

2. **ENTT required**: Every trap handler MUST start with an ENTT instruction. The CPU verifies this before jumping.

3. **OTE cleared on entry**: When entering a trap handler, OTE1/OTE2 are cleared to prevent recursive traps. They are restored by RETT.

4. **Non-reentrant**: The trap handler data field is fixed at THA+256, so handlers cannot be recursively invoked in the same domain.

5. **Domain-aware**: If no local handler (OTE bit clear), the trap may propagate to mother domain (if MTE bit set).

6. **Priority**: If multiple traps occur, the highest bit number is handled first.

---

## Trap Enable Registers

### OTE1/OTE2 - Own Trap Enable
Controls which ignorable traps invoke handler in current domain.

### MTE1/MTE2 - Mother Trap Enable
Read-only. Indicates mother domain has handler for trap.

### CTE1/CTE2 - Child Trap Enable
Indicates this domain handles child domain traps.

### TEMM1/TEMM2 - Trap Enable Modification Mask
Read-only. Controls which OTE bits can be modified.

---

## Trap Propagation and Domain Hierarchy

### Domain Tree Structure

The ND-500 organizes domains in a hierarchical tree structure:

```
                    I/O Processor (ND-100/ND-110)
                           |
                    +------+------+
                    |             |
              Upper Domain    Upper Domain
             (Operating System)
                    |
              +-----+-----+
              |           |
           Domain A    Domain B
              |
         +----+----+
         |         |
      Domain C  Domain D
       (User)    (User)
```

- Each domain has exactly one **mother domain** (closer to root)
- A domain can have multiple **child domains**
- The **I/O processor** is the ultimate "great grandmother" of all domains
- The **upper domain** (operating system) is the topmost CPU domain

### Trap Enable Register Relationships

| Register | Domain | Modifiable | Purpose |
|----------|--------|------------|---------|
| OTE | Current | Yes (via TEMM) | "I have a handler for this trap" |
| CTE | Current | No | "I handle this trap for my children" |
| MTE | Current | No (system-set) | "My mother/grandmother handles this trap" |
| TEMM | Current | No | "Which OTE bits I'm allowed to modify" |

**Key relationship:** A bit is set in a domain's MTE if ANY mother domain in the tree has the corresponding CTE bit set.

### Trap Handler Search Algorithm

When a trap condition occurs, the CPU searches for a handler using this algorithm:

```
TRAP_CONDITION_OCCURS:
    |
    v
[1] Is OTE bit set for this trap?
    |
    +-- YES --> Invoke LOCAL trap handler (current domain)
    |           - Read handler address from THA[trap_number]
    |           - Verify address points to ENTT instruction
    |           - If address = 0 or not ENTT --> THM trap (fatal)
    |           - Execute ENTT, clear OTE, handle trap
    |
    +-- NO --> [2] Is MTE bit set for this trap?
                   |
                   +-- YES --> [3] Is current domain the upper domain?
                   |               |
                   |               +-- YES --> Report to I/O processor
                   |               |
                   |               +-- NO --> PROPAGATE to mother domain
                   |                          - Save state in DIT
                   |                          - Switch to mother domain
                   |                          - Mother's CTE checked
                   |                          - Repeat from [1] in mother
                   |
                   +-- NO --> IGNORE trap condition
                              (trap bit remains set but no action)
```

### What Happens When Handler Address is Zero

**If `THA[trap_number] == 0`:**

The CPU raises **THM (Trap Handler Missing, trap 37)** - a FATAL trap.

From the Reference Manual (Section 6.5.3.3):
> **THM:** Trap Handler Missing. The location pointed to by the trap handler vector does not contain an ENTT instruction, or the ENTT operands contain values causing non-ignorable traps.

THM is triggered in these cases:
1. Handler address in vector is 0 (null pointer)
2. Handler address points to memory that doesn't contain ENTT instruction
3. ENTT operands would cause non-ignorable traps (e.g., invalid stack demand)

### THM Trap - The "Catch-All" Fatal Trap

**THM (bit 37) is NOT a catch-all handler** - it's a FATAL error indicating the system is misconfigured.

| Aspect | Description |
|--------|-------------|
| Type | Fatal (F) |
| Handler | None in CPU - reported to I/O processor |
| Recovery | I/O processor decides (typically process termination) |
| Cause | Missing or invalid trap handler |

**There is no "default trap handler" mechanism.** If a trap is enabled (OTE bit set) but the handler address is 0 or invalid, the system fails with THM.

### Propagation to Mother Domain

When a trap propagates to the mother domain:

1. **State saved in child's Domain Information Table (DIT):**
   - Trapped domain number (offset 213B)
   - Alternative of trapped domain (offset 214B)
   - Status register (offset 216B)

2. **Domain switch occurs:**
   - CED loaded with mother domain number
   - CAD loaded with trapped domain number (child can access child's data via ALT)
   - TOS, THA, LL, HL loaded from mother's DIT
   - OTE loaded from mother's DIT

3. **Mother's handler invoked:**
   - Mother's THA used to find handler
   - Mother's trap handler executes with access to child's state

### The I/O Processor - Ultimate Trap Handler

The I/O processor (ND-100/ND-110) is the final backstop:

| Condition | Action |
|-----------|--------|
| Fatal trap (THM, PGF, PWF, PRF, HWF) | Always reported to I/O processor |
| Non-ignorable trap not handled by any CPU domain | Reported to I/O processor |
| Trap in upper domain with no handler | Reported to I/O processor |

The I/O processor typically:
- Logs the error
- Terminates the offending process
- May trigger a system crash dump
- Notifies the operator

### Trap During Trap Handler Execution

**OTE is cleared on ENTT entry** to prevent recursive traps in the same domain.

If a trap occurs while executing a trap handler:
1. Local handling is impossible (OTE = 0)
2. Trap propagates to mother domain (if MTE bit set)
3. If mother is also in a trap handler, propagates to grandmother
4. Eventually reaches I/O processor if no handler found

From the Reference Manual (Section 6.4):
> "A mother domain which itself is inside a trap handler will not be entered to handle a trap for one of its child domains. A trap in that case not handled locally in the child domain will be propagated to its grandmother."

### Inside Trap Handler Flag

The Domain Information Table contains an "Inside trap handler flag" (offset 273B):

| Value | Meaning |
|-------|---------|
| 0 | Domain not currently handling a trap |
| 1 | Domain is inside a trap handler |

This flag is checked during trap propagation to skip domains that are already busy handling traps.

### Trap Categories and Propagation Behavior

| Category | Bits | Can Disable? | Propagates? | Ultimate Handler |
|----------|------|--------------|-------------|------------------|
| Ignorable (I) | 9-29 | Yes (OTE) | Yes (to mother via MTE) | Mother domain or ignored |
| Non-ignorable (N) | 30-36 | No | Yes (must be handled) | Mother domain or I/O processor |
| Fatal (F) | 37-41 | No | No | Always I/O processor |

### Example: Complete Trap Flow

```
User program in Domain D executes division by zero:

1. DZ trap condition detected (bit 12)

2. Check Domain D's OTE bit 12:
   - If SET: Call D's handler at D.THA[12]
   - If CLEAR: Check D's MTE bit 12

3. D's MTE bit 12 is SET (mother M has CTE[12] set):
   - Save D's state to D's DIT
   - Switch to mother domain M
   - CAD = D (so M can access D's data)

4. Check Domain M's CTE bit 12:
   - CTE[12] SET: M handles child traps for DZ
   - Call M's handler at M.THA[12]

5. M's handler executes:
   - Can access D's registers via saved register block
   - Can access D's data via ALT() prefix
   - Decides how to handle (log, fix, terminate D)

6. M's handler executes RETT:
   - Restores D's OTE from DIT
   - Returns control to D
   - D retries or continues depending on trap type
```

---

## Assembler Syntax and Instruction Reference

### Trap-Related Instructions

| Instruction | Octal | Hex | Description |
|-------------|-------|-----|-------------|
| ENTT | 274 | 0xBC | Enter trap handler |
| RETT | 203 | 0x83 | Return from trap handler |
| SETE | 176471 | 0xFD39 | Set bit in OTE register |
| CLTE | 176472 | 0xFD3A | Clear bit in OTE register |
| BP | 177003 | 0xFE03 | Breakpoint instruction |
| TRAP | - | - | Programmed trap (sets PRT bit) |

### Special Register Load/Store Instructions

| Instruction | Octal | Description |
|-------------|-------|-------------|
| TE1:: | 176673 | Load first trap enable register (OTE1) |
| TE2:: | 176674 | Load second trap enable register (OTE2) |
| TE1=: | 176705 | Store first trap enable register (OTE1) |
| TE2=: | 176706 | Store second trap enable register (OTE2) |
| THA:: | 176712 | Load trap handler address register |
| THA=: | 176713 | Store trap handler address register |
| ST1:: | 176671 | Load first status register |
| ST1=: | 176703 | Store first status register |

### SETE - Set Trap Enable Bit

**Format:** `SETE <bit_number>`

**Operation:** Sets the specified bit in the Own Trap Enable (OTE) register.

**Example:**
```asm
; Enable specific traps
SETE 9          ; Enable integer Overflow trap (bit 9)
SETE 12         ; Enable Divide by Zero trap (bit 12)
SETE 17         ; Enable Single Instruction Trap (bit 17)
SETE 27         ; Enable Stack Overflow trap (bit 27)
```

**Notes:**
- The bit number is compared with TEMM (Trap Enable Modification Mask)
- Attempting to modify a non-modifiable bit causes IOV (Illegal Operand Value) trap

### CLTE - Clear Trap Enable Bit

**Format:** `CLTE <bit_number>`

**Operation:** Clears the specified bit in the Own Trap Enable register.

**Example:**
```asm
; Disable specific traps
CLTE 17         ; Disable Single Instruction Trap
CLTE 18         ; Disable Branch Trap
```

### Loading/Storing Trap Enable Registers

**Load entire OTE register:**
```asm
OTE1:: W1       ; Load OTE1 from W1
OTE2:: W2       ; Load OTE2 from W2
```

**Store entire OTE register:**
```asm
W1 := OTE1=:    ; Store OTE1 to W1
W2 := OTE2=:    ; Store OTE2 to W2
```

### Stack Header Intrinsic Constants

The assembler provides predefined constants for accessing the stack entry header:

| Name | Value | Description |
|------|-------|-------------|
| PREVB | 0 | Saved B-register (previous stack frame) |
| RETA | 4 | Saved return address |
| SP | 8 | Stack pointer (first free location) |
| AUX | 12 | Auxiliary/system cell |
| NARG | 16 | Number of arguments supplied in call |

**Usage in assembly:**
```asm
; Access stack header fields
W1 := B.PREVB       ; Load previous B value
W2 := B.RETA        ; Load return address
W3 := B.NARG        ; Load argument count
```

### ENTT - Enter Trap Handler

**Format:** `ENTT <stack_demand>, <max_args>`

**Operation:**
1. B register set to THA + 256 (local data field)
2. Register block saved to local data field
3. OTE cleared (prevents recursive traps)
4. Stack frame initialized

**Example:**
```asm
DZ_HANDLER:
    ENTT 100, 200       ; Stack demand=100, max args=200

    ; Access saved registers
    W1 := B.20          ; arg1 = Trapping P
    W2 := B.24          ; arg2 = Saved P (return address)
    W3 := B.28          ; arg3 = Saved L

    ; Handle the trap...

    RETT                ; Return from trap
```

### RETT - Return from Trap Handler

**Format:** `RETT`

**Operation:**
1. Restores OTE from domain information table
2. Clears the trap status bit that caused the trap
3. Returns to saved PC (retry or continue based on trap type)

**Notes:**
- For "before" traps: instruction is re-executed
- For "after" traps: execution continues at next instruction
- Status bits in saved register block may be modified by handler

---

## Trap Handler Invocation

### Trap Handler Data Field Layout
Located at THA + 256 bytes (after 64-word vector):

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 4 | B.PREVB - Previous base |
| 4 | 4 | B.RETA - Return address |
| 8 | 4 | B.SP - Stack pointer |
| 12 | 4 | B.AUX - Auxiliary |
| 16 | 4 | B.N - Argument count (0 for traps) |
| 20 | 4 | Trapping PC |
| 24 | 156 | Register block (39 words) |
| 180+ | - | Local data area |

### Handler Entry (ENTT)
```
ENTT <stack_demand>, <max_args>
```
- Allocates stack space
- OTE cleared (prevents recursive traps in same domain)
- B register points to trap handler data field

### Handler Return (RETT)
- Restores OTE from saved value
- Clears trap status bit
- Returns to saved PC
- Non-ignorable: retry instruction
- Ignorable: continue to next instruction

---

## References

### Primary Documentation
- **ND-05.009.4 EN ND-500 Reference Manual**
  - Chapter 2: The Register Block (Table 5 - Register Numbers)
  - Chapter 6: The Trap System (Sections 6.1-6.5)
  - Section 13.10: Subroutine Entry Points (ENTT instruction)
  - Section 13.11: Subroutine Return (RETT instruction)
  - Sections 15.13-15.14: GETB/FREEB instructions
  - Sections 16.5-16.6: SETE/CLTE instructions

- **ND-60.113.02 EN Assembler Reference Manual**
  - Section 2.4.2: Intrinsic Constants (PREVB, RETA, SP, AUX, NARG)
  - Table 12: Special Instructions and Opcodes

### Source Code References
- `/home/ronny/repos/nd500x/src/cpu/instructions/BITFIELD/Getb.c`
- `/home/ronny/repos/nd500x/src/cpu/instructions/SYSTEM/Freeb.c`
- `/home/ronny/repos/nd500x/src/cpu/cpu.c` (trap handling implementation)
