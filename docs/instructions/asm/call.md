# CALL - Call Subroutine

## Overview

**Mnemonic:** `call`
**Function:** Call subroutine at absolute address
**Class:** CALL
**Privilege:** user

**Format:** `CALL <address>`

---

## Description

Calls a subroutine by pushing the return address onto the stack and jumping to the target address. This is the fundamental subroutine call mechanism in the ND-500 architecture, enabling modular programming and code reuse.

**Operation:**
1. Push current PC (return address) → stack
2. Load target address → PC
3. Execution continues at subroutine

The return address (address of instruction following CALL) is saved on the hardware stack, allowing the called subroutine to return using RET instruction. This implements standard call/return semantics found in most architectures.

**Stack Management:**
- Return address pushed as word (32-bit)
- Stack pointer automatically decremented
- Stack overflow checked (traps if insufficient space)

**Common Use Cases:**
- Function/procedure calls
- Code modularization
- Recursive algorithms
- Library routine invocation
- Event handler dispatch

**Operands:** 2
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0x00C3 | CALL |

---

## Operands

**Operand 1** (Implicit):
- **Role**: Stack location for return address

**Operand 2** (Target Address, Read):
- **Addressing modes**: CONSTANT (absolute address)
- **Data type**: Word (W) - 32-bit address
- **Role**: Subroutine entry point

**Result**: PC = target address, return address on stack

---

## Trap Conditions

- **Addressing traps**: Invalid target address
- **Stack overflow (STO)**: Insufficient stack space
- **Branch trap (BT)**: Target protection violation

---

## Data Status Bits

**All flags unaffected**

---

## Examples

### Example 1: Simple subroutine call

```assembly
% Call and return
        CALL PRINT_MESSAGE
        % Execution continues here after return

PRINT_MESSAGE:
        % Subroutine code
        RET
```

### Example 2: Nested calls

```assembly
MAIN:
        CALL FUNC_A
        RET

FUNC_A:
        CALL FUNC_B      % Nested call
        RET

FUNC_B:
        % Inner function
        RET
```

### Example 3: Recursive factorial

```assembly
% Calculate factorial recursively
FACTORIAL:
        W1 COMP N, 1
        IF<=GO BASE_CASE
        % Recursive case
        W2 MOVE N, I2
        W1 SUB N, 1, N
        CALL FACTORIAL
        W1 MUL I2, RESULT, RESULT
        RET
BASE_CASE:
        W1 MOVE 1, RESULT
        RET
```

### Example 4: Library function call

```assembly
% Call standard library
        W1 MOVE BUFFER, I1
        W2 MOVE SIZE, I2
        CALL LIB_MEMCPY
```

### Example 5: Multiple return points

```assembly
VALIDATE:
        W1 COMP INPUT, 0
        IF<GO ERROR
        W1 COMP INPUT, MAX
        IF>GO ERROR
        % Success
        SETK
        RET
ERROR:
        CLRK
        RET
```

### Example 6: Call with parameters

```assembly
% Pass parameters via registers
        W1 MOVE PARAM1, I1
        W2 MOVE PARAM2, I2
        W3 MOVE PARAM3, I3
        CALL PROCESS_DATA
        % Result in W1
```

### Example 7: Function pointer simulation

```assembly
% Call address stored in variable
        W1 MOVE FUNC_PTR, I1
        % Need CALLG for indirect calls
        % CALL only takes immediate addresses
        CALLG I1
```

---

## Performance Notes

- **Size**: 3+ bytes (opcode + address)
- **Execution**: 3-4 cycles (push + jump)
- **Stack**: 1 word pushed per call
- **Nesting**: Limited by stack depth
- **vs CALLG**: CALL uses immediate address, CALLG uses computed address
- **Return**: Use RET to return from CALL

---

## Reference Manual

**Section:** §13.8
**Title:** Call subroutine absolute

---

## See Also

- [RET](ret.md) - Return from subroutine
- [CALLG](callg.md) - Call general (computed address)
- [ENTB](entb.md) - Enter block
- [RETB](retb.md) - Return from block
- [JUMPG](jumpg.md) - Jump general
