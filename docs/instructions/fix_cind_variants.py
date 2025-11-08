#!/usr/bin/env python3
"""
Fix CIND instruction variants in instructions.json

Adds missing Float (F1-F4) and Double (D1-D4) variants to match
Reference Manual §15.9 specification.

Missing opcodes:
- F1 CIND: 0xFFD0
- F2 CIND: 0xFFD1
- F3 CIND: 0xFFD2
- F4 CIND: 0xFFD3
- D1 CIND: 0xFFD4
- D2 CIND: 0xFFD5
- D3 CIND: 0xFFD6
- D4 CIND: 0xFFD7
"""

import json
import sys

def load_instructions(filepath):
    """Load instructions.json"""
    with open(filepath, 'r', encoding='utf-8') as f:
        return json.load(f)

def save_instructions(filepath, data):
    """Save instructions.json with proper formatting"""
    with open(filepath, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)

def find_cind_entries(instructions):
    """Find all CIND instruction entries"""
    cind_entries = []
    for i, instr in enumerate(instructions):
        if instr.get('mnemonic', '').lower() == 'cind':
            cind_entries.append((i, instr))
    return cind_entries

def create_float_double_variants(template_entry):
    """
    Create Float and Double variants based on template entry

    Float variants (F1-F4): 0xFFD0-0xFFD3, variantNumber=3
    Double variants (D1-D4): 0xFFD4-0xFFD7, variantNumber=4
    """
    new_entries = []

    # Float variants (F1-F4)
    for reg in range(4):
        opcode = 0xFFD0 + reg
        entry = template_entry.copy()
        entry['opcode'] = f"0x{opcode:04X}"
        entry['variantNumber'] = 3  # F = 3
        entry['totalVariants'] = 5  # BY, H, W, F, D
        entry['prefixes'] = "InstructionPrefixes.BY | InstructionPrefixes.H | InstructionPrefixes.W | InstructionPrefixes.F | InstructionPrefixes.D | InstructionPrefixes.R_N"
        new_entries.append(entry)

    # Double variants (D1-D4)
    for reg in range(4):
        opcode = 0xFFD4 + reg
        entry = template_entry.copy()
        entry['opcode'] = f"0x{opcode:04X}"
        entry['variantNumber'] = 4  # D = 4
        entry['totalVariants'] = 5  # BY, H, W, F, D
        entry['prefixes'] = "InstructionPrefixes.BY | InstructionPrefixes.H | InstructionPrefixes.W | InstructionPrefixes.F | InstructionPrefixes.D | InstructionPrefixes.R_N"
        new_entries.append(entry)

    return new_entries

def update_existing_cind_entries(cind_entries):
    """Update existing CIND entries to reflect 5 total variants and updated prefixes"""
    updated_count = 0
    for idx, entry in cind_entries:
        if entry['totalVariants'] != 5:
            entry['totalVariants'] = 5
            entry['prefixes'] = "InstructionPrefixes.BY | InstructionPrefixes.H | InstructionPrefixes.W | InstructionPrefixes.F | InstructionPrefixes.D | InstructionPrefixes.R_N"
            updated_count += 1
    return updated_count

def main():
    filepath = '/home/user/nd500x/docs/instructions/instructions.json'

    print("Loading instructions.json...")
    data = load_instructions(filepath)

    instructions = data['instructions']
    total_before = len(instructions)

    print(f"Total instructions before: {total_before}")

    # Find CIND entries
    cind_entries = find_cind_entries(instructions)
    print(f"Found {len(cind_entries)} existing CIND variants")

    if len(cind_entries) == 0:
        print("ERROR: No CIND entries found!")
        return 1

    # Update existing CIND entries
    updated_count = update_existing_cind_entries(cind_entries)
    print(f"Updated {updated_count} existing CIND entries (totalVariants → 5, prefixes → include F|D)")

    # Create new Float and Double variants using first CIND entry as template
    template = cind_entries[0][1]
    new_variants = create_float_double_variants(template)
    print(f"Created {len(new_variants)} new Float/Double variants")

    # Insert new variants after the last CIND entry
    last_cind_index = cind_entries[-1][0]
    insertion_point = last_cind_index + 1

    for i, variant in enumerate(new_variants):
        instructions.insert(insertion_point + i, variant)

    # Update total instruction count
    data['totalInstructions'] = len(instructions)
    total_after = len(instructions)

    print(f"Total instructions after: {total_after}")
    print(f"Added {total_after - total_before} new instructions")

    # Save updated JSON
    print(f"Saving updated instructions.json...")
    save_instructions(filepath, data)

    print("✅ CIND variants fix completed successfully!")
    print("\nSummary:")
    print(f"  - Existing CIND variants: 12 (BY1-BY4, H1-H4, W1-W4)")
    print(f"  - Added Float variants: 4 (F1-F4: 0xFFD0-0xFFD3)")
    print(f"  - Added Double variants: 4 (D1-D4: 0xFFD4-0xFFD7)")
    print(f"  - Total CIND variants now: 20")

    return 0

if __name__ == '__main__':
    sys.exit(main())
