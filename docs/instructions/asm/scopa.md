# SCOPA - String Compare with Pad

## Overview

**Mnemonic:** `scopa`
**Function:** Compare strings with padding (no translation)
**Class:** STRING
**Privilege:** user

**Format:** `BY SCOPA <src1>, <src2>, <pad>`

---

## Description

Compares two byte strings, padding the shorter string with a specified pad byte to make lengths equal. Unlike SCOPT, this instruction does NOT use a translation table - bytes are compared directly. Handles variable-length string comparison with consistent padding behavior.

**Operation:**
```
while not end of strings and S(I1) = D(I2) do:
    I1 + 1 → I1
    I2 + 1 → I2
endwhile
```

**Key Characteristics:**
- Direct byte comparison (no translation)
- Shorter string padded automatically
- I1/I2 not incremented during padding
- Bytes treated as unsigned values
- Handles variable-length strings

**Common Use Cases:**
- Fixed-width field comparison
- Database record comparison
- Text processing with padding
- Legacy data format handling

**Operands:** 3 (source1 via I1, source2 via I2, pad byte)
**Variants:** 1 opcode

---

## Examples

### Example 1: Compare with space padding

```assembly
        % Compare strings padded with spaces
        W1 := TEXT1
        W2 := TEXT2
        BY SCOPA W1, W2, ' '
```

**Explanation:** Variable-length string comparison with space padding.

### Example 2: Database field comparison

```assembly
        % Compare DB VARCHAR fields
        W1 := FIELD1
        W2 := FIELD2
        BY SCOPA W1, W2, 0
```

**Explanation:** Database field comparison with null padding.

### Example 3: Fixed-width record comparison

```assembly
        % Compare fixed-width records
        W1 := REC1_NAME
        W2 := REC2_NAME
        BY SCOPA W1, W2, ' '
```

**Explanation:** Legacy fixed-width record processing.

### Example 4: Text file processing

```assembly
        % Compare lines with padding
        W1 := LINE1
        W2 := LINE2
        BY SCOPA W1, W2, ' '
```

**Explanation:** Text file line comparison.

### Example 5: Key lookup with padding

```assembly
        % Search table with padding
        W1 := SEARCH_KEY
        W2 := TABLE_KEY
        BY SCOPA W1, W2, '_'
```

**Explanation:** Table lookup with underscore padding.

---

## Reference Manual

**Section:** §14.12
**Title:** String compare with pad

---

## See Also

- [SCOPT](scopt.md) - String compare translated with pad
- [SCOTR](scotr.md) - String compare translated
- [SCOMP](scomp.md) - String compare
