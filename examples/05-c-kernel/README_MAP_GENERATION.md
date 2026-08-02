# Assembly Map Generation Integration

## Overview

This example demonstrates the complete integration of `gen-asm-map` for generating assembly source line mappings that are merged with C source mappings for comprehensive source-level debugging.

## Build Process

The Makefile has been enhanced to automatically generate and merge assembly line mappings:

```makefile
# Step 2a: Assemble locore.s with merged maps
$(LOCORE_OBJ): $(LOCORE_ASM)
    # 1. Assemble (creates locore.map with C entries)
    $(AS) -am $(LOCORE_ASM) -o $(LOCORE_OBJ)

    # 2. Generate assembly line mappings
    $(GEN_ASM_MAP) $(LOCORE_ASM) >> $(LOCORE_MAP)

    # Result: locore.map has BOTH C and assembly entries

# Step 2b: Assemble kernel.s with merged maps
$(KERNEL_OBJ): $(ASM_SRC)
    # 1. Assemble (creates kernel.map with C entries from stabs)
    $(AS) -am $(ASM_SRC) -o $(KERNEL_OBJ)

    # 2. Generate and merge assembly line mappings
    $(GEN_ASM_MAP) $(ASM_SRC) >> $(KERNEL_OBJ_MAP)

    # Result: kernel.map has BOTH C and assembly entries

# Step 3: Link (linker merges maps with relocation)
$(KERNEL): $(LOCORE_OBJ) $(KERNEL_OBJ)
    $(LD) -m -o $(KERNEL) $(LOCORE_OBJ) $(KERNEL_OBJ)

    # Result: Final kernel.map has ALL entries with correct relocation
```

## Resulting Map File

The final `kernel.map` contains:

1. **C source mappings** (from stabs debug info)
   - kernel.c:111 (T) -> 000032  # TEXT
   - kernel.c:112 (T) -> 000032  # TEXT - _kernel_main

2. **Assembly source mappings** (from gen-asm-map)
   - kernel.s:1 (T) -> 000032  # TEXT
   - kernel.s:200 (T) -> 000132  # TEXT
   - locore.s:55 (T) -> 000004  # TEXT - start

All addresses are correctly relocated by the linker!

## Usage for Source-Level Debugging

The merged map file enables:

- **Step through C source**: View C code during debugging
- **Step through assembly**: View generated assembly code
- **Cross-reference**: See which assembly lines correspond to C lines
- **Full visibility**: Debug at any level (C, assembly, or both)

## Tools Used

- **nd500-as**: Assembler (generates C mappings from stabs)
- **gen-asm-map**: Tool to generate assembly line mappings
- **nd500-ld**: Linker (merges maps with relocation)

## Integration for Other Projects

To integrate this into your project:

1. **Copy gen-asm-map** to your tools directory (or reference from nd500x repo)

2. **Update Makefile**:
   ```makefile
   GEN_ASM_MAP = path/to/gen-asm-map

   %.o: %.s
       $(AS) -am $< -o $@
       $(GEN_ASM_MAP) $< >> $*.map
   ```

3. **Build**: Run make - assembly mappings are automatically generated

4. **Verify**: Check final map has both `.c:` and `.s:` entries
   ```bash
   grep "\.s:" final.map | wc -l  # Should be > 0
   grep "\.c:" final.map | wc -l  # Should be > 0
   ```

## Documentation

- **Tool documentation**: `tools/gen-asm-map.md`
- **Implementation**: `tools/gen-asm-map`
- **Examples**: This Makefile

## Benefits

✅ **ZERO toolchain modifications** (no assembler/linker changes)
✅ **Automatic**: Integrated into build process
✅ **Correct relocation**: Linker handles address fixup
✅ **Maintainable**: Simple Python script
✅ **Fast**: < 0.1s per file

## See Also

- `tools/README.md` - Tools overview
- `tools/gen-asm-map.md` - Complete documentation
