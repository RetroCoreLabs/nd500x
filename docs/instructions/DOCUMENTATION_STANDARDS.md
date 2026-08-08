# ND-500 Instruction Documentation Standards

## Purpose

This document defines the canonical formatting standards for all ND-500 instruction documentation files generated in `/docs/instructions/asm/`.

---

## 1. Hexadecimal Notation

**Standard:** Always use `0x` prefix (C-style)

✅ **CORRECT:**
```
0xFC04
0x00B0
0xFFD0
```

❌ **INCORRECT:**
```
0FC04H   (Norsk Data style - use only when quoting Reference Manual)
FC04H
$FC04    (assembly style)
```

**Rationale:** Consistency with `instructions.json` and modern programming conventions.

---

## 2. Mnemonic Format

**Standard:** `{prefix}{register} {mnemonic} {operands}`

### Examples:

**Single-operand instructions:**
```assembly
W1 := B.VAR        ; Load word from local variable
H2 =: R.FIELD      ; Store halfword to record field
BY3 := 0xFF        ; Load byte constant
```

**Multi-operand instructions:**
```assembly
W1 CIND B.INDEX, 1, 10       ; Calculate index with bounds 1-10
H1 ADD W2                    ; Add W2 to H1
F1 * F2                      ; Multiply F1 by F2
```

**No-operand instructions:**
```assembly
RET                ; Return from subroutine
NOOP               ; No operation
HALT               ; Halt CPU
```

### Spacing Rules:

- **One space** between prefix-register and mnemonic
- **One space** after mnemonic before first operand
- **Comma + space** between operands
- **No space** before comments (`;` starts on same line or indented)

---

## 3. Addressing Mode Syntax

**Standard:** Use the canonical names from chapter 8 (sections 8.3-8.16) of the
ND-500 Reference Manual. There is no document under `docs/` for these yet - the
explainer that covered them was removed for ranking the modes by speed, which the
manual never does.

| Mode | Syntax | Example |
|------|--------|---------|
| **CONSTANT** | `value` | `42`, `0x100`, `3.14` |
| **CONSTANT_SHORT** | `value:S` | `B.0:S`, `B.4:S` |
| **REGISTER** | `Rn`, `Wn`, `Hn`, `BYn`, `Fn`, `Dn` | `W1`, `H2`, `F3` |
| **LOCAL** | `B.displacement` | `B.8`, `B.VAR` |
| **LOCAL_SHORT** | `B.displacement:S` | `B.0:S`, `B.1:S` |
| **RECORD** | `R.displacement` | `R.4`, `R.FIELD` |
| **RECORD_SHORT** | `R.displacement:S` | `R.0:S` |
| **ABSOLUTE** | `address` | `0x1000`, `GLOBAL_VAR` |
| **LOCAL_INDIRECT** | `IND(B.displacement)` | `IND(B.PTR)` |
| **RECORD_INDIRECT** | `IND(R.displacement)` | `IND(R.NEXT)` |
| **ABSOLUTE_INDIRECT** | `IND(address)` | `IND(0x2000)` |
| **PRE_INDEXED** | `B.disp(Rn)` | `B.ARRAY(W2)` |
| **POST_INDEXED** | `B.disp(Rn)` | `B.DATA(H1)` |
| **DESCRIPTOR** | `DESC(operand)(Rn)` | `DESC(B.STR)(W1)` |
| **ALTERNATIVE_DOMAIN** | `ALT(operand)` | `ALT(B.PARAM)` |

---

## 4. Data Type Prefixes

**Standard:** Use uppercase prefix names

| Prefix | Full Name | Usage |
|--------|-----------|-------|
| **BI** | Bit | `BI1`, `BI2`, `BI3`, `BI4` |
| **BY** | Byte | `BY1`, `BY2`, `BY3`, `BY4` |
| **H** | Halfword | `H1`, `H2`, `H3`, `H4` |
| **W** | Word | `W1`, `W2`, `W3`, `W4` |
| **F** | Float | `F1`, `F2`, `F3`, `F4` |
| **D** | Double | `D1`, `D2`, `D3`, `D4` |
| **R_N** | Register/None | Instruction has no explicit prefix |

**Note on R_N:** This appears in `instructions.json` and means the instruction does NOT use a data type prefix (e.g., `NOOP`, `RET`, `HALT`).

---

## 5. Instruction File Naming

**Standard:** Lowercase, underscores for special characters

### Rules:

1. Use mnemonic as base filename
2. Convert to lowercase
3. Replace special characters:
   - `:=` → `assignto`
   - `=:` → `assignfrom`
   - `b:=` → `b_assignto`
   - `r:=` → `r_assignto`
   - `+` → `add`
   - `-` → `sub`
   - `*` → `mul`
   - `/` → `div`
4. Add `.md` extension

### Examples:

| Mnemonic | Filename |
|----------|----------|
| `:=` | `assignto.md` |
| `=:` | `assignfrom.md` |
| `CIND` | `cind.md` |
| `ADD` | `add.md` |
| `*` | `mul.md` |
| `b:=` | `b_assignto.md` |

---

## 6. Markdown File Structure

**Standard Template:**

