#!/usr/bin/env python3
"""
Check which instructions are still unmapped
"""

import json

# Load actual instructions
with open('/home/user/nd500x/docs/instructions/instructions.json', 'r') as f:
    instr_data = json.load(f)

# Extract all unique mnemonics
actual_mnemonics = set()
for instr in instr_data['instructions']:
    mnemonic = instr.get('mnemonic', '').lower()
    if mnemonic:
        actual_mnemonics.add(mnemonic)

# Load current mapping
with open('/home/user/nd500x/docs/instructions/MANUAL_SECTION_MAPPING.json', 'r') as f:
    mapping_data = json.load(f)

# Get mapped mnemonics
mapped_mnemonics = {entry['mnemonic'].lower() for entry in mapping_data['mappings']}

# Find missing
missing = sorted(list(actual_mnemonics - mapped_mnemonics))

print(f"Total instructions: {len(actual_mnemonics)}")
print(f"Mapped: {len(mapped_mnemonics)}")
print(f"Missing: {len(missing)}\n")

print("Remaining unmapped instructions:")
for i, mnem in enumerate(missing, 1):
    print(f"  {i:3d}. {mnem}")
