#!/usr/bin/env python3
"""
Quality Metrics for ND-500 Instruction Documentation

Scores documentation on three dimensions:
1. Completeness: Are all required sections present?
2. Accuracy: Does content match source data?
3. Usability: Is formatting correct, examples clear?

Target: All scores ≥80%
"""

import json
import re
from pathlib import Path

# Required sections for each instruction doc
REQUIRED_SECTIONS = [
    "# ",           # Title (instruction name)
    "## Overview",
    "## Description",
    "## Variants",
    "## Operands",
    "## Trap Conditions",
    "## Data Status Bits",
    "## Examples",
    "## Performance Notes",
    "## Reference Manual",
    "## See Also"
]

# Required fields in Overview section
REQUIRED_OVERVIEW_FIELDS = [
    "**Mnemonic:**",
    "**Function:**",
    "**Class:**",
    "**Privilege:**",
    "**Format:**"
]


def load_json(filepath):
    """Load JSON file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        return json.load(f)


def load_markdown(filepath):
    """Load markdown file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        return f.read()


def score_completeness(markdown_content):
    """
    Score completeness: Are all required sections present?

    Returns: (score 0-100, details)
    """
    total_required = len(REQUIRED_SECTIONS) + len(REQUIRED_OVERVIEW_FIELDS)
    found = 0
    missing = []

    # Check required sections
    for section in REQUIRED_SECTIONS:
        if section in markdown_content:
            found += 1
        else:
            missing.append(section.strip())

    # Check required overview fields
    for field in REQUIRED_OVERVIEW_FIELDS:
        if field in markdown_content:
            found += 1
        else:
            missing.append(field.strip())

    score = int((found / total_required) * 100)

    details = {
        'total_required': total_required,
        'found': found,
        'missing': missing,
        'score': score
    }

    return score, details


def score_accuracy(markdown_content, instructions_data, mnemonic):
    """
    Score accuracy: Does content match source data?

    Checks:
    - Mnemonic matches
    - Class matches
    - Variant count matches
    - Opcodes present

    Returns: (score 0-100, details)
    """
    checks_passed = 0
    total_checks = 4
    issues = []

    # Find instruction in JSON
    instruction_entries = [i for i in instructions_data['instructions']
                           if i.get('mnemonic', '').lower() == mnemonic.lower()]

    if not instruction_entries:
        return 0, {'error': f"Instruction '{mnemonic}' not found in JSON", 'score': 0}

    # Check 1: Mnemonic present in markdown
    if f"**Mnemonic:** {mnemonic}" in markdown_content or f"**Mnemonic:** `{mnemonic}`" in markdown_content:
        checks_passed += 1
    else:
        issues.append("Mnemonic not found in overview")

    # Check 2: Class matches
    instruction_class = instruction_entries[0].get('class', 'UNKNOWN')
    if f"**Class:** {instruction_class}" in markdown_content:
        checks_passed += 1
    else:
        issues.append(f"Class '{instruction_class}' not found")

    # Check 3: Variant count matches
    actual_variant_count = len(instruction_entries)
    variant_pattern = re.search(r'Total variants:\s*(\d+)', markdown_content)
    if variant_pattern:
        documented_count = int(variant_pattern.group(1))
        if documented_count == actual_variant_count:
            checks_passed += 1
        else:
            issues.append(f"Variant count mismatch: docs={documented_count}, actual={actual_variant_count}")
    else:
        issues.append("Variant count not documented")

    # Check 4: At least one opcode present
    opcode_pattern = re.search(r'0x[0-9A-F]{4}', markdown_content)
    if opcode_pattern:
        checks_passed += 1
    else:
        issues.append("No opcodes found in documentation")

    score = int((checks_passed / total_checks) * 100)

    details = {
        'checks_passed': checks_passed,
        'total_checks': total_checks,
        'issues': issues,
        'score': score
    }

    return score, details


def score_usability(markdown_content):
    """
    Score usability: Is formatting correct, examples clear?

    Checks:
    - Hex notation uses 0x prefix
    - Code examples present
    - Cross-references present
    - No broken formatting

    Returns: (score 0-100, details)
    """
    checks_passed = 0
    total_checks = 5
    issues = []

    # Check 1: Hex notation uses 0x (not H suffix)
    hex_h_pattern = re.findall(r'\b[0-9A-F]{4}H\b', markdown_content)
    if not hex_h_pattern:
        checks_passed += 1
    else:
        issues.append(f"Found {len(hex_h_pattern)} instances of H-suffix hex notation (should use 0x)")

    # Check 2: Code examples present (at least one assembly block)
    if '```assembly' in markdown_content or '```asm' in markdown_content:
        checks_passed += 1
    else:
        issues.append("No assembly code examples found")

    # Check 3: Cross-references present (See Also section has links)
    see_also_match = re.search(r'## See Also(.+?)(?=##|$)', markdown_content, re.DOTALL)
    if see_also_match:
        see_also_content = see_also_match.group(1)
        if '[' in see_also_content and '](' in see_also_content:
            checks_passed += 1
        else:
            issues.append("See Also section has no markdown links")
    else:
        issues.append("See Also section not found")

    # Check 4: No obvious formatting errors (unmatched code blocks)
    code_block_count = markdown_content.count('```')
    if code_block_count % 2 == 0:
        checks_passed += 1
    else:
        issues.append(f"Unmatched code blocks (found {code_block_count} ``` markers)")

    # Check 5: Reference Manual section present and formatted
    ref_pattern = re.search(r'\*\*Section:\*\*\s+[0-9]+\.[0-9]+', markdown_content)
    if ref_pattern:
        checks_passed += 1
    else:
        issues.append("Reference Manual section number not properly formatted")

    score = int((checks_passed / total_checks) * 100)

    details = {
        'checks_passed': checks_passed,
        'total_checks': total_checks,
        'issues': issues,
        'score': score
    }

    return score, details


