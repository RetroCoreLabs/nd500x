# ND-500 Domain Address Validity - Code Reference

**Purpose**: Complete code reference for understanding and implementing domain-aware address validation

---

## 1. Domain Capability Storage

### File: `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.h` (Lines 128-165)

**Process Control Block Structure**:
```c
typedef struct {
    uint16_t program_capabilities[MAXSEG];      /* [32] - For instruction fetches */
    uint16_t data_capabilities[MAXSEG];         /* [32] - For data access */
    
    /* ... other fields ... */
    
    uint8_t current_alternative_domain;         /* pcb_cad */
    uint8_t current_executing_domain;           /* pcb_ced */
} ProcessControlBlock;
```

**Constants**:
```c
#define MAXSEG          32          /* Segments per domain */
#define MAXDOM          256         /* Domains per process */

/* Capability Masks (Program Capability - PC) */
#define PC_PSN          0x1FFF      /* Physical Segment Number mask (13 bits) */
#define PC_DIR          0x0000      /* Direct Segment */
#define PC_IND          0x8000      /* Indirect Segment */

/* Capability Masks (Data Capability - DC) */
#define DC_WRP          0x8000      /* Write Permitted: 0=writable, 1=read-only */
#define DC_PAC          0x4000      /* Parameter Access (user mode) */
#define DC_PSN          0x1FFF      /* Physical Segment Number mask */
```

---

## 2. Address Translation Code

### File: `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.c` (Lines 83-240)

