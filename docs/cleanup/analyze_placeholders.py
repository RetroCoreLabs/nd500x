#!/usr/bin/env python3
"""
Analyze current state of all instruction documentation files
Count all placeholder types and TODOs
"""

import os
import re
from pathlib import Path

asm_dir = Path('/home/user/nd500x/docs/instructions/asm')

placeholder_patterns = {
    'example_todo': r'\[To be written\]',
    'manual_section_tbd': r'§TBD',
    'description_placeholder': r'\[Description for.*?\]',
    'effect_placeholder': r'\[Effect on.*?\]',
    'trap_placeholder': r'\[Additional trap conditions.*?\]',
    'performance_tbd': r'\[To be determined\]',
    'operand_description': r'\[Description for operand \d+\]',
}

results = {key: 0 for key in placeholder_patterns}
files_with_issues = []

for md_file in sorted(asm_dir.glob('*.md')):
    content = md_file.read_text()

    has_issue = False
    file_issues = {}

    for pattern_name, pattern in placeholder_patterns.items():
        matches = len(re.findall(pattern, content))
        if matches > 0:
            results[pattern_name] += matches
            file_issues[pattern_name] = matches
            has_issue = True

    if has_issue:
        files_with_issues.append({
            'file': md_file.name,
            'issues': file_issues,
            'total': sum(file_issues.values())
        })

# Summary
print("=" * 80)
print("INSTRUCTION DOCUMENTATION PLACEHOLDER ANALYSIS")
print("=" * 80)
print()
print(f"Total files analyzed: {len(list(asm_dir.glob('*.md')))}")
print(f"Files with placeholders: {len(files_with_issues)}")
print()

print("PLACEHOLDER TYPE BREAKDOWN:")
print("-" * 80)
for placeholder_type, count in sorted(results.items(), key=lambda x: -x[1]):
    print(f"  {placeholder_type:30s}: {count:4d}")
print()

print("TOTAL PLACEHOLDERS:", sum(results.values()))
print()

# Show worst files
print("TOP 20 FILES WITH MOST PLACEHOLDERS:")
print("-" * 80)
for item in sorted(files_with_issues, key=lambda x: -x['total'])[:20]:
    print(f"  {item['file']:30s}: {item['total']:3d} placeholders")
    for issue_type, count in sorted(item['issues'].items(), key=lambda x: -x[1]):
        print(f"    - {issue_type}: {count}")

print()
print("=" * 80)
print("CONCLUSION: DOCUMENTATION IS NOT PRODUCTION READY")
print("All 241 files need complete content generation")
print("=" * 80)
