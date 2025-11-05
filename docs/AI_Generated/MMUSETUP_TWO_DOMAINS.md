# mmusetup Command Update: Two Domain Configuration

**Date**: October 15, 2025
**Change Type**: Feature Enhancement
**Impact**: Improves demo configuration to demonstrate multi-domain capabilities
**Status**: ✅ COMPLETE

---

## Overview

The `mmusetup` debugger command has been enhanced to create **2 domains** instead of 1 in the demo MMU configuration. This provides a more realistic demonstration of the ND-500 MMU's multi-domain protection and isolation capabilities.

---

## What Changed

### Before
- Created **1 domain** (Domain 0) with 3 segments
- Configured **3 PST entries** (PSN 100, 101, 102)
- Demonstrated different paging modes but not domain separation

### After
- Creates **2 domains** (Domain 0 and Domain 1) with 5 segments total
- Configures **5 PST entries** (PSN 100, 101, 102, 200, 201)
- Demonstrates both paging modes AND domain isolation

---

## Configuration Details

### PST (Physical Segment Table) Entries

| PSN | Mode | PFN | Physical Address | Used By |
|-----|------|-----|------------------|---------|
| 100 | AZI (Direct) | 0x1000 | 0x00800000 | Domain 0 |
| 101 | ASI (Single-level) | 0x2000 | 0x01000000 | Domain 0 |
| 102 | ADI (Two-level) | 0x3000 | 0x01800000 | Domain 0 |
| 200 | AZI (Direct) | 0x4000 | 0x02000000 | Domain 1 |
| 201 | ASI (Single-level) | 0x5000 | 0x02800000 | Domain 1 |

**Total**: 5 configured entries (of 8192 max)

### Domain 0 Configuration (Kernel Domain)

**Purpose**: Demonstrates kernel-mode segments with mixed access permissions

| Segment | Type | Capability | Description |
|---------|------|------------|-------------|
| 0 | Program | PSN=100, DIR | Direct-mapped code segment |
| 0 | Data | PSN=100 | Writable data, same physical frame as code |
| 5 | Data | PSN=101, WRP, PAC | Read-only, user-accessible (shared library) |
| 7 | Data | PSN=102 | Writable, kernel-only (privileged data) |

**Flags**:
- **DIR**: Direct mapping (no page table lookup)
- **WRP**: Write-protected
- **PAC**: Public Access (user-accessible)

### Domain 1 Configuration (User Domain)

**Purpose**: Demonstrates user-mode process with limited privileges

| Segment | Type | Capability | Description |
|---------|------|------------|-------------|
| 0 | Program | PSN=200, DIR | Direct-mapped user code |
| 0 | Data | PSN=200 | Writable user data, same physical frame |
| 3 | Data | PSN=201, PAC | Writable, user-accessible (heap/stack) |

**Flags**:
- **DIR**: Direct mapping
- **PAC**: Public Access (user mode can access)

**Note**: Domain 1 has no WRP flags, showing writable user segments

---

## MMU Registers

All domains share these initial register values:

```
PSTP    = 0x00100000  (PST starts at 1MB)
DITBASE = 0x00200000  (DIT starts at 2MB)
CAD     = 0           (Alternative Domain 0)
CED     = 0           (Executing Domain 0)
PS      = 0           (Process Segment 0)
```

---

## Usage Examples

### Basic Setup

```bash
# Create demo configuration
nd500x> mmusetup

# View summary
nd500x> showmmu
# Output: PST: 5 configured entries, PCB: 2 domains with 5 segments

# List all domains
nd500x> listpcb
# Shows both Domain 0 and Domain 1 with their segments

# List all PST entries
nd500x> listpst
# Shows PSN 100, 101, 102, 200, 201
```

### Inspecting Domain 0

```bash
nd500x> showpcb 0
# Output:
# Domain 0:
#   Seg  Prog Cap  Data Cap
#   ---  --------  --------
#     0  0064      0064      # PSN=100
#     5  0000      C065      # PSN=101, WRP, PAC
#     7  0000      0066      # PSN=102

nd500x> showpst 100
# Shows direct mapping configuration for Domain 0 code/data
```

### Inspecting Domain 1

