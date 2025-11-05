# ND-500 Virtual Address Validity and Domain Configuration Analysis

**Analysis Date**: October 16, 2025  
**Scope**: How virtual address validity depends on domain configuration in the ND-500 MMU system

---

## Executive Summary

Virtual address validity in the ND-500 MMU system is **entirely dependent on domain configuration**. The Load Program window (and address translation generally) must be domain-aware:

1. **Each domain has independent capability tables** - Different segments are accessible to different domains
2. **Program and Data capabilities are separate** - Different permissions for code (PC) and data (DC)
3. **Segment number determines address range** - Segment 0-31 maps to virtual address bits [31:27]
4. **Current domain (CAD/CED) determines accessible segments** - Only segments with capabilities in the current domain are valid
5. **Load window should show domain-specific valid ranges** - Currently shows hardcoded ranges that don't account for selected domain

---

## 1. Domain Configuration and Capabilities

### 1.1 Domain System Architecture

**File**: `/home/ronny/repos/nd500x/src/cpu/nd500_domain.h`

```c
#define KERNEL_DOMAIN       0           /* Domain 0 is always kernel */
#define MAX_DOMAINS         256         /* Maximum domains per process */
#define MAXSEG              32          /* Segments per domain */
```

Each domain has:
- **Program Capabilities** (`program_capabilities[32]`) - For instruction fetches
- **Data Capabilities** (`data_capabilities[32]`) - For data access
- **Domain state** (TOS, LL, HL, THA)
- **Cross-domain call information** (PCB call state)

### 1.2 Capability Structure

**File**: `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.h`

```c
/* Capability Masks (Program Capability) */
#define PC_PSN          0x1FFF      /* Physical Segment Number (13 bits) */
#define PC_DIR          0x0000      /* Direct Segment */
#define PC_IND          0x8000      /* Indirect Segment */

/* Capability Masks (Data Capability) */
#define DC_WRP          0x8000      /* Write Permitted (0=writable, 1=read-only) */
#define DC_PAC          0x4000      /* Parameter Access (user mode) */
#define DC_PSN          0x1FFF      /* Physical Segment Number (13 bits) */
```

**Key observation**: Each capability has a **Physical Segment Number (PSN)**, which determines where in physical memory the segment maps.

### 1.3 Virtual Address Structure

The ND-500 uses a 32-bit virtual address:

```
Bit Layout: [Segment(5) | Page(16) | Offset(11)]
            [31:27]      [26:11]     [10:0]

NBSG = 0x8000000 (128MB per segment)
NBPG = 2048 bytes per page
```

**Segment-to-Address Mapping:**
- Segment 0: 0x00000000 - 0x07FFFFFF (128MB)
- Segment 1: 0x08000000 - 0x0FFFFFFF (128MB)
- ...
- Segment 31: 0xF8000000 - 0xFFFFFFFF (128MB)

---

## 2. How Virtual-to-Physical Address Translation Works

**File**: `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.c:83-240`

The translation is a 3-level process:

```
LEVEL 1: Virtual Address → Capability (via PCB)
  ├─ Extract segment from VA: (addr >> 27) & 0x1F
  ├─ Get current domain: CAD register
  ├─ Lookup: PCB[domain].program_capabilities[segment] or data_capabilities[segment]
  └─ If capability == 0: TRAP (no access)

LEVEL 2: Capability → PST Entry (via PSN)
  ├─ Extract PSN: capability & 0x1FFF
  ├─ Lookup PST[PSN]
  └─ If PSN invalid: TRAP (protection violation)

LEVEL 3: PST Entry → Physical Address (mode-dependent)
  ├─ Mode 0 (PS_AZI): Direct addressing
  │  └─ Physical PFN = PST[PSN].physical_pfn
  ├─ Mode 1 (PS_ASI): Single-level paging
  │  └─ Page table at PST[PSN].physical_pfn
  │     └─ Physical PFN = PTE[page]
  └─ Mode 2 (PS_ADI): Two-level paging
     └─ L1 table at PST[PSN].physical_pfn
        └─ L1 PTE → L2 table
           └─ L2 PTE → Physical PFN
```

---

