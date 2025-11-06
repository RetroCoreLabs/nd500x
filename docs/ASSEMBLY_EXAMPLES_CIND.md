# ND-500 Assembly Examples: CIND Instruction

## Understanding CIND (Calculate Index)

**Purpose:** Calculate the address of an element in a multi-dimensional array.

**Operation:** `Rn * (<upper> - <lower> + 1) + <index> -> Rn`

**Format:** `tn CIND <index/r/t>, <lower/r/t>, <upper/r/t>`

Where:
- `t` = data type prefix (BY, H, W, F, D)
- `n` = register number (1, 2, 3, 4)
- Three operands: index, lower bound, upper bound

---

## Part 1: Understanding Prefixes (Data Types)

The prefix determines the data type and which register is used:

| Prefix | Meaning | Data Size | Register | Opcode Base |
|--------|---------|-----------|----------|-------------|
| **BY** | Byte | 8 bits | BYn (BY1-BY4) | 0xFD14-0xFD17 |
| **H** | Halfword | 16 bits | Hn (H1-H4) | 0xFD18-0xFD1B |
| **W** | Word | 32 bits | Wn (W1-W4) | 0x00B0-0x00B3 |
| **F** | Float | 32 bits | Fn (F1-F4) | 0xFFD0-0xFFD3 |
| **D** | Double | 64 bits | Dn (D1-D4) | 0xFFD4-0xFFD7 |

---

## Part 2: All 12 Variants with Specific Opcodes

### Byte Variants (BY1-BY4) - Opcodes 0xFD14 to 0xFD17

```assembly
; Variant 1/12: BY1 CIND (opcode 0xFD14)
BY1 CIND B.INDEX, B.LOWER, B.UPPER

; Variant 2/12: BY2 CIND (opcode 0xFD15)
BY2 CIND B.INDEX, B.LOWER, B.UPPER

; Variant 3/12: BY3 CIND (opcode 0xFD16)
BY3 CIND B.INDEX, B.LOWER, B.UPPER

; Variant 4/12: BY4 CIND (opcode 0xFD17)
BY4 CIND B.INDEX, B.LOWER, B.UPPER
```

### Halfword Variants (H1-H4) - Opcodes 0xFD18 to 0xFD1B

```assembly
; Variant 5/12: H1 CIND (opcode 0xFD18)
H1 CIND B.INDEX, B.LOWER, B.UPPER

; Variant 6/12: H2 CIND (opcode 0xFD19)
H2 CIND B.INDEX, B.LOWER, B.UPPER

; Variant 7/12: H3 CIND (opcode 0xFD1A)
H3 CIND B.INDEX, B.LOWER, B.UPPER

; Variant 8/12: H4 CIND (opcode 0xFD1B)
H4 CIND B.INDEX, B.LOWER, B.UPPER
```

### Word Variants (W1-W4) - Opcodes 0x00B0 to 0x00B3

```assembly
; Variant 9/12: W1 CIND (opcode 0x00B0)
W1 CIND B.INDEX, B.LOWER, B.UPPER

; Variant 10/12: W2 CIND (opcode 0x00B1)
W2 CIND B.INDEX, B.LOWER, B.UPPER

; Variant 11/12: W3 CIND (opcode 0x00B2)
W3 CIND B.INDEX, B.LOWER, B.UPPER

; Variant 12/12: W4 CIND (opcode 0x00B3)
W4 CIND B.INDEX, B.LOWER, B.UPPER
```

---

## Part 3: All Addressing Modes

CIND supports 6 addressing modes for each of its 3 operands:

### 1. LOCAL Addressing (B.<displacement>)

**Access relative to Base register (B)**

```assembly
; Example: Calculate index for ARR(IX1, IX2, IX3) declared as ARR(1..3, 5..10, 2..9)
H1 CIND B.IX1, 1, 3          ; Operand 1: local var at B+IX1
H1 CIND B.IX2, 5, 10         ; Operand 1: local var at B+IX2
H1 CIND B.IX3, 2, 9          ; Operand 1: local var at B+IX3
```

