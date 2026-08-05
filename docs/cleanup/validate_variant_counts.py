#!/usr/bin/env python3
"""
Validate instruction variant counts in instructions.json

Performs two types of validation:
1. Internal consistency: Verify all variants of same instruction have matching totalVariants
2. External validation: Cross-check with Reference Manual section data

Reports any mismatches or inconsistencies.
"""

import json
import sys
from collections import defaultdict

def load_json(filepath):
    """Load JSON file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        return json.load(f)

def analyze_variant_counts(instructions):
    """
    Analyze variant counts per mnemonic

    Returns:
        dict: {mnemonic: {
            'entries': [(index, instruction), ...],
            'total_variants_claimed': set of totalVariants values,
            'actual_count': number of entries found,
            'opcodes': list of opcodes
        }}
    """
    mnemonic_data = defaultdict(lambda: {
        'entries': [],
        'total_variants_claimed': set(),
        'actual_count': 0,
        'opcodes': [],
        'function_names': set(),
        'classes': set()
    })

    for i, instr in enumerate(instructions):
        mnemonic = instr.get('mnemonic', '').lower()
        if not mnemonic:
            continue

        data = mnemonic_data[mnemonic]
        data['entries'].append((i, instr))
        data['total_variants_claimed'].add(instr.get('totalVariants', 0))
        data['actual_count'] += 1
        data['opcodes'].append(instr.get('opcode', 'UNKNOWN'))
        data['function_names'].add(instr.get('functionName', 'UNKNOWN'))
        data['classes'].add(instr.get('class', 'UNKNOWN'))

    return dict(mnemonic_data)

def validate_internal_consistency(mnemonic_data):
    """
    Check internal consistency within instructions.json

    Returns:
        list: [(mnemonic, issue_description), ...]
    """
    issues = []

    for mnemonic, data in sorted(mnemonic_data.items()):
        actual = data['actual_count']
        claimed = data['total_variants_claimed']

        # Check 1: Multiple different totalVariants values
        if len(claimed) > 1:
            issues.append((
                mnemonic,
                f"INCONSISTENT totalVariants: entries claim {claimed}, but should all be same"
            ))

        # Check 2: totalVariants doesn't match actual count
        if len(claimed) == 1:
            claimed_value = list(claimed)[0]
            if claimed_value != actual:
                issues.append((
                    mnemonic,
                    f"MISMATCH: totalVariants={claimed_value}, but found {actual} entries"
                ))

    return issues

def load_manual_mapping():
    """Load MANUAL_SECTION_MAPPING.json if available"""
    try:
        mapping_path = '/home/user/nd500x/docs/instructions/MANUAL_SECTION_MAPPING.json'
        data = load_json(mapping_path)

        # Build lookup: mnemonic -> expected_variants
        manual_data = {}
        for entry in data.get('mappings', []):
            mnemonic = entry.get('mnemonic', '').lower()
            expected = entry.get('expected_variants')
            if mnemonic and expected:
                manual_data[mnemonic] = expected

        return manual_data
    except FileNotFoundError:
        return {}

def validate_against_manual(mnemonic_data, manual_mapping):
    """
    Cross-check variant counts against Reference Manual data

    Returns:
        list: [(mnemonic, issue_description), ...]
    """
    issues = []

    for mnemonic, expected in sorted(manual_mapping.items()):
        if mnemonic not in mnemonic_data:
            issues.append((
                mnemonic,
                f"MISSING: Reference Manual lists {expected} variants, but not found in JSON"
            ))
            continue

        actual = mnemonic_data[mnemonic]['actual_count']
        if actual != expected:
            issues.append((
                mnemonic,
                f"MANUAL MISMATCH: Reference Manual expects {expected} variants, JSON has {actual}"
            ))

    return issues

def generate_summary_report(mnemonic_data):
    """Generate summary statistics"""
    total_mnemonics = len(mnemonic_data)
    total_variants = sum(d['actual_count'] for d in mnemonic_data.values())

    # Count by class
    class_counts = defaultdict(int)
    for data in mnemonic_data.values():
        for cls in data['classes']:
            class_counts[cls] += 1

    print("\n" + "="*70)
    print("SUMMARY STATISTICS")
    print("="*70)
    print(f"Total unique mnemonics: {total_mnemonics}")
    print(f"Total instruction variants: {total_variants}")
    print("\nInstructions by class:")
    for cls, count in sorted(class_counts.items()):
        print(f"  {cls:20s}: {count:3d} unique mnemonics")
    print("="*70)

def main():
    print("ND-500 Instruction Variant Count Validation")
    print("="*70)

    # Load instructions.json
    instructions_path = '/home/user/nd500x/docs/instructions/instructions.json'
    print(f"\nLoading {instructions_path}...")
    data = load_json(instructions_path)
    instructions = data.get('instructions', [])
    print(f"Loaded {len(instructions)} instruction entries")

    # Analyze variant counts
    print("\nAnalyzing variant counts per mnemonic...")
    mnemonic_data = analyze_variant_counts(instructions)
    print(f"Found {len(mnemonic_data)} unique mnemonics")

    # Validate internal consistency
    print("\n" + "="*70)
    print("INTERNAL CONSISTENCY CHECK")
    print("="*70)
    internal_issues = validate_internal_consistency(mnemonic_data)

    if internal_issues:
        print(f"\n❌ Found {len(internal_issues)} internal consistency issues:\n")
        for mnemonic, issue in internal_issues:
            print(f"  {mnemonic:15s}: {issue}")
    else:
        print("\n✅ All instructions have consistent totalVariants values")

    # Load manual mapping and validate
    print("\n" + "="*70)
    print("REFERENCE MANUAL CROSS-CHECK")
    print("="*70)
    manual_mapping = load_manual_mapping()

    if manual_mapping:
        print(f"\nLoaded {len(manual_mapping)} manual variant counts")
        manual_issues = validate_against_manual(mnemonic_data, manual_mapping)

        if manual_issues:
            print(f"\n❌ Found {len(manual_issues)} manual cross-check issues:\n")
            for mnemonic, issue in manual_issues:
                print(f"  {mnemonic:15s}: {issue}")
        else:
            print("\n✅ All mapped instructions match Reference Manual variant counts")
    else:
        print("\n⚠️  MANUAL_SECTION_MAPPING.json not found - skipping manual validation")

    # Generate summary report
    generate_summary_report(mnemonic_data)

    # Overall status
    total_issues = len(internal_issues) + (len(manual_issues) if manual_mapping else 0)

    print("\n" + "="*70)
    if total_issues == 0:
        print("✅ VALIDATION PASSED: No issues found")
        return 0
    else:
        print(f"❌ VALIDATION FAILED: {total_issues} issue(s) found")
        return 1

if __name__ == '__main__':
    sys.exit(main())