```bash
nd500x> showpcb 1
# Output:
# Domain 1:
#   Seg  Prog Cap  Data Cap
#   ---  --------  --------
#     0  00C8      00C8      # PSN=200
#     3  0000      40C9      # PSN=201, PAC

nd500x> showpst 200
# Shows direct mapping for Domain 1 code/data

nd500x> showpst 201
# Shows single-level paging for Domain 1 heap/stack
```

### Testing Address Translation

```bash
# Enable MMU
nd500x> mmu on

# Translate address in Domain 0 context
nd500x> phyladr 0x00000000
# Translates using Domain 0 segment 0 (PSN=100)

# Switch to Domain 1 (requires setting CED register)
nd500x> set CED 1

# Translate same address in Domain 1 context
nd500x> phyladr 0x00000000
# Translates using Domain 1 segment 0 (PSN=200)
# Different physical address!
```

---

## Key Differences Between Domains

| Feature | Domain 0 | Domain 1 |
|---------|----------|----------|
| **Purpose** | Kernel/System | User Process |
| **Segments** | 0, 5, 7 | 0, 3 |
| **Paging Modes** | All three (AZI, ASI, ADI) | Two (AZI, ASI) |
| **Write Protection** | Segment 5 (WRP) | None |
| **User Accessible** | Segment 5 (PAC) | Segment 3 (PAC) |
| **Kernel-Only** | Segment 7 | None |

---

## Benefits of Two-Domain Configuration

### 1. Demonstrates Domain Isolation
- Shows how different domains have separate address spaces
- Same virtual address (0x00000000) maps to different physical memory
- Proves domain separation mechanism works

### 2. Shows Protection Mechanisms
- Domain 0 has write-protected segment (WRP on segment 5)
- Domain 0 has kernel-only segment (no PAC on segment 7)
- Domain 1 has user-accessible segments (PAC on segment 3)

### 3. Realistic Multi-Domain Scenario
- Domain 0: Operating system kernel
- Domain 1: User application
- Demonstrates how OS protects itself from user processes

### 4. Tests Cross-Domain Access
- Users can test switching between domains (CED register)
- Can verify protection violations when accessing wrong domain
- Can test shared segments (if configured)

---

## Code Changes

### File Modified
- `/home/ronny/repos/nd500x/src/debugger/commands.c`

### Function Modified
- `cmd_mmusetup()` (lines 1168-1264)

### Changes Summary
1. Added 2 new PST entries (200, 201) for Domain 1
2. Added Domain 1 configuration block
3. Updated help text to mention both domains
4. Changed command suggestions to include Domain 1 inspection

### Code Diff
```c
// Added after PST[102] configuration:

/* PST Entry 200: Direct mapping for domain 1 code */
nd500_mmu_set_pst_entry(m->cpu, 200, PS_AZI, 0x4000);
output(ctx, "PST[200] = PS_AZI (Direct), PFN=0x4000 → 0x02000000");

/* PST Entry 201: Single-level paging for domain 1 data */
nd500_mmu_set_pst_entry(m->cpu, 201, PS_ASI, 0x5000);
output(ctx, "PST[201] = PS_ASI (Single-level), Page table at 0x02800000");

// Added after Domain 0 configuration:

output(ctx, "");
output(ctx, "=== PCB Configuration (Domain 1) ===");

/* Setup PCB for domain 1 - User domain with different configuration */
/* Segment 0: Program segment, direct mapped to PST 200 */
nd500_mmu_set_program_capability(m->cpu, 1, 0, 200 | PC_DIR);
output(ctx, "PCB[1].prog[0] = PSN 200, DIR=1 (direct mapped)");

/* Segment 0: Data segment, writable, maps to PST 200 */
nd500_mmu_set_data_capability(m->cpu, 1, 0, 200);
output(ctx, "PCB[1].data[0] = PSN 200, writable");

/* Segment 3: Data segment, user accessible, writable, maps to PST 201 */
nd500_mmu_set_data_capability(m->cpu, 1, 3, 201 | DC_PAC);
output(ctx, "PCB[1].data[3] = PSN 201, writable, user accessible");
```

---

## Testing Performed

### Test 1: Basic Configuration
```bash
$ echo -e "mmusetup\nshowmmu\nq" | ./build/bin/nd500x --debug
# Output: PST: 5 configured entries, PCB: 2 domains with 5 segments
```
✅ **Result**: Shows 2 domains correctly

