#!/usr/bin/env python3
"""
Add final batch of missing instruction mappings discovered through systematic manual search

This script adds the remaining mappings to reach as close to 100% as possible.
"""

import json

# Load current mapping
with open('/home/user/nd500x/docs/instructions/MANUAL_SECTION_MAPPING.json', 'r') as f:
    data = json.load(f)

# Final batch of mappings discovered
new_mappings = [
    # Conditional branches (§13.3)
    {"mnemonic": "if-kgo", "section": "13.3", "title": "Conditional jump - if K set", "category": "CONTROL"},
    {"mnemonic": "if-stgo", "section": "13.3", "title": "Conditional jump - if status bit set", "category": "CONTROL"},
    {"mnemonic": "if<<=go", "section": "13.3", "title": "Conditional jump - if less magnitude", "category": "CONTROL"},
    {"mnemonic": "if<<go", "section": "13.3", "title": "Conditional jump - if less magnitude (byte disp)", "category": "CONTROL"},
    {"mnemonic": "if>>=go", "section": "13.3", "title": "Conditional jump - if greater/equal magnitude", "category": "CONTROL"},
    {"mnemonic": "if>>go", "section": "13.3", "title": "Conditional jump - if greater magnitude", "category": "CONTROL"},
    {"mnemonic": "ifkret", "section": "13.11", "title": "If K set return from subroutine", "category": "CONTROL"},

    # Cache and memory management (§16.10-16.16, §16.24)
    {"mnemonic": "dcc", "section": "16.10", "title": "Data cache clear", "category": "SYSTEM"},
    {"mnemonic": "pcc", "section": "16.12", "title": "Program cache clear", "category": "SYSTEM"},
    {"mnemonic": "pmon", "section": "16.14", "title": "Program memory management on", "category": "SYSTEM"},
    {"mnemonic": "dmof", "section": "16.15", "title": "Data memory management off", "category": "SYSTEM"},
    {"mnemonic": "pmof", "section": "16.16", "title": "Program memory management off", "category": "SYSTEM"},
    {"mnemonic": "pctsb", "section": "16.24", "title": "Clear program translation speedup buffer", "category": "SYSTEM"},
    {"mnemonic": "dctsb", "section": "16.24", "title": "Clear data translation speedup buffer", "category": "SYSTEM"},

    # String search operations (§14.11-14.20)
    {"mnemonic": "scotr", "section": "14.11", "title": "String compare translated", "category": "STRING"},
    {"mnemonic": "scopa", "section": "14.12", "title": "String compare with pad", "category": "STRING"},
    {"mnemonic": "scopt", "section": "14.13", "title": "String compare translated with pad", "category": "STRING"},
    {"mnemonic": "sloca", "section": "14.15", "title": "String locate element", "category": "STRING"},
    {"mnemonic": "sspar", "section": "14.19", "title": "Set parity in string", "category": "STRING"},
    {"mnemonic": "schpar", "section": "14.20", "title": "Check parity in string", "category": "STRING"},

    # Comparison and test (§10.10)
    {"mnemonic": "comp2", "section": "10.10", "title": "Compare two operands", "category": "ARITHMETIC"},

    # Process/trap control (§16.1, §16.2, §16.6)
    {"mnemonic": "solo", "section": "16.1", "title": "Disable process switch", "category": "SYSTEM"},
    {"mnemonic": "tutti", "section": "16.2", "title": "Enable process switch", "category": "SYSTEM"},
    {"mnemonic": "clte", "section": "16.6", "title": "Clear bit in trap enable register", "category": "SYSTEM"},

    # Page table management (§16.21, §16.22)
    {"mnemonic": "zpgu", "section": "16.21", "title": "Clear page used bit", "category": "SYSTEM"},
    {"mnemonic": "cpgu", "section": "16.22", "title": "Clear page used table", "category": "SYSTEM"},

    # I/O and system (§16.18, §16.23)
    {"mnemonic": "zwip", "section": "16.18", "title": "Clear written in page bit", "category": "SYSTEM"},
    {"mnemonic": "riom", "section": "16.23", "title": "Read I/O processor memory", "category": "SYSTEM"},

    # Instructions not found in manual - marked as TBD
    {"mnemonic": "ps:=", "section": "TBD", "title": "Load process segment (not documented in manual)", "category": "SYSTEM"},
    {"mnemonic": "wdus", "section": "TBD", "title": "Write data unsynchronized (not documented in manual)", "category": "SYSTEM"},
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
        status = "⚠" if mapping['section'] == "TBD" else "✓"
        print(f"{status} Added: {mapping['mnemonic']:15s} (§{mapping['section']})")
    else:
        print(f"⊙ Skipped (already exists): {mapping['mnemonic']}")

# Update counts
data['mapped_instructions'] = len(data['mappings'])
data['coverage_percentage'] = int((len(data['mappings']) / 241) * 100)

# Count TBD entries
tbd_count = sum(1 for entry in data['mappings'] if entry.get('section') == 'TBD')

if tbd_count > 0:
    data['mapping_status'] = f"{data['coverage_percentage']}% coverage ({data['mapped_instructions']}/241) - {tbd_count} marked TBD"
else:
    data['mapping_status'] = f"{data['coverage_percentage']}% coverage ({data['mapped_instructions']}/241) - COMPLETE"

# Save
with open('/home/user/nd500x/docs/instructions/MANUAL_SECTION_MAPPING.json', 'w') as f:
    json.dump(data, f, indent=2)

print(f"\n✅ Added {added_count} new mappings")
print(f"📊 Total mappings: {data['mapped_instructions']}/241 ({data['coverage_percentage']}%)")
print(f"📋 Remaining unmapped: {241 - data['mapped_instructions']}")
if tbd_count > 0:
    print(f"⚠️  Instructions marked TBD (not found in manual): {tbd_count}")
