# Phase 2 - Execution Plan
## ND-500 Instruction Documentation Generation

**Date:** 2025-11-08
**Phase:** 2 - Planning
**Status:** In Progress
**Target:** Define complete workflow for Phase 3 generation

---

## 1. File Naming and Structure

### 1.1 Directory Structure

```
/docs/instructions/
├── asm/                          ← Generated instruction files
│   ├── assignto.md              (:= instruction)
│   ├── assignfrom.md            (=: instruction)
│   ├── b_assignto.md            (b:= instruction)
│   ├── r_assignto.md            (r:= instruction)
│   ├── add.md                   (add instruction)
│   ├── mul.md                   (* instruction)
│   ├── cind.md                  (cind instruction)
│   └── ... (241 files total)
│
├── instructions.json             ← Source data
├── instruction_schema.json       ← YAML schema
├── DOCUMENTATION_STANDARDS.md    ← Formatting rules
├── MANUAL_SECTION_MAPPING.json   ← Reference manual cross-ref
├── TODO.md                       ← Progress tracking
└── yaml/                         ← Existing YAML files (241 files)
```

### 1.2 File Naming Convention

**Rule:** `{mnemonic_normalized}.md`

**Normalization Rules:**

| Original Mnemonic | Normalized Filename | Reason |
|-------------------|---------------------|--------|
| `:=` | `assignto.md` | Special chars → descriptive name |
| `=:` | `assignfrom.md` | Special chars → descriptive name |
| `b:=` | `b_assignto.md` | Prefix preserved with underscore |
| `r:=` | `r_assignto.md` | Prefix preserved with underscore |
| `b=:` | `b_assignfrom.md` | Prefix preserved with underscore |
| `r=:` | `r_assignfrom.md` | Prefix preserved with underscore |
| `+` | `add.md` | Operator → descriptive name |
| `-` | `sub.md` | Operator → descriptive name |
| `*` | `mul.md` | Operator → descriptive name |
| `/` | `div.md` | Operator → descriptive name |
| `ADD` | `add.md` | Lowercase |
| `CIND` | `cind.md` | Lowercase |
| `SIN` | `sin.md` | Lowercase |

**Implementation:**
```python
def normalize_filename(mnemonic):
    # Special character mappings
    mappings = {
        ':=': 'assignto',
        '=:': 'assignfrom',
        'b:=': 'b_assignto',
        'r:=': 'r_assignto',
        'b=:': 'b_assignfrom',
        'r=:': 'r_assignfrom',
        '+': 'add',
        '-': 'sub',
        '*': 'mul',
        '/': 'div',
    }

    if mnemonic in mappings:
        return mappings[mnemonic] + '.md'
    else:
        return mnemonic.lower() + '.md'
```

### 1.3 File Structure Template

Each instruction file follows this structure (based on DOCUMENTATION_STANDARDS.md):

```markdown
# {INSTRUCTION_NAME}

## Overview

**Mnemonic:** {mnemonic}
**Function:** {FunctionName}
**Class:** {CLASS}
**Privilege:** {user|supervisor}
**Operand Count:** {N}

**Format:** {syntax_template}

---

## Description

{detailed_description}

**Operation:**
```
{pseudo_code}
```

**Addressing Modes:** {summary}

---

## Variants

Total variants: {N}

{variant_table}

---

## Operands

{for each operand}
### Operand {N}: {description}

**Role:** {Source|Destination|SourceDestination}
**Access:** {Read|Write|ReadWrite}
**Data Type:** {constraint}

**Supported Addressing Modes:**
- **MODE1:** {example}
- **MODE2:** {example}
...
{end for}

---

## Trap Conditions

{list_of_traps}

---

## Data Status Bits

{status_bit_effects}

---

## Examples

{real_world_examples}

---

## Performance Notes

{cycle_estimates_and_notes}

---

## Reference Manual

**Section:** §{X.Y}
**Page:** {page}
**Title:** {section_title}

---

## See Also

{related_instructions}
```

---

## 2. Section Layout Details

### 2.1 Overview Section

**Required Fields:**
- Mnemonic (from instructions.json)
- Function name (PascalCase, from instructions.json)
- Class (from instructions.json)
- Privilege level (user/supervisor, from instructions.json or YAML)
- Operand count (from instructions.json)
- Format/Syntax template