## 3. Domain-Dependent Valid Address Ranges

### 3.1 Address Validity Depends on Domain

A virtual address is valid for a given domain **if and only if**:

1. The segment number [31:27] identifies a segment in range 0-31
2. The domain (CAD) has a **non-zero capability** for that segment
3. The capability points to a valid PST entry
4. For data access: write permission (DC_WRP) must be checked for writes
5. For instruction fetch: program capability must exist

### 3.2 Example Configuration

**Default mmusetup configuration** (from `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.c`):

```c
/* Domain 0 (Kernel) */
PCB[0].program_capabilities[1] = PSN (1) | PC_DIR   // Kernel code: segment 1
PCB[0].data_capabilities[0]    = PSN (2) | DC_WRP   // Kernel data: segment 0

/* Domain 1 (User) */
PCB[1].program_capabilities[26] = PSN (3) | PC_DIR  // User code: segment 26
PCB[1].data_capabilities[30]    = PSN (5) | DC_WRP  // User data: segment 30
```

**Valid Address Ranges by Domain:**

For **Domain 0 (Kernel)**:
- **Code (PC)**: Segment 1 = 0x08000000 - 0x0FFFFFFF ✓
- **Data (DC)**: Segment 0 = 0x00000000 - 0x07FFFFFF ✓
- All other segments = TRAP

For **Domain 1 (User)**:
- **Code (PC)**: Segment 26 = 0xD0000000 - 0xD7FFFFFF ✓
- **Data (DC)**: Segment 30 = 0xF0000000 - 0xF7FFFFFF ✓
- All other segments = TRAP

### 3.3 Key Insight: Address Ranges Are Domain-Specific

The Load window currently shows:

```javascript
// From debugger.js line 153-159
if (mode === 'kernel') {
    mmuStatusHint.textContent = 'Virtual addresses: Code 0x08000000-0x0FFFFFFF, Data 0x00000000-0x07FFFFFF';
} else {
    mmuStatusHint.textContent = 'Virtual addresses: Code 0xD0000000-0xD7FFFFFF, Data 0xF0000000-0xF7FFFFFF';
}
```

**This hardcodes address ranges** that assume a specific default configuration!

---

## 4. Current Load Program Window Implementation

**Files**:
- `/home/ronny/repos/nd500x/src/frontend/nd500wasm/web/debugger.js` (lines 93-294)
- `/home/ronny/repos/nd500x/src/frontend/nd500wasm/main.c` (lines 92-108)

### 4.1 Load Window Domain Selection

The Load window accepts a **domain parameter**:

```javascript
// debugger.js lines 256-281
const mode = modeSelect.value;
const domain = parseInt(domainSelect.value, 10);
const psegBase = (mode === 'kernel') ? (0x08000000 >>> 0) : (0xD0000000 >>> 0);
const dsegBase = (mode === 'kernel') ? (0x00000000 >>> 0) : (0xF0000000 >>> 0);

// Calls:
await this.loadSplitViaMemfs(pseg, psegBase, dseg, dsegBase, startPc.value, domain);
```

**Problem**: The `domain` parameter is selected but **NOT used for address validation**!

### 4.2 Backend Load Functions

**File**: `/home/ronny/repos/nd500x/src/machine/machine_loader.c:42-71`

```c
int nd500_load_file_to_memory(Nd500Machine* m, const char* path, uint32_t base_addr) {
    /* Check if MMU is enabled */
    int mmu_enabled = nd500_machine_mmu_is_enabled(m);

    for (;;) {
        /* Only check physical memory range if MMU is disabled */
        if (!mmu_enabled && (base_addr + offset + rd > m->memory_size)) {
            rc = -2; /* out of range */
            break;
        }

        /* MMU translation via nd500_bus_write8 */
        nd500_bus_write8(m, base_addr + offset + i, buffer[i]);
    }
}
```

**Key observation**: This function checks physical memory bounds but **assumes any virtual address is valid** when MMU is enabled!

### 4.3 Missing Validation

There is **no validation** that:
1. The selected domain has a capability for the address segment
2. The address's segment is valid for the selected domain
3. The user has write permission for data loads

---

## 5. Domain-Aware Address Validation Requirements

