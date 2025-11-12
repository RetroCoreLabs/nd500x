# ENTSN - Simple Entry with Argument Count

## Overview

**Mnemonic:** `entsn`
**Function:** Enter simple subroutine with variable argument validation
**Class:** CALL
**Privilege:** user

**Format:** `ENTSN <stack demand/r/W>,<max no. of arg./r/W>`

---

## Description

Enters a simple subroutine with argument count validation. Similar to ENTS but validates that the number of actual arguments does not exceed the maximum specified. Supports variadic functions and runtime argument checking. If too many arguments are provided, a trap may occur depending on implementation.

**Operation:**
```
If arg_count > max_args: trap
Save return address and frame pointer
Allocate <stack demand> words
Set up new frame pointer
```

**Key Characteristics:**
- Validates argument count at runtime
- Stack-based local variables
- Variable argument support (variadic functions)
- Traps on excessive arguments
- Paired with RET or RETK
- Supports C-style variadic functions

**Common Use Cases:**
- Printf-style variadic functions
- Flexible API functions with optional parameters
- Logging functions with variable arguments
- Command handlers with variable inputs
- Callback interfaces with flexible signatures

**Operands:** 2 (stack demand, max arguments)
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFD78 | ENTSN |

---

## Operands

### Operand 1 (Stack Demand)

Number of words for local variables.

**Type:** Word
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

### Operand 2 (Max Arguments)

Maximum number of arguments accepted.

**Type:** Word
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

---

## Examples

### Example 1: Variadic printf-style function

```assembly
PRINTF: ENTSN 10, 8         % Max 8 args, 10 words locals
        % Format string in first arg
        % Variable args follow
        % Process arguments
        RET
```

**Explanation:** Logging function accepting up to 8 arguments.

### Example 2: Command dispatcher

```assembly
DISPATCH: ENTSN 20, 6       % Max 6 command parameters
        % Parse command and arguments
        % Validate arg count
        % Execute command
        RET
```

**Explanation:** Command handler with flexible parameter count.

### Example 3: Callback with validation

```assembly
CALLBACK: ENTSN 15, 4       % Up to 4 callback params
        % Trap if caller provides > 4 args
        % Process validated arguments
        RET
```

**Explanation:** Protected callback interface with argument limits.

---

## Reference Manual

**Section:** §13.10
**Title:** Subroutine entry points

---

## See Also

- [ENTS](ents.md) - Simple entry
- [ENTFN](entfn.md) - Fortran entry with args
