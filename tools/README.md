# ND500X Build Tools

This directory contains tools for building and processing ND-500 binaries.

## Tools

### gen-asm-map

**Purpose:** Generate assembly source line to address mappings for source-level debugging.

**Location:** `tools/gen-asm-map`

**Full Documentation:** See [gen-asm-map.md](gen-asm-map.md)

**Quick Usage:**
```bash
# Generate assembly map for kernel.s
./tools/gen-asm-map kernel.s > kernel_asm.map

# Merge with existing C map from assembler
cat kernel_asm.map >> kernel.map

# Now linker will include both C and assembly mappings
nd500-ld -m -o kernel locore.o kernel.o
```

**Integration:** See Makefile examples in individual project directories (e.g., `examples/05-c-kernel/Makefile`)

## Installation

These tools are part of the ND500X repository and require Python 3.6+.

No separate installation needed - they're used directly from the `tools/` directory.

## Contributing

When adding new tools:
1. Create executable script in `tools/`
2. Add comprehensive documentation in `tools/<toolname>.md`
3. Update this README.md
4. Add integration examples to relevant Makefiles
5. Include unit tests if applicable