### 5.1 What Should Be Validated

Before loading at address `addr` in domain `domain`:

```c
int validate_load_address(Nd500Cpu* cpu, uint32_t addr, uint8_t domain, int is_instruction) {
    /* Extract segment from virtual address */
    int segment = (addr >> 27) & 0x1F;  /* Bits [31:27] */

    /* Get capability for this domain and segment */
    uint16_t capability;
    if (is_instruction) {
        capability = nd500_mmu_get_program_capability(cpu, domain, segment);
    } else {
        capability = nd500_mmu_get_data_capability(cpu, domain, segment);
    }

    /* Check if capability exists (non-zero) */
    if (capability == 0) {
        return 0;  /* Invalid - no capability for this segment */
    }

    /* For data writes, check write permission (DC_WRP) */
    if (!is_instruction) {
        if (capability & DC_WRP) {
            return 0;  /* Invalid - read-only segment */
        }
    }

    return 1;  /* Valid */
}
```

### 5.2 Address Range Calculation per Domain

To show valid address ranges in the Load window for a selected domain:

```c
void get_domain_address_ranges(Nd500Cpu* cpu, uint8_t domain,
                               uint32_t* code_start, uint32_t* code_end,
                               uint32_t* data_start, uint32_t* data_end) {
    /* Scan all 32 segments for this domain */
    *code_start = 0xFFFFFFFF;
    *code_end = 0;
    *data_start = 0xFFFFFFFF;
    *data_end = 0;

    for (int seg = 0; seg < 32; seg++) {
        uint16_t prog_cap = nd500_mmu_get_program_capability(cpu, domain, seg);
        uint16_t data_cap = nd500_mmu_get_data_capability(cpu, domain, seg);

        uint32_t seg_base = (uint32_t)seg << 27;
        uint32_t seg_end = seg_base + 0x08000000 - 1;

        if (prog_cap != 0) {
            if (seg_base < *code_start) *code_start = seg_base;
            if (seg_end > *code_end) *code_end = seg_end;
        }

        if (data_cap != 0) {
            if (seg_base < *data_start) *data_start = seg_base;
            if (seg_end > *data_end) *data_end = seg_end;
        }
    }
}
```

---

## 6. What the Load Window Needs to Change

### 6.1 Current Flow

```
User selects:
├─ Domain: 1
├─ Mode: "user"
├─ Program address: 0xD0000000
└─ Data address: 0xF0000000

Window shows hardcoded:
├─ "Code: 0xD0000000-0xD7FFFFFF" ✓
├─ "Data: 0xF0000000-0xF7FFFFFF" ✓

But doesn't verify:
├─ Domain 1 actually has capability for segment 26 ❌
├─ Domain 1 actually has capability for segment 30 ❌
├─ User typed address is valid for Domain 1 ❌
```

### 6.2 Proposed Improved Flow

```
1. User selects domain (via domainSelect dropdown)

2. Load window queries MMU:
   ├─ Get all program capabilities for domain
   ├─ Get all data capabilities for domain
   └─ Calculate valid ranges for this domain

3. Display domain-specific hints:
   ├─ "Domain 1 code: 0xD0000000-0xD7FFFFFF (segment 26)"
   ├─ "Domain 1 data: 0xF0000000-0xF7FFFFFF (segment 30)"
   └─ Or if no capabilities: "This domain has no segments!"

4. Validate user input:
   ├─ When user enters address, check if segment is valid for domain
   ├─ If not: show error "Address 0x... (segment X) not accessible to domain Y"
   └─ Allow load only if valid

5. Set domain context:
   ├─ Set CED and CAD registers to selected domain
   └─ MMU will then allow/deny access based on capabilities
```

---

## 7. Key Finding: Program vs. Data Capabilities

### 7.1 Separate Capability Tables

The **Program Capability (PC)** and **Data Capability (DC)** are stored separately:

```c
typedef struct {
    uint16_t program_capabilities[MAXSEG];      /* For instruction fetches */
    uint16_t data_capabilities[MAXSEG];         /* For data access */
    /* ... other fields ... */
} ProcessControlBlock;
```

### 7.2 Implications

