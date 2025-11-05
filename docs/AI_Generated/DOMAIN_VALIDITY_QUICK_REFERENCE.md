# ND-500 Domain Address Validity - Quick Reference

**Purpose**: Understand how virtual address validity depends on domain configuration

---

## Key Facts

### 1. Virtual Address Structure
```
32-bit address: [Segment(5 bits) | Page(16 bits) | Offset(11 bits)]
                [31:27]          [26:11]          [10:0]

Segment 0 = 0x00000000 - 0x07FFFFFF (128 MB)
Segment 1 = 0x08000000 - 0x0FFFFFFF (128 MB)
...
Segment 26 = 0xD0000000 - 0xD7FFFFFF (128 MB)
Segment 30 = 0xF0000000 - 0xF7FFFFFF (128 MB)
```

### 2. Domain-Specific Capabilities
Each domain has **32 segments** with independent access:

```c
ProcessControlBlock[domain] {
    program_capabilities[32]    /* For code (PC) */
    data_capabilities[32]       /* For data (DC) */
}
```

### 3. Address Validity Formula

A virtual address is **valid** for domain D when:

```
For Code (Instruction Fetch):
    capability = PCB[D].program_capabilities[addr[31:27]]
    Valid IF: capability != 0

For Data (Read):
    capability = PCB[D].data_capabilities[addr[31:27]]
    Valid IF: capability != 0

For Data (Write):
    capability = PCB[D].data_capabilities[addr[31:27]]
    Valid IF: capability != 0 AND (capability & 0x8000 == 0)
             ↑ DC_WRP flag: 0=writable, 1=read-only
```

### 4. 3-Level Address Translation

```
Virtual Address
    ↓ [Extract segment, get domain, lookup PCB]
Capability (or TRAP if no access)
    ↓ [Extract PSN, lookup PST entry]
Physical Segment Table Entry
    ↓ [Map via paging mode: direct/single/two-level]
Physical Address
```

### 5. Default mmusetup Configuration

```
Domain 0 (Kernel):
├─ Code: Segment 1  (0x08000000 - 0x0FFFFFFF) ✓
└─ Data: Segment 0  (0x00000000 - 0x07FFFFFF) ✓

Domain 1 (User):
├─ Code: Segment 26 (0xD0000000 - 0xD7FFFFFF) ✓
└─ Data: Segment 30 (0xF0000000 - 0xF7FFFFFF) ✓
```

---

## Current Load Window Problem

### What It Shows (Hardcoded)
```javascript
if (mode === 'kernel') {
    hint = 'Code: 0x08000000-0x0FFFFFFF, Data: 0x00000000-0x07FFFFFF'
} else {
    hint = 'Code: 0xD0000000-0xD7FFFFFF, Data: 0xF0000000-0xF7FFFFFF'
}
```

### What It Should Do (Domain-Aware)
1. Read actual domain capabilities from MMU
2. Calculate valid ranges dynamically
3. Show "Domain 1 has segments: 26 (code), 30 (data)"
4. Validate user input against actual capabilities
5. Reject addresses not in domain's segments

### What's Missing
- ❌ No validation that domain has capability for address segment
- ❌ No check for write permission (DC_WRP flag)
- ❌ Hardcoded ranges assume specific configuration
- ❌ Domain parameter selected but not used for validation

---

## How to Query Domain Valid Ranges

### C API Function to Add

```c
int validate_load_address(Nd500Cpu* cpu, uint32_t addr, uint8_t domain, int is_write) {
    int segment = (addr >> 27) & 0x1F;  /* Extract segment */
    
    uint16_t capability = is_write
        ? nd500_mmu_get_data_capability(cpu, domain, segment)
        : nd500_mmu_get_program_capability(cpu, domain, segment);
    
    if (capability == 0) return 0;  /* No capability for segment */
    
    if (is_write && (capability & 0x8000)) {
        return 0;  /* Read-only - can't write */
    }
    
    return 1;  /* Valid */
}
```

### JavaScript WASM Export Needed

```javascript
// Query valid ranges for a domain from WASM
const ranges = await this.module.ccall(
    'nd500_dbg_get_domain_segments_js',  // NEW FUNCTION
    'string',                             // Returns JSON
    ['number'],                           // domain param
    [domainNum]
);

// Expected return: 
// { code_segments: [1, 26], data_segments: [0, 30] }
```

---

## File Locations

| Purpose | File | Lines |
|---------|------|-------|
| **Domain Definitions** | `src/cpu/nd500_domain.h` | 1-125 |
| **Domain Implementation** | `src/cpu/nd500_domain.c` | 399-461 |
| **MMU Translation Logic** | `src/cpu/nd500_mmu.c` | 83-240 |
| **Capability Getters** | `src/cpu/nd500_mmu.c` | 286-300 |
| **Load Window UI** | `src/frontend/nd500wasm/web/debugger.js` | 93-294 |
| **File Loading Logic** | `src/machine/machine_loader.c` | 42-71 |
| **Validation Location** | *Missing - needs creation* | - |

---

## Test Cases

### Test 1: Domain 0 (Kernel) Loading
```
Domain: 0
Code address: 0x08000000 (Segment 1) → VALID ✓
Data address: 0x00000000 (Segment 0) → VALID ✓
Code address: 0xD0000000 (Segment 26) → INVALID ✗
```

### Test 2: Domain 1 (User) Loading
```
Domain: 1
Code address: 0xD0000000 (Segment 26) → VALID ✓
Data address: 0xF0000000 (Segment 30) → VALID ✓
Code address: 0x08000000 (Segment 1) → INVALID ✗
```

### Test 3: Custom Domain Configuration
```
Create domain 2 with:
  - Code: Segment 5
  - Data: Segment 10

Load to 0x28000000 (Segment 5) → VALID ✓
Load to 0x50000000 (Segment 10) → VALID ✓
Load to 0x08000000 (Segment 1) → INVALID ✗
```

---

## Implementation Checklist

- [ ] Create `validate_load_address()` function in MMU code
- [ ] Add WASM export `nd500_dbg_get_domain_segments_js()`
- [ ] Update Load window to query domain capabilities
- [ ] Calculate valid address ranges dynamically
- [ ] Display domain-specific hints
- [ ] Validate user input before loading
- [ ] Show error messages for invalid addresses
- [ ] Set CED/CAD registers when loading
- [ ] Test with both default and custom configurations

---

**See full analysis**: `/home/ronny/repos/nd500x/docs/DOMAIN_ADDRESS_VALIDITY_ANALYSIS.md`
