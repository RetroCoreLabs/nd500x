# SCOPT - String Compare Translated with Pad

## Overview

**Mnemonic:** `scopt`
**Function:** Compare translated strings with padding
**Class:** STRING
**Privilege:** user

**Format:** `BY SCOPT <src1>, <src2>, <trans table>, <pad>`

---

## Description

Compares two byte strings with translation table, padding the shorter string with a specified pad byte to make lengths equal. Allows comparison of variable-length strings with consistent padding behavior. Both string bytes and pad byte are translated.

**Operation:**
```
while not end of strings and tr(S(I1)) = tr(D(I2)) do:
    I1 + 1 → I1
    I2 + 1 → I2  (not incremented when padding)
endwhile
```

**Key Characteristics:**
- Translates bytes before comparison
- Shorter string padded automatically
- Pad byte also translated
- I1/I2 not incremented during padding
- Handles variable-length strings

**Common Use Cases:**
- Database key comparison with padding
- Fixed-width field comparison
- Text sorting with padding rules
- Legacy data format handling

**Operands:** 4 (source1 via I1, source2 via I2, trans table, pad byte)
**Variants:** 1 opcode

---

## Examples

### Example 1: Compare with space padding

```assembly
        % Compare padded with spaces
        W1 := NAME1
        W2 := NAME2
        BY SCOPT W1, W2, UPPER_TABLE, ' '
```

**Explanation:** Case-insensitive comparison with space padding.

### Example 2: Database key comparison

```assembly
        % Compare DB keys with padding
        W1 := KEY_FIELD1
        W2 := KEY_FIELD2
        BY SCOPT W1, W2, COLLATION, 0
```

**Explanation:** Database index comparison with null padding.

### Example 3: Fixed-width record compare

```assembly
        % Compare fixed-width fields
        W1 := RECORD1_FIELD
        W2 := RECORD2_FIELD
        BY SCOPT W1, W2, TRANS, PAD_CHAR
```

**Explanation:** Legacy fixed-width record comparison.

### Example 4: Sort with padding

```assembly
        % Sort names with padding
        W1 := SURNAME1
        W2 := SURNAME2
        BY SCOPT W1, W2, LOCALE_TABLE, ' '
```

**Explanation:** Sorting variable-length names consistently.

### Example 5: Padded text search

```assembly
        % Search with padding
        W1 := SEARCH_KEY
        W2 := TABLE_ENTRY
        BY SCOPT W1, W2, NORMALIZE, '_'
```

**Explanation:** Table lookup with underscore padding.

---

## Reference Manual

**Section:** §14.13
**Title:** String compare translated with pad

---

## See Also

- [SCOTR](scotr.md) - String compare translated
- [SCOPA](scopa.md) - String compare with pad
- [SCOMP](scomp.md) - String compare
