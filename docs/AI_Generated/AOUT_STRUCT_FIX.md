# ND-500 a.out Header Structure Fix

## Issue

The `nd500_exec` structure in `src/ndlib/ndlib_aout.c` had an incorrect definition that worked by accident but was semantically wrong.

## Problem

### Original (Incorrect) Definition
```c
struct nd500_exec {
    unsigned int   a_magic;    // WRONG: 4 bytes (platform-dependent)
    unsigned int   a_text;
    unsigned int   a_data;
    unsigned int   a_bss;
    unsigned int   a_syms;
    unsigned int   a_entry;
    unsigned int   a_trsize;
    unsigned int   a_drsize;
};
```

**Issues:**
1. Used `unsigned int` which is platform-dependent (could be 4 or 8 bytes)
2. Magic field should be 2 bytes + 2 bytes padding, not 4 bytes
3. No explicit packing attribute
4. No documentation of actual binary layout

**Why it worked:** On LP64 systems, `unsigned int` happens to be 4 bytes, and the first 4 bytes of the header contain magic (2 bytes) + padding (2 bytes), so reading worked accidentally.

## Solution

### Corrected Definition
```c
#include <stdint.h>

/* Minimal ND-500 a.out header and symbol structures based on ragge/pcc-nd500
 *
 * IMPORTANT: This matches the actual binary format from ragge/pcc-nd500 toolchain.
 * Total size: 32 bytes (0x20)
 *
 * Magic field is 2 bytes + 2 bytes padding to maintain 4-byte alignment.
 * All size fields are 4 bytes (uint32_t).
 *
 * See: ../ragge/pcc-nd500/src/include/nd500/a.out.h
 * See: ../ragge/pcc-nd500/docs/toolchain/OBJECT_VS_EXECUTABLE_DETECTION.md
 */
struct nd500_exec {
    uint16_t   a_magic;     /* Magic number (2 bytes) - offset 0 */
    uint16_t   a_pad;       /* Padding (2 bytes) - offset 2 */
    uint32_t   a_text;      /* Size of text segment (4 bytes) - offset 4 */
    uint32_t   a_data;      /* Size of initialized data (4 bytes) - offset 8 */
    uint32_t   a_bss;       /* Size of uninitialized data (4 bytes) - offset 12 */
    uint32_t   a_syms;      /* Size of symbol table (4 bytes) - offset 16 */
    uint32_t   a_entry;     /* Entry point (4 bytes) - offset 20 */
    uint32_t   a_trsize;    /* Text relocation size (4 bytes) - offset 24 */
    uint32_t   a_drsize;    /* Data relocation size (4 bytes) - offset 28 */
} __attribute__((packed));
```

**Improvements:**
1. ✅ Uses fixed-size types (`uint16_t`, `uint32_t`) - platform-independent
2. ✅ Correctly represents magic as 2 bytes + 2 bytes padding
3. ✅ Added `__attribute__((packed))` for exact binary layout
4. ✅ Documented field offsets and total size
5. ✅ Added cross-references to toolchain documentation

## Verification

Tested with `../ragge/pcc-nd500/examples/05-c-kernel/kernel`:

```
Header size: 32 bytes
Magic:       0x0109 (411 octal)
Padding:     0x0000
Text size:   736 bytes (0x2E0)
Data size:   72 bytes (0x48)
BSS size:    13072 bytes (0x3310)
Syms size:   648 bytes (0x288)
Entry:       0x00000000
Text reloc:  0 bytes
Data reloc:  0 bytes
```

Matches toolchain's `nd500-dump` output exactly ✅

## Related Fixes

This fix is part of a larger effort to standardize a.out handling across the toolchain:

1. **nd500-dump** - Fixed object vs executable detection (uses UNDF|EXT symbols)
2. **Documentation** - Created `../ragge/pcc-nd500/docs/toolchain/OBJECT_VS_EXECUTABLE_DETECTION.md`
3. **nd500x emulator** - Fixed struct definition (this fix) + detection logic

## References

- Toolchain header: `../ragge/pcc-nd500/src/include/nd500/a.out.h`
- Detection docs: `../ragge/pcc-nd500/docs/toolchain/OBJECT_VS_EXECUTABLE_DETECTION.md`
- Emulator file: `src/ndlib/ndlib_aout.c`

## Date

Fixed: 2025-10-15
