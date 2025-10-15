# MMU Testing Guide

This guide explains how to test the ND-500 MMU (Memory Management Unit) implementation in both the native debugger and web browser.

## Quick Start

### Native Debugger (nd500x)

```bash
./build/bin/nd500x --debug

# At the prompt, run:
> mmusetup          # Create demo MMU configuration
> showmmu           # View MMU status
> showpst 100       # View PST entry 100
> showpcb 0         # View domain 0 capabilities
> mmu on            # Enable MMU
> phyladr 0x00000000  # Translate virtual address
```

### Web Browser

1. **Start a local web server** (required for WebAssembly):
   ```bash
   cd src/frontend/nd500wasm/web
   python3 -m http.server 8000
   ```

2. **Open in browser**: http://localhost:8000

3. **Click "💻 Console" button** to open the command console

4. **Run MMU commands**:
   ```
   > mmusetup
   > showmmu
   > listpst
   > listpcb
   > showpst 100
   > phyladr 0x00000000
   ```

## Available MMU Commands

### 1. `mmusetup` - Setup Demo Configuration

Creates a test MMU configuration with three PST entries demonstrating all paging modes:

```
> mmusetup
```

**What it does:**
- Creates PST entry 100 (PS_AZI - direct mapping)
- Creates PST entry 101 (PS_ASI - single-level paging)
- Creates PST entry 102 (PS_ADI - two-level paging)
- Sets up PCB for domain 0 with 4 segment mappings
- Initializes all MMU registers

**Output:**
```
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
```

---

### 2. `mmu` - Enable/Disable MMU

Control MMU address translation:

```
> mmu          # Show current status
> mmu on       # Enable MMU
> mmu off      # Disable MMU
```

---

### 3. `showmmu` - View MMU Status

Display complete MMU state:

```
> showmmu
```

**Output:**
```
=== MMU STATUS ===
Machine MMU flag: disabled
CPU MMU state:    disabled

MMU Registers:
  PSTP    = 0x00100000  (Physical Segment Table Pointer)
  DITBASE = 0x00200000  (Domain Information Table Base)
  CED     = 0x00000000  (Current Executing Domain)
  CAD     = 0x00000000  (Current Alternative Domain)
  PS      = 0x00000000  (Process Segment)

PST: 3 configured entries (of 8192 max)
PCB: 1 domains with 3 segments (of 256 domains max)
Page size: 2048 bytes

Use 'listpst' to see all configured PST entries
Use 'listpcb' to see all configured domains and segments
```

---

### 4. `listpst` - List Configured PST Entries

Display all configured (non-zero) PST entries:

```
> listpst
```

**Output:**
```
=== Configured PST Entries ===
PSN   Mode  PFN     Physical Address
----  ----  ------  ----------------
 100  AZI   0x1000  0x00800000
 101  ASI   0x2000  0x01000000
 102  ADI   0x3000  0x01800000

Total: 3 configured entries (of 8192 max)
```

**When empty:**
```
=== Configured PST Entries ===
PSN   Mode  PFN     Physical Address
----  ----  ------  ----------------
(no configured entries)

Use 'mmusetup' to create a demo configuration
```

**Use Case**: Quickly see which PST entries are actually in use without scanning through all 8192 entries.

---

### 5. `showpst` - View PST Entry

Display Physical Segment Table entry details:

```
> showpst 100      # View PST entry 100
> showpst 101      # View PST entry 101
```

**Output:**
```
=== PST Entry 100 ===
Index Mode:    0 (PS_AZI - Direct)
Physical PFN:  0x1000 (Physical address: 0x00800000)
```

**Paging Modes:**
- **PS_AZI (0)**: Direct mapping - no page table lookup
- **PS_ASI (1)**: Single-level paging - one page table lookup
- **PS_ADI (2)**: Two-level paging - two page table lookups

---

### 6. `listpcb` - List Configured PCB Domains

Display all domains with configured segments:

```
> listpcb
```

**Output:**
```
=== Configured PCB Domains ===

Domain 0:
  Seg  Prog   Data   Description
  ---  ----   ----   -----------
    0  0064   0064   P:PSN=100 D:PSN=100
    5  0000   C065   D:PSN=101,WRP,PAC
    7  0000   0066   D:PSN=102

Total: 1 domains with 3 configured segments
(Maximum: 256 domains × 32 segments)
```

**When empty:**
```
=== Configured PCB Domains ===
(no configured domains)

Use 'mmusetup' to create a demo configuration
```

**Description Format:**
- `P:PSN=X` - Program capability points to PSN X
- `D:PSN=X` - Data capability points to PSN X
- `,DIR` - Direct mapped (program)
- `,WRP` - Write protected (data)
- `,PAC` - Public access/user accessible (data)

**Use Case**: Quickly see which domains are configured and what segments they use, without checking all 256 domains × 32 segments = 8192 possible entries.

---

### 7. `showpcb` - View PCB Capabilities

Display Process Control Block (domain capabilities) in detail:

```
> showpcb 0           # View all segments in domain 0
> showpcb 0 5         # View only segment 5 in domain 0
```

**Output:**
```
=== PCB Domain 0 ===

Program Capabilities:
  Seg 0:  PSN=100  DIR=1 (direct)

Data Capabilities:
  Seg 0:  PSN=100  WRP=0 PAC=0 (writable, kernel)
  Seg 5:  PSN=101  WRP=1 PAC=1 (read-only, user)
  Seg 7:  PSN=102  WRP=0 PAC=0 (writable, kernel)
```

