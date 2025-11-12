# SSPAR - String Set Parity

## Overview

**Mnemonic:** `sspar`
**Function:** Set parity bit in all string bytes
**Class:** STRING
**Privilege:** user

**Format:** `BY SSPAR <string/rw/BY/I1>, <mode/r/BY>`

---

## Description

Sets the parity bit (bit 7, the high-order bit) in every byte of a string according to a specified parity mode. The operation modifies the string in place, processing all bytes from the current I1 position to the end of the string.

**Operation:**
```
for each byte in string[I1..end]:
    if (mode == 0): byte = byte & 0x7F  // Clear bit 7
    if (mode == 1): byte = byte | 0x80  // Set bit 7
    if (mode == 2): byte = set_even_parity(byte)
    if (mode == 3): byte = set_odd_parity(byte)
    I1 = I1 + 1
K = 1
```

**Key Characteristics:**
- In-place parity bit modification for entire string
- 4 parity modes: Clear (0), Set (1), Even (2), Odd (3)
- Uses I1 as implicit index register (auto-incremented)
- K flag set to 1 upon successful completion
- IOV trap if mode not in range 0-3
- Essential for serial communication protocol preparation
- Modifies string directly (read-write operation)
- O(n) complexity where n = string length
- Used for 7-bit/8-bit ASCII conversion and error detection

SSPAR supports four parity modes controlled by the `<mode>` operand:
- **Mode 0 (Clear)**: Sets bit 7 to 0 in all bytes
- **Mode 1 (Set)**: Sets bit 7 to 1 in all bytes
- **Mode 2 (Even)**: Sets bit 7 to make total number of 1-bits even
- **Mode 3 (Odd)**: Sets bit 7 to make total number of 1-bits odd

This instruction is essential for preparing data for transmission over serial communication protocols that use parity checking, or for processing data in formats that store metadata in the high bit (e.g., some terminal protocols, older text encodings).

The operation processes the entire string from I1 to the end. After completion, I1 points past the end of the string and the K flag is set to 1 to indicate successful completion.

**Operands:** 2
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFDB4 | BY SSPAR |

---

## Operands

### Operand 1: `<string/rw/BY/I1>`

String to modify (parity bits set in place).

**Role:** Source and Destination
**Access:** Read-Write
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the string whose parity bits will be modified. The I1 register serves as the index pointer and is incremented for each byte processed. The string is modified in place.

### Operand 2: `<mode/r/BY>`

Parity mode selector.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the parity mode:
- **0**: Clear parity (bit 7 = 0)
- **1**: Set parity (bit 7 = 1)
- **2**: Even parity (bit 7 set to make even number of 1-bits)
- **3**: Odd parity (bit 7 set to make odd number of 1-bits)

Any other value causes an Illegal Operand Value (IOV) trap.

---

## Trap Conditions

- **Illegal Operand Value (IOV)**: Occurs when the `<mode>` operand is not in the range 0-3.

---

## Data Status Bits

| Bit | Name | Description |
|-----|------|-------------|
| K | Carry | Always set to 1 upon completion |
| Z | Zero | Undefined after operation |
| S | Sign | Undefined after operation |

---

## Examples

### Example 1: Set even parity for transmission

```assembly
% Prepare local string for serial transmission with even parity
OUTPUT: (80 bytes) "Hello World"

        W1 CLR
        BY SSPAR B.OUTPUT, 2    % Even parity
        % OUTPUT bytes now have even parity
        % Ready for 8-bit serial transmission
```

### Example 2: Clear high bits for 7-bit ASCII

```assembly
% Clear bit 7 to ensure 7-bit ASCII
% (Remove parity or high-bit markers)
TEXT: (100 bytes)

        W1 CLR
        BY SSPAR TEXT, 0        % Clear parity
        % All bytes now in range 0x00-0x7F
```

### Example 3: Set parity bits for protocol

```assembly
% Set high bit in all bytes (mark parity)
BUFFER: (256 bytes)

        W1 CLR
        BY SSPAR BUFFER, 1      % Set parity
        % All bytes now have bit 7 = 1
```

### Example 4: Odd parity for error detection

```assembly
% Apply odd parity to data block
DATA_BLOCK: (512 bytes)

        W1 CLR
        BY SSPAR DATA_BLOCK, 3  % Odd parity
        % Each byte now has odd number of 1-bits
```

### Example 5: Record field parity setting

```assembly
RECORD:
        .MESSAGE: (60 bytes)
        .PARITY:  1             % Desired parity mode

        W1 CLR
        BY MOVE R.PARITY, I2    % Get parity mode
        BY SSPAR R.MESSAGE, I2
        % MESSAGE has parity bits set per mode
```

### Example 6: Conditional parity modes

```assembly
% Apply parity based on configuration
CONFIG_PARITY: 2        % Even parity

MESSAGE: (120 bytes)

APPLY_PARITY:
        W1 CLR
        BY MOVE CONFIG_PARITY, I2
        BY SSPAR MESSAGE, I2
```

### Example 7: Local buffer parity processing

```assembly
PREPARE_TX: ENTS 100
        % Clear bit 7 then apply even parity
        W1 CLR
        BY SSPAR B.BUFFER, 0    % Clear first
        W1 CLR
        BY SSPAR B.BUFFER, 2    % Then even parity
        % Buffer ready for transmission
        RET
```

### Example 8: Multi-buffer parity setup

```assembly
% Process multiple buffers with same parity
BUF1: (80 bytes)
BUF2: (80 bytes)
BUF3: (80 bytes)
MODE: 2                 % Even parity

        BY MOVE MODE, I4        % Save mode

        W1 CLR
        BY SSPAR BUF1, I4

        W1 CLR
        BY SSPAR BUF2, I4

        W1 CLR
        BY SSPAR BUF3, I4
```

---

## Performance Notes

- **Termination**: Always sets K=1 upon completion
- **In-Place Modification**: String is modified directly
- **I1 Advance**: Incremented for each byte, points past end when done
- **Parity Modes**:
  - **Clear (0)**: Fast - simply AND each byte with 0x7F
  - **Set (1)**: Fast - simply OR each byte with 0x80
  - **Even (2)**: Count 1-bits in lower 7 bits, set bit 7 to make total even
  - **Odd (3)**: Count 1-bits in lower 7 bits, set bit 7 to make total odd
- **Performance**: O(n) where n is number of bytes in string
- **Typical Use**: Serial communication preparation, protocol formatting, 7-bit/8-bit conversion

---

## Reference Manual

**Section:** §14.19
**Title:** Set parity in string

---

## See Also

- [SMVTR](smvtr.md) - String move translated (for other transformations)
- [SMVTU](smvtu.md) - String move translated until