**Example:**
```markdown
## Overview

**Mnemonic:** `cind`
**Function:** `Cind`
**Class:** SYSTEM
**Privilege:** supervisor
**Operand Count:** 3

**Format:** `tn CIND <index/r/t>, <lower/r/t>, <upper/r/t>`
```

### 2.2 Description Section

**Content Sources:**
1. Reference Manual (primary, §X.Y)
2. YAML `description` field (secondary)
3. Generated summary if neither available

**Components:**
- What the instruction does
- Operation in pseudo-code or mathematical notation
- When to use it
- Addressing mode summary

**Example:**
```markdown
## Description

Calculates the address of an element in a multi-dimensional array. The range
of the dimension (`<upper>` - `<lower>` + 1) is multiplied by the contents
of the specified register. `<index>` is added to the product and the result
loaded into the specified register.

**Operation:**
```
Rn * (<upper> - <lower> + 1) + <index> -> Rn

if <index> < <lower> OR <index> > <upper> then
    K := 1
    raise IllegalIndex trap
else
    K := 0
endif
```

**Addressing Modes:** Supports LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED,
and ABSOLUTE addressing for all three operands.
```

### 2.3 Variants Section

**Format:** Table showing all opcode variants

**Columns:**
- Variant number (X/Y)
- Opcode (hex)
- Prefix
- Register
- Addressing modes (summary)

**Example:**
```markdown
## Variants

Total variants: 20

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/20 | 0xFD14 | BY | BY1 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
| 2/20 | 0xFD15 | BY | BY2 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
...
| 13/20 | 0xFFD0 | F | F1 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
...
| 20/20 | 0xFFD7 | D | D4 | LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE |
```

### 2.4 Operands Section

**For each operand, document:**
- Role (Source, Destination, or both)
- Access pattern (Read, Write, ReadWrite)
- Data type constraint
- Supported addressing modes with examples

**Example:**
```markdown
## Operands

### Operand 1: Index Value

**Role:** Source
**Access:** Read
**Data Type:** Same as instruction prefix

**Supported Addressing Modes:**

- **CONSTANT:** `W1 CIND 5, 1, 10` (index is literal 5)
- **REGISTER:** `W1 CIND W2, 1, 10` (index in W2)
- **LOCAL:** `W1 CIND B.INDEX, 1, 10` (index from local var)
- **RECORD:** `W1 CIND R.INDEX, 1, 10` (index from record field)
- **PRE_INDEXED:** `W1 CIND B.ARRAY(W2), 1, 10` (indexed access)
- **ABSOLUTE:** `W1 CIND 0x1000, 1, 10` (index from absolute address)

### Operand 2: Lower Bound
...

### Operand 3: Upper Bound
...
```

### 2.5 Examples Section

**Types of examples to include:**

1. **Basic Usage** - Simplest form
2. **Common Pattern** - Typical real-world use
3. **Advanced Usage** - Complex scenario
4. **All Addressing Modes** - Demonstrate each mode
5. **Error Cases** - What causes traps

**Example Structure:**
```markdown
## Examples

### Example 1: Basic 3D Array Indexing

```assembly
; Array declared as: CUBE(0..9, 0..9, 0..9)
; Calculate address of CUBE(I, J, K)

        W1 := 0                 ; Initialize accumulator
        W1 CIND B.I, 0, 9      ; First dimension
        W1 CIND B.J, 0, 9      ; Second dimension
        W1 CIND B.K, 0, 9      ; Third dimension
        ; W1 now contains linear index
```

**Explanation:** CIND calculates multi-dimensional array offsets by
multiplying the accumulator by the range and adding the index.

**Result:** W1 = I×100 + J×10 + K

### Example 2: Bounds Checking

```assembly
; Access array element with automatic bounds check
        W1 := 0
        W1 CIND B.INDEX, 1, 100

; If B.INDEX < 1 or B.INDEX > 100:
;   - K flag set to 1
;   - IllegalIndex (IX) trap raised
;   - Trap handler can log error and recover
```

...
```

---

## 3. Validation and Verification Workflow

