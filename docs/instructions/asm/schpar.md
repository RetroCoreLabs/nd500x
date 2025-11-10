# SCHPAR - Check Parity in String

## Overview

**Mnemonic:** `schpar`
**Function:** Check parity bits in byte string
**Class:** STRING
**Privilege:** user

**Format:** `BY SCHPAR <string>, <mode>`

---

## Description

Checks the parity bits of bytes in a string according to a specified mode. The I1 register points to the string and auto-increments. Used for data integrity verification, communication protocols, and error detection in stored data.

**Operation:**
```
for each byte in string:
    check parity according to mode
    if parity error: flag and optionally stop
    I1 + 1 → I1
```

**Key Characteristics:**
- Verifies parity of byte string
- I1 register points to string
- Mode operand specifies parity type (even/odd)
- Used for error detection
- Sets flags on parity errors

**Common Use Cases:**
- Data integrity verification
- Communication protocol validation
- Error detection in stored data
- Tape/disk read verification
- Serial communication validation

**Operands:** 2 (string via I1, mode)
**Variants:** 1 opcode

---

## Examples

### Example 1: Check even parity

```assembly
        % Verify even parity in buffer
        W1 := DATA_BUFFER
        BY SCHPAR W1, EVEN_PARITY
```

**Explanation:** Check data buffer for even parity errors.

### Example 2: Validate received data

```assembly
        % Check serial port data
        W1 := RX_BUFFER
        BY SCHPAR W1, ODD_PARITY
        IF><GO PARITY_ERROR
```

**Explanation:** Validate data received from serial port.

### Example 3: Verify tape read

```assembly
        % Check tape data integrity
        W1 := TAPE_BLOCK
        BY SCHPAR W1, PARITY_MODE
        IF><GO READ_ERROR
```

**Explanation:** Verify magnetic tape read operation.

### Example 4: Communication protocol validation

```assembly
        % Validate protocol frame
        W1 := FRAME_DATA
        BY SCHPAR W1, PROTOCOL_PARITY
```

**Explanation:** Check communication frame parity.

### Example 5: Storage verification

```assembly
        % Verify stored record
        W1 := RECORD_DATA
        BY SCHPAR W1, STORAGE_PARITY
        IF><GO CORRUPTION_DETECTED
```

**Explanation:** Detect data corruption in storage.

---

## Reference Manual

**Section:** §14.20
**Title:** Check parity in string

---

## See Also

- [SCHK](schk.md) - Check condition
- [String Operations](../ND500_STRING_OPERATIONS.md)
