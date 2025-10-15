# ND-500 Register Reference Guide
**Complete Documentation of All CPU and MMU Registers**

**Date**: October 15, 2025
**Source Files**:
- `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h` - CPU structure and trap definitions
- `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.h` - MMU constants and structures
- `/home/ronny/repos/nd500x/src/frontend/nd500wasm/web/debugger.js` - Bit editor definitions

---

## Table of Contents

1. [Register Overview](#register-overview)
2. [Core Registers](#core-registers)
3. [Status and Trap Registers](#status-and-trap-registers)
4. [MMU Registers](#mmu-registers)
5. [Bit-Mapped Register Details](#bit-mapped-register-details)
6. [Usage Examples](#usage-examples)

---

## Register Overview

The ND-500 CPU has **33 main registers** organized into several groups:

### Register Categories

| Category | Registers | Purpose | Bit-Mapped? |
|----------|-----------|---------|-------------|
| **Program Counter** | PC | Instruction address | No |
| **Integer Registers** | I1-I4 | General-purpose integer arithmetic | No |
| **Float Accumulators** | A1-A4 | Floating-point accumulation | No |
| **Float Extensions** | E1-E4 | Extended precision floating-point | No |
| **Addressing** | L, B, R | Memory addressing (Link, Base, Record) | No |
| **Stack/Trap** | TOS, LL, HL, THA | Stack and trap management | No |
| **Status** | ST1, ST2 | Combined 64-bit status register | **YES** |
| **Trap Enable** | OTE1/2, CTE1/2, MTE1/2, TEMM1/2 | Trap enable masks (64-bit pairs) | **YES** |
| **Control** | FLAGS | Simplified CPU flags | **YES** (TBD) |
| **MMU** | PSTP, DITBASE, CED, CAD, PS | Memory management | No |

**Total**: 33 registers (10 bit-mapped, 23 numeric)

---

## Core Registers

### 1. PC - Program Counter (32-bit)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:8`

```c
uint32_t PC;  /* Program counter */
```

**Purpose**: Points to the next instruction to execute.

**Format**: 32-bit virtual address (5-bit segment + 27-bit offset)
- Bits 31-27: Segment number (0-31)
- Bits 26-0: Offset within segment (0-128MB)

**Example**:
```
PC = 0xD0001234
  Segment = 0x1A (26 in decimal)
  Offset  = 0x00001234
```

**Usage**: Automatically incremented during instruction fetch. Can be modified for jumps/calls.

---

### 2. I1-I4 - Integer Registers (32-bit each)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:10`

```c
uint32_t I[4];  /* Integer registers I1-I4 */
```

**Purpose**: General-purpose integer arithmetic and data manipulation.

**Format**: Signed or unsigned 32-bit integers.

**Usage**: Used for arithmetic operations (ADD, SUB, MUL, DIV), comparisons, and temporary storage.

**Example**:
```assembly
LOAD I1, #100      ; I1 = 100
ADD  I1, I2, I3    ; I1 = I2 + I3
```

---

### 3. A1-A4 - Float Accumulators (32-bit each)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:12`

```c
uint32_t A[4];  /* Float accumulators */
```

**Purpose**: Floating-point arithmetic accumulators.

**Format**: 32-bit floating-point (ND-500 format, similar to IEEE-754)

**Usage**: Main registers for floating-point operations (FADD, FSUB, FMUL, FDIV).

---

### 4. E1-E4 - Float Extensions (32-bit each)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:13`

```c
uint32_t E[4];  /* Float extensions for extended precision */
```

**Purpose**: Extended precision mantissa bits for double-precision floating-point.

**Format**: Extended mantissa bits paired with A registers.

**Usage**: Used with A registers for 64-bit double-precision operations.

---

### 5. L - Link Register (32-bit)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:15`

```c
uint32_t L;  /* Link register */
```

**Purpose**: Stores return addresses for subroutine calls.

**Format**: 32-bit virtual address.

**Usage**:
- Set by CALL instruction to point to instruction after call
- Used by RETURN to resume execution after subroutine

---

### 6. B - Base Register (32-bit)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:16`

```c
uint32_t B;  /* Base register */
```

**Purpose**: Base address for local variable access (stack frame base).

**Format**: 32-bit virtual address.

**Usage**: Referenced by "Local" addressing mode (@B+offset).

**Example**:
```assembly
LOAD I1, @B+8    ; Load from [B + 8] into I1 (local variable)
```

---

### 7. R - Record Register (32-bit)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:17`

```c
uint32_t R;  /* Record register */
```

**Purpose**: Base address for record/structure access.

**Format**: 32-bit virtual address.

**Usage**: Referenced by "Record" addressing mode (@R+offset).

---

### 8. TOS - Top of Stack (32-bit)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:19`

```c
uint32_t TOS;  /* Top of stack pointer */
```

**Purpose**: Points to the top of the current stack.

**Format**: 32-bit virtual address.

**Usage**: Automatically adjusted by PUSH/POP operations. Checked against LL/HL for stack overflow/underflow.

---

### 9. LL - Lower Limit (32-bit)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:19`

```c
uint32_t LL;  /* Stack lower limit */
```

**Purpose**: Lower bound for stack overflow detection.

**Format**: 32-bit virtual address.

**Usage**: If TOS < LL, raises Stack Underflow trap (TRAP_STU).

---

### 10. HL - Higher Limit (32-bit)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:19`

```c
uint32_t HL;  /* Stack higher limit */
```

**Purpose**: Upper bound for stack overflow detection.

**Format**: 32-bit virtual address.

**Usage**: If TOS > HL, raises Stack Overflow trap (TRAP_STO).

---

### 11. THA - Trap Handler Address (32-bit)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:19`

```c
uint32_t THA;  /* Trap handler address */
```

**Purpose**: Entry point for trap handler routine.

**Format**: 32-bit virtual address.

**Usage**: When a trap occurs, PC is set to THA to invoke the trap handler.

---

## Status and Trap Registers

### 12-13. ST1/ST2 - Status Registers (64-bit combined)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:23`

```c
uint32_t ST1, ST2;  /* Status register split into two 32-bit halves */
```

**Purpose**: Combined 64-bit status register containing trap condition flags.

**Format**: 64-bit bit field (ST1 holds bits 11-31, ST2 holds bits 0-10)

**⚠️ BIT-MAPPED REGISTER** - Use the bit editor in the web UI!

#### ST2 - Lower Status Bits (Bits 0-10)

| Bit | Name | Full Name | Description |
|-----|------|-----------|-------------|
| **0** | **XSE** | Index Scaling Error | Invalid index in scaled addressing mode |
| **1** | **IIC** | Illegal Instruction Code | Unrecognized opcode encountered |
| **2** | **IOS** | Illegal Operand Specifier | Invalid addressing mode or operand |
| **3** | **ISE** | Instruction Sequence Error | Invalid instruction sequence detected |
| **4** | **PV** | Protect Violation | Memory access violation (write to read-only, user access to kernel) |
| **5** | **THM** | Trap Handler Missing | No trap handler installed at THA |
| **6** | **PGF** | Page Fault | Page not present in memory (MMU) |
| **7** | **NXM** | Non-Existent Memory | Access to unmapped physical memory |
| **8** | **MXM** | Memory Expansion Missing | Access to non-existent memory expansion |
| **9** | **ILL** | Illegal Memory Access | General memory access error |
| **10** | *Reserved* | | |

**Trap Behavior**: Bits 0-9 are **non-ignorable** - they interrupt instruction execution and invoke the trap handler immediately via `longjmp`.

#### ST1 - Upper Status Bits (Bits 11-31)

| Bit | Name | Full Name | Description |
|-----|------|-----------|-------------|
| **11** | **PE** | Invalid Operation | Invalid floating-point or arithmetic operation |
| **12** | **PF** | Divide by Zero | Division by zero attempted |
| **13** | **PI** | Floating Underflow | Floating-point result too small to represent |
| **14** | **PD** | Floating Overflow | Floating-point result too large to represent |
| **15** | **PS** | BCD Overflow | BCD arithmetic overflow |
| **16** | **PO** | Illegal Operand Value | Operand value out of valid range |
| **17** | **PU** | Single Instruction Trap | Trap after each instruction (single-step debugging) |
| **18** | **PZ** | Branch Trap | Trap on branch instructions |
| **19** | **PM** | Call Trap | Trap on subroutine calls |
| **20** | **PK** | Breakpoint Trap | Software breakpoint hit |
| **21** | **PX** | Address Trap Fetch | Trap on instruction fetch from address |
| **22** | **PN** | Address Trap Read | Trap on memory read from address |
| **23** | **PC** | Address Trap Write | Trap on memory write to address |
| **24** | **PL** | Address Zero Access | Trap on access to address zero (NULL pointer) |
| **25** | **PW** | Descriptor Range | Descriptor out of valid range |
| **26** | **PG** | Illegal Index | Index register value out of bounds |
| **27** | **PV** | Stack Overflow | Stack grew beyond HL (higher limit) |
| **28** | **PT** | Stack Underflow | Stack shrunk below LL (lower limit) |
| **29** | **PR** | Programmed Trap | Explicit trap instruction executed |
| **30** | **PA** | *Reserved* | |
| **31** | **PY** | *Reserved* | |

**Trap Behavior**: Bits 11-29 are **ignorable** - they set the status bit but don't interrupt execution unless explicitly enabled in trap enable masks.

**Reference**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:36-78`

---

### 14-15. OTE1/OTE2 - Own Trap Enable (64-bit combined)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:21`

```c
uint32_t OTE1, OTE2;  /* Own Trap Enable mask */
```

**Purpose**: Enable/disable trap handling for the **current domain**.

**Format**: 64-bit bit mask (same bit positions as ST1/ST2)

**⚠️ BIT-MAPPED REGISTER** - Use the bit editor!

**Bit Mapping**: Identical to ST1/ST2 (see above table).

**Usage**:
- Bit set to **1**: Trap is **enabled** (will invoke handler if trap occurs)
- Bit set to **0**: Trap is **disabled** (trap is silently ignored)

**Example**:
```c
OTE1 = 0x00000800;  // Enable only bit 11 (PE - Invalid Operation)
OTE2 = 0x00000003;  // Enable bits 0-1 (XSE, IIC)
```

**When to Use**: Set bits for traps you want to catch in your trap handler.

---

### 16-17. CTE1/CTE2 - Child Trap Enable (64-bit combined)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:21`

```c
uint32_t CTE1, CTE2;  /* Child Trap Enable mask */
```

**Purpose**: Enable/disable trap handling for **child domains** (domains called from current domain).

**Format**: 64-bit bit mask (same bit positions as ST1/ST2)

**⚠️ BIT-MAPPED REGISTER** - Use the bit editor!

**Bit Mapping**: Identical to ST1/ST2 (see above table).

**Usage**: Controls which traps are enabled when the current domain calls into a child domain.

---

### 18-19. MTE1/MTE2 - Mother Trap Enable (64-bit combined)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:21`

```c
uint32_t MTE1, MTE2;  /* Mother Trap Enable mask */
```

**Purpose**: Enable/disable trap handling for the **parent domain** (domain that called current domain).

**Format**: 64-bit bit mask (same bit positions as ST1/ST2)

**⚠️ BIT-MAPPED REGISTER** - Use the bit editor!

**Bit Mapping**: Identical to ST1/ST2 (see above table).

**Usage**: Controls which traps propagate back to the parent domain when a child domain encounters a trap.

---

### 20-21. TEMM1/TEMM2 - Trap Enable Modification Mask (64-bit combined)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:21`

```c
uint32_t TEMM1, TEMM2;  /* Trap Enable Mod Mask */
```

**Purpose**: Controls which trap enable bits can be modified by the current domain.

**Format**: 64-bit bit mask (same bit positions as ST1/ST2)

**⚠️ BIT-MAPPED REGISTER** - Use the bit editor!

**Bit Mapping**: Identical to ST1/ST2 (see above table).

**Usage**:
- Bit set to **1**: Corresponding OTE/CTE/MTE bit **can be modified**
- Bit set to **0**: Corresponding OTE/CTE/MTE bit **cannot be modified** (protected)

**Purpose**: Security mechanism to prevent untrusted domains from disabling critical traps.

**Example**:
```c
TEMM1 = 0x00000000;  // No trap enable bits can be modified
TEMM2 = 0x00000001;  // Only bit 0 (XSE) enable can be modified
```

---

### 22. FLAGS - Simplified Status Flags (32-bit)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:25`

```c
uint32_t FLAGS;  /* Simplified CPU flags */
```

**Purpose**: Simplified status flags for condition codes (carry, zero, negative, overflow).

**Format**: 32-bit bit field

**⚠️ BIT-MAPPED REGISTER** - Bit definitions TODO (not yet documented in instructions.md)

**Note**: This is a simplified view separate from ST1/ST2. Exact bit layout needs further documentation.

---

## MMU Registers

### 23. PSTP - Physical Segment Table Pointer (32-bit)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:27`

```c
uint32_t PSTP;  /* Physical Segment Table Pointer */
```

**Purpose**: Points to the base address of the Physical Segment Table (PST) in physical memory.

**Format**: 32-bit physical address (must be aligned to 64KB boundary)

**Usage**: MMU uses this to locate the PST during address translation.

**PST Structure**:
- 8192 entries maximum (13-bit index)
- Each entry: 8 bytes (index_mode + physical_pfn)
- Total size: 64 KB

**Example**:
```c
PSTP = 0x00800000;  // PST starts at physical address 8 MB
```

**Reference**: `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.h:36`

---

### 24. DITBASE - Domain Information Table Base (32-bit)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:28`

```c
uint32_t DITBASE;  /* Domain Information Table Base */
```

**Purpose**: Points to the base address of the Domain Information Table (DIT) in physical memory.

**Format**: 32-bit physical address

**Usage**: Used by the domain system to store/restore per-domain state (TOS, LL, HL, THA).

**DIT Structure**:
- 256 domain entries (8-bit domain ID)
- Each entry: 16 bytes (4 words: TOS, LL, HL, THA)
- Total size: 4 KB

**Example**:
```c
DITBASE = 0x00200000;  // DIT starts at physical address 2 MB
```

---

### 25. CED - Current Executing Domain (8-bit in 32-bit register)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:29`

```c
uint32_t CED;  /* Current Executing Domain (0-255) */
```

**Purpose**: Identifies the currently executing domain (user/kernel separation).

**Format**: 8-bit domain ID (0-255) stored in 32-bit register

**Domain Convention**:
- Domain 0: Kernel (privileged)
- Domains 1-255: User processes

**Usage**: MMU uses CED to index into PCB table to get current domain's memory capabilities.

**Example**:
```c
CED = 0;   // Running in kernel domain
CED = 1;   // Running in user domain 1
```

---

### 26. CAD - Current Alternative Domain (8-bit in 32-bit register)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:30`

```c
uint32_t CAD;  /* Current Alternative Domain (0-255) */
```

**Purpose**: Specifies an alternative domain for memory access (used with `@` prefix).

**Format**: 8-bit domain ID (0-255) stored in 32-bit register

**Usage**: Allows current domain to access memory from another domain's address space using alternative addressing mode.

**Example**:
```assembly
; Assuming CAD = 5
LOAD I1, @@100    ; Load from address 100 in domain 5's address space
```

---

### 27. PS - Process Segment (32-bit)
**Location**: `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h:31`

```c
uint32_t PS;  /* Process Segment (paging mode) */
```

**Purpose**: Controls MMU paging mode and process state.

**Format**: 32-bit value with mode bits

**Paging Modes** (from `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.h:38-41`):
- **PS_AZI (0)**: Direct addressed page (no indirection) - fastest
- **PS_ASI (1)**: Single index page (one-level page table)
- **PS_ADI (2)**: Double index page (two-level page table) - most flexible

**Usage**: Set by operating system to control memory translation granularity.

---

## Bit-Mapped Register Details

### How Bit-Mapped Registers Work

Bit-mapped registers have individual bits with specific meanings. Each bit acts as a flag or enable bit for a particular function.

#### ST1/ST2 Example - Status Register

The status register is 64 bits wide (ST1 + ST2) with each bit representing a specific trap condition:

```
ST2 (bits 0-10):
Bit  9  8  7  6  5  4  3  2  1  0
    ILL MXM NXM PGF THM PV ISE IOS IIC XSE

ST1 (bits 11-31):
Bit 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11
    PY PA PR PT PV PG PW PL PC PN PX PK PM PZ PU PO PS PD PI PF PE
```

**Reading Status**:
```c
if (ST2 & 0x00000001) {  // Check bit 0 (XSE)
    printf("Index Scaling Error occurred!\n");
}

if (ST1 & 0x00000800) {  // Check bit 11 (PE)
    printf("Invalid Operation occurred!\n");
}
```

**Setting Traps** (for trap handlers):
```c
// Enable divide-by-zero trap (bit 12 - PF)
OTE1 |= 0x00001000;  // Set bit 12

// Disable floating overflow trap (bit 14 - PD)
OTE1 &= ~0x00004000;  // Clear bit 14
```

#### Using the Web UI Bit Editor

1. Click on any bit-mapped register (ST1, ST2, OTE1, OTE2, CTE1, CTE2, MTE1, MTE2, TEMM1, TEMM2)
2. Bit editor modal opens showing all 32 bits
3. Click individual bits to toggle them on/off (1/0)
4. Real-time preview shows hex value
5. Click "Apply" to write new value to register

**Implementation**: `/home/ronny/repos/nd500x/src/frontend/nd500wasm/web/debugger.js:1661-1754`

---

## Usage Examples

### Example 1: Setting Up Trap Handler

```c
// Kernel initialization code
void setup_traps(void) {
    // Set trap handler address
    THA = 0x08001000;  // Trap handler at 8MB + 4KB

    // Enable critical traps in OTE (Own Trap Enable)
    OTE2 = 0x0000003F;  // Enable XSE, IIC, IOS, ISE, PV, THM (bits 0-5)
    OTE1 = 0x00001000;  // Enable divide-by-zero (bit 12 - PF)

    // Protect trap enable bits - only kernel can modify
    TEMM1 = 0x00000000;  // No modifications allowed
    TEMM2 = 0x00000000;
}
```

### Example 2: Checking for Divide-by-Zero

```c
// Before division operation
uint32_t divide(uint32_t a, uint32_t b) {
    // Clear divide-by-zero trap bit
    ST1 &= ~0x00001000;  // Clear bit 12 (PF)

    if (b == 0) {
        // Set divide-by-zero trap
        ST1 |= 0x00001000;  // Set bit 12 (PF)
        return 0;
    }

    return a / b;
}
```

### Example 3: Setting Up MMU for User Process

```c
// Create user domain with MMU protection
void setup_user_domain(uint8_t domain) {
    // Switch to user domain
    CED = domain;

    // Set up stack limits
    TOS = 0xF0100000;  // Top of user stack
    LL  = 0xF0000000;  // 1 MB stack (lower limit)
    HL  = 0xF0200000;  // 2 MB stack (upper limit)

    // Enable stack overflow/underflow traps
    OTE1 |= 0x18000000;  // Enable bits 27-28 (PV, PT)

    // Set page table pointer
    PSTP = 0x00800000;  // PST at 8 MB

    // Set paging mode
    PS = PS_ADI;  // Two-level page tables
}
```

### Example 4: Reading Trap Status in Trap Handler

```c
// Trap handler entry point (at THA)
void trap_handler(void) {
    // Check which trap occurred by examining ST1/ST2
    uint64_t status = ((uint64_t)ST1 << 32) | ST2;

    if (status & TRAP_IIC) {
        printf("Illegal instruction at PC=0x%08X\n", PC);
    }
    if (status & TRAP_PGF) {
        printf("Page fault - page not present\n");
    }
    if (status & TRAP_STO) {
        printf("Stack overflow - TOS exceeded HL\n");
    }

    // Clear trap bits
    ST1 = 0;
    ST2 = 0;

    // Return from trap (restore execution)
}
```

---

## Summary

### Quick Reference Table

| Register | Bits | Type | Web UI Editor | Usage |
|----------|------|------|---------------|-------|
| PC | 32 | Numeric | Prompt | Program counter |
| I1-I4 | 32 | Numeric | Prompt | Integer arithmetic |
| A1-A4 | 32 | Numeric | Prompt | Float accumulators |
| E1-E4 | 32 | Numeric | Prompt | Float extensions |
| L | 32 | Numeric | Prompt | Link register |
| B | 32 | Numeric | Prompt | Base register |
| R | 32 | Numeric | Prompt | Record register |
| TOS | 32 | Numeric | Prompt | Top of stack |
| LL | 32 | Numeric | Prompt | Stack lower limit |
| HL | 32 | Numeric | Prompt | Stack higher limit |
| THA | 32 | Numeric | Prompt | Trap handler address |
| **ST1** | **32** | **Bit-Mapped** | **Bit Editor** | **Status bits 11-31** |
| **ST2** | **32** | **Bit-Mapped** | **Bit Editor** | **Status bits 0-10** |
| **OTE1** | **32** | **Bit-Mapped** | **Bit Editor** | **Own trap enable 11-31** |
| **OTE2** | **32** | **Bit-Mapped** | **Bit Editor** | **Own trap enable 0-10** |
| **CTE1** | **32** | **Bit-Mapped** | **Bit Editor** | **Child trap enable 11-31** |
| **CTE2** | **32** | **Bit-Mapped** | **Bit Editor** | **Child trap enable 0-10** |
| **MTE1** | **32** | **Bit-Mapped** | **Bit Editor** | **Mother trap enable 11-31** |
| **MTE2** | **32** | **Bit-Mapped** | **Bit Editor** | **Mother trap enable 0-10** |
| **TEMM1** | **32** | **Bit-Mapped** | **Bit Editor** | **Trap mod mask 11-31** |
| **TEMM2** | **32** | **Bit-Mapped** | **Bit Editor** | **Trap mod mask 0-10** |
| **FLAGS** | **32** | **Bit-Mapped** | **Bit Editor** | **CPU condition flags (TBD)** |
| PSTP | 32 | Numeric | Prompt | Physical segment table ptr |
| DITBASE | 32 | Numeric | Prompt | Domain info table base |
| CED | 8 | Numeric | Prompt | Current executing domain |
| CAD | 8 | Numeric | Prompt | Current alternative domain |
| PS | 32 | Numeric | Prompt | Process segment / paging mode |

---

## For Beginners: Understanding Bit Operations

### What is a Bit?

A bit is the smallest unit of data - it can be either **0** (off/false) or **1** (on/true).

A 32-bit register contains 32 individual bits, numbered 0-31 (right to left):

```
Bit position:  31 30 29 28 ... 3  2  1  0
               [  ] [  ] [  ] [  ] ... [  ] [  ] [  ] [  ]
```

### Setting a Bit (Turn ON)

To set bit 5 to 1:
```c
register |= (1 << 5);  // Use bitwise OR with a mask
```

### Clearing a Bit (Turn OFF)

To clear bit 5 to 0:
```c
register &= ~(1 << 5);  // Use bitwise AND with inverted mask
```

### Testing a Bit (Check if ON)

To check if bit 5 is set:
```c
if (register & (1 << 5)) {
    printf("Bit 5 is ON\n");
}
```

### Hexadecimal Representation

Bit patterns are often shown in hexadecimal (base-16):
- Each hex digit represents 4 bits
- `0x00000001` = bit 0 is set
- `0x00000800` = bit 11 is set
- `0x0000003F` = bits 0-5 are set (binary: 00111111)

---

## Document Metadata

**Created**: October 15, 2025
**Status**: Complete
**Version**: 1.0
**Target Audience**: Developers, system programmers, emulator users

**Related Files**:
- `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h` - CPU structure and trap definitions
- `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.h` - MMU structures and constants
- `/home/ronny/repos/nd500x/src/frontend/nd500wasm/web/index.html` - Bit editor modal HTML
- `/home/ronny/repos/nd500x/src/frontend/nd500wasm/web/debugger.js` - Bit editor implementation
- `/home/ronny/repos/nd500x/src/frontend/nd500wasm/web/style.css` - Bit editor styling

**See Also**:
- `docs/MMU_COMPLETE_FINAL_SUMMARY.md` - MMU implementation summary
- `docs/PHASE_4_DOMAIN_SYSTEM_COMPLETE.md` - Domain system details
- `docs/reference/nd500/instructions.md` - ND-500 instruction set reference
