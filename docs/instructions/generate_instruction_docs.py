#!/usr/bin/env python3
"""
ND-500 Instruction Documentation Generator

Generates comprehensive markdown documentation for all 241 ND-500 instructions
based on instructions.json data, manual section mappings, and the validated template.

Usage:
    python3 generate_instruction_docs.py [--batch CLASS] [--limit N] [--dry-run]

Options:
    --batch CLASS   Generate only instructions from specified class (MOVE, ARITHMETIC, etc.)
    --limit N       Generate only first N instructions
    --dry-run       Show what would be generated without creating files
"""

import json
import sys
import argparse
from pathlib import Path
from collections import defaultdict

# Filename normalization for special characters
FILENAME_MAPPING = {
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
    '<': 'lt',
    '>': 'gt',
    '=': 'eq',
    '<=': 'le',
    '>=': 'ge',
    '<>': 'ne',
    '><': 'ne2'
}

def normalize_filename(mnemonic):
    """Convert mnemonic to valid filename"""
    if mnemonic in FILENAME_MAPPING:
        return FILENAME_MAPPING[mnemonic]
    # Replace any remaining special chars
    safe = mnemonic.lower()
    safe = safe.replace(':', '_').replace('/', '_').replace('*', '_')
    return safe

def load_json(filepath):
    """Load JSON file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        return json.load(f)

def group_instructions_by_mnemonic(instructions):
    """Group instruction variants by mnemonic"""
    grouped = defaultdict(list)
    for instr in instructions:
        mnemonic = instr.get('mnemonic', '').lower()
        if mnemonic:
            grouped[mnemonic].append(instr)
    return dict(grouped)

def parse_prefixes(prefix_str):
    """Parse prefix string to list of prefix names"""
    if not prefix_str or prefix_str == 'None':
        return []

    prefixes = []
    if 'BI' in prefix_str:
        prefixes.append('BI')
    if 'BY' in prefix_str:
        prefixes.append('BY')
    if 'H' in prefix_str:
        prefixes.append('H')
    if 'W' in prefix_str:
        prefixes.append('W')
    if 'F' in prefix_str:
        prefixes.append('F')
    if 'D' in prefix_str:
        prefixes.append('D')
    if 'R_N' in prefix_str:
        prefixes.append('R_N')

    return prefixes

def parse_addressing_modes(modes_str):
    """Parse addressing modes string to list"""
    if not modes_str or modes_str == 'None':
        return []

    modes = []
    mode_keywords = [
        'LOCAL', 'RECORD', 'CONSTANT', 'REGISTER', 'PRE_INDEXED',
        'POST_INDEXED', 'ABSOLUTE', 'DESCRIPTOR', 'INDIRECT',
        'ALTERNATIVE_DOMAIN'
    ]

    for keyword in mode_keywords:
        if keyword in modes_str:
            modes.append(keyword)

    return modes

def get_manual_reference(mnemonic, manual_mapping):
    """Get manual section reference for instruction"""
    for entry in manual_mapping.get('mappings', []):
        if entry.get('mnemonic', '').lower() == mnemonic.lower():
            return {
                'section': entry.get('section', 'TBD'),
                'title': entry.get('title', 'TBD'),
                'page': entry.get('page'),
                'note': entry.get('note')
            }
    return {'section': 'TBD', 'title': 'TBD', 'page': None, 'note': None}

def generate_variant_table(variants):
    """Generate markdown table of instruction variants"""
    lines = []
    lines.append("| Variant | Opcode | Prefix | Register | Addressing Modes |")
    lines.append("|---------|--------|--------|----------|------------------|")

    # Sort variants by opcode
    sorted_variants = sorted(variants, key=lambda v: v.get('opcode', '0x0000'))

    for i, variant in enumerate(sorted_variants, 1):
        opcode = variant.get('opcode', '0x????')
        prefixes = parse_prefixes(variant.get('prefixes', ''))
        prefix_display = prefixes[0] if prefixes else 'R_N'
        if prefix_display == 'R_N':
            prefix_display = '-'

        # Infer register from opcode or variant number
        reg_num = (i - 1) % 4 + 1 if len(sorted_variants) > 1 else 1

        modes = parse_addressing_modes(variant.get('allowedModes', [''])[0] if variant.get('allowedModes') else '')
        modes_display = ', '.join(modes[:3]) + ('...' if len(modes) > 3 else '') if modes else 'ALL'

        lines.append(f"| {i}/{len(sorted_variants)} | {opcode} | {prefix_display} | {reg_num} | {modes_display} |")

    return '\n'.join(lines)

def generate_markdown(mnemonic, variants, manual_ref):
    """Generate complete markdown documentation for an instruction"""

    # Get representative variant
    repr_variant = variants[0]

    function_name = repr_variant.get('functionName', 'Unknown')
    instr_class = repr_variant.get('class', 'UNKNOWN')
    operand_count = repr_variant.get('operandCount', 0)
    total_variants = len(variants)

    prefixes = parse_prefixes(repr_variant.get('prefixes', ''))
    prefix_str = ','.join(prefixes) if prefixes else 'none'

    # Generate markdown
    md = []

    # Title
    title = f"{mnemonic.upper()}"
    if function_name != 'Unknown':
        title += f" - {function_name}"
    md.append(f"# {title}")
    md.append("")

    # Overview
    md.append("## Overview")
    md.append("")
    md.append(f"**Mnemonic:** `{mnemonic}`")
    md.append(f"**Function:** {function_name}")
    md.append(f"**Class:** {instr_class}")
    md.append(f"**Privilege:** user")
    md.append("")

    # Format - simplified for now
    if operand_count == 0:
        md.append(f"**Format:** `{mnemonic.upper()}`")
    elif operand_count == 1:
        md.append(f"**Format:** `{{prefix}}{{register}} {mnemonic.upper()} <operand>`")
    elif operand_count == 2:
        md.append(f"**Format:** `{{prefix}}{{register}} {mnemonic.upper()} <op1>,<op2>`")
    else:
        md.append(f"**Format:** `{{prefix}}{{register}} {mnemonic.upper()} <operands>`")

    md.append("")
    md.append("---")
    md.append("")

    # Description
    md.append("## Description")
    md.append("")
    md.append(f"[Description for {mnemonic.upper()} instruction to be written based on Reference Manual §{manual_ref['section']}]")
    md.append("")
    md.append(f"**Operands:** {operand_count}")
    md.append(f"**Variants:** {total_variants} opcode(s)")
    md.append("")
    md.append("---")
    md.append("")

    # Variants
    md.append("## Variants")
    md.append("")
    md.append(f"Total variants: {total_variants}")
    md.append("")
    md.append(generate_variant_table(variants))
    md.append("")
    md.append("---")
    md.append("")

    # Operands
    md.append("## Operands")
    md.append("")
    if operand_count == 0:
        md.append("This instruction takes no operands.")
    else:
        for i in range(operand_count):
            md.append(f"### Operand {i+1}")
            md.append("")
            md.append(f"[Description for operand {i+1}]")
            md.append("")
            modes = parse_addressing_modes(repr_variant.get('allowedModes', [''])[i] if len(repr_variant.get('allowedModes', [])) > i else '')
            if modes:
                md.append("**Supported modes:**")
                for mode in modes:
                    md.append(f"- **{mode}**")
                md.append("")

    md.append("---")
    md.append("")

    # Trap Conditions
    md.append("## Trap Conditions")
    md.append("")
    md.append("- **OPERAND_ERROR (Bit 5):** Invalid addressing mode or alignment")
    md.append("")
    md.append("[Additional trap conditions based on instruction type]")
    md.append("")
    md.append("---")
    md.append("")

    # Data Status Bits
    md.append("## Data Status Bits")
    md.append("")
    md.append("- **Z (Zero):** [Effect on zero flag]")
    md.append("- **S (Sign):** [Effect on sign flag]")
    md.append("- **O (Overflow):** [Effect on overflow flag]")
    md.append("- **K (Flag):** [Effect on K flag]")
    md.append("")
    md.append("---")
    md.append("")

    # Examples
    md.append("## Examples")
    md.append("")
    md.append("### Example 1: Basic Usage")
    md.append("")
    md.append("```assembly")
    md.append(f"        ; Example usage of {mnemonic.upper()}")
    md.append(f"        ; [To be written]")
    md.append("```")
    md.append("")
    md.append("---")
    md.append("")

    # Performance Notes
    md.append("## Performance Notes")
    md.append("")
    md.append("- **Typical cycles:** [To be determined]")
    md.append("- **Best case:** [To be determined]")
    md.append("- **Worst case:** [To be determined]")
    md.append("")
    md.append("---")
    md.append("")

    # Reference Manual
    md.append("## Reference Manual")
    md.append("")
    md.append(f"**Section:** §{manual_ref['section']}")
    md.append(f"**Title:** {manual_ref['title']}")
    if manual_ref['page']:
        md.append(f"**Page:** {manual_ref['page']}")
    if manual_ref['note']:
        md.append("")
        md.append(f"**Note:** {manual_ref['note']}")
    md.append("")
    md.append("---")
    md.append("")

    # See Also
    md.append("## See Also")
    md.append("")
    md.append("- [Addressing Modes](../AddressingModes.md)")
    md.append("- [Data Type Prefixes](../Prefixes.md)")
    md.append("- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)")
    md.append("")

    return '\n'.join(md)

def generate_instruction_file(mnemonic, variants, manual_mapping, output_dir, dry_run=False):
    """Generate documentation file for a single instruction"""

    filename = normalize_filename(mnemonic) + '.md'
    filepath = output_dir / filename

    manual_ref = get_manual_reference(mnemonic, manual_mapping)

    markdown = generate_markdown(mnemonic, variants, manual_ref)

    if dry_run:
        print(f"Would generate: {filepath} ({len(variants)} variants)")
        return True

    try:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(markdown)
        print(f"✓ Generated: {filename} ({len(variants)} variants, class={variants[0].get('class')})")
        return True
    except Exception as e:
        print(f"✗ Failed to generate {filename}: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Generate ND-500 instruction documentation')
    parser.add_argument('--batch', type=str, help='Generate only specified class (MOVE, ARITHMETIC, etc.)')
    parser.add_argument('--limit', type=int, help='Generate only first N instructions')
    parser.add_argument('--dry-run', action='store_true', help='Show what would be generated')
    parser.add_argument('--list-classes', action='store_true', help='List all instruction classes')

    args = parser.parse_args()

    # Paths
    base_dir = Path('/home/user/nd500x/docs/instructions')
    instructions_json = base_dir / 'instructions.json'
    manual_mapping_json = base_dir / 'MANUAL_SECTION_MAPPING.json'
    output_dir = base_dir / 'asm'

    # Load data
    print("Loading instructions data...")
    instructions_data = load_json(instructions_json)
    manual_mapping = load_json(manual_mapping_json)

    print(f"Total instruction variants: {len(instructions_data['instructions'])}")

    # Group by mnemonic
    print("Grouping instructions by mnemonic...")
    grouped = group_instructions_by_mnemonic(instructions_data['instructions'])
    print(f"Unique mnemonics: {len(grouped)}")

    # List classes if requested
    if args.list_classes:
        classes = set()
        for variants in grouped.values():
            classes.add(variants[0].get('class', 'UNKNOWN'))
        print("\nInstruction classes:")
        for cls in sorted(classes):
            count = sum(1 for v in grouped.values() if v[0].get('class') == cls)
            print(f"  {cls:20s}: {count:3d} instructions")
        return 0

    # Filter by class if specified
    if args.batch:
        print(f"\nFiltering for class: {args.batch}")
        grouped = {m: v for m, v in grouped.items() if v[0].get('class') == args.batch}
        print(f"Instructions in {args.batch}: {len(grouped)}")

    # Limit if specified
    if args.limit:
        print(f"\nLimiting to first {args.limit} instructions")
        grouped = dict(list(grouped.items())[:args.limit])

    # Generate documentation
    print(f"\n{'DRY RUN: ' if args.dry_run else ''}Generating documentation...")
    print("="*70)

    success_count = 0
    fail_count = 0

    for mnemonic, variants in sorted(grouped.items()):
        if generate_instruction_file(mnemonic, variants, manual_mapping, output_dir, args.dry_run):
            success_count += 1
        else:
            fail_count += 1

    print("="*70)
    print(f"\nGeneration complete:")
    print(f"  Success: {success_count}")
    print(f"  Failed: {fail_count}")
    print(f"  Total: {success_count + fail_count}")

    return 0 if fail_count == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
