# ND-500 MMU Usage Guide

## Overview

The ND-500 Memory Management Unit (MMU) provides three-level address translation with protection and paging support. This guide explains how to configure and use the MMU in the nd500x emulator.

## Table of Contents

1. [MMU Architecture Overview](#mmu-architecture-overview)
2. [Quick Start with Demo Configuration](#quick-start-with-demo-configuration)
3. [Manual MMU Configuration](#manual-mmu-configuration)
4. [MMU Debug Commands](#mmu-debug-commands)
5. [Address Translation Examples](#address-translation-examples)
6. [Common Use Cases](#common-use-cases)
7. [Troubleshooting](#troubleshooting)

---

## MMU Architecture Overview

### Three-Level Translation

Virtual Address → Capability → PST Entry → Physical Address

1. **Level 1: Virtual Address → Capability**
   - Virtual address bits [31:27] select segment (0-31)
   - PCB[domain].capabilities[segment] provides PSN and protection
   - Separate instruction and data spaces

2. **Level 2: Capability → PST Entry**
   - PSN (Physical Segment Number) indexes into PST
   - PST entry contains paging mode and physical frame number

3. **Level 3: PST Entry → Physical Address**
   - Three paging modes:
     - **PS_AZI (0)**: Direct mapping - no page tables
     - **PS_ASI (1)**: Single-level paging - one page table lookup
     - **PS_ADI (2)**: Two-level paging - two page table lookups

### Key Data Structures

- **PST (Physical Segment Table)**: 8192 entries, maps PSN → physical frames
- **PCB (Process Control Block)**: 256 domains × 32 segments, stores capabilities
- **Page Size**: 2048 bytes (2KB)
- **Virtual Address Format**: [Segment:5 | Page:16 | Offset:11]

### MMU Registers

- **PSTP**: Physical Segment Table Pointer (base address of PST)
- **DITBASE**: Domain Information Table Base
- **CAD**: Current Alternative Domain (0-255)
- **CED**: Current Executing Domain (0-255)
- **PS**: Process Segment pointer

---

## Quick Start with Demo Configuration

The easiest way to explore MMU functionality is using the built-in demo configuration:

```bash
./bin/nd500x
```

```
[00000000] mmusetup
Setting up demo MMU configuration...

=== PST Configuration ===
PST[100] = PS_AZI (Direct), PFN=0x1000 → 0x00800000
PST[101] = PS_ASI (Single-level), Page table at 0x01000000
PST[102] = PS_ADI (Two-level), L1 table at 0x01800000

=== PCB Configuration (Domain 0) ===
PCB[0].prog[0] = PSN 100, DIR=1 (direct mapped)
PCB[0].data[0] = PSN 100, writable
PCB[0].data[5] = PSN 101, read-only, user accessible
PCB[0].data[7] = PSN 102, writable, kernel only

=== MMU Registers ===
PSTP    = 0x00100000
DITBASE = 0x00200000
CAD     = 0 (Alternative Domain)
CED     = 0 (Executing Domain)
PS      = 0 (Process Segment)

Demo configuration complete!
```

### Exploring the Demo Configuration

```
[00000000] showmmu
=== MMU STATUS ===
Machine MMU flag: disabled
CPU MMU state:    disabled

MMU Registers:
  PSTP    = 0x00100000  (Physical Segment Table Pointer)
  DITBASE = 0x00200000  (Domain Information Table Base)
  CED     = 0x00000000  (Current Executing Domain)
  CAD     = 0x00000000  (Current Alternative Domain)
  PS      = 0x00000000  (Process Segment)

PST: 8192 entries max
PCB: 256 domains max
Page size: 2048 bytes

[00000000] showpst 100
=== PST Entry 100 ===
Index Mode:    0 (PS_AZI - Direct)
Physical PFN:  0x1000 (Physical address: 0x00800000)
Direct mapping: segment maps to physical frame 0x1000

[00000000] showpcb 0
=== PCB Domain 0 ===
Seg  Prog Cap  Data Cap
---  --------  --------
  0  0064      0064
  5  0000      C065
  7  0000      0066

[00000000] showpcb 0 5
=== PCB Domain 0 Segment 5 ===
Program Capability: 0x0000
  PSN:     0 (0x000)
  DIR bit: 0 (Not direct)

Data Capability:    0xC065
  PSN:     101 (0x065)
  WRP bit: 1 (Write-protected)
  PAC bit: 1 (User accessible)

[00000000] mmu on
MMU enabled

[00000000] phyladr 0x00000000
=== Virtual Address Translation ===
Virtual Address: 0x00000000
  Segment: 0 (0x00)
  Page:    0 (0x0000)
  Offset:  0 (0x000)

Access Type:
  Read access
  Data space

Current Domain:
  CAD (Alternative): 0
  CED (Executing):   0

Physical Address: 0x00800000
  PFN:    0x1000
  Offset: 0x000
```

---

## Manual MMU Configuration

To configure the MMU from scratch, you need to set up three things:

### 1. Initialize MMU Registers

Use the `set` command to configure MMU registers:

```
set PSTP 0x00100000      # PST base address (must be allocated in memory)
set DITBASE 0x00200000   # DIT base address (for domain system)
set CAD 0                # Set current alternative domain
set CED 0                # Set current executing domain
set PS 0                 # Set process segment
```

### 2. Configure PST Entries

PST entries cannot be set directly from the debugger. You need to either:

**Option A: Use mmusetup for testing**
```
mmusetup                 # Creates demo configuration
```

**Option B: Load a program that configures PST**

The operating system kernel typically sets up PST entries during boot. You would need code like:

```c
// Pseudocode - would be in kernel initialization
nd500_mmu_set_pst_entry(cpu, psn, mode, pfn);
```

**PST Entry Configuration Parameters:**

- **PSN** (0-8191): Physical Segment Number
- **mode**: Paging mode
  - `0` (PS_AZI): Direct mapping - PFN is physical frame
  - `1` (PS_ASI): Single-level paging - PFN points to page table
  - `2` (PS_ADI): Two-level paging - PFN points to L1 page table
- **PFN** (0-65535): Physical Frame Number

**Examples:**

```c
// Direct mapping: PSN 100 maps to physical frame 0x1000
nd500_mmu_set_pst_entry(cpu, 100, 0, 0x1000);
// Physical address = 0x1000 << 11 = 0x00800000

// Single-level paging: PSN 101 has page table at frame 0x2000
nd500_mmu_set_pst_entry(cpu, 101, 1, 0x2000);
// Page table at physical address = 0x2000 << 11 = 0x01000000

// Two-level paging: PSN 102 has L1 page table at frame 0x3000
nd500_mmu_set_pst_entry(cpu, 102, 2, 0x3000);
// L1 page table at physical address = 0x3000 << 11 = 0x01800000
```

### 3. Configure PCB Capabilities

PCB capabilities define which PST entries each segment in each domain can access.

**Capability Structure:**

- **Program Capability** (16 bits):
  - Bits [12:0]: PSN (Physical Segment Number)
  - Bit [13]: DIR (Direct mapping flag)
  - Bits [15:14]: Reserved

- **Data Capability** (16 bits):
  - Bits [12:0]: PSN (Physical Segment Number)
  - Bit [13]: Reserved
  - Bit [14]: WRP (Write Protect - 1=read-only, 0=writable)
  - Bit [15]: PAC (Public Access - 1=user accessible, 0=kernel only)

**Examples:**

```c
// Segment 0: Program code at PSN 100, direct mapped
nd500_mmu_set_program_capability(cpu, 0, 0, 100 | 0x2000);
// 100 (PSN) | 0x2000 (PC_DIR bit)

// Segment 0: Data at PSN 100, writable, kernel only
nd500_mmu_set_data_capability(cpu, 0, 0, 100);
// 100 (PSN), WRP=0 (writable), PAC=0 (kernel)

// Segment 5: Read-only data at PSN 101, user accessible
nd500_mmu_set_data_capability(cpu, 0, 5, 101 | 0x4000 | 0x8000);
// 101 (PSN) | 0x4000 (DC_WRP read-only) | 0x8000 (DC_PAC user access)

// Segment 7: Writable data at PSN 102, kernel only
nd500_mmu_set_data_capability(cpu, 0, 7, 102);
// 102 (PSN), WRP=0 (writable), PAC=0 (kernel)
```

### 4. Enable the MMU

Once PST and PCB are configured:

```
mmu on
```

---

## MMU Debug Commands

### mmu [on|off]

Enable or disable MMU address translation.

```
mmu              # Show current status
mmu on           # Enable MMU
mmu off          # Disable MMU
```

**Note**: MMU must be enabled for address translation to occur. When disabled, all addresses are physical.

### mmusetup

Create a demo MMU configuration for testing (recommended for exploration).

```
mmusetup
```

Creates:
- PST entries 100, 101, 102 with different paging modes
- PCB entries for domain 0, segments 0, 5, 7
- Initializes MMU registers

### showmmu

Display complete MMU status and configuration.

```
showmmu
```

Shows:
- MMU enabled/disabled status
- All MMU registers (PSTP, DITBASE, CED, CAD, PS)
- PST/PCB table sizes
- Page size

### showpst <psn>

Show Physical Segment Table entry details.

```
showpst 100      # Show PST entry 100
showpst 0x64     # Hex notation also works
```

Displays:
- Index mode (PS_AZI/PS_ASI/PS_ADI)
- Physical frame number (PFN)
- Calculated physical address
- Paging mode explanation

### showpcb <domain> [segment]

Show Process Control Block capabilities for a domain.

```
showpcb 0        # Show all segments in domain 0
showpcb 0 5      # Show only segment 5 in domain 0
```

Displays:
- Program capabilities (PSN, DIR flag)
- Data capabilities (PSN, WRP, PAC flags)
- Table of all non-zero capabilities (when segment not specified)

### phyladr <vaddr> [is_write] [is_instruction]

Translate virtual address to physical address.

```
phyladr 0x00000000           # Translate address (default: read, data)
phyladr 0x08000000 0 1       # Read from instruction space
phyladr 0xD0000000 1 0       # Write to data space
```

Parameters:
- **vaddr**: Virtual address (hex or decimal)
- **is_write**: 0=read (default), 1=write
- **is_instruction**: 0=data (default), 1=instruction

Displays:
- Address breakdown (segment, page, offset)
- Access type (read/write, instruction/data)
- Current domain (CAD, CED)
- Translation result (physical address or failure reason)

---

## Address Translation Examples

### Example 1: Direct Mapping (PS_AZI)

**Setup:**
```
mmusetup
showpst 100
```

**Output:**
```
Index Mode:    0 (PS_AZI - Direct)
Physical PFN:  0x1000 (Physical address: 0x00800000)
```

**Translation:**

Virtual address `0x00000000` (segment 0, page 0, offset 0):

1. Segment 0 → PCB[0].data[0] = PSN 100
2. PST[100] = mode 0 (direct), PFN 0x1000
3. Physical address = (0x1000 << 11) | 0 = `0x00800000`

### Example 2: Single-Level Paging (PS_ASI)

**Setup:**
```
mmusetup
showpst 101
```

**Output:**
```
Index Mode:    1 (PS_ASI - Single-level paging)
Physical PFN:  0x2000 (Physical address: 0x01000000)
Page table at: 0x01000000 (single-level)
```

**Translation:**

Virtual address `0x28000000` (segment 5, page 0, offset 0):

1. Segment 5 → PCB[0].data[5] = PSN 101
2. PST[101] = mode 1 (single-level), PFN 0x2000
3. Page table base = 0x2000 << 11 = 0x01000000
4. PTE address = 0x01000000 + (page × 4) = 0x01000000
5. Read PTE from physical memory at 0x01000000
6. PTE.PFN gives final physical frame
7. Physical address = (PTE.PFN << 11) | offset

**Note**: For this to work, you must have valid page table data in memory at 0x01000000.

### Example 3: Two-Level Paging (PS_ADI)

**Setup:**
```
mmusetup
showpst 102
```

**Output:**
```
Index Mode:    2 (PS_ADI - Two-level paging)
Physical PFN:  0x3000 (Physical address: 0x01800000)
L1 page table at: 0x01800000 (two-level)
```

**Translation:**

Virtual address `0x38000000` (segment 7, page 0, offset 0):

1. Segment 7 → PCB[0].data[7] = PSN 102
2. PST[102] = mode 2 (two-level), PFN 0x3000
3. L1 table base = 0x3000 << 11 = 0x01800000
4. L1 index = page >> 8, L2 index = page & 0xFF
5. L1 PTE address = 0x01800000 + (L1_index × 4)
6. Read L1 PTE, get L2 table PFN
7. L2 table base = L2_PFN << 11
8. L2 PTE address = L2_table_base + (L2_index × 4)
9. Read L2 PTE, get final PFN
10. Physical address = (final_PFN << 11) | offset

**Note**: For this to work, you must have valid L1 and L2 page tables in memory.

---

## Common Use Cases

### Case 1: Testing Direct-Mapped Kernel

```bash
# Setup demo config
mmusetup

# Enable MMU
mmu on

# Verify segment 0 is direct-mapped
showpcb 0 0
# Should show PSN 100, DIR=1

# Check PST entry
showpst 100
# Should show PS_AZI (direct), PFN 0x1000

# Translate kernel address
phyladr 0x00000000
# Should map to 0x00800000 (0x1000 << 11)
```

### Case 2: Testing User Data Access

```bash
# Setup demo config
mmusetup

# Check user-accessible segment
showpcb 0 5
# Should show PSN 101, WRP=1 (read-only), PAC=1 (user)

# Translate user data address (segment 5)
phyladr 0x28000000 0 0
# Read access should succeed (if page tables exist)

# Try write access (should fail - write protected)
phyladr 0x28000000 1 0
# Should fail with "Protection violation"
```

### Case 3: Inspecting Current Domain State

```bash
# Show complete MMU state
showmmu

# Check current domain capabilities
regs
# Look at CAD and CED registers

# Show capabilities for current domain
showpcb 0
```

---

## Troubleshooting

### Problem: "Translation FAILED (trap would occur)"

**Possible causes:**

1. **Invalid capability (null)**
   - PCB entry for segment is 0
   - Solution: Configure PCB with `nd500_mmu_set_data_capability()`

2. **Protection violation**
   - Write to read-only segment (DC_WRP=1)
   - User access to kernel segment (DC_PAC=0 in user mode)
   - Solution: Check capability flags with `showpcb`

3. **Page fault (PFN=0)**
   - Page table entry has PFN=0 (unmapped page)
   - Page tables don't exist in memory
   - Solution: Initialize page tables in memory or use direct mapping

### Problem: "PST entry shows all zeros"

PST entries are initialized to zero. You must either:

1. Use `mmusetup` to create demo configuration
2. Load a kernel that initializes PST during boot
3. Wait for full MMU domain system implementation (Phase 4)

### Problem: "MMU is disabled - addresses are already physical"

The MMU is disabled by default. Enable it with:

```
mmu on
```

### Problem: Page table lookups fail

For PS_ASI and PS_ADI modes to work, you need:

1. Valid page tables in physical memory at specified addresses
2. Page table entries with non-zero PFN values
3. Proper page table structure:
   - PS_ASI: Array of 32-bit PTEs at (PFN << 11)
   - PS_ADI: Two-level structure with L1 and L2 tables

**Note**: The current implementation (Phase 5) performs translation but doesn't automatically create page tables. For testing paging modes, you would need to manually populate memory with page table data or wait for OS integration.

---

## Memory Layout Requirements

### Typical MMU Memory Layout

```
Physical Memory Layout:
0x00000000 - 0x000FFFFF: Low memory (1MB)
0x00100000 - 0x001FFFFF: PST (Physical Segment Table)
                         8192 entries × 4 bytes = 32KB
0x00200000 - 0x002FFFFF: DIT (Domain Information Table)
                         256 domains × 16 bytes = 4KB
0x00300000 - 0x003FFFFF: PCB (Process Control Blocks)
                         256 domains × 128 bytes = 32KB
0x00400000 - ...        : Page tables (for PS_ASI/PS_ADI)
0x00800000 - ...        : Kernel code/data (example)
0x01000000 - ...        : User space (example)
```

### Page Table Structure

**Single-Level (PS_ASI):**
```
Page Table Entry (32 bits):
  Bits [31:11]: Physical Frame Number (PFN)
  Bit  [1]:     Protection (0=writable, 1=read-only)
  Bit  [0]:     Present (must be 1)
```

**Two-Level (PS_ADI):**
```
L1 Table Entry → L2 Table Entry → Physical Frame
Each entry has same format as single-level PTE
```

---

## Next Steps

1. **Basic Testing**: Use `mmusetup` to explore MMU commands
2. **Manual Configuration**: Try setting individual PST/PCB entries from code
3. **Address Translation**: Use `phyladr` to understand translation flow
4. **Protection Testing**: Try read-only segments and observe failures
5. **OS Integration**: Implement kernel that initializes MMU during boot

For OS-level MMU initialization, see `docs/MMU_DOMAIN_MIGRATE_PLAN.md` Phase 4 (Domain System).
