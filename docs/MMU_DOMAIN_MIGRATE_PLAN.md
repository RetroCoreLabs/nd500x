# ND-500 MMU and Domain System Migration Plan

**Project**: nd500x C Emulator
**Source**: C# RetroCore Emulator (`/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500`)
**Target**: C Implementation (`/home/ronny/repos/nd500x`)
**Status**: Planning Phase
**Last Updated**: 2025-01-15

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Source Files Reference](#source-files-reference)
3. [Current Implementation Status](#current-implementation-status)
4. [Migration Tasks](#migration-tasks)
5. [Web UI Design Plan](#web-ui-design-plan)
6. [Success Criteria](#success-criteria)
7. [Progress Tracking](#progress-tracking)

---

## Executive Summary

Migrate the complete MMU (Memory Management Unit) and domain system from the working C# emulator to the C implementation. This includes:

- **Registers**: Add MMU-specific registers (PSTP, DITBASE, CED, CAD, PS)
- **MMU**: Three-level address translation (Virtual → Capability → PST → Physical)
- **Domains**: Cross-domain calling for kernel/user separation
- **Console Commands**: Symbol-aware debugging commands (already partially implemented!)
- **Web Interface**: Complete MMU/Domain visualization and control panel

---

## Source Files Reference

### From `/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500`:

| File | Lines | Purpose | Key Features |
|------|-------|---------|-------------|
| `Registers.cs` | 950 | Complete register set | MMU registers: PSTP, DITBASE, CED, CAD, PS |
| `CpuND500.MMU.cs` | 617 | MMU address translation | 3-level translation, PST, PCB, phyladr() |
| `CpuND500.Domain.cs` | 682 | Domain system | Cross-domain calls, DIT access, PCB management |
| `CpuND500.IndirectSegments.cs` | 220 | Indirect segments | Start Address Vector, PC_OMC flag |
| `CpuND500.ND100Bridge.cs` | 220 | ND-100 bridge | Shared memory, _private offset |
| `CpuND500.Loader.cs` | 250 | Segment loader | Domain-aware program loading |
| `MMUConfiguration.cs` | 717 | Configuration system | Command-line args, presets |

**Total Reference Code**: ~3,656 lines

### Documentation Reference

| File | Purpose |
|------|---------|
| `spec/ND500_MMU_IMPLEMENTATION_STATUS.md` | Phase tracking, implementation status |
| `spec/MMU_CONFIGURATION_EXAMPLES.md` | Usage examples, command-line reference |
| `spec/ND500_MMU_DOMAIN_MASTER_REFERENCE.md` | Complete architecture reference |

---

## Current Implementation Status

### ✅ Already Implemented

| Component | Location | Status |
|-----------|----------|--------|
| Basic CPU registers | `src/cpu/cpu_protos.h` lines 6-28 | ✅ Complete |
| **MMU registers** | `src/cpu/cpu_protos.h` lines 26-31 | ✅ **NEW: Phase 1** |
| Status registers (ST1, ST2) | `src/cpu/cpu_protos.h` lines 22-23 | ✅ Complete |
| Trap enable registers | `src/cpu/cpu_protos.h` lines 21 | ✅ Complete |
| Trap system | `src/cpu/cpu.c` lines 104-277 | ✅ Complete |
| **MMU initialization** | `src/cpu/cpu.c` line 22 | ✅ **NEW: Calls nd500_mmu_init()** |
| **MMU structures** | `src/cpu/nd500_mmu.h` | ✅ **NEW: Phase 2 (269 lines)** |
| **MMU implementation** | `src/cpu/nd500_mmu.c` | ✅ **NEW: Phase 2 (252 lines stub)** |
| Symbol loading | `src/ndlib/ndlib_symbols.c` | ✅ Complete (188 lines) |
| Symbol lookup | `src/ndlib/ndlib.h` lines 17-35 | ✅ Complete |
| Segment info | `src/ndlib/ndlib.h` lines 13-15 | ✅ Complete |
| **Console: segments** | `src/debugger/debugger.c` lines 975-984 | ✅ **Already done!** |
| **Console: goto** | `src/debugger/debugger.c` lines 985-1001 | ✅ **Already done!** |
| **Console: msym** | `src/debugger/debugger.c` lines 1002-1033 | ✅ **Already done!** |
| **Console: dsym** | `src/debugger/debugger.c` lines 1034-1070 | ✅ **Already done!** |
| **Web: Register display** | `src/frontend/nd500wasm/web/debugger.js` | ✅ Complete |
| **Web: Memory viewer** | `src/frontend/nd500wasm/web/index.html` | ✅ Complete |
| **Web: Disassembly** | `src/frontend/nd500wasm/web/debugger.js` | ✅ Complete |

**Recent Progress**: ✅ Phase 1, 2 & 3 Complete! Complete MMU translation implemented (2025-01-15)

### ❌ Missing Components

| Component | Priority | Status |
|-----------|----------|--------|
| ~~MMU registers (PSTP, DITBASE, etc.)~~ | HIGH | ✅ Phase 1 Complete |
| ~~MMU data structures (PST, PCB, PTE)~~ | HIGH | ✅ Phase 2 Complete |
| ~~MMU address translation~~ | HIGH | ✅ Phase 3 Complete |
| Domain switching logic | MEDIUM | ⏳ Phase 4 Pending |
| DIT access functions | MEDIUM | ~200 lines |
| PCB management | MEDIUM | ~150 lines |
| MMU console commands | MEDIUM | ~200 lines |
| **Web: MMU panel** | HIGH | ~300 lines HTML/JS |
| **Web: Domain viewer** | HIGH | ~250 lines HTML/JS |
| **Web: PST inspector** | MEDIUM | ~200 lines HTML/JS |
| **Web: PCB viewer** | MEDIUM | ~200 lines HTML/JS |
| ND-100 bridge (optional) | LOW | ~220 lines |

**Total New Code**: ~2,575 lines (including web UI)

---

## Migration Tasks

### Phase 1: MMU Registers ✅ COMPLETE

**Goal**: Add MMU-specific registers to CPU structure

**Priority**: HIGH (Blocks all other phases)

**Estimated Time**: 0.5 days

**Status**: ✅ Completed 2025-01-15

**Changes Made**:
- ✅ Added PSTP, DITBASE, CED, CAD, PS to `Nd500Cpu` structure
- ✅ Added MMU registers to `Nd500Regs` structure
- ✅ Updated `nd500_cpu_reset()` to initialize MMU registers
- ✅ Updated `nd500_cpu_get_regs()` to copy MMU registers
- ✅ Updated console `regs` command to display MMU registers
- ✅ Updated console `set` command to support MMU registers
- ✅ Added MMU registers to tab completion arrays
- ✅ Updated help text and usage messages
- ✅ Build and test successful

#### 1.1 Update CPU Structure

**File**: `src/cpu/cpu_protos.h`

**Changes**:
```c
typedef struct Nd500Cpu {
    /* Core registers */
    uint32_t PC;
    uint32_t I[4];
    uint32_t A[4];
    uint32_t E[4];
    uint32_t L, B, R;
    uint32_t TOS, LL, HL, THA;
    uint32_t OTE1, OTE2, CTE1, CTE2, MTE1, MTE2, TEMM1, TEMM2;
    uint32_t ST1, ST2;
    uint32_t FLAGS;

    /* ===== NEW: MMU Registers ===== */
    uint32_t PSTP;      // Physical Segment Table Pointer
    uint32_t DITBASE;   // Domain Information Table base address
    uint32_t CED;       // Current Executing Domain
    uint32_t CAD;       // Current Alternative Domain
    uint32_t PS;        // Process Segment pointer

    Nd500Machine* machine;
} Nd500Cpu;
```

**Also Update**:
```c
typedef struct Nd500Regs {
    uint32_t PC;
    uint32_t FLAGS;
    uint32_t I[4];
    uint32_t A[4];
    uint32_t E[4];
    uint32_t L, B, R;
    uint32_t TOS, LL, HL, THA;
    uint32_t OTE1, OTE2, CTE1, CTE2, MTE1, MTE2, TEMM1, TEMM2;
    uint32_t ST1, ST2;

    /* NEW: MMU Registers */
    uint32_t PSTP;
    uint32_t DITBASE;
    uint32_t CED;
    uint32_t CAD;
    uint32_t PS;
} Nd500Regs;
```

#### 1.2 Update CPU Functions

**File**: `src/cpu/cpu.c`

**Update `nd500_cpu_reset()`**:
```c
void nd500_cpu_reset(Nd500Cpu* cpu) {
    if (!cpu) return;
    cpu->PC = 0;
    cpu->FLAGS = 0;
    memset(cpu->I, 0, sizeof(cpu->I));
    memset(cpu->A, 0, sizeof(cpu->A));
    memset(cpu->E, 0, sizeof(cpu->E));
    cpu->L = cpu->B = cpu->R = 0;
    cpu->TOS = cpu->LL = cpu->HL = cpu->THA = 0;
    cpu->OTE1 = cpu->OTE2 = cpu->CTE1 = cpu->CTE2 = 0;
    cpu->MTE1 = cpu->MTE2 = cpu->TEMM1 = cpu->TEMM2 = 0;
    cpu->ST1 = cpu->ST2 = 0;

    /* NEW: Reset MMU registers */
    cpu->PSTP = 0;
    cpu->DITBASE = 0;
    cpu->CED = 0;
    cpu->CAD = 0;
    cpu->PS = 0;

    nd500_trap_clear();
}
```

**Update `nd500_cpu_get_regs()`**:
```c
void nd500_cpu_get_regs(Nd500Cpu* cpu, Nd500Regs* out) {
    if (!cpu || !out) return;
    out->PC = cpu->PC;
    out->FLAGS = cpu->FLAGS;
    for (int i = 0; i < 4; ++i) {
        out->I[i] = cpu->I[i];
        out->A[i] = cpu->A[i];
        out->E[i] = cpu->E[i];
    }
    out->L = cpu->L; out->B = cpu->B; out->R = cpu->R;
    out->TOS = cpu->TOS; out->LL = cpu->LL; out->HL = cpu->HL; out->THA = cpu->THA;
    out->OTE1 = cpu->OTE1; out->OTE2 = cpu->OTE2;
    out->CTE1 = cpu->CTE1; out->CTE2 = cpu->CTE2;
    out->MTE1 = cpu->MTE1; out->MTE2 = cpu->MTE2;
    out->TEMM1 = cpu->TEMM1; out->TEMM2 = cpu->TEMM2;
    out->ST1 = cpu->ST1; out->ST2 = cpu->ST2;

    /* NEW: Copy MMU registers */
    out->PSTP = cpu->PSTP;
    out->DITBASE = cpu->DITBASE;
    out->CED = cpu->CED;
    out->CAD = cpu->CAD;
    out->PS = cpu->PS;
}
```

#### 1.3 Update Console Commands

**File**: `src/debugger/debugger.c`

**Update `regs` command** (around line 801):
```c
else if (strcmp(tok, "regs") == 0) {
    if (!m->cpu) { printf("no cpu linked\n"); continue; }
    Nd500Regs r; memset(&r, 0, sizeof(r));
    nd500_dbg_regs(m->cpu, &r);
    printf("PC=%08X FLAGS=%08X\n", r.PC, r.FLAGS);
    printf("I: %08X %08X %08X %08X\n", r.I[0], r.I[1], r.I[2], r.I[3]);
    printf("A: %08X %08X %08X %08X\n", r.A[0], r.A[1], r.A[2], r.A[3]);
    printf("E: %08X %08X %08X %08X\n", r.E[0], r.E[1], r.E[2], r.E[3]);
    printf("L=%08X B=%08X R=%08X\n", r.L, r.B, r.R);
    printf("TOS=%08X LL=%08X HL=%08X THA=%08X\n", r.TOS, r.LL, r.HL, r.THA);
    printf("OTE1=%08X OTE2=%08X CTE1=%08X CTE2=%08X\n", r.OTE1, r.OTE2, r.CTE1, r.CTE2);
    printf("MTE1=%08X MTE2=%08X TEMM1=%08X TEMM2=%08X\n", r.MTE1, r.MTE2, r.TEMM1, r.TEMM2);

    /* NEW: Display MMU registers */
    printf("\n=== MMU/Domain Registers ===\n");
    printf("PSTP=%08X DITBASE=%08X PS=%08X\n", r.PSTP, r.DITBASE, r.PS);
    printf("CED=%u CAD=%u\n", r.CED, r.CAD);
}
```

**Update `set` command** (around line 1195) - add cases:
```c
} else if (strcmp(reg_name, "PSTP") == 0) {
    m->cpu->PSTP = value;
    printf("PSTP = 0x%08X\n", value);
} else if (strcmp(reg_name, "DITBASE") == 0) {
    m->cpu->DITBASE = value;
    printf("DITBASE = 0x%08X\n", value);
} else if (strcmp(reg_name, "CED") == 0) {
    m->cpu->CED = value;
    printf("CED = %u\n", value);
} else if (strcmp(reg_name, "CAD") == 0) {
    m->cpu->CAD = value;
    printf("CAD = %u\n", value);
} else if (strcmp(reg_name, "PS") == 0) {
    m->cpu->PS = value;
    printf("PS = 0x%08X\n", value);
```

**Update help message**:
```c
printf("  set <register> <value>      Set register value\n");
printf("      registers: PC, I1-I4, A1-A4, E1-E4, L, B, R, FLAGS,\n");
printf("                 TOS, LL, HL, THA, ST1, ST2,\n");
printf("                 PSTP, DITBASE, CED, CAD, PS\n");
```

**Update tab completion** (around line 55):
```c
static const char* set_subcommands[] = {
    "PC", "I1", "I2", "I3", "I4", "A1", "A2", "A3", "A4", "E1", "E2", "E3", "E4",
    "L", "B", "R", "FLAGS", "TOS", "LL", "HL", "THA", "ST1", "ST2",
    "PSTP", "DITBASE", "CED", "CAD", "PS"  /* NEW */
};
```

#### 1.4 Acceptance Criteria

- [ ] New registers compile without errors
- [ ] `regs` command shows MMU registers
- [ ] `set` command can modify MMU registers (PSTP, DITBASE, CED, CAD, PS)
- [ ] Tab completion works for new register names
- [ ] Registers reset to 0 on CPU reset
- [ ] `nd500_cpu_get_regs()` includes MMU registers

---

### Phase 2: MMU Data Structures ✅ COMPLETE

**Goal**: Define MMU data structures for address translation

**Priority**: HIGH

**Estimated Time**: 0.5 days

**Status**: ✅ Completed 2025-01-15

**New Files Created**:
- ✅ `src/cpu/nd500_mmu.h` (269 lines) - Header with constants and structures
- ✅ `src/cpu/nd500_mmu.c` (252 lines) - Stub implementation
- ✅ `test/test_mmu_structures.c` (87 lines) - Structure size verification

**Key Content**:
- MMU constants (NBPG, PGSHIFT, PS_AZI/ASI/ADI, PC_xxx, DC_xxx masks)
- `PhysicalSegmentTableEntry` struct (8 bytes)
- `PageTableEntry` struct (8 bytes)
- `ProcessControlBlock` struct (208 bytes)
- Function declarations for MMU operations

**Reference**: See C# `CpuND500.MMU.cs` lines 34-198

**Acceptance Criteria**:
- ✅ Header compiles without errors
- ✅ All constants match C# implementation
- ✅ Structures have correct sizes (verified: PST=8B, PTE=8B, PCB=208B)
- ✅ No conflicting macro definitions
- ✅ CMakeLists.txt updated to build nd500_mmu.c
- ✅ Build successful
- ✅ Test program verifies structure sizes

---

### Phase 3: MMU Address Translation ✅ COMPLETE

**Goal**: Implement three-level address translation

**Priority**: HIGH

**Estimated Time**: 1-2 days

**Status**: ✅ Completed 2025-01-15

**Modified Files**:
- ✅ `src/cpu/nd500_mmu.c` (expanded to 419 lines) - Complete 3-level translation
- ✅ `src/cpu/nd500_mmu.h` (no changes needed - declarations already present)

**Key Functions Implemented**:
1. ✅ `nd500_mmu_init()` - Allocate PST (8192 entries) and PCB table (256 domains)
2. ✅ `nd500_mmu_enable()` / `nd500_mmu_disable()` - Control MMU state
3. ✅ `nd500_mmu_translate()` - **Core function**: Virtual → Physical (3 levels)
   - Level 1: Virtual Address → Capability (via PCB)
   - Level 2: Capability → PST Entry (via PSN)
   - Level 3: PST Entry → Physical Address (mode-dependent)
4. ✅ `nd500_mmu_phyladr()` - Public wrapper for debugger
5. ✅ `nd500_mmu_read_pte()` - Read Page Table Entry from memory
6. ✅ `nd500_mmu_write_pte()` - Write Page Table Entry to memory
7. ✅ PST accessors (`get_pst_entry`, `set_pst_entry`)
8. ✅ PCB accessors (`get_pcb`, `get_program_capability`, `get_data_capability`)
9. ✅ Cache control (`dctsb`, `pctsb`)

**Translation Flow**:
```
Virtual Address (32-bit)
    ↓
[Segment (5), Page (16), Offset (11)]
    ↓
PCB[domain].capabilities[segment] → Capability (16-bit)
    ↓
PST[PSN].index_mode + PST[PSN].pfn
    ↓
Mode 0 (PS_AZI): Direct → Physical PFN
Mode 1 (PS_ASI): Single-level → PTE → Physical PFN
Mode 2 (PS_ADI): Two-level → L1 PTE → L2 PTE → Physical PFN
    ↓
Physical Address = (PFN << 11) | Offset
```

**Reference**: See C# `CpuND500.MMU.cs` lines 273-448

**Acceptance Criteria**:
- ✅ MMU initializes PST and PCB tables without memory leaks
- ✅ `nd500_mmu_translate()` performs 3-level translation correctly
- ✅ Direct mode (PS_AZI) extracts PFN directly from PST entry
- ✅ Single-level paging (PS_ASI) reads PTE from memory at (base + page*4)
- ✅ Two-level paging (PS_ADI) reads L1 and L2 PTEs
- ✅ Write protection enforced (DC_WRP flag checked for data writes)
- ✅ User access protection enforced (DC_PAC flag - ready for integration)
- ✅ Protection violations trigger `trap_protect_violation()`
- ✅ Page faults trigger `trap_page_fault()`
- ✅ Build successful
- ✅ MMU initializes on emulator startup

---

### Phase 4: Domain System ⏳ NOT STARTED

**Goal**: Implement cross-domain calling and DIT management

**Priority**: MEDIUM

**Estimated Time**: 1-2 days

**New File**: `src/cpu/nd500_domain.h` (~100 lines)

**Constants**:
```c
#define DIT_ENTRY_SIZE    16    /* 16 bytes per domain */
#define DIT_TOS_OFFSET    0
#define DIT_LL_OFFSET     4
#define DIT_HL_OFFSET     8
#define DIT_THA_OFFSET    12
#define KERNEL_DOMAIN     0
#define MAX_DOMAINS       256

#define PCB_SIZE          256
#define PCB_CALL_OFFSET   128
#define PCB_CALL_CE_OFFSET   (PCB_CALL_OFFSET + 0)
#define PCB_CALL_CA_OFFSET   (PCB_CALL_OFFSET + 1)
#define PCB_CALL_P_OFFSET    (PCB_CALL_OFFSET + 3)
#define PCB_CALL_B_OFFSET    (PCB_CALL_OFFSET + 7)
```

**New File**: `src/cpu/nd500_domain.c` (~700 lines)

**Key Functions**:
1. `nd500_domain_init()` - Initialize domain system (set CAD=0, CED=0)
2. `nd500_domain_setup_dit()` - Allocate DIT in memory, clear all entries
3. `nd500_domain_switch()` - **Critical**: Cross-domain call
   - Save caller context to TARGET domain's PCB.pcb_call
   - Save current domain state to DIT[CAD]
   - Update CAD and CED registers
   - Load target domain state from DIT[target]
   - Update B register, mark domain boundary (PREVB=0, RETA=0)
4. `nd500_domain_return()` - Return from cross-domain call
   - Load caller context from PCB.pcb_call
   - Restore calling domain state
   - Update CAD, CED, B, P registers
5. `nd500_domain_is_boundary()` - Check if PREVB=0 and RETA=0
6. DIT access functions:
   - `nd500_domain_read_tos/ll/hl/tha(domain)`
   - `nd500_domain_write_tos/ll/hl/tha(domain, value)`
7. `nd500_domain_from_address()` - Determine domain from virtual address
8. `nd500_domain_is_cross_domain_call()` - Detect cross-domain calls

**Reference**: See C# `CpuND500.Domain.cs` lines 1-682

**Acceptance Criteria**:
- [ ] Domain system initializes with CAD=0, CED=0
- [ ] DIT setup allocates memory at DITBASE
- [ ] Domain switch saves caller context to PCB
- [ ] Domain switch updates CAD, CED registers correctly
- [ ] Domain boundary marked with PREVB=0, RETA=0
- [ ] Domain return restores calling domain
- [ ] DIT read/write functions access correct memory offsets
- [ ] No memory corruption during domain switches

---

### Phase 5: Integration with Memory Bus ✅ COMPLETE

**Goal**: Hook MMU translation into memory bus operations

**Priority**: HIGH

**Estimated Time**: 0.5 days

**Status**: ✅ COMPLETE - All MMU translation integrated into CPU instruction execution

#### 5.1 Update Machine Structure

**File**: `src/machine/machine_types.h`

```c
typedef struct Nd500Machine {
    uint8_t* memory;
    uint32_t memory_size;
    volatile int run_flag;
    struct Nd500Cpu* cpu;
    struct BreakpointManager* bp_mgr;

    /* NEW: MMU flag */
    int mmu_enabled;
} Nd500Machine;
```

#### 5.2 Add MMU Control Functions

**File**: `src/machine/machine_protos.h`

```c
/* MMU control */
void nd500_machine_enable_mmu(Nd500Machine* m);
void nd500_machine_disable_mmu(Nd500Machine* m);
int nd500_machine_mmu_is_enabled(Nd500Machine* m);
```

**File**: `src/machine/machine.c`

```c
void nd500_machine_enable_mmu(Nd500Machine* m) {
    if (!m) return;
    m->mmu_enabled = 1;
    if (m->cpu) nd500_mmu_enable(m->cpu);
}

void nd500_machine_disable_mmu(Nd500Machine* m) {
    if (!m) return;
    m->mmu_enabled = 0;
    if (m->cpu) nd500_mmu_disable(m->cpu);
}

int nd500_machine_mmu_is_enabled(Nd500Machine* m) {
    return m ? m->mmu_enabled : 0;
}
```

#### 5.3 Update Bus Functions

**File**: `src/machine/machine.c`

**Modify all bus read/write functions**:

```c
uint8_t nd500_bus_read8(Nd500Machine* m, uint32_t addr) {
    if (!m || !m->memory) return 0xFF;

    /* If MMU enabled and CPU linked, translate address */
    if (m->mmu_enabled && m->cpu) {
        addr = nd500_mmu_translate(m->cpu, addr, 0, 0);
        if (addr == 0) return 0xFF;  /* Translation failed */
    }

    if (addr >= m->memory_size) return 0xFF;
    return m->memory[addr];
}

void nd500_bus_write8(Nd500Machine* m, uint32_t addr, uint8_t val) {
    if (!m || !m->memory) return;

    /* If MMU enabled and CPU linked, translate address */
    if (m->mmu_enabled && m->cpu) {
        addr = nd500_mmu_translate(m->cpu, addr, 1, 0);  /* is_write=1 */
        if (addr == 0) return;  /* Translation failed */
    }

    if (addr >= m->memory_size) return;
    m->memory[addr] = val;
}

/* Similar changes for read16, write16, read32, write32 */
```

**Acceptance Criteria**:
- [x] MMU flag in machine structure compiles
- [x] Enable/disable functions work
- [x] MMU-aware memory access helpers created (`mmu_read8/16/32`, `mmu_write8/16/32`)
- [x] Instruction fetch uses MMU translation (when CPU linked and MMU enabled)
- [x] Data reads/writes use MMU translation (when CPU linked and MMU enabled)
- [x] Indirect addressing pointer reads use MMU translation
- [x] Debugger/disassembler still works (uses physical access when no CPU linked)
- [x] Direct access when MMU disabled
- [x] No performance degradation when MMU disabled (inline functions with simple flag check)
- [x] All existing tests pass

---

### Phase 6: Console Debug Commands ✅ COMPLETE

**Goal**: Add MMU-specific console commands for debugging

**Priority**: MEDIUM

**Estimated Time**: 0.5-1 day

**Status**: ✅ COMPLETE - All 5 MMU commands implemented and integrated

**File**: `src/debugger/commands.c`

#### 6.1 New Commands

**Add to command list** (around line 31):
```c
static const char* debugger_commands[] = {
    "help", "?", "m", "d", "dis", "disasm", "step", "s", "regs", "set", "load", "run", "stop",
    "continue", "c", "cont", "symb", "symbols", "show", "bp", "break", "breakpoint",
    "wp", "watch", "watchpoint", "profile", "backtrace", "bt", "clear-traps", "history",
    "segments", "seg", "goto", "msym", "dsym",
    "mmu", "showmmu", "showpst", "showpcb", "phyladr",  /* NEW MMU commands */
    "q", "quit", "exit", "dap"
};
```

#### 6.2 Command: `mmu [on|off]`

**Location**: Add after line 1070 (after dsym command)

```c
} else if (strcmp(tok, "mmu") == 0) {
    /* Enable/disable MMU or show status */
    char* state = strtok(NULL, " \t\r\n");
    if (!m->cpu) { printf("no cpu linked\n"); continue; }

    if (!state) {
        /* Show current state */
        int enabled = nd500_mmu_is_enabled(m->cpu);
        printf("MMU: %s\n", enabled ? "enabled" : "disabled");
        if (enabled) {
            printf("PSTP:    0x%08X\n", m->cpu->PSTP);
            printf("DITBASE: 0x%08X\n", m->cpu->DITBASE);
            printf("CAD:     %u\n", m->cpu->CAD);
            printf("CED:     %u\n", m->cpu->CED);
        }
    } else if (strcasecmp(state, "on") == 0) {
        nd500_mmu_enable(m->cpu);
        m->mmu_enabled = 1;
        printf("MMU enabled\n");
    } else if (strcasecmp(state, "off") == 0) {
        nd500_mmu_disable(m->cpu);
        m->mmu_enabled = 0;
        printf("MMU disabled\n");
    } else {
        printf("usage: mmu [on|off]\n");
    }
```

#### 6.3 Command: `showmmu`

```c
} else if (strcmp(tok, "showmmu") == 0) {
    /* Display complete MMU state */
    if (!m->cpu) { printf("no cpu linked\n"); continue; }

    printf("=== MMU STATE ===\n");
    printf("Enabled:  %s\n", nd500_mmu_is_enabled(m->cpu) ? "yes" : "no");
    printf("PSTP:     0x%08X (Physical Segment Table Pointer)\n", m->cpu->PSTP);
    printf("DITBASE:  0x%08X (Domain Information Table base)\n", m->cpu->DITBASE);
    printf("PS:       0x%08X (Process Segment pointer)\n", m->cpu->PS);
    printf("\n=== DOMAIN STATE ===\n");
    printf("CED:      %u (Current Executing Domain)\n", m->cpu->CED);
    printf("CAD:      %u (Current Alternative Domain)\n", m->cpu->CAD);

    /* Show current domain's DIT entry */
    if (m->cpu->DITBASE != 0) {
        uint32_t tos = nd500_domain_read_tos(m->cpu, m->cpu->CAD);
        uint32_t ll = nd500_domain_read_ll(m->cpu, m->cpu->CAD);
        uint32_t hl = nd500_domain_read_hl(m->cpu, m->cpu->CAD);
        uint32_t tha = nd500_domain_read_tha(m->cpu, m->cpu->CAD);
        printf("\n=== DOMAIN %u DIT ENTRY ===\n", m->cpu->CAD);
        printf("TOS:      0x%08X (Top of Stack)\n", tos);
        printf("LL:       0x%08X (Lower Limit)\n", ll);
        printf("HL:       0x%08X (Higher Limit)\n", hl);
        printf("THA:      0x%08X (Trap Handler Address)\n", tha);
    }
```

#### 6.4 Command: `showpst <psn>`

```c
} else if (strcmp(tok, "showpst") == 0) {
    /* Show PST entry details */
    char* psn_str = strtok(NULL, " \t\r\n");
    if (!psn_str) {
        printf("usage: showpst <psn>\n");
        printf("       showpst 0-8191\n");
        continue;
    }

    if (!m->cpu) { printf("no cpu linked\n"); continue; }

    int psn = (int)parse_u32(psn_str, 0);
    PhysicalSegmentTableEntry* pste = nd500_mmu_get_pst_entry(m->cpu, psn);

    if (!pste) {
        printf("Invalid PSN: %d (must be 0-8191)\n", psn);
        continue;
    }

    printf("=== PST ENTRY %d ===\n", psn);
    printf("Index Mode: %d ", pste->index_mode);
    switch (pste->index_mode) {
        case PS_AZI: printf("(PS_AZI - Direct addressed page)\n"); break;
        case PS_ASI: printf("(PS_ASI - Single-level paging)\n"); break;
        case PS_ADI: printf("(PS_ADI - Two-level paging)\n"); break;
        default: printf("(Unknown mode)\n"); break;
    }
    printf("PFN:        0x%08X (Page Frame Number)\n", pste->physical_pfn);
    printf("Phys Addr:  0x%08X (Physical base address)\n", pste->physical_pfn << PGSHIFT);
    printf("Page Size:  %d bytes\n", NBPG);
```

#### 6.5 Command: `showpcb <domain>`

```c
} else if (strcmp(tok, "showpcb") == 0) {
    /* Show PCB capabilities for domain */
    char* domain_str = strtok(NULL, " \t\r\n");
    if (!domain_str) {
        printf("usage: showpcb <domain>\n");
        printf("       showpcb 0-255\n");
        continue;
    }

    if (!m->cpu) { printf("no cpu linked\n"); continue; }

    uint8_t domain = (uint8_t)parse_u32(domain_str, 0);
    ProcessControlBlock* pcb = nd500_mmu_get_pcb(m->cpu, domain);

    if (!pcb) {
        printf("Invalid domain: %u (must be 0-255)\n", domain);
        continue;
    }

    printf("=== PCB DOMAIN %u ===\n", domain);

    /* Program Capabilities */
    printf("\n--- Program Capabilities (32 segments) ---\n");
    printf("Seg  Capability  PSN   Type      Flags\n");
    printf("---  ----------  ----  --------  -----\n");
    for (int seg = 0; seg < MAXSEG; seg++) {
        uint16_t cap = pcb->program_capabilities[seg];
        if (cap != 0) {
            int psn = cap & PC_PSN;
            const char* type = (cap & PC_TYP) ? "INDIRECT" : "DIRECT";
            const char* flags = "";
            if (cap & PC_OMC) flags = "OMC";
            printf("%2d   0x%04X      %4d  %-8s  %s\n", seg, cap, psn, type, flags);
        }
    }

    /* Data Capabilities */
    printf("\n--- Data Capabilities (32 segments) ---\n");
    printf("Seg  Capability  PSN   Flags\n");
    printf("---  ----------  ----  -----\n");
    for (int seg = 0; seg < MAXSEG; seg++) {
        uint16_t cap = pcb->data_capabilities[seg];
        if (cap != 0) {
            int psn = cap & DC_PSN;
            char flags[16] = "";
            if (cap & DC_WRP) strcat(flags, "W");
            else strcat(flags, "R");
            if (cap & DC_PAC) strcat(flags, ",USER");
            if (cap & DC_SHS) strcat(flags, ",SHARED");
            printf("%2d   0x%04X      %4d  %s\n", seg, cap, psn, flags);
        }
    }

    /* Domain call info */
    printf("\n--- Domain Call Info ---\n");
    printf("Calling Domain: %u\n", pcb->calling_domain);
    printf("Calling P:      0x%08X\n", pcb->calling_p);
    printf("Calling B:      0x%08X\n", pcb->calling_b);
```

#### 6.6 Command: `phyladr <vaddr>`

```c
} else if (strcmp(tok, "phyladr") == 0) {
    /* Translate virtual address to physical */
    char* addr_str = strtok(NULL, " \t\r\n");
    if (!addr_str) {
        printf("usage: phyladr <virtual_address>\n");
        printf("       phyladr 0x08000000\n");
        continue;
    }

    if (!m->cpu) { printf("no cpu linked\n"); continue; }

    uint32_t vaddr = parse_u32(addr_str, 0);

    if (!nd500_mmu_is_enabled(m->cpu)) {
        printf("MMU disabled - virtual address maps directly to physical\n");
        printf("Virtual:  0x%08X\n", vaddr);
        printf("Physical: 0x%08X (direct mapping)\n", vaddr);
        continue;
    }

    /* Extract address components */
    int segment = (vaddr >> 27) & 0x1F;
    int page = (vaddr >> 11) & 0xFFFF;
    int offset = vaddr & 0x7FF;

    printf("=== ADDRESS TRANSLATION ===\n");
    printf("Virtual:  0x%08X\n", vaddr);
    printf("Segment:  %d (bits 31-27)\n", segment);
    printf("Page:     %d (bits 26-11)\n", page);
    printf("Offset:   %d (bits 10-0)\n", offset);
    printf("Domain:   %u (CAD)\n", m->cpu->CAD);

    /* Perform translation */
    uint32_t paddr = nd500_mmu_phyladr(m->cpu, vaddr);

    if (paddr == 0) {
        printf("\nTranslation FAILED (protection violation or page fault)\n");
    } else {
        printf("\nPhysical: 0x%08X\n", paddr);

        /* Show capability used */
        ProcessControlBlock* pcb = nd500_mmu_get_pcb(m->cpu, m->cpu->CAD);
        if (pcb && segment < MAXSEG) {
            uint16_t pc = pcb->program_capabilities[segment];
            uint16_t dc = pcb->data_capabilities[segment];
            printf("\nCapabilities:\n");
            printf("  PC[%d]: 0x%04X", segment, pc);
            if (pc & PC_TYP) printf(" (INDIRECT)");
            else printf(" (DIRECT, PSN=%d)", pc & PC_PSN);
            printf("\n");
            printf("  DC[%d]: 0x%04X (%s%s)\n", segment, dc,
                   (dc & DC_WRP) ? "W" : "R",
                   (dc & DC_PAC) ? ", USER" : "");
        }
    }
```

#### 6.7 Update Help

**Update help command** (around line 1296):

```c
printf("\nMMU and Domain Commands:\n");
printf("  mmu [on|off]                Enable/disable MMU or show status\n");
printf("  showmmu                     Show complete MMU and domain state\n");
printf("  showpst <psn>               Show Physical Segment Table entry (0-8191)\n");
printf("  showpcb <domain>            Show Process Control Block for domain (0-255)\n");
printf("  phyladr <vaddr>             Translate virtual address to physical\n");
```

#### 6.8 Acceptance Criteria

- [x] `mmu on` enables MMU
- [x] `mmu off` disables MMU
- [x] `mmu` (no args) shows current state
- [x] `showmmu` displays complete MMU state
- [x] `showpst <psn>` shows PST entry with mode and PFN
- [x] `showpcb <domain>` shows all capabilities for domain
- [x] `showpcb <domain> <seg>` shows specific segment capabilities
- [x] `phyladr <addr>` translates addresses correctly
- [x] All commands added to command table
- [x] Help text documents all 5 commands
- [x] Commands compile and build successfully
- [x] `mmusetup` command creates demo configuration for testing
- [x] Comprehensive usage documentation created (docs/MMU_USAGE_GUIDE.md)

---

### Phase 7: Build System Updates ⏳ NOT STARTED

**Goal**: Add new source files to build system

**Priority**: HIGH

**Estimated Time**: 0.5 days

**File**: `src/cpu/CMakeLists.txt`

```cmake
add_library(cpu_objects OBJECT
    cpu.c
    cpu_instr.c
    instructions_gen.c
    nd500_instructions_gen.c
    nd500_mmu.c           # NEW
    nd500_domain.c        # NEW
)

target_include_directories(cpu_objects PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../machine
    ${CMAKE_CURRENT_SOURCE_DIR}/../ndlib
)
```

**Acceptance Criteria**:
- [ ] CMake configures without errors
- [ ] New files compile without warnings (-Wall -Wextra)
- [ ] Linker resolves all symbols
- [ ] `make clean && make` completes successfully
- [ ] Binary size increases by expected amount (~50KB)

---

### Phase 8: WebAssembly Exports ⏳ NOT STARTED

**Goal**: Export new MMU/domain functions to JavaScript

**Priority**: HIGH

**Estimated Time**: 0.5 days

**File**: `CMakeLists.txt` (root)

**Update EXPORTED_FUNCTIONS list** (around line 100):

```cmake
set_property(TARGET nd500wasm APPEND_STRING PROPERTY LINK_FLAGS
    " -s EXPORTED_FUNCTIONS=['_malloc','_free','_nd500wasm_init',
    '_nd500_dbg_mem_json','_nd500_dbg_disasm_json','_nd500_dbg_regs_json',
    '_nd500_dbg_step_js','_nd500_dbg_run_js','_nd500_dbg_stop_js',
    '_nd500_dbg_load_aout_js','_nd500_dbg_load_aout_path_js',
    '_nd500_dbg_load_pseg_path_js','_nd500_dbg_load_dseg_path_js',
    '_nd500_dbg_load_strerror_js','_nd500_dbg_get_memory_size_js',
    '_nd500_dbg_reset_memory_js','_nd500_dbg_set_pc_js',
    '_nd500_dbg_bp_add_js','_nd500_dbg_bp_del_js',
    '_nd500_dbg_bp_enable_js','_nd500_dbg_bp_disable_js',
    '_nd500_dbg_bp_list_json','_nd500_dbg_status_json',
    '_nd500_dbg_traps_json','_nd500_dbg_clear_traps_js',
    '_nd500_dbg_set_reg_js','_nd500_dbg_instr_count_js',
    '_nd500_dbg_mnemonic_js','_nd500_dbg_build_info_js',
    '_nd500_dbg_clear_symbols_js','_nd500_dbg_symbols_json',

    # NEW: MMU/Domain exports
    '_nd500_dbg_mmu_enable_js',
    '_nd500_dbg_mmu_disable_js',
    '_nd500_dbg_mmu_status_json',
    '_nd500_dbg_pst_entry_json',
    '_nd500_dbg_pcb_json',
    '_nd500_dbg_phyladr_js',
    '_nd500_dbg_domain_info_json'
    ]")
```

**New File**: `src/debugger/wasm_exports_mmu.c`

```c
#include "../cpu/nd500_mmu.h"
#include "../cpu/nd500_domain.h"
#include "../cpu/cpu_protos.h"
#include "../machine/machine_protos.h"
#include <emscripten.h>
#include <cjson/cJSON.h>

/**
 * Enable MMU
 */
EMSCRIPTEN_KEEPALIVE
void nd500_dbg_mmu_enable_js(void) {
    extern Nd500Machine* g_wasm_machine;
    if (!g_wasm_machine || !g_wasm_machine->cpu) return;
    nd500_mmu_enable(g_wasm_machine->cpu);
    g_wasm_machine->mmu_enabled = 1;
}

/**
 * Disable MMU
 */
EMSCRIPTEN_KEEPALIVE
void nd500_dbg_mmu_disable_js(void) {
    extern Nd500Machine* g_wasm_machine;
    if (!g_wasm_machine || !g_wasm_machine->cpu) return;
    nd500_mmu_disable(g_wasm_machine->cpu);
    g_wasm_machine->mmu_enabled = 0;
}

/**
 * Get MMU status as JSON
 * Returns: {"enabled": true, "pstp": "0x20000", "ditbase": "0x10000", ...}
 */
EMSCRIPTEN_KEEPALIVE
const char* nd500_dbg_mmu_status_json(void) {
    extern Nd500Machine* g_wasm_machine;
    static char json_buffer[512];

    if (!g_wasm_machine || !g_wasm_machine->cpu) {
        snprintf(json_buffer, sizeof(json_buffer), "{\"error\":\"no machine\"}");
        return json_buffer;
    }

    Nd500Cpu* cpu = g_wasm_machine->cpu;
    cJSON* root = cJSON_CreateObject();

    cJSON_AddBoolToObject(root, "enabled", nd500_mmu_is_enabled(cpu));

    char addr_buf[32];
    snprintf(addr_buf, sizeof(addr_buf), "0x%08X", cpu->PSTP);
    cJSON_AddStringToObject(root, "pstp", addr_buf);

    snprintf(addr_buf, sizeof(addr_buf), "0x%08X", cpu->DITBASE);
    cJSON_AddStringToObject(root, "ditbase", addr_buf);

    snprintf(addr_buf, sizeof(addr_buf), "0x%08X", cpu->PS);
    cJSON_AddStringToObject(root, "ps", addr_buf);

    cJSON_AddNumberToObject(root, "ced", cpu->CED);
    cJSON_AddNumberToObject(root, "cad", cpu->CAD);

    char* json_str = cJSON_PrintUnformatted(root);
    strncpy(json_buffer, json_str, sizeof(json_buffer) - 1);
    json_buffer[sizeof(json_buffer) - 1] = '\0';

    cJSON_free(json_str);
    cJSON_Delete(root);

    return json_buffer;
}

/**
 * Get PST entry as JSON
 * Returns: {"psn": 100, "mode": 0, "pfn": "0x1000", "physAddr": "0x800000"}
 */
EMSCRIPTEN_KEEPALIVE
const char* nd500_dbg_pst_entry_json(int psn) {
    extern Nd500Machine* g_wasm_machine;
    static char json_buffer[512];

    if (!g_wasm_machine || !g_wasm_machine->cpu) {
        snprintf(json_buffer, sizeof(json_buffer), "{\"error\":\"no machine\"}");
        return json_buffer;
    }

    PhysicalSegmentTableEntry* pste = nd500_mmu_get_pst_entry(g_wasm_machine->cpu, psn);
    if (!pste) {
        snprintf(json_buffer, sizeof(json_buffer), "{\"error\":\"invalid PSN\"}");
        return json_buffer;
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "psn", psn);
    cJSON_AddNumberToObject(root, "mode", pste->index_mode);

    const char* mode_name = "Unknown";
    switch (pste->index_mode) {
        case PS_AZI: mode_name = "PS_AZI (Direct)"; break;
        case PS_ASI: mode_name = "PS_ASI (Single-level)"; break;
        case PS_ADI: mode_name = "PS_ADI (Two-level)"; break;
    }
    cJSON_AddStringToObject(root, "modeName", mode_name);

    char addr_buf[32];
    snprintf(addr_buf, sizeof(addr_buf), "0x%08X", pste->physical_pfn);
    cJSON_AddStringToObject(root, "pfn", addr_buf);

    snprintf(addr_buf, sizeof(addr_buf), "0x%08X", pste->physical_pfn << PGSHIFT);
    cJSON_AddStringToObject(root, "physAddr", addr_buf);

    char* json_str = cJSON_PrintUnformatted(root);
    strncpy(json_buffer, json_str, sizeof(json_buffer) - 1);
    json_buffer[sizeof(json_buffer) - 1] = '\0';

    cJSON_free(json_str);
    cJSON_Delete(root);

    return json_buffer;
}

/**
 * Get PCB as JSON (truncated for performance - only non-zero capabilities)
 */
EMSCRIPTEN_KEEPALIVE
const char* nd500_dbg_pcb_json(int domain) {
    extern Nd500Machine* g_wasm_machine;
    static char json_buffer[4096];

    if (!g_wasm_machine || !g_wasm_machine->cpu) {
        snprintf(json_buffer, sizeof(json_buffer), "{\"error\":\"no machine\"}");
        return json_buffer;
    }

    ProcessControlBlock* pcb = nd500_mmu_get_pcb(g_wasm_machine->cpu, (uint8_t)domain);
    if (!pcb) {
        snprintf(json_buffer, sizeof(json_buffer), "{\"error\":\"invalid domain\"}");
        return json_buffer;
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "domain", domain);

    /* Program capabilities (only non-zero) */
    cJSON* pc_array = cJSON_CreateArray();
    for (int seg = 0; seg < MAXSEG; seg++) {
        if (pcb->program_capabilities[seg] != 0) {
            cJSON* pc_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(pc_obj, "seg", seg);

            char cap_buf[16];
            snprintf(cap_buf, sizeof(cap_buf), "0x%04X", pcb->program_capabilities[seg]);
            cJSON_AddStringToObject(pc_obj, "cap", cap_buf);

            cJSON_AddNumberToObject(pc_obj, "psn", pcb->program_capabilities[seg] & PC_PSN);
            cJSON_AddBoolToObject(pc_obj, "indirect", (pcb->program_capabilities[seg] & PC_TYP) != 0);
            cJSON_AddBoolToObject(pc_obj, "omc", (pcb->program_capabilities[seg] & PC_OMC) != 0);

            cJSON_AddItemToArray(pc_array, pc_obj);
        }
    }
    cJSON_AddItemToObject(root, "programCapabilities", pc_array);

    /* Data capabilities (only non-zero) */
    cJSON* dc_array = cJSON_CreateArray();
    for (int seg = 0; seg < MAXSEG; seg++) {
        if (pcb->data_capabilities[seg] != 0) {
            cJSON* dc_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(dc_obj, "seg", seg);

            char cap_buf[16];
            snprintf(cap_buf, sizeof(cap_buf), "0x%04X", pcb->data_capabilities[seg]);
            cJSON_AddStringToObject(dc_obj, "cap", cap_buf);

            cJSON_AddNumberToObject(dc_obj, "psn", pcb->data_capabilities[seg] & DC_PSN);
            cJSON_AddBoolToObject(dc_obj, "writable", (pcb->data_capabilities[seg] & DC_WRP) != 0);
            cJSON_AddBoolToObject(dc_obj, "user", (pcb->data_capabilities[seg] & DC_PAC) != 0);
            cJSON_AddBoolToObject(dc_obj, "shared", (pcb->data_capabilities[seg] & DC_SHS) != 0);

            cJSON_AddItemToArray(dc_array, dc_obj);
        }
    }
    cJSON_AddItemToObject(root, "dataCapabilities", dc_array);

    /* Domain call info */
    cJSON* call_info = cJSON_CreateObject();
    cJSON_AddNumberToObject(call_info, "callingDomain", pcb->calling_domain);

    char addr_buf[32];
    snprintf(addr_buf, sizeof(addr_buf), "0x%08X", pcb->calling_p);
    cJSON_AddStringToObject(call_info, "callingP", addr_buf);

    snprintf(addr_buf, sizeof(addr_buf), "0x%08X", pcb->calling_b);
    cJSON_AddStringToObject(call_info, "callingB", addr_buf);

    cJSON_AddItemToObject(root, "callInfo", call_info);

    char* json_str = cJSON_PrintUnformatted(root);
    strncpy(json_buffer, json_str, sizeof(json_buffer) - 1);
    json_buffer[sizeof(json_buffer) - 1] = '\0';

    cJSON_free(json_str);
    cJSON_Delete(root);

    return json_buffer;
}

/**
 * Translate virtual address to physical
 * Returns physical address or 0 on failure
 */
EMSCRIPTEN_KEEPALIVE
uint32_t nd500_dbg_phyladr_js(uint32_t vaddr) {
    extern Nd500Machine* g_wasm_machine;
    if (!g_wasm_machine || !g_wasm_machine->cpu) return 0;
    return nd500_mmu_phyladr(g_wasm_machine->cpu, vaddr);
}

/**
 * Get domain info as JSON
 * Returns: {"ced": 0, "cad": 0, "tos": "0x...", "ll": "0x...", ...}
 */
EMSCRIPTEN_KEEPALIVE
const char* nd500_dbg_domain_info_json(int domain) {
    extern Nd500Machine* g_wasm_machine;
    static char json_buffer[512];

    if (!g_wasm_machine || !g_wasm_machine->cpu) {
        snprintf(json_buffer, sizeof(json_buffer), "{\"error\":\"no machine\"}");
        return json_buffer;
    }

    if (domain < 0 || domain >= MAX_DOMAINS) {
        snprintf(json_buffer, sizeof(json_buffer), "{\"error\":\"invalid domain\"}");
        return json_buffer;
    }

    Nd500Cpu* cpu = g_wasm_machine->cpu;
    cJSON* root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "domain", domain);

    if (cpu->DITBASE != 0) {
        uint32_t tos = nd500_domain_read_tos(cpu, (uint8_t)domain);
        uint32_t ll = nd500_domain_read_ll(cpu, (uint8_t)domain);
        uint32_t hl = nd500_domain_read_hl(cpu, (uint8_t)domain);
        uint32_t tha = nd500_domain_read_tha(cpu, (uint8_t)domain);

        char addr_buf[32];
        snprintf(addr_buf, sizeof(addr_buf), "0x%08X", tos);
        cJSON_AddStringToObject(root, "tos", addr_buf);

        snprintf(addr_buf, sizeof(addr_buf), "0x%08X", ll);
        cJSON_AddStringToObject(root, "ll", addr_buf);

        snprintf(addr_buf, sizeof(addr_buf), "0x%08X", hl);
        cJSON_AddStringToObject(root, "hl", addr_buf);

        snprintf(addr_buf, sizeof(addr_buf), "0x%08X", tha);
        cJSON_AddStringToObject(root, "tha", addr_buf);
    } else {
        cJSON_AddStringToObject(root, "error", "DITBASE not initialized");
    }

    char* json_str = cJSON_PrintUnformatted(root);
    strncpy(json_buffer, json_str, sizeof(json_buffer) - 1);
    json_buffer[sizeof(json_buffer) - 1] = '\0';

    cJSON_free(json_str);
    cJSON_Delete(root);

    return json_buffer;
}
```

**Update**: `src/debugger/CMakeLists.txt`

```cmake
add_library(debugger_objects OBJECT
    debugger.c
    debug_api.c
    wasm_exports.c
    wasm_exports_mmu.c    # NEW
)
```

**Acceptance Criteria**:
- [ ] New exports compile without errors
- [ ] WASM build includes new functions
- [ ] JavaScript can call `nd500_dbg_mmu_enable_js()`
- [ ] JSON functions return valid JSON
- [ ] No memory leaks in JSON generation

---

## Web UI Design Plan

### Phase 9: Web UI - MMU Panel ⏳ NOT STARTED

**Goal**: Add MMU status and control panel to web debugger

**Priority**: HIGH

**Estimated Time**: 1 day

**Files to Modify**:
- `src/frontend/nd500wasm/web/index.html`
- `src/frontend/nd500wasm/web/debugger.js`
- `src/frontend/nd500wasm/web/style.css`

#### 9.1 UI Layout Design

**Location**: New tab/section in main debugger interface

```
┌─────────────────────────────────────────────────────────────┐
│ [CPU] [Memory] [Disasm] [Symbols] [Breakpoints] [MMU] ← NEW │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ╔══════════════════════════════════════════════════════╗   │
│  ║ MMU STATUS                                  [OFF][ON]║   │
│  ╠══════════════════════════════════════════════════════╣   │
│  ║ Enabled:      ● OFF                                  ║   │
│  ║ PSTP:         0x00020000  (Physical Segment Table)   ║   │
│  ║ DITBASE:      0x00010000  (Domain Information Table) ║   │
│  ║ PS:           0x00000000  (Process Segment)          ║   │
│  ╚══════════════════════════════════════════════════════╝   │
│                                                               │
│  ╔══════════════════════════════════════════════════════╗   │
│  ║ DOMAIN STATE                              [Refresh]  ║   │
│  ╠══════════════════════════════════════════════════════╣   │
│  ║ CED: 0  (Current Executing Domain)                   ║   │
│  ║ CAD: 0  (Current Alternative Domain)                 ║   │
│  ║                                                       ║   │
│  ║ ┌─ Domain 0 DIT Entry ────────────────────────────┐ ║   │
│  ║ │ TOS:  0x00100000  (Top of Stack)                │ ║   │
│  ║ │ LL:   0x00000000  (Lower Limit)                 │ ║   │
│  ║ │ HL:   0x00200000  (Higher Limit)                │ ║   │
│  ║ │ THA:  0x00001000  (Trap Handler Address)        │ ║   │
│  ║ └──────────────────────────────────────────────────┘ ║   │
│  ╚══════════════════════════════════════════════════════╝   │
│                                                               │
│  ╔══════════════════════════════════════════════════════╗   │
│  ║ ADDRESS TRANSLATOR                                    ║   │
│  ╠══════════════════════════════════════════════════════╣   │
│  ║ Virtual:  [0x08000000        ] [Translate]           ║   │
│  ║                                                       ║   │
│  ║ Results:                                              ║   │
│  ║   Segment:  1                                         ║   │
│  ║   Page:     0                                         ║   │
│  ║   Offset:   0                                         ║   │
│  ║   Physical: 0x00800000                                ║   │
│  ║   PSN:      100                                       ║   │
│  ╚══════════════════════════════════════════════════════╝   │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

#### 9.2 HTML Structure

**File**: `src/frontend/nd500wasm/web/index.html`

**Add new tab button** (after Breakpoints tab):

```html
<div class="tab-buttons">
    <button class="tab-button active" data-tab="cpu">CPU</button>
    <button class="tab-button" data-tab="memory">Memory</button>
    <button class="tab-button" data-tab="disasm">Disassembly</button>
    <button class="tab-button" data-tab="symbols">Symbols</button>
    <button class="tab-button" data-tab="breakpoints">Breakpoints</button>
    <button class="tab-button" data-tab="mmu">MMU</button>  <!-- NEW -->
</div>
```

**Add MMU tab content**:

```html
<!-- MMU Tab -->
<div class="tab-content" id="mmu-tab" style="display: none;">
    <!-- MMU Status Section -->
    <div class="panel">
        <div class="panel-header">
            <h3>MMU Status</h3>
            <div class="panel-controls">
                <button id="mmu-off-btn" class="btn-small">OFF</button>
                <button id="mmu-on-btn" class="btn-small btn-primary">ON</button>
            </div>
        </div>
        <div class="panel-body">
            <table class="info-table">
                <tr>
                    <td class="label">Enabled:</td>
                    <td id="mmu-enabled-status" class="value">
                        <span class="status-indicator off">● OFF</span>
                    </td>
                </tr>
                <tr>
                    <td class="label">PSTP:</td>
                    <td id="mmu-pstp" class="value mono">0x00000000</td>
                    <td class="hint">Physical Segment Table Pointer</td>
                </tr>
                <tr>
                    <td class="label">DITBASE:</td>
                    <td id="mmu-ditbase" class="value mono">0x00000000</td>
                    <td class="hint">Domain Information Table Base</td>
                </tr>
                <tr>
                    <td class="label">PS:</td>
                    <td id="mmu-ps" class="value mono">0x00000000</td>
                    <td class="hint">Process Segment Pointer</td>
                </tr>
            </table>
        </div>
    </div>

    <!-- Domain State Section -->
    <div class="panel">
        <div class="panel-header">
            <h3>Domain State</h3>
            <button id="domain-refresh-btn" class="btn-small">Refresh</button>
        </div>
        <div class="panel-body">
            <div class="domain-selector">
                <label>View Domain:</label>
                <select id="domain-select">
                    <!-- Populated dynamically 0-255 -->
                </select>
            </div>

            <table class="info-table">
                <tr>
                    <td class="label">CED:</td>
                    <td id="domain-ced" class="value mono">0</td>
                    <td class="hint">Current Executing Domain</td>
                </tr>
                <tr>
                    <td class="label">CAD:</td>
                    <td id="domain-cad" class="value mono">0</td>
                    <td class="hint">Current Alternative Domain</td>
                </tr>
            </table>

            <div class="dit-entry">
                <h4>DIT Entry for Domain <span id="dit-domain-num">0</span></h4>
                <table class="info-table">
                    <tr>
                        <td class="label">TOS:</td>
                        <td id="dit-tos" class="value mono">0x00000000</td>
                        <td class="hint">Top of Stack</td>
                    </tr>
                    <tr>
                        <td class="label">LL:</td>
                        <td id="dit-ll" class="value mono">0x00000000</td>
                        <td class="hint">Lower Limit</td>
                    </tr>
                    <tr>
                        <td class="label">HL:</td>
                        <td id="dit-hl" class="value mono">0x00000000</td>
                        <td class="hint">Higher Limit</td>
                    </tr>
                    <tr>
                        <td class="label">THA:</td>
                        <td id="dit-tha" class="value mono">0x00000000</td>
                        <td class="hint">Trap Handler Address</td>
                    </tr>
                </table>
            </div>
        </div>
    </div>

    <!-- Address Translator Section -->
    <div class="panel">
        <div class="panel-header">
            <h3>Address Translator</h3>
        </div>
        <div class="panel-body">
            <div class="translator-input">
                <label>Virtual Address:</label>
                <input type="text" id="phyladr-input" placeholder="0x08000000" class="addr-input">
                <button id="phyladr-btn" class="btn-primary">Translate</button>
            </div>

            <div id="phyladr-result" class="translator-result" style="display: none;">
                <h4>Translation Result:</h4>
                <table class="info-table">
                    <tr>
                        <td class="label">Segment:</td>
                        <td id="trans-segment" class="value mono">-</td>
                    </tr>
                    <tr>
                        <td class="label">Page:</td>
                        <td id="trans-page" class="value mono">-</td>
                    </tr>
                    <tr>
                        <td class="label">Offset:</td>
                        <td id="trans-offset" class="value mono">-</td>
                    </tr>
                    <tr class="highlight">
                        <td class="label">Physical:</td>
                        <td id="trans-physical" class="value mono">-</td>
                    </tr>
                    <tr>
                        <td class="label">PSN:</td>
                        <td id="trans-psn" class="value mono">-</td>
                    </tr>
                </table>

                <div class="capability-info">
                    <h5>Capabilities Used:</h5>
                    <div id="trans-capabilities">
                        <!-- Populated dynamically -->
                    </div>
                </div>
            </div>
        </div>
    </div>

    <!-- Quick Actions -->
    <div class="panel">
        <div class="panel-header">
            <h3>Quick Actions</h3>
        </div>
        <div class="panel-body">
            <div class="quick-actions">
                <button id="show-pst-btn" class="btn-action">
                    <span class="icon">📋</span> View PST Entries
                </button>
                <button id="show-pcb-btn" class="btn-action">
                    <span class="icon">🔐</span> View PCB Capabilities
                </button>
                <button id="domain-switch-btn" class="btn-action">
                    <span class="icon">🔄</span> Switch Domain
                </button>
            </div>
        </div>
    </div>
</div>
```

#### 9.3 CSS Styling

**File**: `src/frontend/nd500wasm/web/style.css`

```css
/* ═══════════════════════════════════════════════════════ */
/* MMU TAB STYLES */
/* ═══════════════════════════════════════════════════════ */

.panel {
    background: white;
    border-radius: 8px;
    margin-bottom: 20px;
    box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
}

.panel-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 15px 20px;
    border-bottom: 2px solid #e0e0e0;
    background: linear-gradient(to bottom, #f8f9fa, #ffffff);
}

.panel-header h3 {
    margin: 0;
    font-size: 18px;
    color: #2c3e50;
    font-weight: 600;
}

.panel-controls {
    display: flex;
    gap: 10px;
}

.panel-body {
    padding: 20px;
}

/* Info Table */
.info-table {
    width: 100%;
    border-collapse: collapse;
}

.info-table tr {
    border-bottom: 1px solid #f0f0f0;
}

.info-table tr:last-child {
    border-bottom: none;
}

.info-table td {
    padding: 10px;
    vertical-align: middle;
}

.info-table .label {
    font-weight: 600;
    color: #555;
    width: 120px;
}

.info-table .value {
    color: #2c3e50;
    font-size: 14px;
}

.info-table .value.mono {
    font-family: 'Courier New', monospace;
    color: #3498db;
}

.info-table .hint {
    color: #999;
    font-size: 12px;
    font-style: italic;
}

.info-table tr.highlight {
    background: #fff9e6;
}

.info-table tr.highlight .value {
    font-weight: bold;
    color: #e67e22;
}

/* Status Indicator */
.status-indicator {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    padding: 4px 12px;
    border-radius: 12px;
    font-weight: 600;
    font-size: 13px;
}

.status-indicator.on {
    background: #d4edda;
    color: #155724;
}

.status-indicator.off {
    background: #f8d7da;
    color: #721c24;
}

.status-indicator.on::before {
    content: '●';
    color: #28a745;
    font-size: 16px;
}

.status-indicator.off::before {
    content: '●';
    color: #dc3545;
    font-size: 16px;
}

/* Buttons */
.btn-small {
    padding: 6px 16px;
    font-size: 13px;
    border: 1px solid #ccc;
    border-radius: 4px;
    background: white;
    cursor: pointer;
    transition: all 0.2s;
}

.btn-small:hover {
    background: #f0f0f0;
    border-color: #999;
}

.btn-small.btn-primary {
    background: #3498db;
    color: white;
    border-color: #3498db;
}

.btn-small.btn-primary:hover {
    background: #2980b9;
    border-color: #2980b9;
}

.btn-action {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 12px 20px;
    border: 2px solid #e0e0e0;
    border-radius: 8px;
    background: white;
    cursor: pointer;
    transition: all 0.2s;
    font-size: 14px;
    font-weight: 500;
}

.btn-action:hover {
    border-color: #3498db;
    background: #f0f8ff;
    transform: translateY(-2px);
    box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
}

.btn-action .icon {
    font-size: 18px;
}

/* Domain Selector */
.domain-selector {
    margin-bottom: 20px;
    display: flex;
    align-items: center;
    gap: 10px;
}

.domain-selector label {
    font-weight: 600;
    color: #555;
}

.domain-selector select {
    padding: 8px 12px;
    border: 1px solid #ddd;
    border-radius: 4px;
    font-size: 14px;
    font-family: 'Courier New', monospace;
}

/* DIT Entry */
.dit-entry {
    margin-top: 20px;
    padding: 15px;
    background: #f8f9fa;
    border-radius: 6px;
    border: 1px solid #e0e0e0;
}

.dit-entry h4 {
    margin: 0 0 15px 0;
    color: #2c3e50;
    font-size: 15px;
    font-weight: 600;
}

/* Address Translator */
.translator-input {
    display: flex;
    align-items: center;
    gap: 10px;
    margin-bottom: 20px;
}

.translator-input label {
    font-weight: 600;
    color: #555;
    min-width: 120px;
}

.translator-input .addr-input {
    flex: 1;
    padding: 10px 15px;
    border: 2px solid #ddd;
    border-radius: 6px;
    font-family: 'Courier New', monospace;
    font-size: 14px;
}

.translator-input .addr-input:focus {
    outline: none;
    border-color: #3498db;
}

.translator-result {
    padding: 20px;
    background: #f0f8ff;
    border-radius: 8px;
    border: 2px solid #3498db;
}

.translator-result h4 {
    margin: 0 0 15px 0;
    color: #2c3e50;
    font-size: 16px;
    font-weight: 600;
}

.capability-info {
    margin-top: 20px;
    padding: 15px;
    background: white;
    border-radius: 6px;
    border: 1px solid #e0e0e0;
}

.capability-info h5 {
    margin: 0 0 10px 0;
    color: #2c3e50;
    font-size: 14px;
    font-weight: 600;
}

/* Quick Actions */
.quick-actions {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 15px;
}
```

#### 9.4 JavaScript Implementation

**File**: `src/frontend/nd500wasm/web/debugger.js`

**Add MMU update function**:

```javascript
// Update MMU status (call this on refresh or after step)
updateMMUStatus() {
    if (!this.module) return;

    try {
        // Get MMU status from WASM
        const statusJson = this.module.ccall('nd500_dbg_mmu_status_json', 'string', [], []);
        const status = JSON.parse(statusJson);

        // Update enabled indicator
        const enabledEl = document.getElementById('mmu-enabled-status');
        if (status.enabled) {
            enabledEl.innerHTML = '<span class="status-indicator on">● ON</span>';
        } else {
            enabledEl.innerHTML = '<span class="status-indicator off">● OFF</span>';
        }

        // Update register values
        document.getElementById('mmu-pstp').textContent = status.pstp;
        document.getElementById('mmu-ditbase').textContent = status.ditbase;
        document.getElementById('mmu-ps').textContent = status.ps;
        document.getElementById('domain-ced').textContent = status.ced;
        document.getElementById('domain-cad').textContent = status.cad;

        // Update DIT entry for current domain
        this.updateDomainInfo(status.cad);

    } catch (e) {
        console.error('Failed to update MMU status:', e);
    }
}

// Update domain information
updateDomainInfo(domain) {
    if (!this.module) return;

    try {
        const infoJson = this.module.ccall('nd500_dbg_domain_info_json', 'string', ['number'], [domain]);
        const info = JSON.parse(infoJson);

        if (info.error) {
            console.warn('Domain info error:', info.error);
            return;
        }

        document.getElementById('dit-domain-num').textContent = domain;
        document.getElementById('dit-tos').textContent = info.tos;
        document.getElementById('dit-ll').textContent = info.ll;
        document.getElementById('dit-hl').textContent = info.hl;
        document.getElementById('dit-tha').textContent = info.tha;

    } catch (e) {
        console.error('Failed to update domain info:', e);
    }
}

// Translate virtual address to physical
translateAddress(vaddr) {
    if (!this.module) return;

    try {
        // Parse address (hex or decimal)
        let addr = parseInt(vaddr, vaddr.startsWith('0x') ? 16 : 10);
        if (isNaN(addr)) {
            alert('Invalid address format');
            return;
        }

        // Call WASM function
        const paddr = this.module.ccall('nd500_dbg_phyladr_js', 'number', ['number'], [addr]);

        // Extract address components
        const segment = (addr >> 27) & 0x1F;
        const page = (addr >> 11) & 0xFFFF;
        const offset = addr & 0x7FF;

        // Show results
        document.getElementById('trans-segment').textContent = segment;
        document.getElementById('trans-page').textContent = page;
        document.getElementById('trans-offset').textContent = offset;

        if (paddr === 0) {
            document.getElementById('trans-physical').textContent = 'TRANSLATION FAILED';
            document.getElementById('trans-physical').style.color = '#dc3545';
            document.getElementById('trans-psn').textContent = '-';
        } else {
            document.getElementById('trans-physical').textContent = '0x' + paddr.toString(16).toUpperCase().padStart(8, '0');
            document.getElementById('trans-physical').style.color = '#28a745';

            // Get PSN from capability (would need additional WASM call)
            document.getElementById('trans-psn').textContent = '(see PCB)';
        }

        // Show result panel
        document.getElementById('phyladr-result').style.display = 'block';

    } catch (e) {
        console.error('Address translation failed:', e);
        alert('Translation failed: ' + e.message);
    }
}

// Initialize MMU tab event handlers
initMMUTab() {
    // Enable/Disable MMU
    document.getElementById('mmu-on-btn').addEventListener('click', () => {
        if (this.module) {
            this.module.ccall('nd500_dbg_mmu_enable_js', null, [], []);
            this.updateMMUStatus();
        }
    });

    document.getElementById('mmu-off-btn').addEventListener('click', () => {
        if (this.module) {
            this.module.ccall('nd500_dbg_mmu_disable_js', null, [], []);
            this.updateMMUStatus();
        }
    });

    // Refresh domain info
    document.getElementById('domain-refresh-btn').addEventListener('click', () => {
        this.updateMMUStatus();
    });

    // Domain selector
    const domainSelect = document.getElementById('domain-select');
    for (let i = 0; i < 256; i++) {
        const option = document.createElement('option');
        option.value = i;
        option.textContent = `Domain ${i}`;
        domainSelect.appendChild(option);
    }
    domainSelect.addEventListener('change', (e) => {
        this.updateDomainInfo(parseInt(e.target.value));
    });

    // Address translator
    document.getElementById('phyladr-btn').addEventListener('click', () => {
        const vaddrInput = document.getElementById('phyladr-input');
        this.translateAddress(vaddrInput.value);
    });

    // Enter key in address input
    document.getElementById('phyladr-input').addEventListener('keypress', (e) => {
        if (e.key === 'Enter') {
            this.translateAddress(e.target.value);
        }
    });

    // Quick action buttons
    document.getElementById('show-pst-btn').addEventListener('click', () => {
        this.openPSTModal();
    });

    document.getElementById('show-pcb-btn').addEventListener('click', () => {
        this.openPCBModal();
    });

    document.getElementById('domain-switch-btn').addEventListener('click', () => {
        const newDomain = prompt('Enter target domain number (0-255):');
        if (newDomain !== null) {
            // Would need WASM function to perform switch
            alert('Domain switching not yet implemented');
        }
    });
}

// Call this in init()
init() {
    // ... existing init code ...

    this.initMMUTab();

    // Update MMU status on every step/run
    this.originalStep = this.step;
    this.step = function() {
        this.originalStep();
        this.updateMMUStatus();
    };
}
```

**Acceptance Criteria**:
- [ ] MMU tab displays correctly
- [ ] ON/OFF buttons toggle MMU state
- [ ] Register values update in real-time
- [ ] Domain selector shows all 256 domains
- [ ] DIT entry displays for selected domain
- [ ] Address translator computes correct physical address
- [ ] Quick action buttons open modal dialogs
- [ ] UI updates after step/run operations

---

### Phase 10: Web UI - PST Inspector Modal ⏳ NOT STARTED

**Goal**: Add modal dialog to inspect PST entries

**Priority**: MEDIUM

**Estimated Time**: 0.5 days

#### 10.1 UI Design

```
╔═══════════════════════════════════════════════════════════╗
║ Physical Segment Table (PST) Inspector            [X]    ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  PSN: [100        ] [Go]                    [All] [Used] ║
║                                                           ║
║  ┌────────────────────────────────────────────────────┐  ║
║  │ PSN │ Mode      │ PFN        │ Physical Address   │  ║
║  ├─────┼───────────┼────────────┼───────────────────┤  ║
║  │ 0   │ PS_AZI    │ 0x00000000 │ 0x00000000        │  ║
║  │ 1   │ PS_AZI    │ 0x00000001 │ 0x00000800        │  ║
║  │ 10  │ PS_ASI    │ 0x00000010 │ 0x00008000        │  ║
║  │ 100 │ PS_ADI    │ 0x00000064 │ 0x00032000        │  ║
║  │ ... │           │            │                   │  ║
║  └────────────────────────────────────────────────────┘  ║
║                                                           ║
║  Showing 42 of 8192 entries                               ║
║                                                           ║
║                         [Close]                           ║
╚═══════════════════════════════════════════════════════════╝
```

#### 10.2 Implementation

**HTML** (add to `index.html`):

```html
<!-- PST Inspector Modal -->
<div id="pst-modal" class="modal" style="display: none;">
    <div class="modal-content large">
        <div class="modal-header">
            <h3>Physical Segment Table (PST) Inspector</h3>
            <button class="modal-close" onclick="debugger.closePSTModal()">&times;</button>
        </div>
        <div class="modal-body">
            <div class="modal-controls">
                <label>Jump to PSN:</label>
                <input type="number" id="pst-psn-input" min="0" max="8191" placeholder="PSN">
                <button id="pst-go-btn">Go</button>
                <div class="filter-buttons">
                    <button id="pst-filter-all" class="filter-btn active">All</button>
                    <button id="pst-filter-used" class="filter-btn">Used Only</button>
                </div>
            </div>

            <div id="pst-table-container" class="table-container">
                <table class="pst-table">
                    <thead>
                        <tr>
                            <th>PSN</th>
                            <th>Mode</th>
                            <th>PFN</th>
                            <th>Physical Address</th>
                            <th>Actions</th>
                        </tr>
                    </thead>
                    <tbody id="pst-table-body">
                        <!-- Populated dynamically -->
                    </tbody>
                </table>
            </div>

            <div class="modal-footer">
                <span id="pst-count">Showing 0 of 8192 entries</span>
            </div>
        </div>
        <div class="modal-actions">
            <button onclick="debugger.closePSTModal()">Close</button>
        </div>
    </div>
</div>
```

**JavaScript** (`debugger.js`):

```javascript
// Open PST inspector modal
openPSTModal() {
    if (!this.module) return;

    document.getElementById('pst-modal').style.display = 'flex';
    this.refreshPSTTable('all');
}

// Close PST modal
closePSTModal() {
    document.getElementById('pst-modal').style.display = 'none';
}

// Refresh PST table
refreshPSTTable(filter) {
    const tbody = document.getElementById('pst-table-body');
    tbody.innerHTML = '';

    let count = 0;
    const MAX_ENTRIES = 100;  // Limit for performance

    try {
        for (let psn = 0; psn < 8192 && count < MAX_ENTRIES; psn++) {
            const json = this.module.ccall('nd500_dbg_pst_entry_json', 'string', ['number'], [psn]);
            const entry = JSON.parse(json);

            if (entry.error) continue;

            // Filter: skip zero entries if "Used Only"
            if (filter === 'used' && entry.pfn === '0x00000000') continue;

            const row = document.createElement('tr');
            row.innerHTML = `
                <td class="mono">${psn}</td>
                <td>${entry.modeName}</td>
                <td class="mono">${entry.pfn}</td>
                <td class="mono">${entry.physAddr}</td>
                <td>
                    <button class="btn-mini" onclick="debugger.memoryAt('${entry.physAddr}')">View</button>
                </td>
            `;
            tbody.appendChild(row);
            count++;
        }

        document.getElementById('pst-count').textContent = `Showing ${count} entries`;

    } catch (e) {
        console.error('Failed to refresh PST table:', e);
    }
}

// Navigate to memory address
memoryAt(addr) {
    // Switch to Memory tab and set address
    this.switchTab('memory');
    document.getElementById('mem-addr').value = addr;
    this.updateMemoryView();
}
```

**CSS**:

```css
.modal {
    display: none;
    position: fixed;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background: rgba(0, 0, 0, 0.5);
    z-index: 1000;
    align-items: center;
    justify-content: center;
}

.modal-content.large {
    width: 90%;
    max-width: 1200px;
    max-height: 90vh;
    overflow: auto;
}

.modal-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 20px;
    border-bottom: 2px solid #e0e0e0;
}

.modal-close {
    font-size: 28px;
    font-weight: bold;
    color: #999;
    background: none;
    border: none;
    cursor: pointer;
}

.modal-controls {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 15px 20px;
    background: #f8f9fa;
    border-bottom: 1px solid #e0e0e0;
}

.filter-buttons {
    margin-left: auto;
    display: flex;
    gap: 5px;
}

.filter-btn {
    padding: 6px 12px;
    border: 1px solid #ddd;
    background: white;
    cursor: pointer;
    border-radius: 4px;
}

.filter-btn.active {
    background: #3498db;
    color: white;
    border-color: #3498db;
}

.table-container {
    max-height: 500px;
    overflow-y: auto;
}

.pst-table {
    width: 100%;
    border-collapse: collapse;
}

.pst-table thead {
    position: sticky;
    top: 0;
    background: #f8f9fa;
    z-index: 10;
}

.pst-table th {
    padding: 12px;
    text-align: left;
    font-weight: 600;
    border-bottom: 2px solid #e0e0e0;
}

.pst-table td {
    padding: 10px 12px;
    border-bottom: 1px solid #f0f0f0;
}

.pst-table .mono {
    font-family: 'Courier New', monospace;
    color: #3498db;
}

.btn-mini {
    padding: 4px 8px;
    font-size: 12px;
    border: 1px solid #ddd;
    background: white;
    cursor: pointer;
    border-radius: 3px;
}

.btn-mini:hover {
    background: #f0f0f0;
}

.modal-footer {
    padding: 15px 20px;
    background: #f8f9fa;
    border-top: 1px solid #e0e0e0;
    text-align: center;
    color: #666;
}

.modal-actions {
    padding: 20px;
    text-align: right;
    border-top: 2px solid #e0e0e0;
}
```

**Acceptance Criteria**:
- [ ] PST modal opens when "View PST Entries" clicked
- [ ] Table shows PSN, Mode, PFN, Physical Address
- [ ] "All" filter shows all 8192 entries (paginated)
- [ ] "Used Only" filter shows only non-zero entries
- [ ] "Go" button jumps to specific PSN
- [ ] "View" button opens Memory tab at physical address
- [ ] Modal is scrollable and responsive

---

### Phase 11: Web UI - PCB Capabilities Viewer ⏳ NOT STARTED

**Goal**: Add modal dialog to view PCB capabilities

**Priority**: MEDIUM

**Estimated Time**: 0.5 days

#### 11.1 UI Design

```
╔═══════════════════════════════════════════════════════════╗
║ Process Control Block (PCB) - Domain 0            [X]    ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  Domain: [0  ▼]                                          ║
║                                                           ║
║  ┌── Program Capabilities ───────────────────────────┐  ║
║  │ Seg │ Capability │ PSN  │ Type     │ Flags       │  ║
║  ├─────┼────────────┼──────┼──────────┼─────────────┤  ║
║  │ 0   │ 0x0000     │ 0    │ DIRECT   │             │  ║
║  │ 1   │ 0x0064     │ 100  │ DIRECT   │             │  ║
║  │ 31  │ 0xC000     │ 0    │ INDIRECT │ OMC         │  ║
║  └─────────────────────────────────────────────────────┘  ║
║                                                           ║
║  ┌── Data Capabilities ──────────────────────────────┐  ║
║  │ Seg │ Capability │ PSN  │ Flags                   │  ║
║  ├─────┼────────────┼──────┼─────────────────────────┤  ║
║  │ 0   │ 0x0000     │ 0    │ R                       │  ║
║  │ 1   │ 0x8064     │ 100  │ W                       │  ║
║  │ 2   │ 0xC064     │ 100  │ W, USER                 │  ║
║  └─────────────────────────────────────────────────────┘  ║
║                                                           ║
║  ┌── Domain Call Info ─────────────────────────────┐    ║
║  │ Calling Domain:  1                               │    ║
║  │ Calling P:       0x08001234                      │    ║
║  │ Calling B:       0x08002000                      │    ║
║  └───────────────────────────────────────────────────┘    ║
║                                                           ║
║                         [Close]                           ║
╚═══════════════════════════════════════════════════════════╝
```

#### 11.2 Implementation

**HTML**:

```html
<!-- PCB Viewer Modal -->
<div id="pcb-modal" class="modal" style="display: none;">
    <div class="modal-content large">
        <div class="modal-header">
            <h3>Process Control Block (PCB) - Domain <span id="pcb-domain-title">0</span></h3>
            <button class="modal-close" onclick="debugger.closePCBModal()">&times;</button>
        </div>
        <div class="modal-body">
            <div class="modal-controls">
                <label>Select Domain:</label>
                <select id="pcb-domain-select">
                    <!-- 0-255 populated dynamically -->
                </select>
            </div>

            <!-- Program Capabilities -->
            <div class="capability-section">
                <h4>Program Capabilities (32 segments)</h4>
                <div class="table-container">
                    <table class="cap-table">
                        <thead>
                            <tr>
                                <th>Seg</th>
                                <th>Capability</th>
                                <th>PSN</th>
                                <th>Type</th>
                                <th>Flags</th>
                            </tr>
                        </thead>
                        <tbody id="pcb-pc-table">
                            <!-- Populated dynamically -->
                        </tbody>
                    </table>
                </div>
            </div>

            <!-- Data Capabilities -->
            <div class="capability-section">
                <h4>Data Capabilities (32 segments)</h4>
                <div class="table-container">
                    <table class="cap-table">
                        <thead>
                            <tr>
                                <th>Seg</th>
                                <th>Capability</th>
                                <th>PSN</th>
                                <th>Flags</th>
                            </tr>
                        </thead>
                        <tbody id="pcb-dc-table">
                            <!-- Populated dynamically -->
                        </tbody>
                    </table>
                </div>
            </div>

            <!-- Domain Call Info -->
            <div class="call-info-section">
                <h4>Domain Call Information</h4>
                <table class="info-table">
                    <tr>
                        <td class="label">Calling Domain:</td>
                        <td id="pcb-calling-domain" class="value mono">-</td>
                    </tr>
                    <tr>
                        <td class="label">Calling P:</td>
                        <td id="pcb-calling-p" class="value mono">-</td>
                    </tr>
                    <tr>
                        <td class="label">Calling B:</td>
                        <td id="pcb-calling-b" class="value mono">-</td>
                    </tr>
                </table>
            </div>
        </div>
        <div class="modal-actions">
            <button onclick="debugger.closePCBModal()">Close</button>
        </div>
    </div>
</div>
```

**JavaScript**:

```javascript
// Open PCB viewer modal
openPCBModal() {
    if (!this.module) return;

    // Populate domain selector
    const select = document.getElementById('pcb-domain-select');
    select.innerHTML = '';
    for (let i = 0; i < 256; i++) {
        const option = document.createElement('option');
        option.value = i;
        option.textContent = `Domain ${i}`;
        if (i === this.cpu.CAD) option.selected = true;
        select.appendChild(option);
    }

    // Show modal
    document.getElementById('pcb-modal').style.display = 'flex';

    // Load current domain
    this.refreshPCBView(this.cpu.CAD);

    // Setup event listener
    select.addEventListener('change', (e) => {
        this.refreshPCBView(parseInt(e.target.value));
    });
}

// Close PCB modal
closePCBModal() {
    document.getElementById('pcb-modal').style.display = 'none';
}

// Refresh PCB view for domain
refreshPCBView(domain) {
    if (!this.module) return;

    try {
        const json = this.module.ccall('nd500_dbg_pcb_json', 'string', ['number'], [domain]);
        const pcb = JSON.parse(json);

        if (pcb.error) {
            alert('Error loading PCB: ' + pcb.error);
            return;
        }

        // Update title
        document.getElementById('pcb-domain-title').textContent = domain;

        // Program capabilities
        const pcTable = document.getElementById('pcb-pc-table');
        pcTable.innerHTML = '';

        if (pcb.programCapabilities.length === 0) {
            pcTable.innerHTML = '<tr><td colspan="5" class="empty">No program capabilities set</td></tr>';
        } else {
            pcb.programCapabilities.forEach(pc => {
                const row = document.createElement('tr');
                const flags = [];
                if (pc.omc) flags.push('OMC');
                row.innerHTML = `
                    <td class="mono">${pc.seg}</td>
                    <td class="mono">${pc.cap}</td>
                    <td class="mono">${pc.psn}</td>
                    <td>${pc.indirect ? 'INDIRECT' : 'DIRECT'}</td>
                    <td>${flags.join(', ')}</td>
                `;
                pcTable.appendChild(row);
            });
        }

        // Data capabilities
        const dcTable = document.getElementById('pcb-dc-table');
        dcTable.innerHTML = '';

        if (pcb.dataCapabilities.length === 0) {
            dcTable.innerHTML = '<tr><td colspan="4" class="empty">No data capabilities set</td></tr>';
        } else {
            pcb.dataCapabilities.forEach(dc => {
                const flags = [];
                if (dc.writable) flags.push('W');
                else flags.push('R');
                if (dc.user) flags.push('USER');
                if (dc.shared) flags.push('SHARED');

                const row = document.createElement('tr');
                row.innerHTML = `
                    <td class="mono">${dc.seg}</td>
                    <td class="mono">${dc.cap}</td>
                    <td class="mono">${dc.psn}</td>
                    <td>${flags.join(', ')}</td>
                `;
                dcTable.appendChild(row);
            });
        }

        // Call info
        document.getElementById('pcb-calling-domain').textContent = pcb.callInfo.callingDomain;
        document.getElementById('pcb-calling-p').textContent = pcb.callInfo.callingP;
        document.getElementById('pcb-calling-b').textContent = pcb.callInfo.callingB;

    } catch (e) {
        console.error('Failed to refresh PCB view:', e);
        alert('Error: ' + e.message);
    }
}
```

**CSS**:

```css
.capability-section {
    margin-bottom: 30px;
}

.capability-section h4 {
    margin: 0 0 15px 0;
    padding: 10px 15px;
    background: #f8f9fa;
    border-left: 4px solid #3498db;
    font-size: 15px;
    font-weight: 600;
}

.cap-table {
    width: 100%;
    border-collapse: collapse;
}

.cap-table thead {
    background: #f8f9fa;
}

.cap-table th {
    padding: 10px 12px;
    text-align: left;
    font-weight: 600;
    border-bottom: 2px solid #e0e0e0;
}

.cap-table td {
    padding: 8px 12px;
    border-bottom: 1px solid #f0f0f0;
}

.cap-table .mono {
    font-family: 'Courier New', monospace;
    color: #3498db;
}

.cap-table .empty {
    text-align: center;
    color: #999;
    font-style: italic;
    padding: 20px;
}

.call-info-section {
    padding: 20px;
    background: #f0f8ff;
    border-radius: 8px;
    border: 1px solid #d0e8ff;
}

.call-info-section h4 {
    margin: 0 0 15px 0;
    color: #2c3e50;
    font-size: 15px;
    font-weight: 600;
}
```

**Acceptance Criteria**:
- [ ] PCB modal opens when "View PCB Capabilities" clicked
- [ ] Domain selector shows all 256 domains
- [ ] Program capabilities table shows all non-zero entries
- [ ] Data capabilities table shows all non-zero entries with W/R/USER flags
- [ ] Empty tables show "No capabilities set" message
- [ ] Domain call info displays calling domain, P, and B registers
- [ ] Modal updates when domain selector changes

---

### Phase 12: Web UI - Register Display Update ⏳ NOT STARTED

**Goal**: Update register display to show new MMU registers

**Priority**: HIGH

**Estimated Time**: 0.5 days

**File**: `src/frontend/nd500wasm/web/debugger.js`

**Update `updateRegisters()` function**:

```javascript
updateRegisters() {
    if (!this.module) return;

    try {
        const json = this.module.ccall('nd500_dbg_regs_json', 'string', [], []);
        const regs = JSON.parse(json);

        // Existing registers
        document.getElementById('reg-pc').textContent = regs.pc;
        document.getElementById('reg-flags').textContent = regs.flags;

        for (let i = 0; i < 4; i++) {
            document.getElementById(`reg-i${i+1}`).textContent = regs.i[i];
            document.getElementById(`reg-a${i+1}`).textContent = regs.a[i];
            document.getElementById(`reg-e${i+1}`).textContent = regs.e[i];
        }

        document.getElementById('reg-l').textContent = regs.l;
        document.getElementById('reg-b').textContent = regs.b;
        document.getElementById('reg-r').textContent = regs.r;

        document.getElementById('reg-tos').textContent = regs.tos;
        document.getElementById('reg-ll').textContent = regs.ll;
        document.getElementById('reg-hl').textContent = regs.hl;
        document.getElementById('reg-tha').textContent = regs.tha;

        // NEW: MMU registers (add to HTML first)
        document.getElementById('reg-pstp').textContent = regs.pstp || '0x00000000';
        document.getElementById('reg-ditbase').textContent = regs.ditbase || '0x00000000';
        document.getElementById('reg-ced').textContent = regs.ced || '0';
        document.getElementById('reg-cad').textContent = regs.cad || '0';
        document.getElementById('reg-ps').textContent = regs.ps || '0x00000000';

    } catch (e) {
        console.error('Failed to update registers:', e);
    }
}
```

**Update `index.html`** (add to CPU tab register display):

```html
<!-- Existing register display -->
<div class="register-section">
    <!-- ... existing registers ... -->
</div>

<!-- NEW: MMU Registers Section -->
<div class="register-section">
    <h4>MMU/Domain Registers</h4>
    <div class="register-grid">
        <div class="register-item">
            <span class="reg-name">PSTP:</span>
            <span id="reg-pstp" class="reg-value">0x00000000</span>
            <span class="reg-hint">Physical Segment Table Pointer</span>
        </div>
        <div class="register-item">
            <span class="reg-name">DITBASE:</span>
            <span id="reg-ditbase" class="reg-value">0x00000000</span>
            <span class="reg-hint">Domain Information Table Base</span>
        </div>
        <div class="register-item">
            <span class="reg-name">PS:</span>
            <span id="reg-ps" class="reg-value">0x00000000</span>
            <span class="reg-hint">Process Segment</span>
        </div>
        <div class="register-item">
            <span class="reg-name">CED:</span>
            <span id="reg-ced" class="reg-value">0</span>
            <span class="reg-hint">Current Executing Domain</span>
        </div>
        <div class="register-item">
            <span class="reg-name">CAD:</span>
            <span id="reg-cad" class="reg-value">0</span>
            <span class="reg-hint">Current Alternative Domain</span>
        </div>
    </div>
</div>
```

**Update WASM export** (`src/debugger/debug_api.c`):

```c
const char* nd500_dbg_regs_json(void) {
    // ... existing code ...

    // Add MMU registers to JSON
    snprintf(buf, sizeof(buf), "\"pstp\":\"0x%08X\",", r.PSTP);
    strcat(json_buf, buf);

    snprintf(buf, sizeof(buf), "\"ditbase\":\"0x%08X\",", r.DITBASE);
    strcat(json_buf, buf);

    snprintf(buf, sizeof(buf), "\"ced\":%u,", r.CED);
    strcat(json_buf, buf);

    snprintf(buf, sizeof(buf), "\"cad\":%u,", r.CAD);
    strcat(json_buf, buf);

    snprintf(buf, sizeof(buf), "\"ps\":\"0x%08X\"", r.PS);
    strcat(json_buf, buf);

    // ... rest of function ...
}
```

**Acceptance Criteria**:
- [ ] CPU tab shows new MMU register section
- [ ] PSTP, DITBASE, PS, CED, CAD display correctly
- [ ] Register values update after step/run
- [ ] Tooltips explain each register's purpose

---

## Success Criteria

### Minimum Viable Product (MVP)
- ✅ MMU registers added to CPU structure
- ✅ MMU data structures defined
- ✅ Address translation works (all 3 modes)
- ✅ Console commands work (mmu, showmmu, phyladr, showpst, showpcb)
- ✅ Integration with memory bus
- ✅ Web UI shows MMU status panel
- ✅ Compiles without errors

### Full Implementation
- ✅ Domain system implemented
- ✅ Cross-domain calling works
- ✅ PCB and DIT management
- ✅ All console commands working
- ✅ All web UI panels working
- ✅ Complete test suite passes
- ✅ Documentation updated

### Optional Enhancements
- ⏸️ ND-100 bridge implementation
- ⏸️ Indirect segments with PC_OMC
- ⏸️ TLB caching for performance
- ⏸️ Domain switch animation in web UI
- ⏸️ PST/PCB export to JSON file

---

## Progress Tracking

### Sprint 1: Foundation (Estimated: 2-3 days)
- [ ] Phase 1: MMU Registers (0.5 day)
- [ ] Phase 2: MMU Data Structures (0.5 day)
- [ ] Phase 3: MMU Address Translation (1-2 days)

### Sprint 2: Integration (Estimated: 2-3 days)
- [ ] Phase 4: Domain System (1-2 days)
- [ ] Phase 5: Memory Bus Integration (0.5 day)
- [ ] Phase 7: Build System Updates (0.5 day)

### Sprint 3: Console & Testing (Estimated: 1-2 days)
- [ ] Phase 6: Console Commands (0.5-1 day)
- [ ] Phase 8: WebAssembly Exports (0.5 day)

### Sprint 4: Web UI (Estimated: 2-3 days)
- [ ] Phase 9: MMU Panel (1 day)
- [ ] Phase 10: PST Inspector Modal (0.5 day)
- [ ] Phase 11: PCB Viewer Modal (0.5 day)
- [ ] Phase 12: Register Display Update (0.5 day)

**Total Estimated Time**: 7-11 days

---

## Testing Checklist

### Unit Tests
- [ ] MMU translation with PS_AZI mode
- [ ] MMU translation with PS_ASI mode (single-level paging)
- [ ] MMU translation with PS_ADI mode (two-level paging)
- [ ] Write protection enforcement (DC_WRP flag)
- [ ] User access protection (DC_PAC flag)
- [ ] Domain switch saves/restores context correctly
- [ ] Domain boundary detection (PREVB=0, RETA=0)
- [ ] DIT read/write functions access correct offsets
- [ ] PCB read/write functions access correct offsets

### Integration Tests
- [ ] Load kernel with `load pseg kernel.pseg`
- [ ] MMU enable/disable via console
- [ ] Translate addresses with `phyladr`
- [ ] View PST entries with `showpst`
- [ ] View PCB with `showpcb`
- [ ] Web UI MMU panel displays correctly
- [ ] Web UI PST inspector shows entries
- [ ] Web UI PCB viewer shows capabilities
- [ ] Web UI address translator computes physical addresses
- [ ] Register display shows MMU registers

### End-to-End Tests
- [ ] Multi-domain kernel boots (if available)
- [ ] Cross-domain system calls work
- [ ] Page faults trigger correctly
- [ ] Protection violations trigger correctly
- [ ] No memory leaks after 1000 steps
- [ ] No crashes with invalid addresses
- [ ] Web UI remains responsive with MMU enabled

---

## Notes

- **Symbol commands already work!** The `segments`, `goto`, `msym`, and `dsym` commands are fully implemented and functional. This saves significant development time!

- **C# reference code is complete and tested**: The source C# implementation is working and well-documented. Use it as the authoritative reference for behavior.

- **Start with MMU, then domains**: The MMU address translation is independent of the domain system. Implement and test MMU first, then add domain switching.

- **Test incrementally**: After each phase, verify functionality before proceeding. Use the debugger console commands to inspect state.

- **Memory layout assumptions**: The current implementation assumes kernel at 0x08000000 (128MB offset) and user at 0xD0000000 (3328MB offset). This matches the ND-500 VMUnix memory layout.

- **Web UI should be responsive**: Avoid loading all 8192 PST entries at once. Use pagination or "Used Only" filter for performance.

- **Error handling is critical**: MMU translation can fail in many ways (invalid PSN, page faults, protection violations). Ensure all error paths are handled gracefully.

---

## References

### C# Source Files
- `/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500/Registers.cs`
- `/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500/CpuND500.MMU.cs`
- `/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500/CpuND500.Domain.cs`

### Documentation
- `/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500/spec/ND500_MMU_IMPLEMENTATION_STATUS.md`
- `/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500/spec/MMU_CONFIGURATION_EXAMPLES.md`

### Current C Implementation
- `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h`
- `/home/ronny/repos/nd500x/src/cpu/cpu.c`
- `/home/ronny/repos/nd500x/src/debugger/debugger.c`
- `/home/ronny/repos/nd500x/src/ndlib/ndlib_symbols.c`
- `/home/ronny/repos/nd500x/src/frontend/nd500wasm/web/index.html`
- `/home/ronny/repos/nd500x/src/frontend/nd500wasm/web/debugger.js`
- `/home/ronny/repos/nd500x/src/frontend/nd500wasm/web/style.css`

---

**End of Migration Plan**
