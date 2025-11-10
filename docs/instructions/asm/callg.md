# CALLG - Call Subroutine General

## Overview

**Mnemonic:** `callg`
**Function:** Call subroutine with argument list
**Class:** CALL
**Privilege:** user

**Format:** `CALLG <entry_point>, <arg_count>, <arg1>, ..., <argN>`

---

## Description

Calls a subroutine at a computed address (from register or memory), passing a list of argument addresses. This is the general-purpose call instruction supporting variable-length argument lists, enabling Pascal-like parameter passing by reference.

**Operation:**
1. Calculate effective addresses of all arguments
2. Store argument addresses for entry point instruction
3. Jump to subroutine entry point
4. Entry point instruction (ENTB/ENTBB) processes arguments

CALLG differs from CALL in two critical ways:
1. **Computed Address**: Target from general operand (register/memory), not immediate
2. **Arguments**: Passes list of argument addresses to called routine

The subroutine entry point MUST be an ENTB or ENTBB instruction, which sets up the stack frame and processes the argument addresses. If the target is not an entry point, an instruction sequence error trap occurs.

**Argument Constraints:**
- Argument count must be constant byte (0-255)
- Arguments must be addresses (not constants or registers)
- Arguments interpreted as word addresses
- Arguments cannot use ALT prefix

**Common Use Cases:**
- High-level language function calls (Pascal, ALGOL)
- Var

iable-argument functions
- Passing arrays by reference
- System calls with parameter blocks
- Dynamic dispatch with arguments

**Operands:** 2+ (entry point + arg count + argument list)
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0x00B5 | CALLG |

---

## Operands

**Operand 1** (Entry Point, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word (W) - address
- **Role**: Address of subroutine entry point (must be ENTB/ENTBB)

**Operand 2** (Argument Count, Read):
- **Addressing modes**: CONSTANT only
- **Data type**: Byte (BY)
- **Range**: 0-255
- **Role**: Number of arguments following

**Operands 3..N** (Arguments):
- **Must be addresses** (not constants/registers)
- **Type**: Word addresses
- **Role**: References to parameters

**Result**: Control transferred to entry point with arguments

---

## Trap Conditions

- **Addressing traps**: Invalid address calculation
- **Call trap (CT)**: General call error
- **Illegal operand specifier (IOS)**: Arg count not constant byte, or arg is constant/register
- **Instruction sequence error (ISE)**: Target is not an entry point instruction

---

## Data Status Bits

**All flags unaffected**

---

## Examples

### Example 1: Simple function call with arguments

```assembly
% Call PRINT with 3 arguments
        CALLG PRINT, 3, UNIT, FORMAT, B.VALUE

PRINT:  ENTB 10, 3     % Entry point with 3 parameters
        % Access arguments via formal parameters
        RET
```

### Example 2: Indirect call via function pointer

```assembly
% Call function via pointer
        W1 MOVE HANDLER_PTR, I1
        CALLG I1, 2, INPUT_BUFFER, OUTPUT_BUFFER
```

### Example 3: No arguments

```assembly
% Call with zero arguments
        CALLG INITIALIZE, 0

INITIALIZE:
        ENTB 5, 0
        % No parameters
        RET
```

### Example 4: Array processing

```assembly
% Pass array and size
        CALLG SORT_ARRAY, 2, MY_ARRAY, ARRAY_SIZE

SORT_ARRAY:
        ENTB 20, 2
        % Param 1: array address
        % Param 2: size
        RET
```

### Example 5: Multiple argument types

```assembly
% Call with various argument types
        CALLG PROCESS, 5, B.COUNT, BUFFER, B.FLAGS, STATUS, B.TEMP

PROCESS:
        ENTB 30, 5
        % All 5 arguments accessible
        RET
```

### Example 6: Dynamic function selection

```assembly
% Call different handlers based on type
        W1 COMP MSG_TYPE, 0
        IF=GO TYPE_0
        W1 COMP MSG_TYPE, 1
        IF=GO TYPE_1
        GO ERROR

TYPE_0:
        CALLG HANDLER_0, 1, MESSAGE
        RET
TYPE_1:
        CALLG HANDLER_1, 1, MESSAGE
        RET
```

### Example 7: Recursive call with arguments

```assembly
% Recursive tree traversal
TRAVERSE:
        ENTB 10, 1
        % Check if node is null
        W1 COMP B.NODE, 0
        IF=GO DONE
        % Process left child
        CALLG TRAVERSE, 1, B.NODE.LEFT
        % Process right child
        CALLG TRAVERSE, 1, B.NODE.RIGHT
DONE:
        RET
```

---

## Performance Notes

- **Size**: Variable (3+ operand addresses)
- **Execution**: 5-10 cycles (depends on arg count)
- **vs CALL**: CALLG slower but supports arguments
- **Argument Addresses**: All args must have addresses (not values)
- **Entry Point**: Target MUST be ENTB/ENTBB instruction
- **Parameter Access**: Arguments accessed via entry point formal parameters

---

## Reference Manual

**Section:** §13.7
**Title:** Call subroutine general

---

## See Also

- [CALL](call.md) - Simple subroutine call
- [ENTB](entb.md) - Enter block (entry point)
- [ENTBB](entbb.md) - Enter block buddy
- [RET](ret.md) - Return from subroutine
- [JUMPG](jumpg.md) - Jump general