def evaluate_document(markdown_path, instructions_data, mnemonic):
    """
    Evaluate a single documentation file

    Returns: dict with scores and overall quality
    """
    content = load_markdown(markdown_path)

    completeness_score, completeness_details = score_completeness(content)
    accuracy_score, accuracy_details = score_accuracy(content, instructions_data, mnemonic)
    usability_score, usability_details = score_usability(content)

    overall_score = int((completeness_score + accuracy_score + usability_score) / 3)

    return {
        'file': markdown_path.name,
        'mnemonic': mnemonic,
        'overall_score': overall_score,
        'completeness': {
            'score': completeness_score,
            'details': completeness_details
        },
        'accuracy': {
            'score': accuracy_score,
            'details': accuracy_details
        },
        'usability': {
            'score': usability_score,
            'details': usability_details
        },
        'passes_threshold': overall_score >= 80
    }


def evaluate_directory(docs_dir, instructions_json_path):
    """
    Evaluate all documentation files in a directory

    Returns: dict with aggregate metrics
    """
    print(f"Loading instructions data from {instructions_json_path}...")
    instructions_data = load_json(instructions_json_path)

    print(f"Scanning {docs_dir} for markdown files...")
    md_files = sorted(Path(docs_dir).glob('*.md'))

    if not md_files:
        print(f"⚠️  No markdown files found in {docs_dir}")
        return None

    print(f"Found {len(md_files)} markdown files")
    print("\nEvaluating documentation quality...")
    print("="*70)

    results = []
    total_completeness = 0
    total_accuracy = 0
    total_usability = 0
    total_overall = 0

    for md_file in md_files:
        # Extract mnemonic from filename (e.g., "assignto.md" -> ":=")
        # For now, use filename without extension as mnemonic
        # (In practice, we'd need a mapping for special chars)
        mnemonic = md_file.stem

        result = evaluate_document(md_file, instructions_data, mnemonic)
        results.append(result)

        total_completeness += result['completeness']['score']
        total_accuracy += result['accuracy']['score']
        total_usability += result['usability']['score']
        total_overall += result['overall_score']

    # Calculate averages
    count = len(results)
    avg_completeness = int(total_completeness / count)
    avg_accuracy = int(total_accuracy / count)
    avg_usability = int(total_usability / count)
    avg_overall = int(total_overall / count)

    passing = sum(1 for r in results if r['passes_threshold'])
    pass_rate = int((passing / count) * 100)

    print(f"\n{'File':<30} {'Completeness':>12} {'Accuracy':>10} {'Usability':>10} {'Overall':>10} {'Pass':>6}")
    print("="*90)

    for result in results[:10]:  # Show first 10
        pass_mark = "✅" if result['passes_threshold'] else "❌"
        print(f"{result['file']:<30} "
              f"{result['completeness']['score']:>11}% "
              f"{result['accuracy']['score']:>9}% "
              f"{result['usability']['score']:>9}% "
              f"{result['overall_score']:>9}% "
              f"{pass_mark:>6}")

    if len(results) > 10:
        print(f"... and {len(results) - 10} more files")

    print("="*90)
    print(f"\n{'AVERAGE SCORES':<30} {avg_completeness:>11}% {avg_accuracy:>9}% {avg_usability:>9}% {avg_overall:>9}% {pass_rate:>5}%")
    print("="*90)

    return {
        'total_files': count,
        'passing_files': passing,
        'pass_rate': pass_rate,
        'average_scores': {
            'completeness': avg_completeness,
            'accuracy': avg_accuracy,
            'usability': avg_usability,
            'overall': avg_overall
        },
        'results': results
    }


def main():
    print("="*70)
    print("ND-500 Documentation Quality Metrics")
    print("="*70)

    # Paths
    docs_dir = Path('/home/user/nd500x/docs/instructions/asm')
    json_path = Path('/home/user/nd500x/docs/instructions/instructions.json')

    if not docs_dir.exists():
        print(f"\n⚠️  Documentation directory not found: {docs_dir}")
        print("This tool is designed to evaluate generated documentation.")
        print("Run after Phase 3 generation completes.\n")
        print("For now, returning example quality thresholds:")
        print("\nQuality Thresholds:")
        print("  - Completeness: ≥80% (all required sections present)")
        print("  - Accuracy: ≥80% (data matches JSON source)")
        print("  - Usability: ≥80% (proper formatting, examples, cross-refs)")
        print("  - Overall: ≥80% (average of above three scores)")
        print("\n✅ Quality metrics framework ready for Phase 3")
        return 0

    # Evaluate all documentation
    metrics = evaluate_directory(docs_dir, json_path)

    if not metrics:
        return 1

    # Check if quality targets met
    print("\nQuality Target: ≥80% for all metrics")

    if (metrics['average_scores']['completeness'] >= 80 and
        metrics['average_scores']['accuracy'] >= 80 and
        metrics['average_scores']['usability'] >= 80):
        print("✅ Quality targets MET")
        return 0
    else:
        print("❌ Quality targets NOT MET")
        return 1


if __name__ == '__main__':
    import sys
    sys.exit(main())
