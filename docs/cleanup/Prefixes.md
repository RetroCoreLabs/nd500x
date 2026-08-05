# ND-500 Data Type Prefixes - Complete Guide

## Table of Contents
1. [Introduction - What are Prefixes?](#introduction)
2. [The Six Basic Prefixes](#the-six-basic-prefixes)
3. [Register Naming and Prefixes](#register-naming)
4. [Detailed Explanations](#detailed-explanations)
5. [Memory Layout](#memory-layout)
6. [Type Conversion](#type-conversion)
7. [Common Pitfalls](#common-pitfalls)

---

## Introduction - What are Prefixes?

### The Simple Answer

A **prefix** tells the CPU what **size** and **type** of data you're working with.

Think of it like this:
- You wouldn't use a teaspoon to measure a gallon of milk
- You wouldn't use a ruler marked in miles to measure a pencil
- **Similarly, you need to tell the CPU if you're working with a small number (byte) or a large number (word)**

### The Six Prefixes

| Prefix | Name | Size | What It's For |
|--------|------|------|---------------|
| **BI** | Bit | 1 bit | Flags, true/false |
| **BY** | Byte | 8 bits | Characters, small numbers (0-255) |
| **H** | Halfword | 16 bits | Medium numbers (±32K) |
| **W** | Word | 32 bits | Large integers (±2 billion) |
| **F** | Float | 32 bits | Decimal numbers (±10³⁸) |
| **D** | Double | 64 bits | High-precision decimals (±10³⁰⁸) |

---

## The Six Basic Prefixes

### 1. BI - Bit (Boolean)

**Size:** 1 bit
**Range:** 0 or 1 (false or true)
**Use:** Flags, switches, boolean values

**Memory representation:**
```
┌─┐
│0│  or  │1│
└─┘      └─┘
```

**Example - Boolean flags:**
```assembly
; Check if a flag is set
BI1 =: B.IS_READY          ; BI1 = flag value (0 or 1)
BI1 COMP 1                 ; Compare with 1
IF = GO ready_to_process   ; If true, jump

; Set a flag
BI2 =: 1                   ; Set to true
BI2 =: B.IS_COMPLETE

; Toggle a flag
BI3 =: B.TOGGLE_FLAG
BI3 XOR 1                  ; Flip 0↔1
BI3 =: B.TOGGLE_FLAG
```

**Real-world uses:**
- Status indicators (on/off, ready/busy, error/ok)
- Feature flags
- Permission bits
- Signal states

**Memory quirk:**
- Stored in memory as part of a byte (8 bits per byte)
- Bit numbering: right-to-left (bit 0 is rightmost)

---

### 2. BY - Byte (8 bits)

**Size:** 8 bits = 1 byte
**Range:** 0 to 255 (unsigned) or -128 to +127 (signed)
**Use:** ASCII characters, small integers, colors

**Memory representation:**
```
┌──────────┐
│ 01010110 │  (binary)
└──────────┘
     86      (decimal)
    'V'      (ASCII character)
```

**Example - Character handling:**
```assembly
; Read a character from input
BY1 =: INPUT_PORT          ; BY1 = character code

; Check if it's uppercase (A-Z)
BY1 COMP 'A'
IF < GO not_upper          ; If less than 'A'
BY1 COMP 'Z'
IF > GO not_upper          ; If greater than 'Z'

; Convert to lowercase
BY1 ADD 32                 ; lowercase = uppercase + 32
BY1 =: OUTPUT_PORT

not_upper:
        RET
```

**Example - Color components:**
```assembly
; RGB color: Red=255, Green=128, Blue=64
BY1 =: 255                 ; Red component
BY1 =: B.RED

BY2 =: 128                 ; Green component
BY2 =: B.GREEN

BY3 =: 64                  ; Blue component
BY3 =: B.BLUE

; Pack into word: 0x00RRGGBB
W1 =: BY1                  ; Start with red
W1 SHL 8                   ; Shift left 8 bits
W1 OR BY2                  ; Add green
W1 SHL 8                   ; Shift left 8 bits
W1 OR BY3                  ; Add blue
; W1 now contains: 0x00FF8040
```

**Real-world uses:**
- Text characters (ASCII, UTF-8)
- Small counters (0-255)
- Color components (RGB)
- Status codes
- Network packet headers

**Important notes:**
- When loaded into a 32-bit register, upper 24 bits are typically cleared
- Arithmetic wraps at 255 (unsigned) or ±127 (signed)

---

### 3. H - Halfword (16 bits)

**Size:** 16 bits = 2 bytes
**Range:** 0 to 65,535 (unsigned) or -32,768 to +32,767 (signed)
**Use:** Medium-sized integers, short text lengths, array indices

**Memory representation:**
```
┌──────────┬──────────┐
│ High Byte│ Low Byte │
└──────────┴──────────┘
   0x12       0x34
      0x1234 = 4660 (decimal)
```

**Example - Array size tracking:**
```assembly
; Array of up to 30,000 elements
H1 =: 0                    ; counter = 0
H2 =: 30000                ; max_size = 30000

loop:
        ; Process element at index H1
        W1 =: B.ARRAY(H1)
        ; ... process W1 ...

        ; Increment counter
        H1 ADD 1
        H1 COMP H2             ; Check if counter < max_size
        IF < GO loop

done:
        H1 =: B.ARRAY_SIZE     ; Store final size
        RET
```

**Example - String length:**
```assembly
; String with length prefix (Pascal-style string)
; First halfword is length, then characters follow

        H1 =: B.STR_LEN        ; H1 = string length
        W2 =: 0                ; i = 0

process_chars:
        W2 COMP H1             ; Check if i < length
        IF >= GO done

        BY1 =: B.STR_DATA(W2)  ; Get character at index i
        ; ... process character in BY1 ...

        W2 ADD 1               ; i++
        GO process_chars

done:
        RET
```

**Real-world uses:**
- Array sizes and indices (most arrays are < 65K elements)
- String lengths
- Loop counters
- Coordinate values (screen pixels)
- Port numbers (networking)
- File handles

**Important notes:**
- Exactly 2 bytes in memory
- Faster than word for some operations
- Upper 16 bits cleared when loaded to 32-bit register

---

### 4. W - Word (32 bits)

**Size:** 32 bits = 4 bytes
**Range:** 0 to 4,294,967,295 (unsigned) or -2,147,483,648 to +2,147,483,647 (signed)
**Use:** Pointers, large integers, addresses, most arithmetic

**Memory representation:**
```
┌────────┬────────┬────────┬────────┐
│ Byte 3 │ Byte 2 │ Byte 1 │ Byte 0 │
└────────┴────────┴────────┴────────┘
  0x12     0x34     0x56     0x78
        0x12345678 = 305,419,896 (decimal)
```

**Example - Pointer arithmetic:**
```assembly
; Allocate memory and track pointer
W1 =: HEAP_START           ; W1 = base address
W2 =: 1000                 ; W2 = number of bytes needed

W3 =: W1                   ; Save original pointer
W1 ADD W2                  ; W1 = new_address = old_address + size
W1 =: HEAP_CURRENT         ; Update heap pointer

; W3 now points to allocated memory
; Use W3 to access the allocated space
```

**Example - Large calculations:**
```assembly
; Calculate factorial of N (N! = 1 * 2 * 3 * ... * N)
; N is in W2, result in W1

        W1 =: 1                ; result = 1
        W3 =: 1                ; i = 1

fact_loop:
        W3 COMP W2             ; Check if i <= N
        IF > GO done

        W1 MULTIPLY W3         ; result *= i
        W3 ADD 1               ; i++
        GO fact_loop

done:
        W1 =: B.FACTORIAL      ; Store result
        RET

; Example: 12! = 479,001,600 (fits in 32-bit word)
```

**Example - Bit manipulation:**
```assembly
; Extract bits 8-15 from a word
W1 =: B.DATA               ; W1 = 0x12345678
W1 SHR 8                   ; W1 = 0x00123456 (shift right 8 bits)
W1 AND 0xFF                ; W1 = 0x00000056 (mask lower 8 bits)
; W1 now contains the value from bits 8-15

; Set specific bits
W2 =: B.FLAGS
W2 OR 0x00000100           ; Set bit 8
W2 =: B.FLAGS

; Clear specific bits
W2 AND 0xFFFFFEFF          ; Clear bit 8 (AND with inverse)
W2 =: B.FLAGS
```

**Real-world uses:**
- **Memory addresses** (32-bit address space = 4GB)
- **Pointers** (all pointers are 32-bit)
- Large integer math
- File sizes
- Timestamps (seconds since epoch)
- Bit fields and flags
- Most general-purpose arithmetic

**Important notes:**
- **Default size** for most operations
- Full register width (no upper bits to clear)
- Can address entire 4GB memory space
- Most efficient for CPU

---

### 5. F - Float (32-bit floating-point)

**Size:** 32 bits = 4 bytes
**Range:** ±1.4 × 10⁻⁴⁵ to ±3.4 × 10³⁸
**Precision:** ~7 decimal digits
**Use:** Scientific calculations, graphics, physics

**Memory representation (IEEE 754 single-precision):**
```
┌─┬─────────┬───────────────────────┐
│S│Exponent │     Mantissa         │
└─┴─────────┴───────────────────────┘
 1    8 bits       23 bits

S = Sign bit (0=positive, 1=negative)
Exponent = Power of 2 (biased by 127)
Mantissa = Fractional part (implicit leading 1)

Example: 3.14159
  Sign: 0 (positive)
  Exponent: 128 (actual=1, stored=128)
  Mantissa: 1.57079... (fractional part)
  Result: (-1)⁰ × 1.57079 × 2¹ ≈ 3.14159
```

**Example - Circle calculations:**
```assembly
; Calculate circle area: A = π × r²
; r is in F1, result in F2

PI = 3.14159265                ; Define constant

        F2 =: PI               ; F2 = π
        F1 MULTIPLY F1         ; F1 = r²
        F2 MULTIPLY F1         ; F2 = π × r²

        F2 =: B.AREA           ; Store area
        RET

; Example: r = 5.0
;   r² = 25.0
;   A = 3.14159 × 25.0 = 78.5398
```

**Example - Physics simulation:**
```assembly
; Calculate projectile position: x = v₀t + ½at²
; Initial velocity v₀ in F1, time t in F2, acceleration a in F3

        F4 =: F2               ; F4 = t
        F4 MULTIPLY F2         ; F4 = t²

        F4 MULTIPLY F3         ; F4 = a × t²
        F4 MULTIPLY 0.5        ; F4 = 0.5 × a × t²

        F5 =: F1               ; F5 = v₀
        F5 MULTIPLY F2         ; F5 = v₀ × t

        F5 ADD F4              ; F5 = v₀t + ½at²
        F5 =: B.POSITION

        RET
```

**Example - Temperature conversion:**
```assembly
; Convert Celsius to Fahrenheit: F = C × 9/5 + 32

        F1 =: B.CELSIUS        ; F1 = temperature in Celsius
        F1 MULTIPLY 9.0
        F1 DIVIDE 5.0
        F1 ADD 32.0
        F1 =: B.FAHRENHEIT

        RET

; Example: 25°C
;   25 × 9 = 225
;   225 / 5 = 45
;   45 + 32 = 77°F
```

**Real-world uses:**
- Scientific calculations
- 3D graphics (vertices, normals)
- Game physics
- Signal processing
- Statistical analysis
- Engineering simulations
- Financial calculations (when precision allows)

**Important notes:**
- **Not exact** - has rounding errors
  ```
  0.1 + 0.2 ≠ 0.3 (exactly)
  0.1 + 0.2 = 0.30000000447...
  ```
- **Special values:**
  - `+Infinity` (overflow)
  - `-Infinity` (negative overflow)
  - `NaN` (Not a Number - invalid operation like 0/0)
  - `+0` and `-0` (yes, two zeros!)

- **Precision loss:**
  ```assembly
  F1 =: 16777216.0       ; OK
  F1 ADD 1.0             ; F1 = 16777217.0 (WRONG! = 16777216.0)
  ; Lost precision because mantissa has only 23 bits
  ```

---

### 6. D - Double (64-bit floating-point)

**Size:** 64 bits = 8 bytes
**Range:** ±5.0 × 10⁻³²⁴ to ±1.7 × 10³⁰⁸
**Precision:** ~15 decimal digits
**Use:** High-precision science, astronomy, financial calculations

**Memory representation (IEEE 754 double-precision):**
```
┌─┬────────────┬──────────────────────────────────────────────┐
│S│ Exponent   │              Mantissa                       │
└─┴────────────┴──────────────────────────────────────────────┘
 1   11 bits                 52 bits

Twice the precision of float!
```

**Example - Astronomical calculations:**
```assembly
; Calculate distance light travels in 1 year
; Speed of light: 299,792,458 m/s
; Seconds in year: 31,536,000

LIGHT_SPEED = 299792458.0
SECONDS_PER_YEAR = 31536000.0

        D1 =: LIGHT_SPEED
        D2 =: SECONDS_PER_YEAR
        D1 MULTIPLY D2
        ; D1 = 9.46073047258 × 10¹⁵ meters (1 light-year)

        D1 =: B.LIGHT_YEAR
        RET
```

**Example - Compound interest:**
```assembly
; Calculate compound interest: A = P(1 + r/n)^(nt)
; P = principal (D1), r = rate (D2), n = compounds/year (D3), t = years (D4)

        D5 =: D2               ; D5 = r
        D5 DIVIDE D3           ; D5 = r/n

        D6 =: 1.0
        D6 ADD D5              ; D6 = 1 + r/n

        D7 =: D3               ; D7 = n
        D7 MULTIPLY D4         ; D7 = nt

        ; D6 = (1 + r/n)^(nt)  [need power function]
        CALL POWER             ; D6 = D6^D7

        D1 MULTIPLY D6         ; D1 = P × (1+r/n)^(nt)
        D1 =: B.FINAL_AMOUNT

        RET

; Example: $10,000 at 5% for 10 years, monthly compounding
;   P = 10000, r = 0.05, n = 12, t = 10
;   A = 10000 × (1 + 0.05/12)^(12×10)
;   A = 10000 × (1.0041667)^120
;   A = 10000 × 1.64700950
;   A = $16,470.10
```

**Example - Precise geometry:**
```assembly
; Calculate diagonal of a rectangle: d = √(w² + h²)
; Width in D1, height in D2

        D3 =: D1               ; D3 = width
        D3 MULTIPLY D3         ; D3 = width²

        D4 =: D2               ; D4 = height
        D4 MULTIPLY D4         ; D4 = height²

        D3 ADD D4              ; D3 = width² + height²
        D3 SQRT                ; D3 = √(width² + height²)

        D3 =: B.DIAGONAL
        RET

; Example: 1920×1080 screen
;   w² = 3,686,400
;   h² = 1,166,400
;   w²+h² = 4,852,800
;   d = 2202.90249433... (exact to 15 digits!)
```

**Real-world uses:**
- **Astronomy** (distances in space)
- **Physics** (quantum mechanics, particle physics)
- **Financial calculations** (precise to the cent over large numbers)
- **GPS coordinates** (latitude/longitude need precision)
- **Engineering** (structural analysis)
- **Scientific research** (any high-precision work)

**Important notes:**
- **8 bytes** - takes two registers (An + En)
  ```assembly
  D1 uses A1 (high 32 bits) + E1 (low 32 bits)
  D2 uses A2 + E2
  D3 uses A3 + E3
  D4 uses A4 + E4
  ```

- **Slower** than float (more data to process)

- **Still not perfect:**
  ```assembly
  ; Even double has limits!
  D1 =: 9007199254740992.0   ; 2^53 (OK)
  D1 ADD 1.0                 ; D1 = 9007199254740993.0 (WRONG! = 9007199254740992.0)
  ; Lost precision at mantissa limit
  ```

- **Special handling:**
  - Loading/storing requires both A and E registers
  - Some operations slower than single float

---

## Register Naming and Prefixes

### How Registers Change Names Based on Prefix

The ND-500 has 4 integer registers and 4 float registers, but they're **called different things** depending on the data type:

```
Physical Register:  I1    I2    I3    I4    (Integer registers)
                    A1    A2    A3    A4    (Float registers)
                    E1    E2    E3    E4    (Float extension registers)

Names by prefix:
┌─────────────────────────────────────────────────────────┐
│ BI1  BI2  BI3  BI4  - Bit operations                   │
│ BY1  BY2  BY3  BY4  - Byte operations                  │
│ H1   H2   H3   H4   - Halfword operations              │
│ W1   W2   W3   W4   - Word operations                  │
│ F1   F2   F3   F4   - Float operations (uses A1-A4)    │
│ D1   D2   D3   D4   - Double operations (uses A1-A4    │
│                       combined with E1-E4)              │
└─────────────────────────────────────────────────────────┘
```

### Example - Same Register, Different Names:

```assembly
; These all access the SAME physical register (I1):
BI1 =: 1            ; Set bit in I1
BY1 =: 0xFF         ; Set byte in I1 (clears upper bits)
H1 =: 0x1234        ; Set halfword in I1 (clears upper bits)
W1 =: 0x12345678    ; Set full word in I1

; These access the float registers:
F1 =: 3.14          ; Set A1 to 3.14
D1 =: 3.14159265    ; Set A1+E1 to 3.14159265 (double)
```

### Visual Register Layout:

```
Integer Register W1 (32 bits):
┌─┬───────┬───────────────┬───────────────────────────────┐
│0│BY1    │H1             │W1                            │
└─┴───────┴───────────────┴───────────────────────────────┘
 BI1 (bit)
  (8 bits)
           (16 bits)
                           (32 bits)

Float Register F1 (32 bits):
┌───────────────────────────────────────────────────────────┐
│A1 (F1)                                                   │
└───────────────────────────────────────────────────────────┘

Double Register D1 (64 bits):
┌───────────────────────────────┬───────────────────────────┐
│A1 (high 32 bits)              │E1 (low 32 bits)          │
└───────────────────────────────┴───────────────────────────┘
                    D1
```

---

## Memory Layout

### How Different Types are Stored

```
Memory Address:  0x1000      0x1001      0x1002      0x1003
                ┌───────────┬───────────┬───────────┬───────────┐
BI data:        │xxxxxxx0   │           │           │           │ (1 bit)
                └───────────┴───────────┴───────────┴───────────┘

BY data:        │ 01010110  │           │           │           │ (1 byte)
                └───────────┴───────────┴───────────┴───────────┘

H data:         │ High Byte │ Low Byte  │           │           │ (2 bytes)
                └───────────┴───────────┴───────────┴───────────┘

W data:         │  Byte 3   │  Byte 2   │  Byte 1   │  Byte 0  │ (4 bytes)
                └───────────┴───────────┴───────────┴───────────┘

F data:         │  Byte 3   │  Byte 2   │  Byte 1   │  Byte 0  │ (4 bytes, IEEE 754)
                └───────────┴───────────┴───────────┴───────────┘

D data:         │  Byte 3   │  Byte 2   │  Byte 1   │  Byte 0  │ (8 bytes, IEEE 754)
                ├───────────┼───────────┼───────────┼───────────┤
                │  Byte 7   │  Byte 6   │  Byte 5   │  Byte 4  │
                └───────────┴───────────┴───────────┴───────────┘
```

### Alignment

**Important:** Types should be aligned to their size:

```assembly
; GOOD alignment:
Struct:
    +0: BY field1        ; byte at 0
    +1: BY field2        ; byte at 1
    +2: H field3         ; halfword at 2 (even address) ✓
    +4: W field4         ; word at 4 (multiple of 4) ✓
    +8: D field5         ; double at 8 (multiple of 8) ✓

; BAD alignment (slower or errors):
Struct:
    +0: BY field1        ; byte at 0
    +1: H field2         ; halfword at 1 (ODD - slow!) ✗
    +3: W field3         ; word at 3 (not multiple of 4 - ERROR!) ✗
```

---

## Type Conversion

### Explicit Conversion Instructions

The ND-500 has instructions to convert between types:

```assembly
; Convert halfword to word
H1 =: B.SMALL_NUM       ; H1 = 1234 (halfword)
W1 =: H1                ; W1 = 1234 (word, zero-extended)

; Convert word to float
W1 =: 42
F1 =: W1                ; F1 = 42.0 (integer to float)

; Convert float to word (truncate)
F1 =: 3.14159
W1 =: F1                ; W1 = 3 (truncated, not rounded!)

; Use conversion instructions for rounding
F1 =: 3.14159
F1 INT                  ; Round to integer: F1 = 3.0
W1 =: F1                ; W1 = 3

F2 =: 3.7
F2 INT                  ; Round: F2 = 4.0
W2 =: F2                ; W2 = 4
```

### Implicit Conversions (Watch Out!)

```assembly
; Mixing types can cause issues:
BY1 =: 300              ; WRONG! 300 doesn't fit in byte (max 255)
                        ; Result: BY1 = 44 (300 mod 256)

H1 =: 100000            ; WRONG! 100000 doesn't fit in halfword (max 65535)
                        ; Result: H1 = 34464 (100000 mod 65536)

; Always use the right size:
W1 =: 100000            ; CORRECT!
```

---

## Common Pitfalls

### Pitfall 1: Forgetting to Clear Upper Bits

```assembly
; BAD: Upper bits may have garbage
BY1 =: B.CHAR           ; Loads byte, but what about upper 24 bits?
W1 =: BY1               ; W1 might have garbage in upper 24 bits!

; GOOD: Explicitly clear or use proper conversion
BY1 =: B.CHAR
W1 =: 0                 ; Clear W1
W1 =: BY1               ; Now W1 = 0x000000XX (clean)

; OR use AND to mask:
BY1 =: B.CHAR
W1 =: BY1
W1 AND 0xFF             ; Ensure only lower 8 bits are used
```

### Pitfall 2: Mixing Floating-Point and Integer

```assembly
; BAD: Can't mix types directly
F1 =: 3.14
W1 =: 2
F1 ADD W1               ; ERROR! Can't add integer to float directly

; GOOD: Convert first
F1 =: 3.14
F2 =: 2.0               ; Use floating constant
F1 ADD F2               ; OK: 3.14 + 2.0 = 5.14

; OR convert W1 to float:
W1 =: 2
F2 WCONV W1             ; Convert word to float
F1 ADD F2               ; OK
```

### Pitfall 3: Overflow

```assembly
; BAD: Silent overflow
BY1 =: 200
BY1 ADD 100             ; 200 + 100 = 300, but BY max is 255
                        ; Result: BY1 = 44 (wrapped around)

; GOOD: Use larger type or check overflow
W1 =: 200               ; Use word instead
W1 ADD 100              ; W1 = 300 (OK)

; OR check overflow flag:
BY1 =: 200
BY1 ADD 100
IF O GO overflow_handler  ; Jump if overflow occurred
```

### Pitfall 4: Floating-Point Comparison

```assembly
; BAD: Direct equality comparison
F1 =: 0.1
F1 ADD F1               ; F1 = 0.2
F1 ADD F1               ; F1 = 0.4
F1 COMP 0.4             ; Might NOT be equal due to rounding!
IF = GO equal           ; Might not jump!

; GOOD: Use epsilon comparison
F1 =: calculated_value
F2 =: expected_value
F1 SUB F2               ; F1 = difference
F1 ABS                  ; F1 = |difference|
F1 COMP 0.0001          ; Compare with epsilon (0.0001)
IF < GO close_enough    ; Jump if difference is small
```

### Pitfall 5: Type Size Assumptions

```assembly
; BAD: Assuming pointer arithmetic
W1 =: B.ARRAY           ; W1 = base address
W1 ADD 1                ; What does +1 mean?
                        ; Adds 1 BYTE, not 1 element!

; GOOD: Scale by element size
W1 =: B.ARRAY           ; W1 = base address
W2 =: 5                 ; Element index
W2 MULTIPLY 4           ; Scale by element size (4 bytes for word)
W1 ADD W2               ; W1 = address of element 5

; OR use indexing:
W1 =: B.ARRAY(5)        ; Automatic scaling by data type!
```

---

## Summary Chart

| Prefix | Size | Integer Range | Common Uses | Speed | Memory |
|--------|------|---------------|-------------|-------|--------|
| **BI** | 1 bit | 0-1 | Booleans, flags | ⚡⚡⚡ | 1 bit |
| **BY** | 8 bits | 0-255 or ±128 | Characters, small ints | ⚡⚡⚡ | 1 byte |
| **H** | 16 bits | 0-65K or ±32K | Medium ints, indices | ⚡⚡ | 2 bytes |
| **W** | 32 bits | 0-4G or ±2G | Pointers, large ints | ⚡⚡⚡ | 4 bytes |
| **F** | 32 bits | ±10³⁸ (~7 digits) | Science, graphics | ⚡⚡ | 4 bytes |
| **D** | 64 bits | ±10³⁰⁸ (~15 digits) | Precision work | ⚡ | 8 bytes |

---

## Rules of Thumb

1. **Start with W** - Use Word for most integer operations
2. **Use BY for text** - Bytes are perfect for ASCII/UTF-8
3. **H for arrays** - Halfwords save space when range allows
4. **F for graphics** - Single float is fast enough for games/3D
5. **D for money** - Double precision for financial accuracy
6. **BI for flags** - Bits are perfect for boolean states

---

## Practice Problems

### Problem 1: Choose the Right Prefix

What prefix should you use for:
1. A person's age (0-120)?
2. A memory address?
3. A pixel X coordinate (0-1920)?
4. The value of π?
5. A yes/no question?
6. A bank account balance?

**Answers:**
1. **BY** (age fits in 0-255)
2. **W** (addresses are 32-bit)
3. **H** (0-1920 fits in ±32K)
4. **F** or **D** (π needs decimals, use D for more precision)
5. **BI** (true/false = 1 bit)
6. **D** (need precision for money)

### Problem 2: Spot the Error

```assembly
; What's wrong with this code?
BY1 =: 500          ; Error 1: 500 > 255 (byte max)
H2 ADD W3           ; Error 2: Mixing H and W types
F1 =: BY1           ; Error 3: No direct BY to F conversion
D1 =: 0x1000(W1)    ; Error 4: Can't index absolute address with D
```

**Corrections:**
```assembly
W1 =: 500           ; Use word for 500
H2 ADD H3           ; Match types: H with H
F1 WCONV BY1        ; Convert explicitly
W2 =: 0x1000(W1)    ; Use W for indexed addressing
```

---

## Congratulations!

You now understand all 6 data type prefixes of the ND-500! Remember:

- **Prefix = data type and size**
- **Choose based on range and precision needed**
- **Watch for type mismatches and overflow**
- **When in doubt, use W for integers and F for decimals**

Happy coding! 🚀
