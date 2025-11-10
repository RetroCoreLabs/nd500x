#!/usr/bin/env python3
"""
Add remaining missing instruction mappings found through systematic manual search

This script adds mappings for the 54 missing instructions identified after cleanup.
"""

import json

# Load current mapping
with open('/home/user/nd500x/docs/instructions/MANUAL_SECTION_MAPPING.json', 'r') as f:
    data = json.load(f)

# New mappings discovered through systematic manual search
new_mappings = [
    # Data type conversion instructions (§15.2)
    {"mnemonic": "biconv", "section": "15.2", "title": "Bit convert", "category": "ARITHMETIC"},
    {"mnemonic": "byconv", "section": "15.2", "title": "Byte convert", "category": "ARITHMETIC"},
    {"mnemonic": "hconv", "section": "15.2", "title": "Halfword convert", "category": "ARITHMETIC"},
    {"mnemonic": "wconv", "section": "15.2", "title": "Word convert", "category": "ARITHMETIC"},
    {"mnemonic": "fconv", "section": "15.2", "title": "Float convert", "category": "ARITHMETIC"},
    {"mnemonic": "dconv", "section": "15.2", "title": "Double float convert", "category": "ARITHMETIC"},
    {"mnemonic": "byconr", "section": "15.2", "title": "Byte convert rounded", "category": "ARITHMETIC"},
    {"mnemonic": "hconr", "section": "15.2", "title": "Halfword convert rounded", "category": "ARITHMETIC"},
    {"mnemonic": "wconr", "section": "15.2", "title": "Word convert rounded", "category": "ARITHMETIC"},
    {"mnemonic": "fconr", "section": "15.2", "title": "Float convert rounded", "category": "ARITHMETIC"},

    # Shift instructions
    {"mnemonic": "shl", "section": "10.24", "title": "Logical shift", "category": "ARITHMETIC"},
    {"mnemonic": "sha", "section": "10.25", "title": "Arithmetical shift", "category": "ARITHMETIC"},
    {"mnemonic": "shr", "section": "10.26", "title": "Rotational shift", "category": "ARITHMETIC"},

    # BCD packed shift instructions (§17.6)
    {"mnemonic": "pshift", "section": "17.6", "title": "Packed shift", "category": "BCD"},
    {"mnemonic": "pshiftr", "section": "17.6", "title": "Packed shift rounded", "category": "BCD"},

    # Bit manipulation (§10.27, §10.28)
    {"mnemonic": "getbi", "section": "10.27", "title": "Get bit", "category": "LOGICAL"},
    {"mnemonic": "putbi", "section": "10.28", "title": "Put bit", "category": "LOGICAL"},

    # Special instructions
    {"mnemonic": "bp", "section": "16.4", "title": "Break point", "category": "SYSTEM"},
    {"mnemonic": "sete", "section": "16.5", "title": "Set bit in trap enable register", "category": "SYSTEM"},

    # Return instructions (§13.11)
    {"mnemonic": "retk", "section": "13.11", "title": "Set flag return from subroutine", "category": "CONTROL"},
    {"mnemonic": "retbk", "section": "13.11", "title": "Set flag buddy subroutine return", "category": "CONTROL"},

    # Store zero / Set to one
    {"mnemonic": "stz", "section": "10.17", "title": "Store zero", "category": "MOVE"},
    {"mnemonic": "set1", "section": "10.18", "title": "Set to one", "category": "MOVE"},

    # Test and set
    {"mnemonic": "tset", "section": "16.3", "title": "Test and set", "category": "SYSTEM"},
]

# Get current mapped mnemonics
current_mnemonics = {entry['mnemonic'].lower() for entry in data['mappings']}

# Add only new mappings (avoid duplicates)
added_count = 0
for mapping in new_mappings:
    if mapping['mnemonic'].lower() not in current_mnemonics:
        data['mappings'].append(mapping)
        current_mnemonics.add(mapping['mnemonic'].lower())
        added_count += 1
        print(f"✓ Added: {mapping['mnemonic']} (§{mapping['section']})")
    else:
        print(f"⊙ Skipped (already exists): {mapping['mnemonic']}")

# Update counts
data['mapped_instructions'] = len(data['mappings'])
data['coverage_percentage'] = int((len(data['mappings']) / 241) * 100)
data['mapping_status'] = f"{data['coverage_percentage']}% coverage ({data['mapped_instructions']}/241)"

# Save
with open('/home/user/nd500x/docs/instructions/MANUAL_SECTION_MAPPING.json', 'w') as f:
    json.dump(data, f, indent=2)

print(f"\n✅ Added {added_count} new mappings")
print(f"📊 Total mappings: {data['mapped_instructions']}/241 ({data['coverage_percentage']}%)")
print(f"📋 Remaining unmapped: {241 - data['mapped_instructions']}")