**Explanation:**
- B = Base register (points to local variables)
- B.IX1 = memory location at (B) + IX1 offset
- Typically used for accessing local variables on the stack

### 2. RECORD Addressing (R.<displacement>)

**Access relative to Record register (R)**

```assembly
; Example: Accessing fields in a record structure
W1 CIND R.YEAR_INDEX, R.MIN_YEAR, R.MAX_YEAR
```

**Explanation:**
- R = Record register (points to a data structure)
- R.YEAR_INDEX = memory location at (R) + YEAR_INDEX offset
- Used for accessing fields within a record/struct

### 3. CONSTANT Addressing

**Use immediate values**

```assembly
; Example: Array with fixed bounds ARR(0..99)
H1 CIND B.I, 0, 99           ; Lower=0, Upper=99 are constants

; Example: 3D array ARR(1..10, 1..20, 1..30)
W1 CIND B.I, 1, 10           ; First dimension
W1 CIND B.J, 1, 20           ; Second dimension
W1 CIND B.K, 1, 30           ; Third dimension
```

**Explanation:**
- Constants are embedded directly in instruction
- No memory access needed for these operands
- Most efficient for known array bounds

### 4. REGISTER Addressing

**Use register contents directly**

```assembly
; Example: Dynamic bounds stored in registers
W2 CIND W3, W4, W1           ; index in W3, lower in W4, upper in W1

; Example: Loop with register-based indexing
; Calculate offset for MATRIX(I, J) where I and J are in registers
H1 CIND H2, 1, ROWS          ; H2 contains I
H1 CIND H3, 1, COLS          ; H3 contains J
```

**Explanation:**
- Operands are register values, not memory addresses
- Fastest: no memory access for operands
- Used when bounds/index are already in registers

### 5. PRE_INDEXED Addressing (with register indexing)

**Base address + displacement + (register * scale)**

```assembly
; Example: Array of descriptors, indexed
W1 CIND DESC(B.ARRAY_DESC)(W2), 0, DESC(B.UPPER_DESC)(W2)

; Example: Table of array bounds
H1 CIND B.INDEX, B.BOUNDS:H(W3), B.BOUNDS:H(W4)
```

**Explanation:**
- B.BOUNDS:H(W3) = address at (B + BOUNDS_offset + W3*2)
- The :H means halfword scale (multiply W3 by 2)
- Used for accessing elements in tables of bounds

### 6. ABSOLUTE Addressing

**Direct memory address**

```assembly
; Example: Global array with absolute addresses
W1 CIND 0x1000, 0x2000, 0x3000     ; All operands at absolute addresses

; Example: Memory-mapped I/O or fixed locations
H1 CIND 0xA000, 0xA010, 0xA020     ; Fixed memory locations
```

**Explanation:**
- Address specified directly (not relative to any register)
- Used for global variables or memory-mapped devices
- Slowest: requires full 32-bit address

---

## Part 4: Complex Real-World Examples

### Example 1: 3D Array Access - Mixed Addressing Modes

```assembly
; Declare: CUBE(0..9, 0..9, 0..9) of integer
; Calculate address of CUBE(I, J, K)
; I, J, K are local variables

        ; First dimension
        W1 CIND B.I, 0, 9            ; LOCAL + CONSTANT
        ; W1 now contains: W1 * (9-0+1) + I = W1*10 + I

        ; Second dimension
        W1 CIND B.J, 0, 9            ; LOCAL + CONSTANT
        ; W1 now contains: W1*10 + J

        ; Third dimension
        W1 CIND B.K, 0, 9            ; LOCAL + CONSTANT
        ; W1 now contains: W1*10 + K

        ; W1 now contains the linear index
        ; Multiply by element size and add base address
```