### 3.1 Automated Validation

**Checks to perform on each generated file:**

1. **File Naming**
   - Matches normalization rules
   - No special characters except underscore
   - All lowercase
   - `.md` extension

2. **Markdown Structure**
   - All required sections present
   - Proper heading hierarchy (# → ## → ###)
   - Code blocks properly fenced
   - Tables properly formatted

3. **Content Completeness**
   - Mnemonic matches instructions.json
   - Opcode values match instructions.json
   - All variants documented
   - All operands documented
   - Examples present (minimum 1)

4. **Cross-References**
   - Manual section reference exists (if available)
   - "See Also" links valid
   - No broken Markdown links

5. **Standards Compliance**
   - Hex notation uses `0x` prefix
   - Mnemonic format: `{prefix}{register} {mnemonic} {operands}`
   - Code comments present and descriptive

**Implementation:** Python validation script

```python
def validate_instruction_file(filepath):
    checks = {
        'file_naming': check_filename(filepath),
        'markdown_structure': check_sections(filepath),
        'content_completeness': check_required_fields(filepath),
        'cross_references': check_links(filepath),
        'standards_compliance': check_formatting(filepath)
    }
    return all(checks.values()), checks
```

### 3.2 Manual Verification

**Human review required for:**

1. **Example Accuracy**
   - Do examples assemble correctly?
   - Are they representative of real-world use?
   - Do they demonstrate the instruction clearly?

2. **Description Clarity**
   - Is the explanation understandable?
   - Are edge cases mentioned?
   - Is the operation correctly described?

3. **Reference Manual Alignment**
   - Does description match manual?
   - Are all traps documented?
   - Are status bit effects correct?

### 3.3 Validation Workflow

```
┌─────────────────────┐
│ Generate .md file   │
└──────────┬──────────┘
           │
           ↓
┌─────────────────────┐
│ Run automated       │
│ validation script   │
└──────────┬──────────┘
           │
      ┌────┴────┐
      │ Pass?   │
      └────┬────┘
           │
    ┌──────┴──────┐
    NO            YES
    │              │
    ↓              ↓
┌─────────┐   ┌─────────┐
│ Fix     │   │ Manual  │
│ issues  │   │ review  │
└────┬────┘   └────┬────┘
    │              │
    └──────┬───────┘
           │
           ↓
    ┌──────────────┐
    │ Mark as      │
    │ validated    │
    │ in TODO.md   │
    └──────────────┘
           │
           ↓
    ┌──────────────┐
    │ Commit to    │
    │ repository   │
    └──────────────┘
```

---

## 4. Generation Order

### 4.1 Grouping Strategy

**Generate instructions in class groups:**

1. **MOVE** (144 instructions) - Foundation
2. **ARITHMETIC** (288 instructions) - Core operations
3. **COMPARE** (38 instructions) - Testing/conditionals
4. **LOGICAL** (68 instructions) - Bit operations
5. **SHIFT** (11 instructions) - Bit shifting
6. **BRANCH** (63 instructions) - Control flow
7. **CALL** (21 instructions) - Subroutines
8. **CONTROL** (14 instructions) - System control
9. **FLOAT_MATH** (150 instructions) - Floating-point
10. **STRING** (74 instructions) - String operations
11. **BITFIELD** (58 instructions) - Bit fields
12. **SYSTEM** (148 instructions) - System operations
13. **IO** (1 instruction) - I/O operations

**Rationale:**
- MOVE first - most fundamental, establishes patterns
- ARITHMETIC next - builds on MOVE
- COMPARE/LOGICAL - needed for understanding BRANCH
- BRANCH/CALL - control flow depends on previous
- FLOAT_MATH - specialized math builds on ARITHMETIC
- STRING/BITFIELD - specialized operations
- SYSTEM last - most complex, references everything else

### 4.2 Within-Class Ordering

**Within each class, order by:**

1. **Simplicity** (simple → complex)
2. **Dependency** (referenced by others first)
3. **Frequency** (common instructions first)

**Example for MOVE class:**

```
1. := (assignto) - Most fundamental
2. =: (assignfrom) - Inverse of :=
3. clr - Simple clear operation
4. b:= (b_assignto) - Base register load
5. r:= (r_assignto) - Record register load
6. b=: (b_assignfrom) - Base register store
7. r=: (r_assignfrom) - Record register store
... (continue with remaining MOVE instructions)
```

### 4.3 Batch Processing

**Group size:** 10 instructions per batch

**Rationale:**
- Small enough to review thoroughly
- Large enough for efficient processing
- Allows incremental commits

**Batch workflow:**
```
1. Generate 10 instruction files
2. Run automated validation on batch
3. Manual review of batch
4. Fix any issues
5. Commit batch to repository
6. Update TODO.md
7. Repeat for next batch
```

---

## 5. Progress Tracking Method

### 5.1 TODO.md Structure

**Format:**

```markdown
## Generation Tasks (241 Instructions)

### MOVE (144 instructions) - 0% complete

- [ ] assignto.md (:=)
- [ ] assignfrom.md (=:)
- [ ] clr.md (clr)
...

### ARITHMETIC (288 instructions) - 0% complete

- [ ] add.md (add)
- [ ] sub.md (sub)
...

### Status Summary

| Class | Total | Completed | Remaining | Progress |
|-------|-------|-----------|-----------|----------|
| MOVE | 144 | 0 | 144 | 0% |
| ARITHMETIC | 288 | 0 | 288 | 0% |
...
| **TOTAL** | **241** | **0** | **241** | **0%** |
```

### 5.2 Update Mechanism

**After each instruction is generated and validated:**

1. Mark checkbox in TODO.md: `- [x] {filename}.md`
2. Update class progress percentage
3. Update status summary table
4. Commit to repository

**Example update:**
```markdown
### MOVE (144 instructions) - 2.1% complete (3/144)

- [x] assignto.md (:=) ✅
- [x] assignfrom.md (=:) ✅
- [x] clr.md (clr) ✅
- [ ] b_assignto.md (b:=)
...
```

### 5.3 Milestone Tracking

**Milestones:**

| Milestone | Instructions | Percentage | Target |
|-----------|-------------|------------|--------|
| Quick Win | 25 | 10% | End of Day 1 |
| Foundation | 50 | 21% | End of Day 2 |
| Half Complete | 120 | 50% | Mid-project |
| Final Sprint | 200 | 83% | Near completion |
| **Complete** | **241** | **100%** | Project end |

---

## 6. Data Sources and Extraction

### 6.1 Primary Data Source: instructions.json

**Extract from JSON:**
- Opcode (hex)
- Mnemonic
- Function name
- Class
- Prefix support
- Operand count
- Operand templates
- Allowed addressing modes
- Metadata

### 6.2 Secondary Source: Reference Manual

**Extract from manual:**
- Description text
- Operation pseudo-code
- Trap conditions
- Status bit effects
- Example code

**Method:**
- Use MANUAL_SECTION_MAPPING.json to find section
- Extract content from §X.Y
- Format as Markdown

### 6.3 Tertiary Source: YAML Files

**Use when:**
- JSON data insufficient
- Manual reference not available
- Need existing examples

**Extract from YAML:**
- Description field
- Examples array
- Manual reference content

### 6.4 Generation Priority

```
1. instructions.json (always)
   ↓
2. Reference Manual (if section mapped)
   ↓
3. YAML file (if manual not available)
   ↓
4. Generated placeholder (if nothing available)
```

---

## 7. Template System

### 7.1 Base Template

```markdown
# {instruction_name_title}

## Overview

**Mnemonic:** `{mnemonic}`
**Function:** `{function_name}`
**Class:** {class}
**Privilege:** {privilege}
**Operand Count:** {operand_count}

**Format:** `{format_syntax}`

---

## Description

{description_from_manual_or_generated}

**Operation:**
```
{operation_pseudocode}
```

{addressing_mode_summary}

---

## Variants

Total variants: {variant_count}

{variant_table_generated}

---

{if operand_count > 0}
## Operands

{for each operand}
### Operand {N}: {operand_description}

**Role:** {role}
**Access:** {access}
**Data Type:** {data_type_constraint}

**Supported Addressing Modes:**
{addressing_mode_list_with_examples}

{end for}
{endif}

---

## Trap Conditions

{trap_list_from_manual}

---

## Data Status Bits

{status_bits_from_manual}

---

## Examples

{examples_from_yaml_or_manual}

---

{if performance_notes_available}
## Performance Notes

{performance_notes}
{endif}

---

## Reference Manual

{if manual_section_mapped}
**Section:** §{section_number}
**Page:** {page_number}
**Title:** {section_title}
{else}
**Section:** Not yet mapped
{endif}

---

## See Also

{related_instructions_list}
- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
```

### 7.2 Template Variables

**All template variables with sources:**

| Variable | Source | Fallback |
|----------|--------|----------|
| `{instruction_name_title}` | Mnemonic (uppercase) | Function name |
| `{mnemonic}` | instructions.json | - |
| `{function_name}` | instructions.json | - |
| `{class}` | instructions.json | - |
| `{privilege}` | YAML or derive from class | "user" |
| `{operand_count}` | instructions.json | - |
| `{format_syntax}` | Reference manual | Generated from operands |
| `{description}` | Reference manual | YAML description field |
| `{operation_pseudocode}` | Reference manual | Generated placeholder |
| `{variant_count}` | Count from instructions.json | - |
| `{variant_table}` | Generated from instructions.json | - |
| `{trap_list}` | Reference manual | "Addressing traps" |
| `{status_bits}` | Reference manual | "Depends on operation" |
| `{examples}` | YAML examples or manual | Generated basic example |
| `{section_number}` | MANUAL_SECTION_MAPPING.json | "Not yet mapped" |

### 7.3 Example Generation

**When manual/YAML examples not available, generate basic example:**

```python
def generate_basic_example(instruction):
    if instruction['operandCount'] == 0:
        return f"{instruction['mnemonic'].upper()}\n; {instruction['functionName']}"

    elif instruction['operandCount'] == 1:
        prefix = instruction['prefixes'][0] if instruction['prefixes'] else 'W'
        return f"{prefix}1 {instruction['mnemonic'].upper()} B.VAR\n; Basic usage"

    elif instruction['operandCount'] == 2:
        prefix = instruction['prefixes'][0] if instruction['prefixes'] else 'W'
        return f"{prefix}1 {instruction['mnemonic'].upper()} {prefix}2\n; Two-operand operation"

    # ... etc for 3 and 4 operands
```

---

## 8. Quality Assurance

### 8.1 Quality Metrics

**For each generated file, measure:**

1. **Completeness Score** (0-100%)
   - All required sections present: 30%
   - All variants documented: 20%
   - Examples present: 20%
   - Manual reference linked: 15%
   - "See Also" links present: 15%

2. **Accuracy Score** (manual review, 0-100%)
   - Description matches manual: 40%
   - Examples are correct: 30%
   - Trap conditions complete: 15%
   - Status bits correct: 15%

3. **Usability Score** (manual review, 0-100%)
   - Clear explanation: 35%
   - Good examples: 35%
   - Helpful notes: 15%
   - Related links: 15%

**Target:** All files ≥ 80% on each metric

### 8.2 Review Checklist

**Before marking instruction as complete:**

- [ ] File name follows convention
- [ ] All required sections present
- [ ] Opcode values verified against JSON
- [ ] All variants listed
- [ ] Operands fully documented
- [ ] At least 1 working example
- [ ] Trap conditions listed
- [ ] Status bit effects documented
- [ ] Manual reference linked (if available)
- [ ] "See Also" section has ≥2 links
- [ ] Code examples use proper formatting
- [ ] No broken Markdown links
- [ ] Passes automated validation
- [ ] Human review completed

---

## 9. Risk Mitigation

### 9.1 Known Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| instructions.json incomplete | HIGH | Medium | Cross-check with Reference Manual, document gaps |
| Manual sections not mapped | MEDIUM | High | Use YAML as fallback, generate placeholders |
| Example code errors | HIGH | Medium | Manual testing, peer review |
| Inconsistent formatting | MEDIUM | Low | Automated validation, pre-commit hooks |
| Large volume (241 files) | MEDIUM | High | Batch processing, incremental commits |

### 9.2 Contingency Plans

**If instructions.json incomplete:**
- Use Reference Manual as primary source
- Document discrepancies in MANUAL_SECTION_MAPPING.json
- Flag for manual review

**If manual sections not mapped:**
- Use YAML description field
- Generate minimal documentation
- Mark for future enhancement

**If example errors found:**
- Add correction to TODO.md
- Prioritize fix in next batch
- Run syntax check if assembler available

---

## 10. Tools and Scripts

### 10.1 Generation Script (Planned)

```python
#!/usr/bin/env python3
"""
generate_instruction_docs.py

Generates Markdown documentation files for ND-500 instructions
from instructions.json and supporting sources.
"""

import json
from pathlib import Path
from typing import Dict, List, Optional

class InstructionDocGenerator:
    def __init__(self, config: Dict):
        self.instructions_json = config['instructions_json']
        self.manual_mapping = config['manual_mapping']
        self.output_dir = Path(config['output_dir'])
        self.template = self.load_template(config['template_file'])

    def generate_all(self):
        """Generate documentation for all instructions"""
        instructions = self.load_instructions()

        for instruction in instructions:
            filename = self.normalize_filename(instruction['mnemonic'])
            filepath = self.output_dir / filename

            content = self.generate_content(instruction)
            self.write_file(filepath, content)

            print(f"Generated: {filename}")

    def generate_content(self, instruction: Dict) -> str:
        """Generate Markdown content for one instruction"""
        # Fill template with data
        # Extract from manual if available
        # Add examples
        # Format properly
        pass

    # ... more methods
```

### 10.2 Validation Script (Planned)

```python
#!/usr/bin/env python3
"""
validate_instruction_docs.py

Validates generated instruction documentation files.
"""

import re
from pathlib import Path

class InstructionDocValidator:
    def validate_file(self, filepath: Path) -> Dict:
        """Validate one instruction file"""
        results = {
            'file_naming': self.check_filename(filepath),
            'sections': self.check_sections(filepath),
            'content': self.check_content(filepath),
            'formatting': self.check_formatting(filepath),
        }

        results['overall'] = all(results.values())
        return results

    def check_sections(self, filepath: Path) -> bool:
        """Verify all required sections present"""
        required = [
            '## Overview',
            '## Description',
            '## Variants',
            '## Examples',
            '## Reference Manual'
        ]

        content = filepath.read_text()
        return all(section in content for section in required)

    # ... more validation methods
```

---

## 11. Timeline Estimate

### 11.1 Phase Breakdown

| Phase | Tasks | Estimated Time |
|-------|-------|----------------|
| **Phase 2 (Current)** | Planning | 2-3 hours |
| **Phase 3 - Setup** | Scripts, templates | 3-4 hours |
| **Phase 3 - Batch 1** | First 25 instructions | 4-6 hours |
| **Phase 3 - Batch 2-10** | Next 225 instructions | 20-30 hours |
| **Phase 3 - Review** | Manual review all | 8-12 hours |
| **Phase 3 - Fixes** | Corrections | 4-6 hours |
| **Total** | | **41-61 hours** |

### 11.2 Realistic Schedule

**Assuming 4 hours/day focused work:**

- **Week 1:** Phase 2 + Phase 3 setup + Batch 1-2 (50 instructions)
- **Week 2:** Batches 3-7 (125 instructions)
- **Week 3:** Batches 8-10 + Review (66 instructions + review)
- **Week 4:** Fixes + Final validation

**Total:** ~4 weeks for complete documentation of all 241 instructions

---

## 12. Success Criteria

### 12.1 Phase 2 Success Criteria

- [x] File structure defined
- [x] Section layout documented
- [x] Validation workflow established
- [x] Generation order planned
- [x] Progress tracking method defined
- [x] Template system designed
- [ ] User approval received

### 12.2 Phase 3 Success Criteria

- [ ] All 241 instruction files generated
- [ ] All files pass automated validation
- [ ] All files manually reviewed
- [ ] All corrections applied
- [ ] All files committed to repository
- [ ] TODO.md shows 100% completion
- [ ] Quality metrics ≥80% for all files

---

**Last Updated:** 2025-11-08
**Status:** Complete - awaiting user approval
**Next:** User must approve plan to proceed to Phase 3