**Capability Flags:**
- **DIR**: Direct mapping (program segments)
- **WRP**: Write protected (data segments)
- **PAC**: Public access - user accessible (data segments)

---

### 8. `phyladr` - Translate Virtual Address

Translate virtual address to physical address through MMU:

```
> phyladr 0x00000000     # Translate with defaults (read, data)
> phyladr 0x00000000 0 1 # Translate (read=0, instruction=1)
> phyladr 0x00000000 1 0 # Translate (write=1, data=0)
```

**Parameters:**
1. Virtual address (32-bit hex)
2. Read/Write flag (0=read, 1=write, default=0)
3. Instruction/Data flag (0=data, 1=instruction, default=0)

**Output (MMU disabled):**
```
MMU is disabled - addresses are already physical
```

**Output (MMU enabled):**
```
=== Address Translation ===
Virtual Address:  0x00000000

Address Breakdown:
  Segment:  0  (bits 31-27)
  Page:     0  (bits 26-11)
  Offset:   0  (bits 10-0)

Capability Lookup:
  Domain:   0  (CED)
  Space:    Data
  PCB[0].data[0] = 0x0064 (PSN=100, WRP=0, PAC=0)

PST Lookup:
  PSN 100 → Mode: PS_AZI (Direct), PFN: 0x1000

Physical Address: 0x00800000
```

---

## Testing Scenarios

### Scenario 1: Basic Setup and Status

Test that MMU structures initialize correctly:

```bash
# Setup
> mmusetup

# Verify status
> showmmu

# Expected: PST and PCB configured, MMU disabled
```

### Scenario 2: PST Entry Inspection

Verify all three paging modes are configured:

```bash
> showpst 100    # Should show PS_AZI (Direct)
> showpst 101    # Should show PS_ASI (Single-level)
> showpst 102    # Should show PS_ADI (Two-level)
```

### Scenario 3: PCB Capability Inspection

Verify domain 0 has correct segment mappings:

```bash
> showpcb 0

# Expected:
# - Program segment 0 mapped to PSN 100 (direct)
# - Data segment 0 mapped to PSN 100 (writable, kernel)
# - Data segment 5 mapped to PSN 101 (read-only, user)
# - Data segment 7 mapped to PSN 102 (writable, kernel)
```

### Scenario 4: Address Translation (MMU Disabled)

Test translation when MMU is disabled:

```bash
> mmu off
> phyladr 0x00000000

# Expected: "MMU is disabled - addresses are already physical"
```

### Scenario 5: Address Translation (MMU Enabled)

Test full translation with MMU enabled:

```bash
> mmu on
> phyladr 0x00000000

# Expected: Complete translation breakdown showing:
# - Virtual address components (segment, page, offset)
# - Capability lookup from PCB
# - PST entry lookup
# - Final physical address
```

### Scenario 6: Manual Configuration

Configure MMU from scratch without using `mmusetup`:

```bash
# View comprehensive setup guide
> help mmu

# See docs/MMU_USAGE_GUIDE.md for detailed manual configuration steps
```

---

## Troubleshooting

### "MMU is disabled"

**Problem**: `phyladr` returns "MMU is disabled - addresses are already physical"

**Solution**: Enable MMU with `mmu on`

### "No CPU linked" Error

**Problem**: Commands fail with "no cpu linked"

**Solution**: Ensure emulator is initialized. In web version, reload page.

### "PSN out of range"

**Problem**: `showpst <N>` fails with range error

**Solution**: PSN must be 0-8191. Use values from `mmusetup` (100-102) or configure custom entries.

### Empty PST/PCB Entries

**Problem**: `showpst` or `showpcb` shows all zeros

**Solution**: MMU structures start empty. Use `mmusetup` to create demo config, or manually configure (see MMU_USAGE_GUIDE.md).

---

## Web Browser Testing Notes

### Recent Fix (2025-01-15)

The WebAssembly version previously returned generic "Error executing command" for all failures. This has been fixed - actual error messages are now returned.

### Console Features

The web console supports:
- **↑/↓ arrows**: Command history
- **Tab**: Command autocomplete (partial)
- **help**: List all commands
- **Clear button**: Clear console output

### Register Display

MMU registers are now exported in the register JSON and should appear in the web UI register panel:
- PSTP (Physical Segment Table Pointer)
- DITBASE (Domain Information Table Base)
- CED (Current Executing Domain)
- CAD (Current Alternative Domain)
- PS (Process Segment)

---

## Additional Resources

- **MMU Usage Guide**: `docs/MMU_USAGE_GUIDE.md` - Comprehensive manual configuration guide
- **Migration Plan**: `docs/MMU_DOMAIN_MIGRATE_PLAN.md` - Implementation status and design
- **ND-500 Reference**: `docs/reference/nd500/` - Official architecture documentation

---

## Implementation Status

### ✅ Completed (as of 2025-01-15)

- MMU registers (PSTP, DITBASE, CED, CAD, PS)
- MMU data structures (PST, PCB, PTE)
- Three-level address translation (Virtual → Capability → PST → Physical)
- All three paging modes (PS_AZI, PS_ASI, PS_ADI)
- Memory bus integration (MMU-aware read/write)
- 6 console debug commands (mmu, showmmu, showpst, showpcb, phyladr, mmusetup)
- WebAssembly exports (commands work in browser)

### ⏳ Pending

- Domain system (cross-domain calling)
- Web UI MMU visualization panel
- PST inspector modal
- PCB capabilities viewer
- Address translator widget

---

**Last Updated**: 2025-01-15 (Phase 8 Complete)
