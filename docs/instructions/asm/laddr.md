# LADDR - Load Address

## Overview

**Mnemonic:** `laddr`
**Function:** Load effective address into register
**Class:** ADDRESS
**Privilege:** user

**Format:** `tn LADDR <operand>`

---

## Description

Loads the address of the operand into the specified register. This instruction computes the effective address of an operand without accessing the value at that address.

The address is loaded into the specified register Rn. Registers and constants have no address in memory and are illegal as operands.

Different data type prefixes (BIn, BYn, Hn, Wn, Fn, Dn) are used to provide the correct scaling factor when the operand is indexed. The Fn variant is functionally equivalent to Wn but may improve code readability when working with float arrays.

This instruction is essential for pointer manipulation, passing addresses as parameters, and implementing indirect addressing schemes.

**Operands:** 1
**Variants:** 24 opcode(s)

---

## Variants

Total variants: 24 (6 data types × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly Notation |
|---------|--------|-----------|----------|-------------------|
| 1/24 | 0xFE20 | BI | 1 | BI1 LADDR |
| 2/24 | 0xFE21 | BI | 2 | BI2 LADDR |
| 3/24 | 0xFE22 | BI | 3 | BI3 LADDR |
| 4/24 | 0xFE23 | BI | 4 | BI4 LADDR |
| 5/24 | 0xFE24 | BY | 1 | BY1 LADDR |
| 6/24 | 0xFE25 | BY | 2 | BY2 LADDR |
| 7/24 | 0xFE26 | BY | 3 | BY3 LADDR |
| 8/24 | 0xFE27 | BY | 4 | BY4 LADDR |
| 9/24 | 0xFE28 | H | 1 | H1 LADDR |
| 10/24 | 0xFE29 | H | 2 | H2 LADDR |
| 11/24 | 0xFE2A | H | 3 | H3 LADDR |
| 12/24 | 0xFE2B | H | 4 | H4 LADDR |
| 13/24 | 0xFD3C | W | 1 | W1 LADDR |
| 14/24 | 0xFD3D | W | 2 | W2 LADDR |
| 15/24 | 0xFD3E | W | 3 | W3 LADDR |
| 16/24 | 0xFD3F | W | 4 | W4 LADDR |
| 17/24 | 0xFD3C | F | 1 | F1 LADDR |
| 18/24 | 0xFD3D | F | 2 | F2 LADDR |
| 19/24 | 0xFD3E | F | 3 | F3 LADDR |
| 20/24 | 0xFD3F | F | 4 | F4 LADDR |
| 21/24 | 0xFE2C | D | 1 | D1 LADDR |
| 22/24 | 0xFE2D | D | 2 | D2 LADDR |
| 23/24 | 0xFE2E | D | 3 | D3 LADDR |
| 24/24 | 0xFE2F | D | 4 | D4 LADDR |

---

## Operands

### Operand 1 (Source Address)

The operand whose address is to be loaded. Only operands with memory addresses are valid.

**Type:** Any data type (specified by prefix)
**Access:** Address only (no value access)

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **PRE_INDEXED** - Indexed addressing with offset (e.g., B.ARRAY(R3))
- **ABSOLUTE** - Absolute memory address
- **INDIRECT** - Indirect addressing (IND(...))
- **DESCRIPTOR** - Descriptor-based addressing (DESC(...))

**Note:** REGISTER and CONSTANT modes are illegal - registers and constants have no memory address.

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation, descriptor range violation
- **Illegal operand:** Register or constant used as operand

---

## Data Status Bits

- **Z (Zero):** Set if computed address = 0, cleared otherwise
- **S (Sign):** Not affected
- **O (Overflow):** Not affected
- **C (Carry):** Not affected

---

## Examples

### Example 1: Load address of array element

```assembly
        % Load the address of the R3rd element of halfword array TABLE into R1
        H1 LADDR B.TABLE(R3)
        % R1 now contains: address of B.TABLE + (R3 * 2)
```

### Example 2: Pass address as parameter

```assembly
        % Pass address of local variable to subroutine
        W1 LADDR B.BUFFER
        % R1 contains address of BUFFER
        CALL PROCESS_BUFFER
```

### Example 3: Pointer arithmetic with correct scaling

```assembly
        % Get address of 10th element in byte array
        W1 := 10
        BY1 LADDR B.BYTEARRAY(I1)
        % R1 = address of B.BYTEARRAY + 10 (byte scaling)
```

### Example 4: Load address for indirect access

```assembly
        % Get address of variable pointed to by pointer
        W1 LADDR IND(B.POINTER)
        % R1 contains the address stored in POINTER
```

### Example 5: Float array address with type prefix

```assembly
        % Load address of float array element for readability
        W2 := B.INDEX
        F1 LADDR B.FLOAT_ARRAY(I2)
        % F1 prefix documents float array, same as W1 LADDR
```

---

## Performance Notes

- **Typical cycles:** 3-5 cycles depending on addressing mode complexity
- **Best case:** 3 cycles (simple local/record addressing)
- **Worst case:** 5+ cycles (indexed with descriptor, page fault)

**Note:** LADDR only computes the address, no memory value is fetched. This makes it faster than a load instruction. The instruction does not increment index registers for indexed addressing.

---

## Reference Manual

**Section:** §15.4
**Title:** Load address

---

## See Also

- [RLADDR](rladdr.md) - Load address into record register
- [BLADDR](bladdr.md) - Load address into base register
- [CHAIN](chain.md) - Load address of multilevel chain
- [LIND](lind.md) - Load indirect
- [IXI](ixi.md) - Index extension indexed
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
