# RLADDR - Load Address into Record Register

## Overview

**Mnemonic:** `rladdr`
**Function:** Load effective address into record register (R)
**Class:** ADDRESS
**Privilege:** user

**Format:** `t RLADDR <operand>`

---

## Description

Loads the address of the operand into the record register R. This instruction computes the effective address of an operand without accessing the value at that address.

The address is loaded into the record register (R register), which is used for record-relative addressing. Registers and constants have no address in memory and are illegal as operands.

Different data type prefixes (BI, BY, H, W, F, D) are used to provide the correct scaling factor when the operand is indexed. The F variant is functionally equivalent to W but may improve code readability when working with float data.

This instruction is commonly used to establish a new record base pointer, access structures through pointers, or navigate stack frames.

**Operands:** 1
**Variants:** 6 opcode(s)

---

## Variants

Total variants: 6 (one per data type)

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/6 | 0xFC55 | BI | BI RLADDR |
| 2/6 | 0xFC54 | BY | BY RLADDR |
| 3/6 | 0xFC81 | H | H RLADDR |
| 4/6 | 0x00BE | W | W RLADDR |
| 5/6 | 0x00BE | F | F RLADDR |
| 6/6 | 0xFCB2 | D | D RLADDR |

---

## Operands

### Operand 1 (Source Address)

The operand whose address is to be loaded into the R register. Only operands with memory addresses are valid.

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

### Example 1: Load base address of stack frame

```assembly
        % Load R with the base address of the first stack frame
        % below the current stack frame
        W RLADDR IND(B.0)
```

### Example 2: Access structure through pointer

```assembly
        % Point R to a structure
        W RLADDR IND(B.STRUCT_PTR)
        % Now can access fields via R.FIELD_NAME
        W1 := R.X
        W2 := R.Y
```

### Example 3: Navigate to previous stack frame

```assembly
        % Follow static link chain
        W RLADDR B.STATIC_LINK
        % R now points to enclosing procedure's local data
```

### Example 4: Establish record base for array of structures

```assembly
        % Point R to specific array element
        W RLADDR B.RECORDS(I2)
        % Access fields of that record
        W1 := R.NAME
        W2 := R.VALUE
```

### Example 5: Switch between data areas

```assembly
        % Save current R
        W1 := R
        % Load new record base
        W RLADDR B.NEW_AREA
        % Work with new area via R.fields
        % Restore old R
        R := I1
```

---

## Performance Notes

- **Typical cycles:** 3-5 cycles depending on addressing mode complexity
- **Best case:** 3 cycles (simple local/record addressing)
- **Worst case:** 5+ cycles (indexed with descriptor, page fault)

**Note:** RLADDR only computes the address, no memory value is fetched. Particularly useful for establishing record context for subsequent R-relative operations.

---

## Reference Manual

**Section:** §15.5
**Title:** Load address into record register

---

## See Also

- [LADDR](laddr.md) - Load address into general register
- [BLADDR](bladdr.md) - Load address into base register
- [R:=](r_assignfrom.md) - Load value into R register
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