```markdown
# {INSTRUCTION_NAME}

## Overview

**Mnemonic:** {mnemonic}
**Function:** {FunctionName}
**Class:** {CLASS}
**Privilege:** {user|supervisor}

**Format:** {prefix}{register} {mnemonic} {operands}

---

## Description

{Detailed description from Reference Manual}

**Operation:**
```
{pseudo-code or mathematical operation}
```

---

## Variants

Total variants: {N}

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/{N} | 0xXXXX | PREFIX | Rn | MODE1, MODE2, ... |
...

---

## Addressing Modes

### Operand 1: {description}

Supported modes:
- **MODE1:** {example}
- **MODE2:** {example}

### Operand 2: {description}
...

---

## Trap Conditions

- **Trap Name (BIT):** {when it occurs}
- ...

---

## Data Status Bits

- **Z (Zero):** {condition}
- **S (Sign):** {condition}
- **O (Overflow):** {condition}
- **K (Flag):** {condition}

---

## Examples

### Example 1: {description}

```assembly
{code}
```

**Explanation:** {what the code does}

### Example 2: {description}
...

---

## Performance Notes

- **Typical cycles:** {estimate}
- **Best case:** {scenario}
- **Worst case:** {scenario}

---

## Reference Manual

**Section:** {§X.Y}
**Page:** {page number}
**Title:** {section title}

---

## See Also

- [{related instruction 1}]({filename1}.md)
- [{related instruction 2}]({filename2}.md)
- [Trap System](../../ND-500-TRAPS.md)
```

---

## 7. Code Comment Style

**Standard:** Descriptive comments, aligned at column 40 (or after instruction)

### Good Examples:

```assembly
        W1 := B.VAR             ; Load local variable VAR
        W1 ADD 10               ; Add constant 10
        W1 =: B.RESULT          ; Store result

loop:
        W2 COMP 100             ; Check if W2 < 100
        IF >= GO done           ; Exit loop if W2 >= 100
        W2 ADD 1                ; Increment counter
        GO loop                 ; Continue loop

done:
        RET                     ; Return from subroutine
```

### Bad Examples:

```assembly
; Don't do this
W1 := B.VAR;load var           ; No space before comment
W1ADD 10                        ; Missing space after W1
W1=:B.RESULT                    ; No spaces around =:
```

---

## 8. Operand Naming Conventions

**Standard:** Use descriptive symbolic names

### Variable Names:

- Use `UPPER_CASE` for symbolic constants
- Use `PascalCase` or `snake_case` for variables
- Prefix local vars with nothing (implied B register)
- Prefix record fields with nothing (implied R register)

### Examples:

```assembly
; Constants
MAX_SIZE = 1000
PI = 3.14159

; Local variables (B register)
        W1 := B.counter         ; Local variable
        W2 := B.array_size      ; Another local

; Record fields (R register)
        H1 := R.age             ; Record field
        W2 := R.salary          ; Another field

; Absolute addresses
        W3 := GLOBAL_COUNTER    ; Global variable
```

---

## 9. Cross-Reference Format

**Standard:** Markdown links, relative paths

### Internal Links:

```markdown
See [LIND instruction](lind.md) for loading indexes.

For more on addressing, see the ND-500 Reference Manual, chapter 8.
```

### External References:

```markdown
**Reference Manual:** §15.9 Calculate Index (page 262)

**Trap Documentation:** See [Trap System](../../ND-500-TRAPS.md)
```

---

## 10. Variant Numbering

**Standard:** `{current}/{total}` format

### Examples:

```
Variant 1/20
Variant 13/20
Variant 20/20
```

**Always:**
- Use 1-based indexing (not 0-based)
- Include total count
- Update if variants change

---

## 11. Opcode Presentation

**Standard:** Show hex, binary, and decimal

### Format:

| Variant | Opcode (Hex) | Opcode (Binary) | Opcode (Decimal) |
|---------|--------------|-----------------|------------------|
| 1/4 | 0xFC04 | 1111_1100_0000_0100 | 64516 |
| 2/4 | 0xFC05 | 1111_1100_0000_0101 | 64517 |

**Binary format:** Use underscores for nibble separation (every 4 bits)

---

## 12. Ambiguity Resolutions

### R_N Prefix

**Meaning:** "Register/None" - instruction has NO explicit data type prefix

**Usage in docs:**
```markdown
**Prefixes:** NONE (R_N in JSON means no prefix required)
```

### POST_INDEXED vs PRE_INDEXED

**Clarification:**
- **PRE_INDEXED:** Index calculated BEFORE base: `BASE + INDEX`
- **POST_INDEXED:** Index calculated AFTER base: `(BASE) + INDEX`

Both can apply to LOCAL (`B`) and RECORD (`R`) addressing.

### SHORT Forms

**Meaning:** Compact encoding with word-unit displacements

**Example:**
- `B.0:S` = B register + 0 words = B + 0 bytes
- `B.1:S` = B register + 1 word = B + 4 bytes
- `B.2:S` = B register + 2 words = B + 8 bytes

---

## Validation Checklist

Before committing a new instruction file, verify:

- [ ] Hex notation uses `0x` prefix
- [ ] Mnemonic format is `{prefix}{register} {mnemonic} {operands}`
- [ ] All addressing modes use canonical names
- [ ] Code examples have descriptive comments
- [ ] Variant numbering is `X/Y` format
- [ ] Cross-references use relative Markdown links
- [ ] File named correctly (lowercase, special chars replaced)
- [ ] All required sections present (see template)
- [ ] UTF-8 encoding (no BOM)

---

**Version:** 1.0
**Last Updated:** 2025-11-08
**Status:** APPROVED
