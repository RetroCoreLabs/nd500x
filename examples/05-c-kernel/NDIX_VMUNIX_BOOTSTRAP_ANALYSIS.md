# NDIX-C vmunix Kernel Bootstrap Analysis

## Overview

This document analyzes the NDIX-C kernel bootstrap process from `/mnt/e/Dev/Ronny/NDIX-C/kernel/MASTER/` to understand how a production ND-500 kernel properly handles startup. This analysis will guide future integration of the full vmunix kernel into the ND500X emulator.

## Key Finding: Assembly-in-C Approach

NDIX uses **locore.c** (not locore.s) - a C file containing inline assembly directives. This solves the "ENTS-without-INIT" problem we encountered with our demo kernel.

### File: `/mnt/e/Dev/Ronny/NDIX-C/kernel/MASTER/machine/locore.c`

## Build Process

### Makefile Analysis (`/mnt/e/Dev/Ronny/NDIX-C/kernel/MASTER/GENERIC/Makefile`)

**Line 203: Link Command**
```makefile
${LD} -i -m -o vmunix -e start locore.o basic.o ${OBJS} ${C2OBJS} vers.o
```

**Critical Points:**
1. **Entry point**: `-e start` - The `start` symbol is the kernel entry point
2. **Link order**: `locore.o` is linked **FIRST** - ensures bootstrap code at beginning of text segment
3. **OMAGIC format**: `-i` flag (impure executable, no I&D separation in binary)
4. **Map generation**: `-m` flag generates symbol map

This matches our demo kernel's approach, but the key difference is in how the entry point is defined.

## Bootstrap Sequence (locore.c)

### Memory Layout Constants (Lines 81-184)

NDIX defines the complete ND-500 virtual memory map using `.set` directives:

```assembly
.set _u,          0xe8000000   # Current process U-area (8KB)
.set _Pst,        0xd8000000   # Physical Segment Table
.set _pcbtab,     0xe0000000   # Process Control Block table
.set _Utext,      0xd0000000   # User text mapping
.set _Udata,      0xf0000000   # User data mapping
.set _Ustack,     0xf8000000   # User stack mapping
.set _Sysbase,    0x18000000   # System data structures
.set _Physbase,   0x10000000   # Physical memory mapping
.set _Textbase,   0x08000000   # Kernel text segment base
.set _usrpt,      0x20000000   # User process page tables
.set _susrpt,     0x28000000   # Swapped user page tables
.set _sharebase,  0x30000000   # Shared memory with ND-100 front-end
```

These match the memory layout documented in our demo kernel's header comments.

### Kernel Stack Initialization (Lines 183-184)

```assembly
.set _Ktrap,   _u+0x736    # Trap vectors in U-area
.set _Kstack,  _u+0xf00    # Kernel stack at offset 0xf00 in U-area
```

The kernel stack is located at offset 0xf00 within the U-area (0xe8000000), giving it address 0xe8000f00.

### Entry Point: start (Lines 193-206)

**Lines 193-197: The Critical Bootstrap Code**
```assembly
	.text
	.org 4                  # Start at text address 4 (skip magic number)
	.globl	start
start:
	init	_Kstack, $20, $UPAGES*NBPG+_u-_Kstack
```

**Analysis:**

1. **`.org 4`**: Positions code at offset 4 in text segment (skips a.out magic number at offset 0)
2. **`start:` label**: Global symbol, referenced by linker `-e start`
3. **`init` instruction**: **FIRST INSTRUCTION** in the kernel
   - Base: `_Kstack` (0xe8000f00)
   - Demand: `$20` (32 bytes - minimum initial frame size)
   - Size: `$UPAGES*NBPG+_u-_Kstack` (total stack space available)
     - `UPAGES` = 8 (pages per U-area)
     - `NBPG` = 2048 (bytes per page)
     - Total = 8 × 2048 + 0xe8000000 - 0xe8000f00 = 16384 - 3840 = 12544 bytes

**This INIT instruction:**
- Initializes stack registers (B, TOS, SP)
- Creates initial stack frame
- **Prevents ENTS trap** - any subsequent function call will have proper stack context

**Lines 198-201: Cache and Main Call**
```assembly
	dctsb                   # Data cache test and set (implies dcc - data cache clear)
	pctsb                   # Program cache test and set (implies pcc - program cache clear)
	d bmove	$0,_u,$(_Kstack-_u)/8   # Initialize U-area (virgin u area)
	call	_main, $0       # Call C kernel main() function
```

**Cache operations:**
- `dctsb`: Clear and enable data cache
- `pctsb`: Clear and enable program cache
- These ensure clean cache state before kernel initialization

