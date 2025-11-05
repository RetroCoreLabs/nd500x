# ND-500 Addressing Modes - Complete Guide

## Table of Contents
1. [Introduction](#introduction)
2. [Overview of All 14 Addressing Modes](#overview)
3. [Detailed Explanations](#detailed-explanations)
4. [Comparison and When to Use Each Mode](#comparison)
5. [Advanced Topics](#advanced-topics)

---

## Introduction

### What is an Addressing Mode?

An **addressing mode** is a way to specify where an operand (data) is located. Think of it like giving directions to find something:
- "It's in your pocket" → REGISTER addressing
- "It's written right here" → CONSTANT addressing
- "It's at house #42 on Main Street" → ABSOLUTE addressing
- "It's 10 steps from where you are" → LOCAL addressing

The ND-500 has 14 different addressing modes, each optimized for different use cases.

### Why Multiple Addressing Modes?

Different programming scenarios need different ways to access data:
- **Local variables** → relative to stack frame
- **Struct fields** → relative to struct pointer
- **Array elements** → using indexing
- **Global variables** → absolute addresses
- **Function parameters** → different memory domain

---

## Overview of All 14 Addressing Modes

| # | Mode | Symbol | Speed | Common Use |
|---|------|--------|-------|------------|
| 1 | Constant | `value` | ⚡⚡⚡ | Immediate values, array bounds |
| 2 | Register | `Rn` | ⚡⚡⚡ | Loop counters, temps |
| 3 | Local | `B.disp` | ⚡⚡ | Local variables |
| 4 | Record | `R.disp` | ⚡⚡ | Struct/record fields |
| 5 | Absolute | `addr` | ⚡ | Global variables |
| 6 | Local Indirect | `IND(B.disp)` | ⚡ | Pointers to local data |
| 7 | Record Indirect | `IND(R.disp)` | ⚡ | Pointers in structs |
| 8 | Absolute Indirect | `IND(addr)` | ⚡ | Global pointers |
| 9 | Pre-indexed | `B.disp(Rn)` | ⚡⚡ | Array access before base |
| 10 | Post-indexed Local | `B.disp(Rn)` | ⚡⚡ | Array access after base |
| 11 | Post-indexed Record | `R.disp(Rn)` | ⚡⚡ | Arrays in structs |
| 12 | Descriptor | `DESC(...)` | ⚡ | Strings and dynamic arrays |
| 13 | Alternative Domain | `ALT(...)` | ⚡ | Cross-domain parameters |
| 14 | Constant Short | `value:S` | ⚡⚡⚡ | Small immediate values |

---

## Detailed Explanations

### 1. CONSTANT - Immediate Values

**What it is:** The value is embedded directly in the instruction.

**Syntax:** Just write the number
```assembly
12          ; decimal
0x10        ; hexadecimal
020B        ; octal (with B suffix)
```

**How it works:**
```
Instruction memory:
[OPCODE] [VALUE_BYTE1] [VALUE_BYTE2] ...
```
The CPU reads the value directly from the next bytes after the opcode.

**Example:**
```assembly
W1 ADD 42               ; Add constant 42 to W1
H2 COMP 100             ; Compare H2 with constant 100
BY3 AND 0xFF            ; Mask BY3 with 0xFF
```

**Real-world scenario:**
```assembly
; Initialize loop counter
W1 =: 0                 ; W1 := 0

; Set maximum iterations
W2 =: 1000              ; W2 := 1000

; Fixed array bounds
H1 CIND B.INDEX, 0, 99  ; Array[0..99]
```

**Advantages:**
- ✓ Fastest - no memory access
- ✓ Compact code
- ✓ Perfect for literals and bounds

**Limitations:**
- ✗ Value can't change at runtime
- ✗ Limited to what fits in instruction (1, 2, or 4 bytes)

---

### 2. REGISTER - Direct Register Access

**What it is:** The operand IS a register (not memory).

**Syntax:**
```assembly
Rn      ; Generic register (R1-R4)
Wn      ; Word register (W1-W4)
Hn      ; Halfword register (H1-H4)
BYn     ; Byte register (BY1-BY4)
Fn      ; Float register (F1-F4)
Dn      ; Double register (D1-D4)
```

**How it works:**
```
CPU Registers:
┌──────────────┐
│ W1: 00001234 │ ← Direct access, no memory involved
│ W2: 00005678 │
│ W3: 0000ABCD │
│ W4: 0000EF00 │
└──────────────┘
```

**Example:**
```assembly
; Register-to-register operations
W1 ADD W2               ; W1 := W1 + W2
H3 COMP H4              ; Compare H3 with H4

; Using registers as operands
W1 CIND W2, W3, W4      ; All operands in registers
```

**Real-world scenario:**
```assembly
; Fast loop with registers only
        W1 =: 0         ; counter
        W2 =: 1000      ; limit
loop:
        ; ... do work with W1 ...
        W1 ADD 1        ; W1++
        W1 COMP W2      ; compare
        IF < GO loop    ; loop if W1 < W2
```

**Advantages:**
- ✓ Fastest - no memory access (1-2 cycles)
- ✓ Perfect for hot loops
- ✓ Compiler's favorite for temporaries

**Limitations:**
- ✗ Only 4 registers per type
- ✗ Data lost if not saved

---

### 3. LOCAL - Stack-Relative Addressing

**What it is:** Address relative to the Base register (B), which points to the current stack frame.

**Syntax:**
```assembly
B.displacement          ; Short form
B.displacement:S        ; Short displacement (word units)
B.displacement:B        ; Byte displacement
B.displacement:H        ; Halfword displacement
B.displacement:W        ; Word displacement
```

**How it works:**
```
Stack Frame:
                ← B register points here
┌──────────────┐ B+0
│ OLD_B        │
├──────────────┤ B+4
│ RETURN_ADDR  │
├──────────────┤ B+8
│ LOCAL_VAR1   │ ← B.8 accesses this
├──────────────┤ B+12
│ LOCAL_VAR2   │ ← B.12 accesses this
├──────────────┤ B+16
│ LOCAL_VAR3   │ ← B.16 accesses this
└──────────────┘
```

**Effective Address Calculation:**
```
EA = (B register) + displacement
```

**Example:**
```assembly
; Function prologue - allocate local space
        ENTB 20         ; Reserve 20 bytes for locals

; Access local variables
        W1 =: B.0       ; W1 := LOCAL_VAR1
        W2 =: B.4       ; W2 := LOCAL_VAR2

        W1 ADD B.8      ; W1 := W1 + LOCAL_VAR3
        W3 =: B.12      ; W3 := LOCAL_VAR4
```

**Real-world scenario:**
```assembly
; C equivalent: int sum(int a, int b) { int temp = a + b; return temp; }
sum:
        ENTB 4          ; Allocate 4 bytes for 'temp'

        ; Parameters are at negative offsets from B
        W1 =: B.-8      ; W1 := parameter 'a'
        W1 ADD B.-4     ; W1 += parameter 'b'
        W1 =: B.0       ; temp := W1

        W1 =: B.0       ; Load temp for return
        RET             ; Return
```

**Advantages:**
- ✓ Natural for local variables
- ✓ Position-independent (relative addressing)
- ✓ Automatic with function calls

**Limitations:**
- ✗ Requires stack frame setup
- ✗ Slower than registers (memory access)
- ✗ Limited by displacement size

---

### 4. RECORD - Structure-Relative Addressing

**What it is:** Address relative to the Record register (R), which points to a data structure.

**Syntax:**
```assembly
R.displacement
R.displacement:B        ; Byte displacement
R.displacement:H        ; Halfword displacement
R.displacement:W        ; Word displacement
```

**How it works:**
```
Data Structure in Memory:
                ← R register points here
┌──────────────┐ R+0
│ FIELD1       │ ← R.0
├──────────────┤ R+4
│ FIELD2       │ ← R.4
├──────────────┤ R+8
│ FIELD3       │ ← R.8
├──────────────┤ R+12
│ FIELD4       │ ← R.12
└──────────────┘
```

**Effective Address Calculation:**
```
EA = (R register) + displacement
```

**Example:**
```assembly
; Define structure layout
; struct Point { int x; int y; int z; }
; sizeof(Point) = 12 bytes

        ; Load structure pointer
        W1 =: B.POINT_PTR       ; Get pointer to Point
        W1 =: R                 ; R := pointer

        ; Access fields
        W2 =: R.0               ; W2 := point.x
        W3 =: R.4               ; W3 := point.y
        W4 =: R.8               ; W4 := point.z

        ; Modify a field
        W2 ADD 10
        W2 =: R.0               ; point.x += 10
```

**Real-world scenario:**
```assembly
; struct Employee { char name[32]; int age; int salary; int dept; }
; Access employee record

process_employee:
        ; R points to Employee structure

        ; Check age
        W1 =: R.32              ; age is at offset 32 (after name)
        W1 COMP 65              ; Compare with 65
        IF >= GO retirement     ; If age >= 65

        ; Adjust salary
        W2 =: R.36              ; salary at offset 36
        W2 ADD 5000             ; Raise by $5000
        W2 =: R.36              ; Store back

        ; Check department
        W3 =: R.40              ; dept at offset 40
        W3 COMP 10
        IF = GO dept10_handler

        RET

retirement:
        ; ... handle retirement ...
        RET
```

**Advantages:**
- ✓ Perfect for structs/records
- ✓ Clean, readable code
- ✓ Natural OOP-style field access

**Limitations:**
- ✗ Must load R register first
- ✗ Only one active record at a time
- ✗ Memory access overhead

---

### 5. ABSOLUTE - Direct Memory Addressing

**What it is:** The operand specifies a complete 32-bit memory address.

**Syntax:**
```assembly
0x1000          ; Hexadecimal address
04000B          ; Octal address with B suffix
```

**How it works:**
```
Memory Map:
0x00000000 ┌────────────┐
           │ OS Kernel  │
0x00100000 ├────────────┤
           │ Program    │
0x00200000 ├────────────┤
           │ Globals    │ ← 0x00201000
           │  VAR1      │ ← Absolute address
           │  VAR2      │
0x00300000 ├────────────┤
           │ Heap       │
           │            │
0xFFFFFFFF └────────────┘
```

**Effective Address:**
```
EA = address_in_instruction
```

**Example:**
```assembly
; Access global variables at fixed addresses
W1 =: 0x00201000        ; Load global VAR1
W2 =: 0x00201004        ; Load global VAR2
W1 ADD W2
W1 =: 0x00201008        ; Store to global RESULT

; Memory-mapped I/O
W3 =: 0xFFFF1000        ; Read from I/O port
W3 AND 0xFF             ; Mask bits
W3 =: 0xFFFF2000        ; Write to output port
```

**Real-world scenario:**
```assembly
; Memory-mapped hardware registers
UART_DATA    = 0xFFFF0000
UART_STATUS  = 0xFFFF0004
UART_CONTROL = 0xFFFF0008

send_char:
        ; Wait for transmitter ready
wait_ready:
        W1 =: UART_STATUS       ; Read status register
        W1 AND 0x01             ; Check bit 0 (TX ready)
        IF = GO wait_ready      ; Loop if not ready

        ; Send character
        W2 =: B.CHAR_TO_SEND    ; Get character
        W2 =: UART_DATA         ; Write to data register

        RET
```

**Advantages:**
- ✓ Access any memory location
- ✓ Perfect for memory-mapped I/O
- ✓ Good for global variables
- ✓ No register setup needed

**Limitations:**
- ✗ Slowest addressing mode
- ✗ Not position-independent
- ✗ 4 bytes for address in instruction
- ✗ Code is not relocatable

---

### 6. LOCAL INDIRECT - Pointer Through Stack

**What it is:** The address stored in local memory points to the actual data.

**Syntax:**
```assembly
IND(B.displacement)
IND(B.displacement:B)
IND(B.displacement:H)
IND(B.displacement:W)
```

**How it works:**
```
Step 1: Get pointer from stack
Step 2: Use pointer to get data

Stack:              Memory:
B+0 ┌────────┐
    │ ...    │
B+8 ├────────┤     0x1234 ┌────────┐
    │ 0x1234 │────────────→│  DATA  │ ← Final data
B+12├────────┤             └────────┘
    │ ...    │
    └────────┘
```

**Effective Address:**
```
EA = Memory[B + displacement]  ; Two-step process
```

**Example:**
```assembly
; Pointer to an integer on stack
        W1 =: B.PTR             ; Direct: W1 = pointer value
        W2 =: IND(B.PTR)        ; Indirect: W2 = *pointer

; Modify through pointer
        W3 =: IND(B.PTR)        ; W3 = *ptr
        W3 ADD 10
        W3 =: IND(B.PTR)        ; *ptr = W3
```

**Real-world scenario:**
```assembly
; C equivalent: void increment(int *ptr) { (*ptr)++; }
increment:
        ENTB 0                  ; No local vars needed

        ; Parameter 'ptr' at B.-4
        W1 =: IND(B.-4)         ; W1 = *ptr (dereference)
        W1 ADD 1                ; W1++
        W1 =: IND(B.-4)         ; *ptr = W1

        RET

; Linked list traversal
traverse_list:
        W1 =: B.LIST_HEAD       ; W1 = head pointer

loop:
        W1 COMP 0               ; Check if NULL
        IF = GO done            ; If NULL, done

        ; Process current node
        W2 =: IND(W1)           ; W2 = node->data (assuming data at offset 0)
        ; ... do something with W2 ...

        ; Move to next node
        W1 =: IND(W1+4)         ; W1 = node->next (assuming next at offset 4)
        GO loop

done:
        RET
```

**Advantages:**
- ✓ Essential for pointers
- ✓ Dynamic data structures
- ✓ Flexible indirection

**Limitations:**
- ✗ Two memory accesses (slow)
- ✗ Null pointer errors possible
- ✗ More complex debugging

---

### 7. RECORD INDIRECT - Pointer in Structure

**What it is:** Follow a pointer stored in a record/structure field.

**Syntax:**
```assembly
IND(R.displacement)
```

**How it works:**
```
Record:             Memory:
R+0 ┌────────┐
    │ field1 │
R+4 ├────────┤     0x5678 ┌────────┐
    │ 0x5678 │────────────→│  DATA  │ ← Final data
R+8 ├────────┤             └────────┘
    │ field3 │
    └────────┘
```

**Example:**
```assembly
; struct Node { int data; struct Node *next; }

        ; R points to Node
        W1 =: R.0               ; W1 = node->data (direct)
        W2 =: IND(R.4)          ; W2 = node->next->data (indirect)
```

**Real-world scenario:**
```assembly
; struct Person { char name[32]; int age; Person *spouse; Person *boss; }

check_relations:
        ; R points to Person

        ; Check spouse's age
        W1 =: R.32              ; Get person's age
        W2 =: IND(R.36)         ; spouse pointer at offset 36
        W3 =: IND(W2+32)        ; spouse->age

        W1 COMP W3              ; Compare ages
        IF > GO person_older

        ; Check boss's age
        W4 =: IND(R.40)         ; boss pointer at offset 40
        W5 =: IND(W4+32)        ; boss->age

        W1 COMP W5
        IF < GO boss_older

        RET
```

**Advantages:**
- ✓ Navigate data structures
- ✓ Object-oriented access patterns
- ✓ Clean syntax for linked structures

**Limitations:**
- ✗ Two memory accesses
- ✗ Requires proper pointer initialization

---

### 8. ABSOLUTE INDIRECT - Global Pointer

**What it is:** Follow a pointer at a fixed global address.

**Syntax:**
```assembly
IND(absolute_address)
```

**Example:**
```assembly
; Global pointer at 0x1000
GLOBAL_PTR = 0x1000

        W1 =: IND(GLOBAL_PTR)           ; W1 = *global_ptr
        W1 ADD 100
        W1 =: IND(GLOBAL_PTR)           ; *global_ptr += 100
```

**Real-world scenario:**
```assembly
; System table with function pointers
SYS_CALL_TABLE = 0x1000

        ; Call function through table
        W1 =: 5                         ; System call number 5
        W2 =: IND(SYS_CALL_TABLE)       ; Get function pointer
        ; ... prepare parameters ...
        CALL W2                         ; Call through pointer
```

---

### 9-11. INDEXED ADDRESSING - Array Access

**What it is:** Add an index (scaled by data type size) to a base address.

**Syntax:**
```assembly
B.displacement(Rn)      ; Local post-indexed
R.displacement(Rn)      ; Record post-indexed
```

**How it works:**
```
Array in memory:
            Base ─────────┐
B+disp → ┌────────┐       │
         │ [0]    │       │
         ├────────┤       │
         │ [1]    │       │  Index * element_size
         ├────────┤       │         ↓
         │ [2]    │←──────┴─── EA = Base + Index*Size
         ├────────┤
         │ [3]    │
         └────────┘
```

**Effective Address:**
```
EA = (B) + displacement + (Rn) * scale_factor

Where scale_factor depends on data type:
- BI (bit):      1/8 byte
- BY (byte):     1 byte
- H (halfword):  2 bytes
- W (word):      4 bytes
- F (float):     4 bytes
- D (double):    8 bytes
```

**Example:**
```assembly
; int array[10] declared as local variable
; Access array[i] where i is in W2

        ; Array starts at B.0, element size = 4 bytes (word)
        W1 =: B.0(W2)           ; W1 = array[W2]

; Loop through array
        W2 =: 0                 ; i = 0
loop:
        W2 COMP 10              ; Check if i < 10
        IF >= GO done

        W1 =: B.ARRAY(W2)       ; W1 = array[i]
        W1 ADD 1                ; W1++
        W1 =: B.ARRAY(W2)       ; array[i]++

        W2 ADD 1                ; i++
        GO loop

done:
```

**Real-world scenario:**
```assembly
; Matrix multiplication: C[i][j] = Sum(A[i][k] * B[k][j])
; A is M×K, B is K×N, C is M×N

matrix_multiply:
        W1 =: 0                 ; i = 0
outer:
        W2 =: 0                 ; j = 0
middle:
        W3 =: 0                 ; k = 0
        W4 =: 0                 ; sum = 0

inner:
        ; A[i][k]: i*K + k
        W5 =: W1                ; temp = i
        W5 MULTIPLY K           ; temp *= K
        W5 ADD W3               ; temp += k
        H5 =: B.A(W5)           ; H5 = A[i][k]

        ; B[k][j]: k*N + j
        W6 =: W3                ; temp = k
        W6 MULTIPLY N           ; temp *= N
        W6 ADD W2               ; temp += j
        H6 =: B.B(W6)           ; H6 = B[k][j]

        ; sum += A[i][k] * B[k][j]
        H5 MULTIPLY H6
        W4 ADD H5

        ; k++
        W3 ADD 1
        W3 COMP K
        IF < GO inner

        ; C[i][j] = sum
        W7 =: W1                ; temp = i
        W7 MULTIPLY N           ; temp *= N
        W7 ADD W2               ; temp += j
        W4 =: B.C(W7)           ; C[i][j] = sum

        ; j++
        W2 ADD 1
        W2 COMP N
        IF < GO middle

        ; i++
        W1 ADD 1
        W1 COMP M
        IF < GO outer

        RET
```

---

### 12. DESCRIPTOR - Dynamic Arrays and Strings

**What it is:** Array descriptor containing length and base address.

**Syntax:**
```assembly
DESC(operand)(index_register)
```

**Descriptor Structure:**
```
Descriptor (8 bytes):
┌─────────────────┐ +0
│ LENGTH (4 bytes)│  Number of elements
├─────────────────┤ +4
│ BASE (4 bytes)  │  Address of element [0]
└─────────────────┘
```

**How it works:**
```
Memory:
        ┌──────────────┐
        │ LENGTH: 100  │  Descriptor
        ├──────────────┤
        │ BASE: 0x2000 │
        └──────────────┘
               │
               └──────────────┐
                              ↓
                    0x2000 ┌──────┐
                           │ [0]  │
                           ├──────┤
                           │ [1]  │
                           ├──────┤
                           │ [2]  │  ← DESC(...)(2) accesses this
                           ├──────┤
                           │ ...  │
                           └──────┘

EA = DESCRIPTOR.BASE + (index * scale_factor)
```

**Example:**
```assembly
; String descriptor in B.STR_DESC
; Access character at index W2

        BY1 =: DESC(B.STR_DESC)(W2)     ; BY1 = string[W2]

; Array descriptor
        W1 =: DESC(B.ARRAY_DESC)(W3)    ; W1 = array[W3]
```

**Real-world scenario:**
```assembly
; String operations with descriptors

string_copy:
        ; Source descriptor at B.SRC_DESC
        ; Dest descriptor at B.DEST_DESC

        W1 =: 0                         ; i = 0
        W2 =: B.SRC_DESC                ; Get source length

loop:
        W1 COMP W2                      ; Check if i < length
        IF >= GO done

        BY1 =: DESC(B.SRC_DESC)(W1)     ; BY1 = src[i]
        BY1 =: DESC(B.DEST_DESC)(W1)    ; dest[i] = BY1

        W1 ADD 1                        ; i++
        GO loop

done:
        RET

; Bounds checking is automatic!
; Accessing beyond array length causes ILLEGAL INDEX trap
```

**Advantages:**
- ✓ Automatic bounds checking
- ✓ Polymorphic arrays (any size)
- ✓ Perfect for strings
- ✓ Safe memory access

**Limitations:**
- ✗ 8-byte overhead per array
- ✗ Slower (descriptor lookup + bounds check)
- ✗ Requires descriptor setup

---

### 13. ALTERNATIVE DOMAIN (ALT) - Cross-Domain Access

**What it is:** Access data in a different memory protection domain.

**Syntax:**
```assembly
ALT(operand)
```

**How it works:**
```
Domain A (Calling):         Domain B (Called):
┌──────────────┐            ┌──────────────┐
│ Parameters   │            │ Code         │
│  - arg1      │←───ALT─────┤              │
│  - arg2      │            │              │
└──────────────┘            └──────────────┘
    Current                  Current Executing
    Alternative              Domain (CED)
    Domain (CAD)
```

**Example:**
```assembly
; In Domain B, access parameters from calling Domain A

subroutine:
        ; Parameters are in caller's domain
        W1 =: ALT(B.-4)         ; Get parameter from caller
        W2 =: ALT(B.-8)         ; Get another parameter

        W1 ADD W2

        W1 =: ALT(B.-4)         ; Return result to caller
        RET
```

**Real-world scenario:**
```assembly
; System call from user domain to kernel domain

user_program:
        ; Setup parameters in user domain
        W1 =: 10                ; Parameter 1
        W1 =: B.PARAM1
        W2 =: 20                ; Parameter 2
        W2 =: B.PARAM2

        ; Call kernel function
        CALLG kernel_func

        RET

kernel_func:
        ; Running in kernel domain
        ; Access user parameters with ALT

        W1 =: ALT(B.PARAM1)     ; Read from user domain
        W2 =: ALT(B.PARAM2)

        ; Do privileged operation
        W1 ADD W2

        ; Return result to user domain
        W1 =: ALT(B.RESULT)

        RET
```

**Advantages:**
- ✓ Memory protection
- ✓ Safe system calls
- ✓ Inter-process communication
- ✓ Modular security

**Limitations:**
- ✗ Slower (domain boundary)
- ✗ Complex setup
- ✗ Must have proper permissions

---

### 14. CONSTANT SHORT - Compact Immediates

**What it is:** Small constants in word units for compact encoding.

**Syntax:**
```assembly
value:S
```

**How it works:**
```
Standard constant:  [OPCODE] [VALUE_4_BYTES]
Short constant:     [OPCODE_WITH_VALUE]    ; Value in opcode

Short form saves 3 bytes per instruction!
```

**Example:**
```assembly
B.0:S           ; Displacement 0 in word units
B.4:S           ; Displacement 1 word (=4 bytes)
B.8:S           ; Displacement 2 words (=8 bytes)
B.12:S          ; Displacement 3 words (=12 bytes)
```

**Advantages:**
- ✓ Compact code
- ✓ Slightly faster
- ✓ Common case optimization

**Limitations:**
- ✗ Limited range (usually 0-15)
- ✗ Word units only

---

## Comparison and When to Use Each Mode

### Performance Ranking (Fastest to Slowest)

1. **REGISTER** (1-2 cycles)
   ```assembly
   W1 ADD W2
   ```

2. **CONSTANT** (1-3 cycles)
   ```assembly
   W1 ADD 42
   ```

3. **LOCAL / RECORD** (3-5 cycles)
   ```assembly
   W1 ADD B.VAR
   ```

4. **INDEXED** (4-6 cycles)
   ```assembly
   W1 ADD B.ARRAY(W2)
   ```

5. **ABSOLUTE** (5-7 cycles)
   ```assembly
   W1 ADD 0x1000
   ```

6. **INDIRECT** (6-10 cycles)
   ```assembly
   W1 ADD IND(B.PTR)
   ```

7. **DESCRIPTOR** (8-12 cycles)
   ```assembly
   W1 ADD DESC(B.DESC)(W2)
   ```

8. **ALTERNATIVE DOMAIN** (10-15 cycles)
   ```assembly
   W1 ADD ALT(B.VAR)
   ```

### Decision Tree: Which Mode to Use?

```
START
  │
  ├─ Is it a literal/constant?
  │   └─ YES → CONSTANT
  │
  ├─ Already in a register?
  │   └─ YES → REGISTER
  │
  ├─ Local variable?
  │   └─ YES → LOCAL (B.disp)
  │
  ├─ Struct/record field?
  │   └─ YES → RECORD (R.disp)
  │
  ├─ Array element?
  │   ├─ Need bounds checking? → DESCRIPTOR
  │   ├─ Fixed bounds? → INDEXED
  │   └─ Dynamic? → INDIRECT
  │
  ├─ Global variable?
  │   └─ YES → ABSOLUTE
  │
  ├─ Pointer?
  │   ├─ In local? → IND(B.disp)
  │   ├─ In record? → IND(R.disp)
  │   └─ Global? → IND(absolute)
  │
  └─ Cross-domain?
      └─ YES → ALT(...)
```

---

## Advanced Topics

### Combining Addressing Modes

Some modes can be combined:

```assembly
; Descriptor in alternative domain with post-indexing
ALT(DESC(B.DESC)(W1))

; Indirect post-indexed
IND(B.PTR)(W2)

; Pre-indexed descriptor
DESC(B.ARRAY:H(W1))(W2)
```

### Addressing Mode Encoding

Each addressing mode has a unique 8-bit code:

```
Format: [7:6][5:0]
        ││││││└─────┴─ Mode-specific data
        │└─────────────Addressing mode class

Examples:
0xC1 = Local, byte displacement
0xC2 = Local, halfword displacement
0xD4 = Local post-indexed, byte displacement
0xE0 = Register R0
```

### Compiler Optimization

Compilers prefer this order:

1. **REGISTER** - Keep hot variables in registers
2. **CONSTANT** - Inline literals
3. **LOCAL** - Stack variables
4. **INDEXED** - Array access
5. **Other modes** - Only when necessary

### Example: Optimized vs Non-Optimized

**Non-optimized:**
```assembly
; Every access goes to memory
W1 =: B.A           ; Load A
W2 =: B.B           ; Load B
W1 ADD W2           ; A + B
W3 =: W1
W3 =: B.C           ; Store to C

W1 =: B.A           ; Load A again!
W2 =: B.B           ; Load B again!
W1 ADD W2
W1 MULTIPLY 2
W1 =: B.D
```

**Optimized:**
```assembly
; Load once, keep in registers
W1 =: B.A           ; Load A
W2 =: B.B           ; Load B
W3 =: W1            ; C = A + B
W3 ADD W2
W3 =: B.C

W1 ADD W2           ; Still in W1, W2!
W1 MULTIPLY 2
W1 =: B.D
```

Savings: 4 memory accesses eliminated!

---

## Summary

The ND-500 provides 14 addressing modes for maximum flexibility:

- **CONSTANT** and **REGISTER** for speed
- **LOCAL** and **RECORD** for structured data
- **ABSOLUTE** for globals and I/O
- **INDIRECT** for pointers
- **INDEXED** for arrays
- **DESCRIPTOR** for safe arrays
- **ALT** for security domains

Choose based on:
1. **Speed requirements** (hot loops → registers)
2. **Data location** (stack vs heap vs global)
3. **Safety requirements** (bounds checking?)
4. **Code size** (compact vs readable)

Master these modes and you master the ND-500!