**Core Translation Function**:
```c
uint32_t nd500_mmu_translate(Nd500Cpu* cpu, uint32_t virtual_addr, int is_write, int is_instruction) {
    if (!g_mmu_enabled || !cpu) {
        return virtual_addr;
    }

    /* ─────────────────────────────────────────────────────────
     * LEVEL 1: Virtual Address → Capability
     * ───────────────────────────────────────────────────────── */

    /* Extract address components: [Segment(5) | Page(16) | Offset(11)] */
    int segment = (virtual_addr >> 27) & 0x1F;         /* Bits 31-27 */
    int page = (virtual_addr >> PGSHIFT) & 0xFFFF;     /* Bits 26-11 */
    int offset = virtual_addr & (NBPG - 1);            /* Bits 10-0 */

    /* Get current domain (CAD = Current Alternative Domain) */
    uint8_t domain = (uint8_t)cpu->CAD;

    /* Get capability from PCB */
    uint16_t capability;
    if (is_instruction) {
        /* Instruction fetch: use program capability */
        capability = g_pcb_table[domain].program_capabilities[segment];
    } else {
        /* Data access: use data capability */
        capability = g_pcb_table[domain].data_capabilities[segment];
    }

    /* Check if capability is valid (non-zero) */
    if (capability == 0) {
        trap_protect_violation(cpu, cpu->PC, virtual_addr);
        return 0;  /* No access rights to this segment */
    }

    /* ─────────────────────────────────────────────────────────
     * LEVEL 2: Capability → PST Entry
     * ───────────────────────────────────────────────────────── */

    /* Extract PSN (Physical Segment Number) from capability */
    int psn = capability & PC_PSN;  /* Lower 13 bits */

    if (psn >= MAX_PST) {
        trap_protect_violation(cpu, cpu->PC, virtual_addr);
        return 0;  /* Invalid PSN */
    }

    /* Check write permission (for data writes only) */
    if (!is_instruction && is_write) {
        /* Check DC_WRP flag: 0=writable, 1=read-only */
        if (capability & DC_WRP) {
            trap_protect_violation(cpu, cpu->PC, virtual_addr);
            return 0;  /* Write to read-only segment */
        }
    }

    /* Get PST entry */
    PhysicalSegmentTableEntry pst_entry = g_pst[psn];

    /* ─────────────────────────────────────────────────────────
     * LEVEL 3: PST Entry → Physical Address (mode-dependent)
     * ───────────────────────────────────────────────────────── */

    uint32_t physical_pfn;

    switch (pst_entry.index_mode) {
        case PS_AZI: {
            /* Mode 0: Direct Addressing (no paging) */
            physical_pfn = pst_entry.physical_pfn;
            break;
        }
        case PS_ASI: {
            /* Mode 1: Single-Level Paging */
            uint32_t page_table_base = pst_entry.physical_pfn << PGSHIFT;
            uint32_t pte_addr = page_table_base + (page * 4);
            PageTableEntry pte = nd500_mmu_read_pte(cpu, pte_addr);
            
            if (pte.physical_pfn == 0) {
                trap_page_fault(cpu, cpu->PC, virtual_addr);
                return 0;
            }
            
            if (is_write && pte.protection != 0) {
                trap_protect_violation(cpu, cpu->PC, virtual_addr);
                return 0;
            }
            
            physical_pfn = pte.physical_pfn;
            break;
        }
        case PS_ADI: {
            /* Mode 2: Two-Level Paging */
            uint32_t l1_table_base = pst_entry.physical_pfn << PGSHIFT;
            int l1_index = (page >> 8) & 0xFF;
            int l2_index = page & 0xFF;
            
            uint32_t l1_pte_addr = l1_table_base + (l1_index * 4);
            PageTableEntry l1_pte = nd500_mmu_read_pte(cpu, l1_pte_addr);
            
            if (l1_pte.physical_pfn == 0) {
                trap_page_fault(cpu, cpu->PC, virtual_addr);
                return 0;
            }
            
            uint32_t l2_table_base = l1_pte.physical_pfn << PGSHIFT;
            uint32_t l2_pte_addr = l2_table_base + (l2_index * 4);
            PageTableEntry l2_pte = nd500_mmu_read_pte(cpu, l2_pte_addr);
            
            if (l2_pte.physical_pfn == 0) {
                trap_page_fault(cpu, cpu->PC, virtual_addr);
                return 0;
            }
            
            if (is_write && (l1_pte.protection != 0 || l2_pte.protection != 0)) {
                trap_protect_violation(cpu, cpu->PC, virtual_addr);
                return 0;
            }
            
            physical_pfn = l2_pte.physical_pfn;
            break;
        }
        default:
            trap_illegal_operand(cpu, cpu->PC);
            return 0;
    }

    /* Construct physical address: (PFN << 11) | Offset */
    uint32_t physical_addr = (physical_pfn << PGSHIFT) | offset;

    return physical_addr;
}
```

---

## 3. Capability Getter Functions

### File: `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.c` (Lines 286-316)

**Access Functions**:
```c
uint16_t nd500_mmu_get_program_capability(Nd500Cpu* cpu, uint8_t domain, int segment) {
    if (!g_pcb_table || segment < 0 || segment >= MAXSEG) {
        return 0;
    }
    return g_pcb_table[domain].program_capabilities[segment];
}

uint16_t nd500_mmu_get_data_capability(Nd500Cpu* cpu, uint8_t domain, int segment) {
    if (!g_pcb_table || segment < 0 || segment >= MAXSEG) {
        return 0;
    }
    return g_pcb_table[domain].data_capabilities[segment];
}

void nd500_mmu_set_program_capability(Nd500Cpu* cpu, uint8_t domain, int segment, uint16_t capability) {
    if (!g_pcb_table || segment < 0 || segment >= MAXSEG) {
        return;
    }
    g_pcb_table[domain].program_capabilities[segment] = capability;
}

void nd500_mmu_set_data_capability(Nd500Cpu* cpu, uint8_t domain, int segment, uint16_t capability) {
    if (!g_pcb_table || segment < 0 || segment >= MAXSEG) {
        return;
    }
    g_pcb_table[domain].data_capabilities[segment] = capability;
}
```

---

## 4. Domain Analysis Functions