**U-area initialization:**
- `d bmove $0,_u,$(_Kstack-_u)/8`: Block move to clear U-area
  - Source: 0 (zeros)
  - Destination: _u (0xe8000000)
  - Count: (_Kstack - _u) / 8 = 0xf00 / 8 = 480 doublewords
  - Clears 3840 bytes (up to stack base)

**Main call:**
- `call _main, $0`: Call C `main()` function with 0 arguments
- At this point, stack is properly initialized, so `_main()` can use ENTS safely

**Lines 202-206: Return and Error Handling**
```assembly
	/* Return to CAD which is we hope /etc/init */
st1 := $0x20000000      # PRT's forever & a clean status!
ret
no_init:
	go 	no_init         # sulk here
```

If `_main()` returns (shouldn't happen in normal operation):
- Sets ST1 register to 0x20000000 (clean status for monitor)
- Returns to calling environment (CAD - Control And Display, the ND-100 front-end)
- Fall-through label `no_init:` infinite loops if something goes wrong

## Comparison with Demo Kernel

### Our Demo Kernel Problem

**File: `/home/ronny/repos/nd500x/examples/05-c-kernel/kernel.c`**

```c
void start()
{
    /* Initialize stack before any function calls */
    /* INIT instruction sets up B, TOS, and creates initial stack frame */
    asm("init _stack_area,$4096,$65536");

    /* Initialize kernel subsystems */
    init_kernel();

    /* Start scheduler */
    scheduler();

    /* Should never return */
    while(1)
        ;
}
```

**Issue:** The pcc-nd500 compiler generates:
```assembly
_start:
    ENTS    $...        # Function prologue BEFORE our inline assembly
    init    ...         # Our inline INIT - too late!
```

**Root Cause:** C functions always get compiler-generated ENTS prologue before function body.

### NDIX Solution

**File: `/mnt/e/Dev/Ronny/NDIX-C/kernel/MASTER/machine/locore.c`**

Uses **assembly-in-C** (locore.c is C preprocessor input with assembly directives):

```assembly
start:
    init    _Kstack, $20, $UPAGES*NBPG+_u-_Kstack   # FIRST instruction
    dctsb
    pctsb
    d bmove $0,_u,$(_Kstack-_u)/8
    call    _main, $0
```

**Why it works:**
- `start:` is a **label**, not a C function
- No ENTS prologue is generated
- INIT comes first, before any function call
- Subsequent `call _main` is safe because stack is initialized

## Memory Layout: NDIX vs Demo Kernel

### NDIX Memory Map (Full vmunix)

| Address       | Symbol        | Purpose                              |
|---------------|---------------|--------------------------------------|
| 0x08000000    | _Textbase     | Kernel text (loaded from PSEG)      |
| 0x10000000    | _Physbase     | Physical memory mapping region      |
| 0x18000000    | _Sysbase      | System data structures              |
| 0x20000000    | _usrpt        | User process page tables            |
| 0x28000000    | _susrpt       | Swapped user page tables            |
| 0x30000000    | _sharebase    | Shared memory with ND-100           |
| 0x40000000    | _cxbsegbase   | No-cache context blocks             |
| 0xd0000000    | _Utext        | User text mapping window            |
| 0xd8000000    | _Pst          | Physical Segment Table              |
| 0xe0000000    | _pcbtab       | Process Control Block table         |
| 0xe8000000    | _u            | Current process U-area (8KB)        |
| 0xf0000000    | _Udata        | User data mapping window            |
| 0xf8000000    | _Ustack       | User stack mapping window           |

### Demo Kernel Memory Map (Simplified)

| Address       | Symbol        | Purpose                              |
|---------------|---------------|--------------------------------------|
| 0x08000000    | _Textbase     | Kernel text (code segment)          |
| 0x10000000    | _Physbase     | Data segment base                   |
| 0x30000000    | _sharebase    | Shared memory with ND-100           |
| 0xe8000000    | _u            | Current process U-area              |

**Note:** Demo kernel uses simplified layout - full vmunix adds many more mapping regions for MMU, page tables, and multi-process support.

## Shared Memory Layout (ND-100 Interface)

Both NDIX and demo kernel define shared memory at 0x30000000:

### NDIX locore.c (Lines 113-121)
```assembly
.set _sharebase,        0x30000000
.set _xmsg_cmd_buf,     0x30000000    # Message command buffer
.set _xmsg_resp_buf,    0x30000800    # Message response buffer (2KB offset)
.set _iplrec,           0x30001000    # IPL record
.set _clockrec,         0x30001040    # Clock record
.set _cin,              0x30001044    # Console input
.set _cout,             0x30001048    # Console output
.set _cons_pkt,         0x3000104c    # Console packet
.set _sub_dev_descrip,  0x30001080    # Subordinate device descriptors
```

### Demo Kernel kernel.c (Lines 132-136)
```c
char xmsg_cmd_buf[2048];    /* Message command buffer (0x30000000) */
char xmsg_resp_buf[2048];   /* Message response buffer (0x30000800) */
int clockrec;               /* Clock record (0x30001040) */
int console_in;             /* Console input (0x30001044) */
int console_out;            /* Console output (0x30001048) */
```

**Compatibility:** Shared memory layout matches between NDIX and demo kernel.

## No-Cache Segments (NDIX Advanced Feature)

NDIX defines no-cache segments for I/O buffers (Lines 129-161):

```assembly
.set _rawbuf,      NO_CACHE_SEG_START
.set _xdata,       _rawbuf + SIZE_RAWBUF
.set _tape_pkt,    _xpara + SIZE_XPARA
.set _disk_pkt,    _mx_count + SIZE_MX_COUNT
.set _cxbsegbase,  0x40000000    # Context blocks (no cache)
.set _cxbtab,      0x40000008
```

**Purpose:** I/O buffers and context blocks must bypass cache to ensure coherency with hardware DMA.

**Demo kernel:** Does not implement no-cache segments (simplified).

## Key Takeaways for vmunix Integration

### 1. Bootstrap Pattern to Adopt

**Replace our C `start()` function with assembly-in-C approach:**

```assembly
# In bootstrap.s or locore-style file
.text
.org 4
.globl start
start:
    init    _Kstack, $20, $STACK_SIZE
    dctsb                       # Clear data cache
    pctsb                       # Clear program cache
    d bmove $0, _u, $UAREA_SIZE # Clear U-area
    call    _kernel_main, $0    # Call C kernel entry
    ret                         # Return if main exits
halt_loop:
    go halt_loop
```

**Then rename kernel.c's `start()` to `kernel_main()` (or `_main` to match NDIX).**

### 2. Toolchain Build Sequence

**NDIX approach:**
1. Compile locore.c with C preprocessor (expands constants, includes headers)
2. Result is pure assembly passed to assembler
3. Link locore.o **first** in object list
4. Entry point at `-e start`

**Required changes for demo kernel:**
1. Create locore.c (assembly-in-C) with proper `start:` label
2. Modify Makefile to compile locore.c → locore.o
3. Link: `nd500-ld -i -e start -x -m -o kernel locore.o kernel.o`
4. Split segments: `nd500-splitseg kernel`

### 3. Memory Management Integration

**NDIX requires:**
- Physical Segment Table at 0xd8000000
- Process Control Blocks at 0xe0000000
- User page tables at 0x20000000
- Multiple mapping windows (Utext, Udata, Ustack)

**ND500X emulator must:**
- Implement full MMU with PST/PCB support
- Support domain-based memory protection
- Handle capability-based addressing
- Implement AZI/ASI/ADI addressing modes (already in MMU code at `/home/ronny/repos/nd500x/src/cpu/nd500_mmu.c`)

### 4. Cache Operations

**NDIX uses cache control:**
- `dctsb` / `pctsb`: Test and set (enable) caches
- `dcc` / `pcc`: Clear caches
- Comments indicate cache was added late, many conservative flush operations

**ND500X emulator:**
- Current implementation doesn't model caches
- For vmunix integration, may need to add cache simulation or stub these operations

### 5. Interrupt/Trap Handling

**NDIX trap vectors** (Line 183):
```assembly
.set _Ktrap, _u+0x736    # Trap vectors at offset 0x736 in U-area
```

**Demo kernel:** No trap vector setup in current code.

**Future work:** Must implement trap vector initialization matching NDIX layout.

## File Structure Comparison

### NDIX vmunix Build

```
kernel/MASTER/
├── GENERIC/
│   ├── Makefile          # Build orchestration
│   └── vers.c            # Version string (auto-generated)
├── machine/
│   ├── locore.c          # Bootstrap + low-level assembly (FIRST linked)
│   ├── basic.c           # Basic kernel functions (SECOND linked)
│   ├── machdep.c         # Machine-dependent C code
│   ├── param.h           # Kernel parameters
│   ├── trap.h            # Trap definitions
│   └── ...
├── sys/                  # High-level kernel (system calls, scheduler, etc.)
└── ...

Link order: locore.o basic.o <sys/*.o> vers.o → vmunix
```

### Demo Kernel Build

```
examples/05-c-kernel/
├── Makefile              # Build orchestration
├── kernel.c              # All-in-one kernel (data structures + code)
├── bootstrap.s           # Failed attempt at separate bootstrap
└── kernel.zip            # Distribution package

Build: kernel.c → kernel.s → kernel.o → kernel → kernel.{pseg,dseg}
```

**Difference:** NDIX separates concerns (bootstrap vs. kernel logic), demo kernel is monolithic.

## Required Changes for Full vmunix Integration

### Phase 1: Bootstrap Fix (Immediate)

1. **Create locore.c** in demo kernel directory:
   - Define `start:` label with INIT as first instruction
   - Clear caches (dctsb, pctsb)
   - Clear U-area
   - Call C kernel entry point

2. **Modify kernel.c**:
   - Rename `start()` → `kernel_main()`
   - Remove inline `asm("init ...")` (now in locore.c)

3. **Update Makefile**:
   - Compile locore.c → locore.o
   - Link: `locore.o kernel.o` (locore first)
   - Entry point: `-e start`

### Phase 2: Memory Layout Alignment

1. **Add NDIX memory constants** to kernel:
   - Define all virtual address regions (see table above)
   - Update data structure placements to match NDIX layout

2. **Implement U-area structure**:
   - Trap vectors at offset 0x736
   - Kernel stack at offset 0xf00
   - Process context save area

### Phase 3: Full vmunix Source Integration

1. **Import NDIX source tree**:
   - Copy `/mnt/e/Dev/Ronny/NDIX-C/kernel/MASTER/` tree
   - Adapt Makefile for pcc-nd500 toolchain paths
   - Build with existing NDIX locore.c

2. **ND500X emulator enhancements**:
   - Full MMU support (PST, PCB, capabilities) - already partially implemented
   - Domain switching
   - Trap/interrupt dispatch
   - ND-100 interface emulation (shared memory region)

3. **Testing**:
   - Boot vmunix in ND500X debugger
   - Single-step through bootstrap
   - Verify main() entry
   - Test process creation (/etc/init launch)

## Technical Notes

### C Preprocessor with Assembly

NDIX locore.c uses C preprocessor for assembly:
- `#include` pulls in header definitions
- `#define` macros expand in assembly context
- `.set` directives create assembly-time constants
- File extension `.c` triggers C preprocessor, output goes to assembler

**Compilation:**
```bash
# C preprocessor → assembly → object
nd500-cc -E locore.c | nd500-as -o locore.o
# Or let compiler driver handle it:
nd500-cc -c locore.c
```

### Symbol Visibility

**NDIX uses `.globl` extensively:**
```assembly
.globl _main, _u, _Ktrap, _Kstack, _dumpsys
.globl _Pst, _pcbtab, _private
```

All kernel entry points and data structures are global symbols for linking.

**Demo kernel:** Relies on C compiler to generate globals, less explicit control.

### Stack Size Calculation

**NDIX stack size** (line 197):
```
Size = UPAGES * NBPG + _u - _Kstack
     = 8 * 2048 + 0xe8000000 - 0xe8000f00
     = 16384 - 3840
     = 12544 bytes (12.25 KB)
```

**Demo kernel stack size:**
```c
char stack_area[65536];  // 64KB - much larger than NDIX
```

NDIX uses smaller kernel stack because:
1. Kernel stack only for current process
2. Each user process has separate stack in U-area
3. Nested interrupts/traps unlikely to exceed 12KB depth

## Conclusion

The NDIX vmunix kernel bootstrap demonstrates the **correct** pattern for ND-500 kernel initialization:

1. **Assembly-in-C** (`locore.c`) avoids compiler-generated ENTS prologue
2. **INIT as first instruction** ensures stack context before any calls
3. **Link locore.o first** positions bootstrap at text segment start
4. **Entry point at `start:` label** (not C function)

Our demo kernel encountered ENTS-without-INIT trap because:
- Used C function `start()` → compiler adds ENTS prologue
- Inline `asm("init ...")` comes AFTER prologue → too late
- Attempted assembly bootstrap failed due to pcc-nd500 linker limitations

**For future vmunix integration:**
- Adopt NDIX locore.c pattern (already working in production kernel)
- Keep existing NDIX locore.c unchanged
- Integrate full NDIX source tree with minimal modifications
- ND500X emulator already has MMU foundation - extend for full vmunix support

**Bootstrap problem: SOLVED** (for future builds - use NDIX pattern).

## References

- NDIX-C Release 3 kernel source: `/mnt/e/Dev/Ronny/NDIX-C/kernel/MASTER/`
- Demo kernel: `/home/ronny/repos/nd500x/examples/05-c-kernel/`
- ND-500 Reference Manual: Page 230 (ENTS instruction specification)
- Bootstrap issue documentation: `KERNEL_STACK_INIT.md`, `BOOTSTRAP_EXPLAINED.md`
