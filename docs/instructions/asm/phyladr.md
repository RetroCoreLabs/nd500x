# PHYLADR - Get Physical Address

## Overview

**Mnemonic:** `phyladr`
**Function:** Translate logical address to physical address
**Class:** SYSTEM
**Privilege:** supervisor
**Format:** `tn PHYLADR <operand>`

---

## Description

Translates a logical (virtual) address to its corresponding physical ND-500/ND-5000 address and loads the result into a specified index register. This privileged instruction is part of the '87 architecture extension and is used by operating systems and memory management routines to obtain physical addresses for DMA operations, debugging, and low-level memory management.

**Operation:**
```
translate(address_of_operand) → In
```

**Key Characteristics:**
- Supervisor-only address translation ('87 extension)
- Converts logical (virtual) to physical address
- Result loaded into index register (I1-I4)
- Essential for DMA controller programming
- Performs MMU translation internally
- No memory access (only address translation)
- 3-5 cycles execution time
- 4 register variants (I1-I4)
- Common in low-level memory management

The instruction takes the logical address of the operand, performs MMU translation, and stores the resulting physical address in the specified index register (I1-I4).

**Common Use Cases:**
- DMA controller programming
- Physical memory inspection for debugging
- Memory-mapped I/O setup
- Page table management

**Operands:** 1
**Variants:** 4 opcodes

---

## Variants

| Variant | Opcode | Register | Assembly |
|---------|--------|----------|----------|
| 1/4 | 0xFFF0 | I1 | W1 PHYLADR |
| 2/4 | 0xFFF1 | I2 | W2 PHYLADR |
| 3/4 | 0xFFF2 | I3 | W3 PHYLADR |
| 4/4 | 0xFFF3 | I4 | W4 PHYLADR |

---

## Operands

**Operand 1** (Source, Write):
- **Addressing modes**: LOCAL, RECORD, PRE_INDEXED, ABSOLUTE
- **Data type**: Word
- **Role**: Logical address to translate

**Result**: Physical address loaded into In

---

## Trap Conditions
- **Addressing traps**: Invalid logical address
- **Privilege violation**: If executed in user mode

---

## Data Status Bits
- **Z,S,C,V**: Unaffected

---

## Examples

### Example 1: Get physical address of variable
```assembly
W1 PHYLADR DATA_VAR
% I1 now contains physical address of DATA_VAR
```

### Example 2: DMA setup
```assembly
DMA_INIT:
        % Get physical address for DMA transfer
        W1 PHYLADR BUFFER
        % Program DMA controller with I1
        W MOVE I1, DMA_ADDR_REG
        RET
```

### Example 3: Memory-mapped I/O
```assembly
        % Translate I/O buffer address
        W2 PHYLADR IO_BUFFER
        % Use I2 for direct hardware access
```

### Example 4: Page table inspection
```assembly
CHECK_MAPPING:
        % Get physical address of page
        W3 PHYLADR PAGE_START
        % Compare with expected value
        W COMP I3, EXPECTED_PHYS
        RET
```

### Example 5: Indexed array physical address
```assembly
        % Get physical address of array element
        W4 PHYLADR ARRAY(W1)
        % I4 has physical address of ARRAY[W1]
```

### Example 6: Multi-buffer DMA chain
```assembly
SETUP_DMA_CHAIN:
        W1 PHYLADR BUF1
        W MOVE I1, DMA_CHAIN(0)
        W2 PHYLADR BUF2
        W MOVE I2, DMA_CHAIN(4)
        W3 PHYLADR BUF3
        W MOVE I3, DMA_CHAIN(8)
        RET
```

### Example 7: Debug memory dump
```assembly
DUMP_PHYSICAL:
        % Get physical address for debugger
        W1 PHYLADR DEBUG_VAR
        % Send I1 to debug console
        CALL PRINT_PHYS_ADDR
        RET
```

---

## Performance Notes
- Execution: 3-5 cycles
- Includes MMU translation
- Requires supervisor mode
- No memory access, only address translation

---

## Reference Manual
**Section:** §16.37
**Title:** PHYLADR - Get physical address ('87 extension)

---

## See Also
- [RPHS](rphs.md) - Read physical segment
- [CPGU](cpgu.md) - Clear page used table
- [PMON](pmon.md) - Program memory management on