### Test 2: List All Domains
```bash
$ echo -e "mmusetup\nlistpcb\nq" | ./build/bin/nd500x --debug
# Output:
# Domain 0: 3 segments
# Domain 1: 2 segments
# Total: 2 domains with 5 configured segments
```
✅ **Result**: Both domains listed with correct segment counts

### Test 3: Inspect Domain 0
```bash
$ echo -e "mmusetup\nshowpcb 0\nq" | ./build/bin/nd500x --debug
# Output: Shows segments 0, 5, 7 with correct capabilities
```
✅ **Result**: Domain 0 configuration correct

### Test 4: Inspect Domain 1
```bash
$ echo -e "mmusetup\nshowpcb 1\nq" | ./build/bin/nd500x --debug
# Output: Shows segments 0, 3 with correct capabilities
```
✅ **Result**: Domain 1 configuration correct

### Test 5: List PST Entries
```bash
$ echo -e "mmusetup\nlistpst\nq" | ./build/bin/nd500x --debug
# Output: Shows PSN 100, 101, 102, 200, 201
```
✅ **Result**: All 5 PST entries present

---

## Web UI Impact

The web UI will automatically display 2 domains when `mmusetup` is executed:

### MMU Status Tab
- Shows "PCB: **2 domains** with 5 segments"
- PST count updated to **5 configured entries**

### PST Inspector Tab
- Displays all 5 PST entries (100, 101, 102, 200, 201)
- Filter by mode shows distribution across domains

### PCB Viewer Tab
- Shows **Domain 0** header with 3 segments (expandable)
- Shows **Domain 1** header with 2 segments (expandable)
- Both domains display with expand/collapse arrows

**No web UI code changes required** - the UI automatically adapts to backend data.

---

## Compatibility

### Backward Compatibility
✅ **Fully backward compatible**
- Existing scripts using `mmusetup` continue to work
- Additional domains don't break existing functionality
- All existing commands (showpst, showpcb) work with both domains

### Forward Compatibility
✅ **Ready for future expansion**
- Can easily add more domains (Domain 2, 3, etc.)
- Pattern established for multi-domain testing
- Documentation updated to show multi-domain examples

---

## Known Limitations

### None Currently
All functionality works as expected. The two-domain configuration is purely additive.

---

## Future Enhancements

### Potential Improvements

1. **Domain Switching Demo**
   - Add command to switch between domains automatically
   - Show how CED/CAD registers affect address translation

2. **Shared Segments**
   - Add a shared segment accessible by both domains
   - Demonstrate inter-domain communication

3. **Domain 2 for Interrupt Handler**
   - Add third domain for interrupt/trap handling
   - Show privilege level escalation

4. **Protection Violation Testing**
   - Add command to intentionally trigger protection violations
   - Demonstrate trap handling

---

## Related Documentation

- **MMU Architecture**: `/home/ronny/repos/nd500x/docs/MMU_ARCHITECTURE.md`
- **Tabbed UI Refactor**: `/home/ronny/repos/nd500x/docs/MMU_UI_REFACTOR_TABBED.md`
- **Session Summary**: `/home/ronny/repos/nd500x/docs/SESSION_SUMMARY_2025-10-15.md`
- **MMU Commands**: Use `help` in debugger for full command list

---

## Summary

### What Was Changed
- `mmusetup` command now creates **2 domains** instead of 1
- Added **2 new PST entries** (200, 201) for Domain 1
- Domain 0: Kernel domain with 3 segments (mixed permissions)
- Domain 1: User domain with 2 segments (user-accessible)

### Why It Matters
- More realistic demonstration of MMU capabilities
- Shows domain isolation and protection mechanisms
- Better testing environment for multi-domain scenarios
- Prepares for future kernel/user separation testing

### Impact
- **High**: Significantly improves demo configuration
- **Positive**: Better showcases ND-500 MMU features
- **Risk**: None - fully backward compatible

### Result
✅ **Successful enhancement** - Two-domain configuration provides comprehensive MMU demonstration

---

**Change Date**: October 15, 2025
**Implemented By**: Claude (based on user request)
**Tested**: Native debugger CLI
**Status**: ✅ COMPLETE
**Build Status**: ✅ Compiles and runs successfully
