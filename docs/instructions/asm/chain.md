# CHAIN - Load Address of Multilevel Chain

## Overview

**Mnemonic:** `chain`
**Function:** Follow multilevel pointer chain and load address
**Class:** ADDRESS
**Privilege:** user

**Format:** `Wn CHAIN <address>,<offset>,<levels>`

---

## Description

Follows a linked chain of pointers for a specified number of levels and loads the final address into the specified register. This instruction is designed for implementing nested procedure scopes in block-structured languages.

The instruction starts at `<address>` and repeatedly follows pointers found at `<offset>` for `<levels>` iterations. Each iteration dereferences the pointer at (current_address + offset) to get the next address in the chain.

If a zero pointer is encountered during traversal, the operation terminates early, sets the K flag, and triggers an illegal operand value trap. The register contains the address of the location containing the zero pointer.

This instruction is essential for accessing variables declared in outer procedures when using static scoping (Pascal, Ada, etc.).

**Operands:** 3
**Variants:** 4 opcode(s)

---

## Variants

Total variants: 4 (word type × 4 registers)

| Variant | Opcode | Register | Assembly Notation |
|---------|--------|----------|-------------------|
| 1/4 | 0xFD6C | 1 | W1 CHAIN |
| 2/4 | 0xFD6D | 2 | W2 CHAIN |
| 3/4 | 0xFD6E | 3 | W3 CHAIN |
| 4/4 | 0xFD6F | 4 | W4 CHAIN |

---

## Operands

### Operand 1 (Initial Address)

The starting address for the chain traversal.

**Type:** Word (address)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (typically B register value)
- **RECORD** - Record-relative access
- **REGISTER** - Integer register containing address
- **PRE_INDEXED** - Indexed addressing
- **ABSOLUTE** - Absolute memory address

### Operand 2 (Link Offset)

The offset from the base address where the next pointer in the chain is located.

**Type:** Word (offset)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local variable containing offset
- **RECORD** - Record field containing offset
- **CONSTANT** - Immediate offset value
- **REGISTER** - Integer register containing offset

### Operand 3 (Number of Levels)

The number of levels to traverse in the chain.

**Type:** Word (count)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local variable containing count
- **RECORD** - Record field containing count
- **CONSTANT** - Immediate count value
- **REGISTER** - Integer register containing count

**Note:** A negative level count triggers an illegal operand value trap. Zero levels is equivalent to LADDR.

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation during chain traversal
- **Illegal operand value (IOV):**
  - Negative `<levels>` value
  - Zero pointer encountered during traversal (K flag also set)

---

## Data Status Bits

- **Z (Zero):** Not typically affected
- **S (Sign):** Set to sign bit of final address
- **O (Overflow):** Not affected
- **C (Carry):** Not affected
- **K (K Flag):** Set if zero pointer encountered during traversal

---

## Examples

### Example 1: Access variable five levels up

```assembly
        % Load W1 with stack base address of a procedure five static levels up
        % Static link is found in local variable STATLINK
        W1 CHAIN B, B.STATLINK, 5
        % W1 now contains address of great-great-great-grandparent stack frame
```

### Example 2: Navigate to outer scope

```assembly
        % Access variable two scopes up
        W2 CHAIN B, 0, 2
        % Follow two levels of static links starting at B+0
        % W2 contains address of grandparent scope
```

### Example 3: Dynamic level access

```assembly
        % Follow chain based on runtime level difference
        W1 := B.LEVEL_DIFF
        W2 CHAIN B, B.STATIC_LINK, I1
        % Follows variable number of levels
```

### Example 4: Implementing display register

```assembly
        % Establish display for nested procedure calls
        W1 := 1
DISPLAY_LOOP:
        W2 CHAIN B, B.LINK, I1
        % Store in display table
        W MOVE I2, B.DISPLAY(I1)
        W INCR I1
        W COMP I1, B.MAX_DEPTH
        IF<GO DISPLAY_LOOP
```

### Example 5: Safe chain traversal with error handling

```assembly
        % Follow chain with zero-pointer check
        W1 CHAIN B, B.LINK, 3
        IF-KGO CHAIN_OK        % K flag set if zero encountered
        % Handle broken chain
        CALL ERROR_HANDLER
CHAIN_OK:
        % W1 contains valid address
```

---

## Performance Notes

- **Typical cycles:** 5-10 cycles base + (2-3 cycles × number of levels traversed)
- **Best case:** 5 cycles (0 levels, equivalent to LADDR)
- **Worst case:** 20+ cycles (maximum levels with page faults at each dereference)

**Note:** Each level requires a memory access to fetch the next pointer. Cache misses can significantly impact performance for deep chains.

---

## Reference Manual

**Section:** §15.7
**Title:** Load address of multilevel chain

---

## See Also

- [LADDR](laddr.md) - Load address (CHAIN with 0 levels)
- [BLADDR](bladdr.md) - Load address into base register
- [RLADDR](rladdr.md) - Load address into record register
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
