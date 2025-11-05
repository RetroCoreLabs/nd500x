# CALL and Entry Point Instructions: Parameter Passing Explained

## Overview

The ND-500 calling convention uses a two-instruction protocol where **CALL/CALLG** (caller) and **ENTR** (callee) instructions work together to pass parameters and set up the subroutine's local data area (stack frame).

## Key Concept: Address-Based Parameter Passing

**CRITICAL:** The ND-500 does NOT pass parameter *values* - it passes parameter *addresses* (pointers). This is why:
- Arguments must be memory locations (not registers or constants)
- Arguments are always interpreted as word addresses
- The callee accesses parameters by dereferencing these addresses

---

## CALL/CALLG Instructions (Caller Side)

### Format

```
CALL  <subr_addr>, <no_of_args>, <arg1>, <arg2>, ..., <argn>
CALLG <subr_addr>, <no_of_args>, <arg1>, <arg2>, ..., <argn>
```

### Parameters Explained

**Parameter 1: `<subr_addr>`** - Subroutine Address
- **CALL**: Direct 4-byte absolute address in the instruction
- **CALLG**: General operand (can be register, local variable, etc.)
- **Must point to an ENTR instruction** (ENTS, ENTSN, ENTF, ENTFN, ENTB, etc.)
- If it doesn't point to an entry instruction → **Instruction Sequence Error trap**

**Parameter 2: `<no_of_args>`** - Number of Arguments
- **Must be a constant byte** (0-255)
- Tells the entry point how many argument addresses follow
- Non-constant values → **Illegal Operand Specifier trap**

**Parameters 3+: `<arg1>`, `<arg2>`, ..., `<argn>`** - Argument Addresses
- **Not actual values, but ADDRESSES** of the arguments
- Must be memory locations (cannot be registers or constants)
- Always interpreted as **word addresses** regardless of actual data type
- Each argument's **effective address** is calculated and stored
- These addresses are passed to the callee via the stack

### What CALL/CALLG Does

1. **Calculates effective addresses** of all arguments
2. **Stores these addresses** in a temporary area
3. **Saves return address** (where to return after RET)
4. **Jumps to the entry point instruction** at `<subr_addr>`
5. The entry point instruction then initializes the stack frame

---

## ENTR Instructions (Callee Side)

There are multiple entry point types, each with different stack frame initialization:

### Entry Point Types

| Entry Point | Parameters | Purpose |
|-------------|------------|---------|
| **ENTS**    | `<stack_demand>` | Standard stack-based subroutine |
| **ENTSN**   | `<stack_demand>`, `<max_args>` | Stack subroutine with argument limit |
| **ENTF**    | `<address_of_data_area>` | Fixed (static) data area |
| **ENTFN**   | `<address_of_data_area>`, `<max_args>` | Fixed data area with argument limit |
| **ENTB**    | `<log_size>` | Heap-allocated block |
| **ENTD**    | `<stack_demand>` | Direct entry (no parameter transfer) |
| **ENTM**    | `<bottom>`, `<main_demand>`, `<total_demand>` | Module entry (new stack) |
| **ENTT**    | `<main_demand>`, `<total_demand>` | Trap handler entry |

---

## ENTS/ENTSN - Stack Subroutine Entry

### ENTS Format
```
ENTS <stack_demand>
```

**Parameter: `<stack_demand>`** - Stack Space Required
- Number of **bytes** needed for the local data area
- Includes the 20-byte header (PREVB, RETA, SP, AUX, N)
- Plus space for all local variables
- Example: `ENTS 100` requests 100 bytes total

### ENTSN Format
```
ENTSN <stack_demand>, <max_no_of_args>
```

**Parameter 1: `<stack_demand>`** - Same as ENTS