### File: `/home/ronny/repos/nd500x/src/cpu/nd500_domain.c` (Lines 399-461)

**Check Capability for Domain and Segment**:
```c
int nd500_domain_has_capability(Nd500Cpu* cpu, uint8_t domain, int segment) {
    if (!cpu || domain >= MAX_DOMAINS || segment < 0 || segment >= MAXSEG) {
        return 0;
    }

    /* Check both program and data capabilities */
    uint16_t prog_cap = nd500_mmu_get_program_capability(cpu, domain, segment);
    uint16_t data_cap = nd500_mmu_get_data_capability(cpu, domain, segment);

    return (prog_cap != 0 || data_cap != 0);
}
```

**Determine Domain from Address**:
```c
uint8_t nd500_domain_from_address(Nd500Cpu* cpu, uint32_t virtual_addr) {
    if (!cpu) return 0xFF;

    /* Extract segment from virtual address */
    int segment = (virtual_addr >> 27) & 0x1F;

    /* Check current domain first */
    uint8_t current_domain = (uint8_t)cpu->CAD;
    if (nd500_domain_has_capability(cpu, current_domain, segment)) {
        return current_domain;
    }

    /* Search all domains for one with capability */
    for (int domain = 0; domain < MAX_DOMAINS; domain++) {
        if (nd500_domain_has_capability(cpu, domain, segment)) {
            return (uint8_t)domain;
        }
    }

    return 0xFF;  /* Not found */
}
```

---

## 5. Load Window Code

### File: `/home/ronny/repos/nd500x/src/frontend/nd500wasm/web/debugger.js` (Lines 93-294)

**Current Implementation** (Problematic - Hardcoded):
```javascript
openLoadModal() {
    const modeSelect = document.getElementById('modeSelect');
    const domainSelect = document.getElementById('domainSelect');
    
    const updateMmuStatus = () => {
        if (this.module && modeSelect) {
            const mmuEnabled = this.module.ccall('nd500_dbg_mmu_is_enabled_js', 'number', [], []);
            const mode = modeSelect.value;

            if (mmuEnabled) {
                // MMU Enabled: Show Mode and Domain, use virtual addresses
                if (mode === 'kernel') {
                    mmuStatusHint.textContent = 'Virtual addresses: Code 0x08000000-0x0FFFFFFF, Data 0x00000000-0x07FFFFFF';
                } else {
                    mmuStatusHint.textContent = 'Virtual addresses: Code 0xD0000000-0xD7FFFFFF, Data 0xF0000000-0xF7FFFFFF';
                }
            } else {
                // MMU Disabled: Hide Mode and Domain, use physical addresses
                mmuStatusHint.textContent = 'Physical memory: 0x00000000-0x00FFFFFF (16MB) - Direct addressing, no domains';
            }
        }
    };

    modeSelect.onchange = () => {
        if (modeSelect.value === 'kernel') {
            domainSelect.value = '0';
        } else {
            domainSelect.value = '1';
        }
        updateMmuStatus();
    };

    updateMmuStatus();
}
```

**Load Execution** (Lines 242-290):
```javascript
confirmBtn.onclick = async () => {
    const mode = modeSelect.value;
    const domain = parseInt(domainSelect.value, 10);
    
    const psegAddrInput = document.getElementById('psegAddr').value.trim();
    const dsegAddrInput = document.getElementById('dsegAddr').value.trim();

    // Use custom addresses or mode defaults
    let psegBase = (mode === 'kernel') ? (0x08000000 >>> 0) : (0xD0000000 >>> 0);
    let dsegBase = (mode === 'kernel') ? (0x00000000 >>> 0) : (0xF0000000 >>> 0);

    if (psegAddrInput) {
        psegBase = this.parseHexOrDec(psegAddrInput) >>> 0;
    }
    if (dsegAddrInput) {
        dsegBase = this.parseHexOrDec(dsegAddrInput) >>> 0;
    }

    // NOTE: These are VIRTUAL addresses - MMU will translate to physical
    await this.loadSplitViaMemfs(pseg, psegBase, dseg, dsegBase, startPc.value, domain);
    
    // Set domain registers
    this.module.ccall('nd500_dbg_set_reg_js', 'number', ['string', 'number'], ['CED', domain]);
    this.module.ccall('nd500_dbg_set_reg_js', 'number', ['string', 'number'], ['CAD', domain]);
};
```

