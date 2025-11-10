#!/usr/bin/env python3
"""
Extract all 46 missing instruction mappings from Reference Manual

This script adds the missing manual section mappings found through systematic
manual reading of the ND-05.009.4 EN ND-500 Reference Manual.
"""

import json

# Load existing mapping
with open('/home/user/nd500x/docs/instructions/MANUAL_SECTION_MAPPING.json', 'r') as f:
    data = json.load(f)

# New mappings extracted from manual (sections 16.7, 16.8, 16.9, 16.27, 16.33, 14.x, 15.x)
new_mappings = [
    # Integer/Float register communication (§16.9)
    {"mnemonic": "a1:=", "section": "16.9", "title": "Integer float register communication - load A1", "category": "MOVE"},
    {"mnemonic": "a2:=", "section": "16.9", "title": "Integer float register communication - load A2", "category": "MOVE"},
    {"mnemonic": "a3:=", "section": "16.9", "title": "Integer float register communication - load A3", "category": "MOVE"},
    {"mnemonic": "a4:=", "section": "16.9", "title": "Integer float register communication - load A4", "category": "MOVE"},
    {"mnemonic": "e1:=", "section": "16.9", "title": "Integer float register communication - load E1", "category": "MOVE"},
    {"mnemonic": "e2:=", "section": "16.9", "title": "Integer float register communication - load E2", "category": "MOVE"},
    {"mnemonic": "e3:=", "section": "16.9", "title": "Integer float register communication - load E3", "category": "MOVE"},
    {"mnemonic": "e4:=", "section": "16.9", "title": "Integer float register communication - load E4", "category": "MOVE"},
    {"mnemonic": "a1=:", "section": "16.9", "title": "Integer float register communication - store A1", "category": "MOVE"},
    {"mnemonic": "a2=:", "section": "16.9", "title": "Integer float register communication - store A2", "category": "MOVE"},
    {"mnemonic": "a3=:", "section": "16.9", "title": "Integer float register communication - store A3", "category": "MOVE"},
    {"mnemonic": "a4=:", "section": "16.9", "title": "Integer float register communication - store A4", "category": "MOVE"},
    {"mnemonic": "e1=:", "section": "16.9", "title": "Integer float register communication - store E1", "category": "MOVE"},
    {"mnemonic": "e2=:", "section": "16.9", "title": "Integer float register communication - store E2", "category": "MOVE"},
    {"mnemonic": "e3=:", "section": "16.9", "title": "Integer float register communication - store E3", "category": "MOVE"},
    {"mnemonic": "e4=:", "section": "16.9", "title": "Integer float register communication - store E4", "category": "MOVE"},

    # Special register loads (§16.7)
    {"mnemonic": "l:=", "section": "16.7", "title": "Load special register - link register", "category": "MOVE"},
    {"mnemonic": "hl:=", "section": "16.7", "title": "Load special register - upper limit", "category": "MOVE"},
    {"mnemonic": "ll:=", "section": "16.7", "title": "Load special register - lower limit", "category": "MOVE"},
    {"mnemonic": "st1:=", "section": "16.7", "title": "Load special register - 1st status", "category": "MOVE"},
    {"mnemonic": "ote1:=", "section": "16.7", "title": "Load special register - 1st own trap enable", "category": "MOVE"},
    {"mnemonic": "ote2:=", "section": "16.7", "title": "Load special register - 2nd own trap enable", "category": "MOVE"},
    {"mnemonic": "tos:=", "section": "16.7", "title": "Load special register - top of stack", "category": "MOVE"},
    {"mnemonic": "tha:=", "section": "16.7", "title": "Load special register - trap handler", "category": "MOVE"},

    # Special register stores (§16.8)
    {"mnemonic": "l=:", "section": "16.8", "title": "Store special register - link register", "category": "MOVE"},
    {"mnemonic": "hl=:", "section": "16.8", "title": "Store special register - upper limit", "category": "MOVE"},
    {"mnemonic": "ll=:", "section": "16.8", "title": "Store special register - lower limit", "category": "MOVE"},
    {"mnemonic": "st1=:", "section": "16.8", "title": "Store special register - 1st status", "category": "MOVE"},
    {"mnemonic": "ote1=:", "section": "16.8", "title": "Store special register - 1st own trap enable", "category": "MOVE"},
    {"mnemonic": "ote2=:", "section": "16.8", "title": "Store special register - 2nd own trap enable", "category": "MOVE"},
    {"mnemonic": "mte1=:", "section": "16.8", "title": "Store special register - 1st mother trap enable", "category": "MOVE"},
    {"mnemonic": "mte2=:", "section": "16.8", "title": "Store special register - 2nd mother trap enable", "category": "MOVE"},
    {"mnemonic": "cte1=:", "section": "16.8", "title": "Store special register - 1st child trap enable", "category": "MOVE"},
    {"mnemonic": "cte2=:", "section": "16.8", "title": "Store special register - 2nd child trap enable", "category": "MOVE"},
    {"mnemonic": "temm1=:", "section": "16.8", "title": "Store special register - 1st trap enable modification mask", "category": "MOVE"},
    {"mnemonic": "temm2=:", "section": "16.8", "title": "Store special register - 2nd trap enable modification mask", "category": "MOVE"},
    {"mnemonic": "ced=:", "section": "16.8", "title": "Store special register - current executing domain", "category": "MOVE"},
    {"mnemonic": "cad=:", "section": "16.8", "title": "Store special register - current alternative domain", "category": "MOVE"},
    {"mnemonic": "ps=:", "section": "16.8", "title": "Store special register - process segment", "category": "MOVE"},
    {"mnemonic": "tos=:", "section": "16.8", "title": "Store special register - top of stack", "category": "MOVE"},
    {"mnemonic": "tha=:", "section": "16.8", "title": "Store special register - trap handler", "category": "MOVE"},
    {"mnemonic": "p=:", "section": "16.8", "title": "Store special register - program counter", "category": "MOVE"},

    # CAD load (§16.33)
    {"mnemonic": "cad:=", "section": "16.33", "title": "Load CAD - current alternative domain", "category": "SYSTEM"},

    # Register/context block manipulation (§16.27)
    {"mnemonic": "lregbl", "section": "16.27.2", "title": "Load register block", "category": "SYSTEM"},
    {"mnemonic": "sregbl", "section": "16.27.1", "title": "Save register block", "category": "SYSTEM"},
    {"mnemonic": "lcntxt", "section": "16.27.4", "title": "Load context block", "category": "SYSTEM"},
    {"mnemonic": "scntxt", "section": "16.27.3", "title": "Save context block", "category": "SYSTEM"},

    # Address loading (§15.4, §15.5, §15.6)
    {"mnemonic": "bladdr", "section": "15.6", "title": "Load address into base register", "category": "MOVE"},
    {"mnemonic": "rladdr", "section": "15.5", "title": "Load address into record register", "category": "MOVE"},

    # String operations (§14.x)
    {"mnemonic": "smvwh", "section": "14.3", "title": "String move while", "category": "STRING"},
    {"mnemonic": "smvun", "section": "14.4", "title": "String move until", "category": "STRING"},
    {"mnemonic": "smvtr", "section": "14.5", "title": "String move translated", "category": "STRING"},
    {"mnemonic": "smvtu", "section": "14.6", "title": "String move translated until", "category": "STRING"},
    {"mnemonic": "smovn", "section": "14.7", "title": "String move n elements", "category": "STRING"},

    # Block move (§15.x - found near line 9241)
    {"mnemonic": "bmove", "section": "15.13", "title": "Block move", "category": "MOVE"},

    # System/special (various sections)
    {"mnemonic": "freeb", "section": "15.14", "title": "Free buddy element", "category": "SYSTEM"},
]

# Add new mappings
data['mappings'].extend(new_mappings)

# Update counts
data['mapped_instructions'] = len(data['mappings'])
data['coverage_percentage'] = int((len(data['mappings']) / 241) * 100)
data['mapping_status'] = f"EXPANDED - {data['coverage_percentage']}% coverage"

# Save
with open('/home/user/nd500x/docs/instructions/MANUAL_SECTION_MAPPING.json', 'w') as f:
    json.dump(data, f, indent=2)

print(f"✅ Added {len(new_mappings)} new mappings")
print(f"📊 Total mappings: {data['mapped_instructions']}/241 ({data['coverage_percentage']}%)")
print(f"📋 Remaining unmapped: {241 - data['mapped_instructions']}")