**Parameter 2: `<max_no_of_args>`** - Argument Limit
- Maximum number of argument addresses to transfer
- If caller passes MORE arguments than this, extras are ignored
- If caller passes FEWER, all are transferred
- `B.N` contains the actual number transferred (min of caller's count and this limit)

### Stack Frame Layout Created by ENTS/ENTSN

When ENTS or ENTSN executes, it creates this structure:

```
        B register points here ↓

Offset  Location    Content
------  ----------  ------------------------------------------
B+0     B.PREVB     Previous B register value (for return)
B+4     B.RETA      Return address (also copied to L register)
B+8     B.SP        Stack pointer (B + stack_demand)
B+12    B.AUX       Auxiliary location (language-specific use)
B+16    B.N         Number of arguments actually transferred
B+20    B.ARG1      Address of first argument  ← CALLER'S <arg1> address
B+24    B.ARG2      Address of second argument ← CALLER'S <arg2> address
B+28    B.ARG3      Address of third argument  ← CALLER'S <arg3> address
...     ...         ... more argument addresses ...
B+20+4n B.ARGn      Address of nth argument
...     ...         Local variables (uninitialized)
        B.SP →      First free location
```

**Key Point:** `B.ARG1`, `B.ARG2`, etc. contain the **addresses** that CALL calculated, NOT the values.

---

## ENTF/ENTFN - Fixed Data Area Entry

### ENTF Format
```
ENTF <address_of_local_data_area>
```

**Parameter: `<address_of_local_data_area>`**
- Direct 4-byte absolute address in the instruction
- Points to a **statically allocated** data area
- This area persists between calls (values retained)
- **Non-reentrant** (cannot be called recursively)

### ENTFN Format
```
ENTFN <address_of_local_data_area>, <max_no_of_args>
```

**Parameter 1: `<address_of_local_data_area>`** - Same as ENTF

**Parameter 2: `<max_no_of_args>`** - Same as ENTSN

### What ENTF/ENTFN Does

1. Sets B register to `<address_of_local_data_area>`
2. Saves old B in `B.PREVB`
3. Saves return address in `B.RETA` and L register
4. Sets `B.SP` to `oldB.SP` (no stack growth)
5. Transfers argument addresses to `B.ARG1`, `B.ARG2`, etc.
6. Sets `B.N` to number of arguments

**Use Case:** Functions with persistent state (e.g., random number generator with seed)

---

## ENTB - Heap Block Entry

### Format
```
ENTB <log_size>
```

**Parameter: `<log_size>`**
- **Logarithm (base 2)** of the block size needed
- Allocates a block from the heap
- Block size = 2^(log_size) bytes
- Example: `ENTB 8` allocates 256 bytes (2^8)

### What ENTB Does

1. Allocates a heap block of size 2^(log_size)
2. Sets B register to the block's address
3. Stores block size in `B.AUX`
4. Initializes stack frame (like ENTS)
5. Transfers argument addresses

**Use Case:** Variable-sized data structures, dynamic allocation

---

## ENTD - Direct Entry (No Parameters)

### Format
```
ENTD <stack_demand>
```

**Parameter: `<stack_demand>`** - Stack space (unused)

### What ENTD Does

**MINIMAL SETUP:**
- Only copies return address to L register
- **No stack frame initialization**
- **No parameter transfer** (must be called with 0 arguments)
- Caller with arguments → **Instruction Sequence Error trap**

**Use Case:** Leaf functions with no parameters, no local variables

---

## Complete Example: Calling a Subroutine

### Scenario
Call a `PRINT` subroutine that formats and prints data.

### Caller Code
```asm
; Define some variables
UNIT:    .WORD   5          ; Output unit number
FORMAT:  .WORD   0x100      ; Format string address
VALUE:   .SPACE  4          ; Local variable on stack at B.16

; Call PRINT with 3 arguments
CALLG PRINT, 3, UNIT, FORMAT, B.VALUE
```

### What Happens During CALLG

1. **Calculate effective addresses:**
   - arg1 address = &UNIT (absolute address of UNIT)
   - arg2 address = &FORMAT (absolute address of FORMAT)
   - arg3 address = B + 16 (effective address of local variable)

2. **Store these three addresses** in temporary area

3. **Save return address** (address of instruction after CALLG)

4. **Jump to PRINT** (which must start with an ENTR instruction)

### Callee Code (PRINT subroutine)
```asm
PRINT:
    ENTSN 100, 10       ; Request 100 bytes, max 10 arguments

    ; Now the stack frame is set up:
    ; B.N = 3 (number of arguments)
    ; B.ARG1 = address of UNIT
    ; B.ARG2 = address of FORMAT
    ; B.ARG3 = address of VALUE (B+16 in caller's frame)

    ; Access the ACTUAL VALUES by dereferencing:
    W1 := IND(B.ARG1)   ; Load unit number (5) into W1
    W2 := IND(B.ARG2)   ; Load format address (0x100) into W2
    W3 := IND(B.ARG3)   ; Load value from caller's local var

    ; ... do printing ...

    RET                 ; Return to caller
```

### Key Insight

The callee **must dereference** `B.ARG1`, `B.ARG2`, etc. using **IND()** to get the actual values:

- `B.ARG1` contains an address, NOT the value 5
- `IND(B.ARG1)` accesses the memory at that address to get 5
- This is **call by reference** semantics

---

## Why Two Parameters on CALL?

You might wonder: why does CALL need both `<no_of_args>` AND the argument list? Can't it just count?

### Reason 1: Variable-Length Encoding

The ND-500 has complex addressing modes. The CPU can't easily tell where one operand ends and the next begins without decoding each one. The `<no_of_args>` parameter tells the CPU exactly how many operands to decode.

### Reason 2: ENTSN/ENTFN Argument Limiting

The `<max_no_of_args>` parameter on ENTSN/ENTFN allows the callee to limit how many arguments it accepts:

```asm
; Caller passes 5 arguments
CALLG SUBR, 5, ARG1, ARG2, ARG3, ARG4, ARG5

SUBR:
    ENTSN 100, 3        ; Only accept first 3 arguments
    ; B.N will be 3 (not 5)
    ; B.ARG1, B.ARG2, B.ARG3 are set
    ; ARG4 and ARG5 are ignored
```

This enables **versioning**: older code with fewer parameters can call newer functions that expect more, or vice versa.

---

## Argument Restrictions

### Cannot Use Registers or Constants

```asm
; ILLEGAL - Will cause trap!
CALLG SUBR, 2, W1, 42

; CORRECT - Use memory locations
TEMP1: .WORD 0
TEMP2: .WORD 42

W1 := TEMP1         ; Store W1's value to memory
CALLG SUBR, 2, TEMP1, TEMP2
```

**Why?** Because CALL passes *addresses*, and registers/constants don't have addresses.

### Data Type Considerations

All argument addresses are treated as **word addresses**. If you're using post-indexed or descriptor addressing with non-word data types, be careful:

```asm
ARRAY:  .BYTE 10, 20, 30, 40

; This might not work as expected:
CALLG SUBR, 1, ARRAY(W1)   ; Index calculated for words, not bytes!

; Better:
BY1 := ARRAY           ; Use proper byte addressing
CALLG SUBR, 1, TEMP    ; Pass address of result
```

---

## Entry Point Mismatches

### What Happens If You Call The Wrong Entry Point?

The CPU detects entry point mismatches at runtime:

```asm
; Caller expects a function with parameters
CALLG SUBR, 3, ARG1, ARG2, ARG3

SUBR:
    ENTD            ; ERROR: ENTD requires 0 arguments!
    ; → Instruction Sequence Error trap
```

### Not An Entry Point At All

```asm
CALLG LABEL, 0, ; LABEL doesn't point to an ENTx instruction

LABEL:
    W1 + 1          ; This is not an entry point!
    ; → Instruction Sequence Error trap
```

---

## Summary Table: Parameter Usage

| Instruction | Parameters | Who Uses Them | Purpose |
|-------------|------------|---------------|---------|
| **CALL/CALLG** | `<subr_addr>` | CPU | Jump target (must be ENTR instruction) |
| | `<no_of_args>` | CPU | How many argument addresses follow |
| | `<arg1>`...`<argn>` | CPU calculates addresses → stored → callee reads from stack | Data passed to callee |
| **ENTS** | `<stack_demand>` | CPU | Allocate stack frame of this size |
| **ENTSN** | `<stack_demand>` | CPU | Allocate stack frame of this size |
| | `<max_no_of_args>` | CPU | Transfer at most this many argument addresses |
| **ENTF** | `<data_area_addr>` | CPU | Use this static area instead of stack |
| **ENTFN** | `<data_area_addr>` | CPU | Use this static area instead of stack |
| | `<max_no_of_args>` | CPU | Transfer at most this many argument addresses |
| **ENTB** | `<log_size>` | CPU | Allocate heap block of 2^(log_size) bytes |
| **ENTD** | `<stack_demand>` | (ignored) | No parameter transfer, caller must pass 0 args |

---

## Accessing Arguments in the Callee

Inside the subroutine, access arguments through the B register:

### Method 1: Direct Indexed Access
```asm
SUBR:
    ENTSN 100, 10

    W1 := IND(B.20)     ; B.N (number of args)
    W2 := IND(B.24)     ; B.ARG1 (first argument address)
    W3 := IND(B.28)     ; B.ARG2 (second argument address)
```

### Method 2: Named Access (Assembler Macros)
```asm
; Define symbolic offsets (typically done by compiler)
N     = 20      ; Offset to B.N
ARG1  = 24      ; Offset to B.ARG1
ARG2  = 28      ; Offset to B.ARG2

SUBR:
    ENTSN 100, 10

    W1 := IND(B.N)      ; Number of arguments
    W2 := IND(B.ARG1)   ; First argument address
    W3 := IND(B.ARG2)   ; Second argument address
```

### Method 3: Load and Dereference
```asm
SUBR:
    ENTSN 100, 10

    ; Get address of first argument
    W1 := B.ARG1        ; W1 = address

    ; Dereference to get value
    W2 := IND(W1)       ; W2 = value at that address

    ; Or in one step:
    W3 := IND(B.ARG1)   ; Direct dereference
```

---

## Real-World Example: String Copy Function

### Caller
```asm
SOURCE: .ASCII "Hello, World!"
DEST:   .SPACE 20

; Call strcpy with 2 arguments
CALLG STRCPY, 2, SOURCE, DEST
```

### Callee (strcpy implementation)
```asm
STRCPY:
    ENTSN 40, 2         ; 40 bytes local space, max 2 args

    ; Check we got 2 arguments
    W1 := IND(B.N)
    W1 - 2              ; Compare with 2
    IF != GO ERROR

    ; Load argument addresses
    W1 := IND(B.ARG1)   ; W1 = address of SOURCE
    W2 := IND(B.ARG2)   ; W2 = address of DEST

    ; Now copy string (pseudocode)
LOOP:
    BY1 := IND(W1)      ; Load byte from source
    IND(W2) := BY1      ; Store byte to dest
    W1 + 1              ; Increment source pointer
    W2 + 1              ; Increment dest pointer
    BY1 - 0             ; Test for null terminator
    IF != GO LOOP

    RET                 ; Return to caller

ERROR:
    ; Handle wrong number of arguments
    RET
```

---

## Common Pitfalls

### 1. Forgetting to Dereference
```asm
; WRONG - Compares addresses, not values!
W1 := B.ARG1
W2 := B.ARG2
W1 - W2             ; Compares the addresses themselves

; CORRECT - Compares values
W1 := IND(B.ARG1)
W2 := IND(B.ARG2)
W1 - W2             ; Compares the values at those addresses
```

### 2. Using Wrong Stack Demand
```asm
SUBR:
    ; Need: 20 (header) + 16 (4 local words) = 36 bytes
    ENTS 36             ; CORRECT

    ; WRONG - Only allocates header!
    ENTS 20             ; Local variables will corrupt stack!
```

### 3. Passing Too Many Arguments with ENTSN
```asm
; Caller passes 10 arguments
CALLG SUBR, 10, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10

SUBR:
    ENTSN 100, 5        ; Only accept 5 arguments

    ; B.N = 5 (not 10)
    ; Only B.ARG1 through B.ARG5 are valid
    ; Accessing B.ARG6+ will read uninitialized memory!
```

### 4. Modifying Caller's Data Unintentionally
```asm
; Since arguments are passed by reference, modifications affect caller!
SUBR:
    ENTSN 40, 1

    W1 := IND(B.ARG1)   ; Get address
    W2 := 999
    IND(W1) := W2       ; Modifies caller's variable!

    RET                 ; Caller's variable now changed to 999
```

This is by design - allows **output parameters**.

---

## Advanced: Cross-Domain Calls

Only **ENTM** (enter module) can be called from another domain. All other entry points are intra-domain only.

When calling across domains:
1. **CALLG** to routine in another domain
2. Routine must start with **ENTM**
3. Domain switch occurs
4. TOS, LL, HL, THA saved to old domain information table
5. New values loaded from new domain information table
6. Return restores original domain context

---

## Performance Considerations

### Fastest: ENTD
- No parameter transfer
- No stack frame setup
- Just return address → L

### Fast: ENTS
- Stack frame setup
- Parameter address transfer
- Good for most subroutines

### Slower: ENTF/ENTFN
- Static allocation
- Parameters still transferred
- Non-reentrant limitation

### Slowest: ENTB
- Heap allocation overhead
- Memory management
- Best for large/variable-sized structures

---

## Conclusion

The ND-500 calling convention is sophisticated:

1. **CALL/CALLG** calculates argument addresses and initiates the call
2. **ENTR** instructions set up the callee's environment
3. **Arguments passed by reference** (addresses, not values)
4. **Multiple entry types** for different allocation strategies
5. **Stack frame** automatically initialized with argument addresses in `B.ARG1`+

This design enables:
- **Efficient parameter passing** (no copying large structures)
- **Output parameters** (callee can modify caller's data)
- **Flexible memory management** (stack, static, heap)
- **Version compatibility** (ENTSN/ENTFN ignore extra arguments)

Understanding this calling convention is essential for:
- Writing assembly subroutines
- Implementing compilers for ND-500
- Debugging calling convention issues
- Optimizing function call overhead