---

## 6. Backend Load Functions

### File: `/home/ronny/repos/nd500x/src/machine/machine_loader.c` (Lines 42-79)

**Generic File to Memory Loader**:
```c
int nd500_load_file_to_memory(Nd500Machine* m, const char* path, uint32_t base_addr) {
    if (!m || !path) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    int rc = 0;
    uint8_t buffer[4096];
    uint32_t offset = 0;

    /* Check if MMU is enabled - if so, allow virtual addresses */
    int mmu_enabled = nd500_machine_mmu_is_enabled(m);

    for (;;) {
        size_t rd = fread(buffer, 1, sizeof(buffer), f);
        if (rd == 0) break;

        /* Only check physical memory range if MMU is disabled */
        if (!mmu_enabled && (base_addr + offset + (uint32_t)rd > m->memory_size)) {
            rc = -2; /* out of range */
            break;
        }

        /* nd500_bus_write8 will handle MMU translation if enabled */
        for (size_t i = 0; i < rd; ++i) {
            nd500_bus_write8(m, base_addr + (uint32_t)offset + (uint32_t)i, buffer[i]);
        }
        offset += (uint32_t)rd;
    }
    fclose(f);
    return rc;
}

int nd500_load_pseg_file(Nd500Machine* m, const char* path, uint32_t pseg_base_addr) {
    return nd500_load_file_to_memory(m, path, pseg_base_addr);
}

int nd500_load_dseg_file(Nd500Machine* m, const char* path, uint32_t dseg_base_addr) {
    return nd500_load_file_to_memory(m, path, dseg_base_addr);
}
```

---

## 7. Default MMU Configuration

### File: `/home/ronny/repos/nd500x/src/debugger/commands.c` (mmusetup command - lines ~400-500)

**Default Capability Setup** (Approximate):
```c
/* Domain 0 (Kernel) */
PCB[0].program_capabilities[1] = (1 << 13) | 0x0000;   /* Segment 1, PSN=1, direct, executable */
PCB[0].data_capabilities[0]    = (2 << 13) | 0x8000;   /* Segment 0, PSN=2, writable */

/* Domain 1 (User) */
PCB[1].program_capabilities[26] = (3 << 13) | 0x0000;  /* Segment 26, PSN=3, direct, executable */
PCB[1].data_capabilities[30]    = (5 << 13) | 0x8000;  /* Segment 30, PSN=5, writable */

/* PST Configuration */
PST[1].index_mode = PS_AZI;  PST[1].physical_pfn = 1;  /* Kernel code */
PST[2].index_mode = PS_AZI;  PST[2].physical_pfn = 2;  /* Kernel data */
PST[3].index_mode = PS_AZI;  PST[3].physical_pfn = 3;  /* User code */
PST[5].index_mode = PS_AZI;  PST[5].physical_pfn = 5;  /* User data */
```

---

## 8. WASM Backend Function

### File: `/home/ronny/repos/nd500x/src/frontend/nd500wasm/main.c` (Lines 92-108)

**Load Segments Function**:
```c
EMSCRIPTEN_KEEPALIVE
int nd500_dbg_load_segments_path_js(const char* pseg_path, uint32_t pseg_base,
                                    const char* dseg_path, uint32_t dseg_base,
                                    int set_pc, uint32_t pc) {
    int rc = 0;
    if (pseg_path && *pseg_path) {
        rc = nd500_load_pseg_file(&g_machine, pseg_path, pseg_base);
        if (rc != 0) return rc;
    }
    if (dseg_path && *dseg_path) {
        rc = nd500_load_dseg_file(&g_machine, dseg_path, dseg_base);
        if (rc != 0) return rc;
    }
    if (set_pc) {
        g_cpu.PC = pc;
    }
    return 0;
}
```

