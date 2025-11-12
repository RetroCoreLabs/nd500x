# SCOTR - String Compare Translated

## Overview

**Mnemonic:** `scotr`
**Function:** Compare strings with translation table
**Class:** STRING
**Privilege:** user

**Format:** `BY SCOTR <src1>, <src2>, <trans table>`

---

## Description

Compares two byte strings element-by-element after translating each byte through a translation table. This allows case-insensitive comparisons or custom collation orders. Comparison continues until unequal translated bytes are found or end of either string is reached.

**Operation:**
```
while not end of strings and tr(S(I1)) = tr(D(I2)) do:
    I1 + 1 → I1
    I2 + 1 → I2
endwhile
```

**Key Characteristics:**
- Compares translated bytes
- I1 and I2 registers point to strings
- Translation table applied to both strings
- Bytes treated as unsigned values
- Sets flags based on comparison result

**Common Use Cases:**
- Case-insensitive string comparison
- Custom collation sequences
- Locale-specific sorting
- Normalized text comparison

**Operands:** 3 (source1 via I1, source2 via I2, translation table)
**Variants:** 1 opcode

---

## Examples

### Example 1: Case-insensitive compare

```assembly
        % Compare ignoring case
        W1 := STRING1
        W2 := STRING2
        BY SCOTR W1, W2, UPPER_TABLE
```

**Explanation:** Case-insensitive comparison using uppercase translation.

### Example 2: Sort with custom collation

```assembly
        % Custom sort order
        W1 := KEY1
        W2 := KEY2
        BY SCOTR W1, W2, COLLATION_TABLE
```

**Explanation:** Database sorting with custom collation.

### Example 3: Normalized text comparison

```assembly
        % Compare with normalization
        W1 := USER_INPUT
        W2 := EXPECTED_VALUE
        BY SCOTR W1, W2, NORMALIZE_TABLE
```

**Explanation:** Input validation with normalization.

### Example 4: Locale-specific comparison

```assembly
        % Compare using locale rules
        W1 := TEXT_A
        W2 := TEXT_B
        BY SCOTR W1, W2, LOCALE_TABLE
```

**Explanation:** Internationalized text comparison.

### Example 5: ASCII to EBCDIC comparison

```assembly
        % Compare different encodings
        W1 := ASCII_STR
        W2 := EBCDIC_STR
        BY SCOTR W1, W2, ASCII_TO_EBCDIC
```

**Explanation:** Compare strings in different encodings.

---

## Reference Manual

**Section:** §14.11
**Title:** String compare translated

---

## See Also

- [SCOMP](scomp.md) - String compare
- [SCOPT](scopt.md) - String compare translated with pad
- [SCOPA](scopa.md) - String compare with pad
