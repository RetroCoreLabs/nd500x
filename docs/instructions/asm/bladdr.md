# BLADDR - Load Address into Base Register

## Overview

**Mnemonic:** `bladdr`
**Function:** Load effective address into base register (B)
**Class:** ADDRESS
**Privilege:** user

**Format:** `t BLADDR <operand>`

---

## Description

Loads the address of the operand into the local base register B. This instruction computes the effective address of an operand without accessing the value at that address.

**Operation:**
```
address(<operand>) → B
```

**Key Characteristics:**
- Loads effective address into B (base) register
- Changes base for all B-relative addressing
- No memory value access (address computation only)
- 6 data type variants (BI, BY, H, W, F, D)
- Registers and constants illegal (no memory address)
- Essential for dynamic scoping and frame switching
- 3-5 cycles depending on addressing complexity
- Sets Z flag if computed address = 0
- Used for context switching and structure access

The address is loaded into the base register (B register), which defines the base for local variable addressing. Registers and constants have no address in memory and are illegal as operands.

Different data type prefixes (BI, BY, H, W, F, D) are used to provide the correct scaling factor when the operand is indexed. The F variant is functionally equivalent to W but may improve code readability.

This instruction is used to change the local data area base, implement dynamic scoping, access different stack frames, or switch between data contexts.

**Operands:** 1
**Variants:** 6 opcode(s)

---

## Variants

Total variants: 6 (one per data type)

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/6 | 0xFCB3 | BI | BI BLADDR |
| 2/6 | 0xFCBC | BY | BY BLADDR |
| 3/6 | 0xFD37 | H | H BLADDR |
| 4/6 | 0xFD63 | W | W BLADDR |
| 5/6 | 0xFD63 | F | F BLADDR |
| 6/6 | 0xFD38 | D | D BLADDR |

---

## Operands

### Operand 1 (Source Address)

The operand whose address is to be loaded into the B register. Only operands with memory addresses are valid.

**Type:** Any data type (specified by prefix)
**Access:** Address only (no value access)

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address
- **INDIRECT** - Indirect addressing (IND(...))
- **DESCRIPTOR** - Descriptor-based addressing (DESC(...))

**Note:** REGISTER and CONSTANT modes are illegal - registers and constants have no memory address.

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation, descriptor range violation

---

## Data Status Bits

- **Z (Zero):** Set if computed address = 0, cleared otherwise
- **S (Sign):** Not affected
- **O (Overflow):** Not affected
- **C (Carry):** Not affected

---

## Examples

### Example 1: Change local base to argument

```assembly
        % Load B with the address of argument NEWB
        W BLADDR B.NEWB
        % B now points to NEWB, all B.xxx references are now relative to NEWB
```

### Example 2: Switch to different stack frame

```assembly
        % Access caller's local variables
        W BLADDR IND(B.CALLER_FRAME)
        % Now B points to caller's locals
```

### Example 3: Implement display register

```assembly
        % Save current B
        W1 := B
        % Switch to outer scope
        W BLADDR B.OUTER_FRAME
        % Access outer scope variables via B.xxx
        % Restore original B
        B := I1
```

### Example 4: Dynamic data area switching

```assembly
        % Save current base
        W BLADDR B.SAVED_BASE
        % Switch to alternate data area
        W BLADDR B.ALT_AREA
        % Work with alternate area
        % Restore original
        B := B.SAVED_BASE
```

### Example 5: Access structure as local area

```assembly
        % Point B to a large structure
        W BLADDR B.BIG_STRUCT
        % Access structure fields as if they were locals
        W1 := B.FIELD1
        W2 := B.FIELD2
```

---

## Performance Notes

- **Typical cycles:** 3-5 cycles depending on addressing mode complexity
- **Best case:** 3 cycles (simple local/record addressing)
- **Worst case:** 5+ cycles (indexed with descriptor, page fault)

**Note:** BLADDR changes the interpretation of all subsequent B-relative addresses. Save and restore B when temporarily switching contexts.

---

## Reference Manual

**Section:** §15.6
**Title:** Load address into base register

---

## See Also

- [LADDR](laddr.md) - Load address into general register
- [RLADDR](rladdr.md) - Load address into record register
- [B:=](b_assignfrom.md) - Load value into B register
- [Trap System](../../ND-500-TRAPS.md)