---

## 9. Proposed New Validation Function

**Recommendation**: Add to `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.h` and `.c`

```c
/* ═══════════════════════════════════════════════════════
   NEW FUNCTION - Address Validation for Loading
   ═══════════════════════════════════════════════════════ */

/**
 * Validate if a virtual address is accessible by a domain for loading
 * 
 * @param cpu CPU structure
 * @param addr Virtual address to validate
 * @param domain Domain number (0-255)
 * @param is_write 1 for write access (data), 0 for read/execute (code)
 * @return 1 if valid, 0 if invalid
 */
int nd500_mmu_validate_load_address(Nd500Cpu* cpu, uint32_t addr, uint8_t domain, int is_write) {
    if (!cpu || domain >= MAXDOM) return 0;

    /* Extract segment from address */
    int segment = (addr >> SGSHIFT) & 0x1F;
    if (segment < 0 || segment >= MAXSEG) return 0;

    /* Get capability */
    uint16_t capability = is_write
        ? nd500_mmu_get_data_capability(cpu, domain, segment)
        : nd500_mmu_get_program_capability(cpu, domain, segment);

    if (capability == 0) return 0;  /* No capability for this segment */

    /* For write access, check DC_WRP flag */
    if (is_write && (capability & DC_WRP)) {
        return 0;  /* Read-only segment */
    }

    return 1;  /* Valid */
}

/**
 * Get all valid segment ranges for a domain
 * 
 * @param cpu CPU structure
 * @param domain Domain number
 * @param code_segments Array to fill with code segment numbers (size must be 32+)
 * @param code_count Pointer to receive count of code segments
 * @param data_segments Array to fill with data segment numbers
 * @param data_count Pointer to receive count of data segments
 */
void nd500_mmu_get_domain_segments(Nd500Cpu* cpu, uint8_t domain,
                                   int* code_segments, int* code_count,
                                   int* data_segments, int* data_count) {
    if (!cpu || domain >= MAXDOM || !code_count || !data_count) return;

    *code_count = 0;
    *data_count = 0;

    for (int seg = 0; seg < MAXSEG; seg++) {
        uint16_t prog_cap = nd500_mmu_get_program_capability(cpu, domain, seg);
        uint16_t data_cap = nd500_mmu_get_data_capability(cpu, domain, seg);

        if (prog_cap != 0 && code_segments) {
            code_segments[(*code_count)++] = seg;
        }

        if (data_cap != 0 && data_segments) {
            data_segments[(*data_count)++] = seg;
        }
    }
}
```

---

## Summary Table

| Operation | Function | File | Purpose |
|-----------|----------|------|---------|
| **Get Program Cap** | `nd500_mmu_get_program_capability()` | nd500_mmu.c:286 | Get code segment capability |
| **Get Data Cap** | `nd500_mmu_get_data_capability()` | nd500_mmu.c:294 | Get data segment capability |
| **Translate Address** | `nd500_mmu_translate()` | nd500_mmu.c:83 | Full 3-level translation |
| **Has Capability** | `nd500_domain_has_capability()` | nd500_domain.c:451 | Check if domain can access segment |
| **Find Domain** | `nd500_domain_from_address()` | nd500_domain.c:399 | Find which domain owns address |
| **Load File** | `nd500_load_file_to_memory()` | machine_loader.c:42 | Load binary to memory |
| **Validate** | *(TO BE CREATED)* | nd500_mmu.c | Validate address for loading |

---

**Next Steps**: Use these functions and code examples to implement domain-aware address validation in the Load window.
