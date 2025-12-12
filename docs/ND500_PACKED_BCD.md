# ND-500 Packed BCD (Binary-Coded Decimal) Reference

This document describes the Packed BCD format and instructions for the ND-500 processor,
based on the ND-500 CPU Reference Manual (Chapter 17).

## Overview

Packed BCD is a decimal arithmetic format where each decimal digit (0-9) is stored in
4 bits (one nibble). Two digits are packed per byte, making it more storage-efficient
than ASCII representation while maintaining exact decimal precision.

**Use Cases:**
- Financial calculations (exact decimal arithmetic, no floating-point errors)
- Accounting systems (currency handling)
- Database field conversions
- Scientific calculations requiring decimal precision

---

## 1. Packed BCD Data Format

### 1.1 Digit Encoding

Each nibble (4 bits) encodes one decimal digit:

| Nibble Value | Decimal Digit |
|--------------|---------------|
| 0x0 | 0 |
| 0x1 | 1 |
| 0x2 | 2 |
| 0x3 | 3 |
| 0x4 | 4 |
| 0x5 | 5 |
| 0x6 | 6 |
| 0x7 | 7 |
| 0x8 | 8 |
| 0x9 | 9 |
| 0xA-0xF | Invalid (causes IVO trap) |

**Example:** The value 12345678 is stored as bytes: 0x12 0x34 0x56 0x78

### 1.2 Sign Encoding

The sign is encoded in a nibble, whose position depends on the sign representation:

| Nibble Value | Meaning |
|--------------|---------|
| 0x0C | Positive (+) |
| 0x0D | Negative (-) |
| 0x0F | Unsigned (no sign) |
| 0x0B | Alternative negative (same as 0x0D) |

**Note:** Values 0x0A, 0x0E are sometimes accepted as positive; implementation should treat
0x0C, 0x0A, 0x0E, 0x0F as non-negative, and 0x0B, 0x0D as negative.

### 1.3 Byte Layout

For a value with field width FW nibbles:
- High-order digits are at lower addresses (big-endian within field)
- Sign nibble position depends on Sign Representation (SGN)
- Even FW: sign is low nibble of last byte
- Odd FW: may have leading or trailing half-byte for sign

**Example:** 8-digit positive value 12345678:
```
Address+0: 0x12  (digits 1, 2)
Address+1: 0x34  (digits 3, 4)
Address+2: 0x56  (digits 5, 6)
Address+3: 0x78  (digits 7, 8)
Address+4: 0x0C  (positive sign - embedded trailing)
```

---

## 2. String Descriptor Format

BCD values are accessed through 8-byte descriptors that specify format and location.

### 2.1 Descriptor Layout (8 bytes)

```
Word 0 (bytes 0-3): Control word
  Bits 31-27: Reserved (must be 0)
  Bits 26-24: Sign Representation (SGN) - 3 bits
  Bits 23-18: Scaling Factor (SC) - 6 bits signed (-32 to +31)
  Bits 17-13: Field Width (FW) - 5 bits (0-31 nibbles)
  Bits 12-0:  Element Count (N) - for arrays

Word 1 (bytes 4-7): Base address of element 0
```

### 2.2 Sign Representation (SGN) - Bits 26-24

| Value | Name | Description |
|-------|------|-------------|
| 0 | Embedded Trailing | Sign in low nibble of last digit byte |
| 1 | Separate Trailing | Sign in separate byte after digits |
| 2 | Embedded Leading | Sign in high nibble of first digit byte |
| 3 | Separate Leading | Sign in separate byte before digits |
| 4 | Unsigned | No sign nibble (value always positive) |
| 5-7 | Reserved | Undefined behavior |

### 2.3 Scaling Factor (SC) - Bits 23-18

6-bit signed value (-32 to +31) indicating decimal point position:

- **SC = 0**: Value is an integer (no decimal point)
- **SC > 0**: Decimal point is SC positions to the RIGHT of digits
  - Value is multiplied by 10^SC
  - Example: digits "123", SC=2 => value = 12300
- **SC < 0**: Decimal point is |SC| positions to the LEFT of rightmost digit
  - Value is divided by 10^|SC|
  - Example: digits "12345", SC=-2 => value = 123.45

### 2.4 Field Width (FW) - Bits 17-13

5-bit value (0-31) specifying the number of decimal digit nibbles.

**Note:** FW=0 causes a descriptor range trap (invalid operand).

The total bytes required for the BCD value depends on FW and SGN:
- Embedded sign: ceil((FW + 1) / 2) bytes
- Separate sign: ceil(FW / 2) + 1 bytes
- Unsigned: ceil(FW / 2) bytes

### 2.5 Element Count (N) - Bits 12-0

13-bit value for array indexing. Single values use N=1.

---

## 3. BCD Instruction Set

### 3.1 Arithmetic Instructions

| Mnemonic | Opcode | Description |
|----------|--------|-------------|
| PADD | 0xFEB0 | Packed add (no rounding) |
| PADDR | 0xFEB1 | Packed add with rounding |
| PSUB | 0xFEB2 | Packed subtract (no rounding) |
| PSUBR | 0xFEB4 | Packed subtract with rounding |
| PMPY | 0xFEB6 | Packed multiply (no rounding) |
| PMPYR | 0xFEB7 | Packed multiply with rounding |
| PSUM | 0xFEA2 | Packed sum (array) |

### 3.2 Comparison Instruction

| Mnemonic | Opcode | Description |
|----------|--------|-------------|
| PCOMP | 0xFEB3 | Compare two packed values |

### 3.3 Conversion Instructions

