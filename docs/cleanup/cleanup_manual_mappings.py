#!/usr/bin/env python3
"""
Clean up MANUAL_SECTION_MAPPING.json to achieve exactly 100% coverage

This script:
1. Loads actual instruction mnemonics from instructions.json
2. Validates all mapping entries against actual mnemonics
3. Removes incorrect/duplicate entries
4. Reports missing instructions that still need mapping
"""

import json
from collections import defaultdict

# Load actual instructions
with open('/home/user/nd500x/docs/instructions/instructions.json', 'r') as f:
    instr_data = json.load(f)

# Extract all unique mnemonics
actual_mnemonics = set()
for instr in instr_data['instructions']:
    mnemonic = instr.get('mnemonic', '').lower()
    if mnemonic:
        actual_mnemonics.add(mnemonic)

print(f"📊 Actual unique mnemonics in instructions.json: {len(actual_mnemonics)}")

# Load current mapping
with open('/home/user/nd500x/docs/instructions/MANUAL_SECTION_MAPPING.json', 'r') as f:
    mapping_data = json.load(f)

print(f"📋 Current mappings: {len(mapping_data['mappings'])}")

# Validate each mapping entry
valid_mappings = []
invalid_mappings = []
seen_mnemonics = set()
duplicates = []

for entry in mapping_data['mappings']:
    mnemonic = entry.get('mnemonic', '').lower()

    # Check if mnemonic exists in instructions.json
    if mnemonic not in actual_mnemonics:
        invalid_mappings.append(entry)
        continue

    # Check for duplicates
    if mnemonic in seen_mnemonics:
        duplicates.append(entry)
        continue

    seen_mnemonics.add(mnemonic)
    valid_mappings.append(entry)

print(f"\n✅ Valid mappings: {len(valid_mappings)}")
print(f"❌ Invalid mappings (mnemonic not in instructions.json): {len(invalid_mappings)}")
print(f"🔄 Duplicate mappings: {len(duplicates)}")

# Show invalid entries
if invalid_mappings:
    print(f"\n❌ Invalid mappings to be removed ({len(invalid_mappings)}):")
    for entry in invalid_mappings[:20]:  # Show first 20
        print(f"   - {entry['mnemonic']}: {entry['section']} - {entry['title']}")
    if len(invalid_mappings) > 20:
        print(f"   ... and {len(invalid_mappings) - 20} more")

# Show duplicates
if duplicates:
    print(f"\n🔄 Duplicate mappings to be removed ({len(duplicates)}):")
    for entry in duplicates[:10]:
        print(f"   - {entry['mnemonic']}: {entry['section']} - {entry['title']}")
    if len(duplicates) > 10:
        print(f"   ... and {len(duplicates) - 10} more")

# Find missing mnemonics
mapped_mnemonics = set(seen_mnemonics)
missing_mnemonics = actual_mnemonics - mapped_mnemonics

print(f"\n📋 Missing mnemonics (in instructions.json but not mapped): {len(missing_mnemonics)}")
if missing_mnemonics:
    sorted_missing = sorted(list(missing_mnemonics))
    print("Missing mnemonics:")
    for i, mnem in enumerate(sorted_missing, 1):
        print(f"   {i:3d}. {mnem}")

# Save cleaned mapping
mapping_data['mappings'] = valid_mappings
mapping_data['mapped_instructions'] = len(valid_mappings)
mapping_data['coverage_percentage'] = int((len(valid_mappings) / 241) * 100)
mapping_data['mapping_status'] = f"CLEANED - {mapping_data['coverage_percentage']}% coverage ({len(valid_mappings)}/241)"

with open('/home/user/nd500x/docs/instructions/MANUAL_SECTION_MAPPING.json', 'w') as f:
    json.dump(mapping_data, f, indent=2)

print(f"\n✅ Cleaned mapping saved")
print(f"📊 Final statistics:")
print(f"   - Total mappings: {len(valid_mappings)}/241")
print(f"   - Coverage: {mapping_data['coverage_percentage']}%")
print(f"   - Still missing: {len(missing_mnemonics)}")
print(f"   - Removed invalid: {len(invalid_mappings)}")
print(f"   - Removed duplicates: {len(duplicates)}")