### Example 2: Dynamic Bounds from Record

```assembly
; Struct describing array: { min, max, data[] }
; Access element ARRAY[index] with bounds checking

        W1 CIND B.index, R.min, R.max    ; RECORD addressing for bounds
```

**Explanation:**
- Operand 1 (index): LOCAL addressing (B.index)
- Operand 2 (lower): RECORD addressing (R.min)
- Operand 3 (upper): RECORD addressing (R.max)
- Bounds stored in record structure pointed to by R register

### Example 3: Register-Only for Maximum Speed

```assembly
; Hot loop: all values already in registers
; MATRIX(i, j) where i in H2, j in H3, bounds in H4

loop_start:
        H1 CIND H2, 1, H4            ; i vs NROWS (all registers)
        H1 CIND H3, 1, H4            ; j vs NCOLS (all registers)
        ; ... do work ...
        ; increment H2, H3
        ; ...
        ; jump loop_start
```

### Example 4: Pre-indexed Array of Array Descriptors

```assembly
; Array of descriptors: ARRAYS[10] where each element has [min, max]
; Access ARRAYS[n].min and ARRAYS[n].max

        ; n is in W2
        W1 CIND B.index,
                DESC(B.ARRAYS)(W2).MIN,      ; Pre-indexed descriptor
                DESC(B.ARRAYS)(W2).MAX       ; Pre-indexed descriptor
```

---

## Part 5: Trap Conditions and Examples

### Illegal Index Trap (IX)

```assembly
; Array declared as ARR(5..10)
H1 CIND 3, 5, 10            ; TRAP! index=3 < lower=5
H1 CIND 15, 5, 10           ; TRAP! index=15 > upper=10
H1 CIND 7, 5, 10            ; OK: 5 <= 7 <= 10
```

**What happens:**
- Flag K bit is set to 1
- Illegal Index trap condition occurs
- Trap handler can catch array bounds violation

### Integer Overflow Trap (O)

```assembly
; Overflow when: result > 2^31-1 or result < -2^31
W1 CIND 100, 0, 0x7FFFFFFF  ; Might overflow if W1 is large
```

---

## Part 6: Performance Considerations

**Fastest to Slowest:**

1. **REGISTER**: No memory access
   ```assembly
   W1 CIND W2, W3, W4           ; ~3-5 cycles
   ```

2. **CONSTANT**: Embedded in instruction
   ```assembly
   W1 CIND B.I, 0, 99           ; ~5-8 cycles
   ```

3. **LOCAL**: Single memory access
   ```assembly
   W1 CIND B.I, B.MIN, B.MAX    ; ~10-15 cycles
   ```

4. **RECORD**: Single memory access
   ```assembly
   W1 CIND R.I, R.MIN, R.MAX    ; ~10-15 cycles
   ```

5. **PRE_INDEXED**: Memory + calculation
   ```assembly
   W1 CIND B.I, B.BOUNDS(W2), B.BOUNDS(W3)  ; ~15-20 cycles
   ```

6. **ABSOLUTE**: Full address decode
   ```assembly
   W1 CIND 0x1000, 0x2000, 0x3000   ; ~20-25 cycles
   ```

---

## Summary

**12 Variants:**
- 4 Byte variants (BY1-BY4): opcodes 0xFD14-0xFD17
- 4 Halfword variants (H1-H4): opcodes 0xFD18-0xFD1B
- 4 Word variants (W1-W4): opcodes 0x00B0-0x00B3

**6 Addressing Modes per Operand:**
- LOCAL: B.<displ>
- RECORD: R.<displ>
- CONSTANT: immediate value
- REGISTER: register name
- PRE_INDEXED: <base>(<reg>)
- ABSOLUTE: memory address

**3 Operands:**
- Operand 1: index value
- Operand 2: lower bound
- Operand 3: upper bound

**Total possible combinations: 12 × 6³ = 2,592 possible instruction forms!**