| Mnemonic | Opcode | Description |
|----------|--------|-------------|
| PWCONV | 0xFEBC-0xFEBF | Packed BCD to binary word (I1-I4) |
| WPCONV | 0xFEB8-0xFEBB | Binary word to packed BCD (I1-I4) |
| PPACK | 0xFEB5 | ASCII to packed BCD |
| PPACKR | 0xFEAE | ASCII to packed BCD with rounding |
| PUPACK | 0xFEAF | Packed BCD to ASCII |
| PUPACKR | 0xFEAD | Packed BCD to ASCII with rounding |

### 3.4 Shift/Scale Instructions

| Mnemonic | Opcode | Description |
|----------|--------|-------------|
| PSHIFT | 0xFEA0 | Rescale packed value (no rounding) |
| PSHIFTR | 0xFEA1 | Rescale packed value with rounding |

### 3.5 Utility Instructions

| Mnemonic | Opcode | Description |
|----------|--------|-------------|
| PCC | 0xFEA3 | Packed condition code (set flags) |
| PCTSB | 0xFEA4 | Count significant bytes |

---

## 4. PWCONV - Packed to Word Conversion

### 4.1 Opcodes

| Opcode | Register | Assembly |
|--------|----------|----------|
| 0xFEBC | I1 | W1 PWCONV <source> |
| 0xFEBD | I2 | W2 PWCONV <source> |
| 0xFEBE | I3 | W3 PWCONV <source> |
| 0xFEBF | I4 | W4 PWCONV <source> |

### 4.2 Operation

```
Packed BCD value -> Binary 32-bit word in Rn
```

1. Load BCD descriptor from operand address
2. Read packed BCD digits from memory
3. Convert to binary integer value
4. Apply scaling factor (truncate fractional part)
5. Store result in target register (I1-I4)
6. Set status flags

### 4.3 Scaling Behavior

- SC >= 0: Result = value * 10^SC (may overflow)
- SC < 0: Result = value / 10^|SC| (truncates fractional part)

### 4.4 Status Flags

| Flag | Condition |
|------|-----------|
| Z | Set if result = 0 |
| S | Set to sign of result |
| O | Set if overflow (value outside int32 range) |
| K | Set if overflow occurred |

### 4.5 Traps

- **IVO (Invalid Operation)**: Invalid BCD digit (0xA-0xF in digit position)
- **Descriptor Range**: FW = 0

---

## 5. WPCONV - Word to Packed Conversion

### 5.1 Opcodes

| Opcode | Register | Assembly |
|--------|----------|----------|
| 0xFEB8 | I1 | W1 WPCONV <dest> |
| 0xFEB9 | I2 | W2 WPCONV <dest> |
| 0xFEBA | I3 | W3 WPCONV <dest> |
| 0xFEBB | I4 | W4 WPCONV <dest> |

### 5.2 Operation

```
Binary 32-bit word in Rn -> Packed BCD value
```

1. Read value from source register (I1-I4)
2. Load BCD descriptor from operand address
3. Apply scaling factor
4. Convert to packed BCD format
5. Write to memory at descriptor address
6. Set status flags

### 5.3 Scaling Behavior

- SC >= 0: Value is extended with SC trailing zeros
- SC < 0: Value is divided by 10^|SC|, truncating remainder

### 5.4 Status Flags

| Flag | Condition |
|------|-----------|
| Z | Set if result = 0 |
| S | Set to sign of result |
| BO | Set if BCD overflow (doesn't fit in FW digits) |
| K | Set if BO occurred |

### 5.5 Traps

- **BO (BCD Overflow)**: Result requires more digits than FW allows
- **Descriptor Range**: FW = 0

---

## 6. Implementation Notes

### 6.1 Decimal Arithmetic

BCD arithmetic automatically aligns operands with different scaling factors before
performing operations. The result is then scaled to match the destination.

**Example:** Adding 123.45 (SC=-2) + 6.789 (SC=-3):
1. Align to common scale (SC=-3): 123.450 + 6.789
2. Add: 130.239
3. Scale to destination (e.g., SC=-2): 130.23 (truncated) or 130.24 (rounded)

### 6.2 Overflow Detection

- **Integer overflow**: Value exceeds int32 range (-2147483648 to 2147483647)
- **BCD overflow**: Value requires more digits than field width allows
- **Scaling overflow**: Multiplying by 10^SC causes overflow

### 6.3 Invalid Digit Detection

Any nibble value >= 0x0A in a digit position (not sign position) causes an
Invalid Operation (IVO) trap.

### 6.4 Zero Handling

- Positive zero (+0) and negative zero (-0) are both treated as zero
- Z flag is set for both +0 and -0
- S flag reflects the sign nibble even for zero values

---

## 7. Examples

### 7.1 Reading a BCD Value

Descriptor at 0x1000:
```
0x1000: 0x00 0x10 0x40 0x01  (SGN=0, SC=-2, FW=8, N=1)
0x1004: 0x00 0x00 0x20 0x00  (Base address = 0x2000)
```

BCD data at 0x2000:
```
0x2000: 0x12 0x34 0x56 0x78 0x0C  (12345678 positive, 8 digits + sign)
```

Result: 123456.78 (scaled by SC=-2)

### 7.2 Writing a BCD Value

Converting -42 to BCD with SC=0, FW=4, SGN=0:
```
Digits: 0042
Sign: 0x0D (negative)
Output: 0x00 0x42 0x0D
```

---

## 8. References

- ND-500 CPU Reference Manual, Chapter 17: Packed Decimal Instructions
- ND-500 Assembler Reference Manual, Data Types section
