# ENTT - Enter Trap Handler

## Overview

**Mnemonic:** `entt`
**Function:** Enter trap handler with dedicated stack
**Class:** CALL
**Privilege:** supervisor

**Format:** `ENTT <trap handler stack demand>, <total trap stack demand>`

---

## Description

Initializes a trap handler with its own dedicated stack space, separate from the user program stack. This prevents trap handlers from overflowing or corrupting the user stack. Allocates local storage for the handler and reserves total stack capacity.

**Key Characteristics:**
- Privileged instruction (supervisor only)
- Switches to dedicated trap stack
- Allocates handler-specific frame
- Reserves total trap stack space
- Paired with RETT instruction
- Protects user stack from trap overhead

**Common Use Cases:**
- Exception handler initialization
- Trap handler entry
- Interrupt service routine setup
- System call entry point
- Error handler initialization

**Operands:** 2 (handler demand, total demand)
**Variants:** 1 opcode

---

## Examples

### Example 1: Overflow trap handler

```assembly
OVF_HANDLER:
        ENTT 50, 500        % 50 words local, 500 total reserve
        % Handle overflow condition
        % Log error or fix issue
        RETT                % Return to interrupted code
```

**Explanation:** Trap handler with 50-word frame, 500-word total reserve.

### Example 2: System call entry

```assembly
SYSCALL_ENTRY:
        ENTT 100, 1000      % Larger frame for syscall processing
        % Validate syscall number
        % Dispatch to service handler
        % Perform system service
        RETT
```

**Explanation:** System call handler with substantial stack space.

### Example 3: Nested trap handling

```assembly
PRIMARY_TRAP:
        ENTT 80, 800
        % May trigger secondary traps
        % Sufficient stack for nesting
        RETT
```

**Explanation:** Reserve extra space for potential nested traps.

---

## Trap Conditions

- **Privilege violation**: Executed in user mode
- **Stack overflow**: Insufficient trap stack space

---

## Data Status Bits

- **Unaffected**: Preserves interrupted state

---

## Reference Manual

**Section:** §13.10
**Title:** Subroutine entry points

---

## See Also

- [RETT](rett.md) - Return from trap handler
- [ENT](ent.md) - Regular subroutine entry
- [ENTB](entb.md) - Buddy subroutine entry
- [INT](int.md) - Generate interrupt