A domain can have:
- ✅ Code at segment 1, but no data at segment 1
- ✅ Data at segment 0, but no code at segment 0
- ❌ No access to segment 5 at all
- ✅ Read-only access to segment 10 for data

**This means the Load window should**:
1. Show separate ranges for code (PSEG) and data (DSEG)
2. Validate each independently
3. Warn if a segment is write-protected

---

## 8. Current Hardcoded Default Configuration

**File**: `/home/ronny/repos/nd500x/src/debugger/commands.c` (mmusetup command)

```c
/* Domain 0 (Kernel) */
PCB[0].program_capabilities[1] = (1 << 13) | 0x0000  // Segment 1, PSN=1, direct
PCB[0].data_capabilities[0]    = (2 << 13) | 0x8000  // Segment 0, PSN=2, writable

/* Domain 1 (User) */
PCB[1].program_capabilities[26] = (3 << 13) | 0x0000  // Segment 26, PSN=3
PCB[1].data_capabilities[30]    = (5 << 13) | 0x8000  // Segment 30, PSN=5
```

This explains why the Load window shows these specific ranges!

---

## 9. Summary: Domain-Aware Address Validity

### Virtual Address Validity Rules

A virtual address `VA` is **valid** for domain `D` when:

| Access Type | Valid If |
|------------|----------|
| **Instruction fetch** | `PCB[D].program_capabilities[VA[31:27]] != 0` |
| **Data read** | `PCB[D].data_capabilities[VA[31:27]] != 0` |
| **Data write** | `PCB[D].data_capabilities[VA[31:27]] & 0x8000 == 0` (writable) |

### Segment-to-Address Mapping

| Segment | VA Range | Size |
|---------|----------|------|
| 0 | 0x00000000 - 0x07FFFFFF | 128 MB |
| 1 | 0x08000000 - 0x0FFFFFFF | 128 MB |
| 2 | 0x10000000 - 0x17FFFFFF | 128 MB |
| ... | ... | ... |
| 26 | 0xD0000000 - 0xD7FFFFFF | 128 MB |
| 30 | 0xF0000000 - 0xF7FFFFFF | 128 MB |
| 31 | 0xF8000000 - 0xFFFFFFFF | 128 MB |

### Default mmusetup Configuration

```
Domain 0 (Kernel):
├─ Code: Segment 1 (0x08000000 - 0x0FFFFFFF)
└─ Data: Segment 0 (0x00000000 - 0x07FFFFFF)

Domain 1 (User):
├─ Code: Segment 26 (0xD0000000 - 0xD7FFFFFF)
└─ Data: Segment 30 (0xF0000000 - 0xF7FFFFFF)
```

---

## 10. Recommendations

### For Load Window Enhancement

1. **Query actual domain capabilities** instead of hardcoding ranges
2. **Show domain-specific hints** with segment numbers
3. **Validate addresses before loading** against domain's capabilities
4. **Check write permissions** when loading data segments
5. **Set CED/CAD registers** to selected domain during load

### For Address Validation Function

Create new function `validate_load_address()` that checks:
- Segment exists in domain's capability table
- Segment has appropriate access (R/W for data)
- Physical segment is mapped (PSN valid)

### For User Experience

- Display hints like: "Domain 1: Code segments: [26], Data segments: [30]"
- Warn when address falls outside domain's segments
- Show address breakdown: "0xD0000000 = Segment 26, Page 0, Offset 0"
- Suggest valid ranges if user enters invalid address

---

## References

### Key Source Files

1. **Domain System**:
   - `/home/ronny/repos/nd500x/src/cpu/nd500_domain.h` - Domain API
   - `/home/ronny/repos/nd500x/src/cpu/nd500_domain.c` - Domain implementation

2. **MMU System**:
   - `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.h` - MMU structures & API
   - `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.c` - Address translation

3. **Load UI**:
   - `/home/ronny/repos/nd500x/src/frontend/nd500wasm/web/debugger.js` - Load window
   - `/home/ronny/repos/nd500x/src/machine/machine_loader.c` - File loading

4. **Configuration**:
   - `/home/ronny/repos/nd500x/src/debugger/commands.c` - mmusetup command

---

**End of Analysis**
