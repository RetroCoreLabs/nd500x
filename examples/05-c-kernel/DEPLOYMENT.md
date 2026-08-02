# Demo Kernel Deployment Guide

## Overview

The demo kernel is built with complete source line mapping (C + assembly) and deployed to multiple locations for use with the ND500X debugger and web debugger.

## Build Status

**Current build:** Multi-object with merged map files
- **locore.o**: Bootstrap code with assembly mappings
- **kernel.o**: Kernel code with C + assembly mappings
- **kernel**: Final linked executable with merged, relocated map
- **kernel.zip**: Distribution package with all sources and maps

## Map File Statistics

**Final kernel.map contains:**
- C source entries (kernel.c): 374 mappings
- Assembly entries (kernel.s): 184 mappings
- Assembly entries (locore.s): 17 mappings
- **Total:** 575 complete source line mappings

**Coverage:**
- ✅ C source-level debugging
- ✅ Assembly source-level debugging
- ✅ Cross-reference between C and assembly
- ✅ All addresses correctly relocated

## Deployment Locations

### 1. Build Directory
**Location:** `examples/05-c-kernel/`

**Files:**
```
kernel           - Executable (IMAGIC format with symbols)
kernel.map       - Merged source map (C + assembly)
kernel.pseg      - Program segment (TEXT)
kernel.dseg      - Data segment (DATA + BSS)
kernel.init      - Initialization script (MMU setup)
kernel.zip       - Distribution package
```

**Usage:**
```bash
cd examples/05-c-kernel
../../build/bin/nd500x --debug
> load kernel
> symb
> m 0 100
> d 0 20
```

### 2. Web Debugger
**Location:** `src/frontend/nd500wasm/web/kernel.zip`

**Access:**
```bash
# Start web server
cd .
make wasm-serve

# Open browser to http://localhost:8000
# Click "Load File" → Select kernel.zip
# Source files and maps are automatically extracted
```

**Features:**
- Step through C source
- Step through assembly source
- View memory and registers
- Set breakpoints on C or assembly lines
- Cross-reference between sources

### 3. WASM Build Output
**Location:** `build_wasm/bin/kernel.zip` (if WASM built)

**Usage:**
```bash
make wasm
# kernel.zip is copied to build_wasm/bin/
```

## Package Contents

The `kernel.zip` file contains:

```
kernel.zip
├── kernel              # Executable binary
├── kernel.map          # Complete source map (C + assembly)
├── kernel.init         # MMU initialization script
├── kernel.dseg         # Data segment
├── kernel.pseg         # Program segment
├── locore.c            # Bootstrap C source
├── locore.s            # Bootstrap assembly
├── kernel.c            # Kernel C source
└── kernel.s            # Kernel assembly (generated from kernel.c)
```

**All source files included for complete debugging experience!**

## Verification

### Check Map File Quality

```bash
# Extract and verify map
unzip -p kernel.zip kernel.map > /tmp/test.map

# Count entries by type
grep "\.c:" /tmp/test.map | wc -l    # C source entries (should be 374)
grep "kernel\.s:" /tmp/test.map | wc -l  # kernel.s entries (should be 184)
grep "locore\.s:" /tmp/test.map | wc -l  # locore.s entries (should be 17)

# Verify addresses are relocated (not all at 0)
grep " -> 0" /tmp/test.map | wc -l   # Should be only a few (locore at base)

# Check format
head -20 /tmp/test.map  # Should show proper format with both .c and .s entries
```

### Test in Native Debugger

```bash
cd examples/05-c-kernel
../../build/bin/nd500x --debug

# In debugger:
> load kernel
> symb                  # Should show symbols
> d 0 50                # Disassemble - should show source lines
```

### Test in Web Debugger

1. Build WASM: `make wasm-serve`
2. Open http://localhost:8000
3. Load kernel.zip
4. Check "Assembly" tab - should show locore.s and kernel.s
5. Check "C" tab - should show kernel.c and locore.c
6. Step through code - should highlight source lines

## Rebuilding

To rebuild with updated sources:

```bash
cd examples/05-c-kernel

# Clean build
make clean

# Full build with packaging
make all

# Deploy to web debugger
cp kernel.zip ../../../src/frontend/nd500wasm/web/
```

**Build automatically:**
- Generates C maps from stabs
- Generates assembly maps with gen-asm-map
- Merges maps before linking
- Creates complete package

## Integration with Other Projects

To use this build system in your project:

1. **Copy tools:**
   ```bash
   cp -r tools /your/project/
   ```

2. **Update Makefile:**
   ```makefile
   GEN_ASM_MAP = path/to/tools/gen-asm-map

   %.o: %.s
       $(AS) -am $< -o $@
       $(GEN_ASM_MAP) $< >> $*.map
   ```

3. **Build and package:**
   ```makefile
   package: $(KERNEL)
       zip $(ZIP) $(KERNEL) $(KERNEL_MAP) *.c *.s *.init
   ```

4. **Deploy:**
   ```bash
   cp your_kernel.zip /path/to/nd500x/src/frontend/nd500wasm/web/
   ```

## Troubleshooting

### Map file missing entries

**Problem:** Some source lines not mapped

**Solution:**
- Verify gen-asm-map ran successfully
- Check map file has both .c and .s entries
- Rebuild: `make clean && make all`

### Wrong addresses in map

**Problem:** Addresses don't match actual code

**Solution:**
- Ensure maps are merged BEFORE linking (not after)
- Check gen-asm-map runs after assembly, before linking
- Verify linker is applying relocation correctly

### Web debugger can't load files

**Problem:** Source files not found in zip

**Solution:**
- Check zip contains all .c and .s files
- Verify kernel.map is in zip
- Rebuild package: `make clean && make all`

## Technical Details

### Build Flow

```
1. Compile:   kernel.c → [cc -g] → kernel.s (with stabs)
2. Assemble:  kernel.s → [as -am] → kernel.o + kernel.map (C entries)
3. Map Gen:   kernel.s → [gen-asm-map] → appended to kernel.map
4. Link:      locore.o + kernel.o → [ld -m] → kernel + final kernel.map
5. Split:     kernel → [splitseg] → kernel.pseg + kernel.dseg
6. Package:   [zip] → kernel.zip (all files)
```

### Map Format

```
; Final map file for kernel
; Format: filename:line (SEG) -> address (octal)  # segment_name
;
locore.s:55 (T) -> 000004  # TEXT - start      ← Assembly entry
locore.s:88 (T) -> 000006  # TEXT              ← Assembly entry
kernel.c:111 (T) -> 000032  # TEXT             ← C entry (from stabs)
kernel.c:112 (T) -> 000032  # TEXT - _kernel_main ← C entry
kernel.s:200 (T) -> 000100  # TEXT             ← Assembly entry
```

**Format details:**
- Addresses in OCTAL (ND-500 convention)
- Segment: T=TEXT, D=DATA, B=BSS
- Labels shown as comments (e.g., "# TEXT - start")
- All addresses relocated to final positions

## See Also

- **Build system:** `Makefile` - Complete build integration
- **Tool docs:** `tools/gen-asm-map.md`
- **Integration guide:** `README_MAP_GENERATION.md`
- **Web debugger:** `src/frontend/nd500wasm/web/`

## Status

✅ **Build system:** Complete and tested
✅ **Map generation:** Working (C + assembly)
✅ **Relocation:** Correct
✅ **Packaging:** Automated
✅ **Deployment:** Web + native debuggers
✅ **Documentation:** Complete

**Ready for production use!**
