#!/usr/bin/env python3
"""
Properly add manual descriptions to YAML files WITHOUT duplication.
Checks if manual_reference already exists before adding.
"""

import re
from pathlib import Path

def extract_manual_section(manual_path, section_number, title_hint):
    """
    Extract a specific section from the manual.
    Returns the complete section text.
    Prioritizes single-hash sections (detailed content) over double-hash (TOC).
    """
    with open(manual_path, 'r', encoding='utf-8') as f:
        content = f.read()

    lines = content.split('\n')

    # Try to find sections in order of preference:
    # 1. Single-hash sections (# 10.12 Negate) - most detailed
    # 2. Double-hash sections (## 10.21 And) - also detailed
    # 3. Triple-hash subsections (### 10.13 Invert) - subsections
    # 4. Quad-hash subsections (#### 15.1 Block Move and Fill) - subsections
    pattern_single = rf'^#\s+{re.escape(section_number)}\s+(.+?)$'
    pattern_double = rf'^##\s+{re.escape(section_number)}\s+(.+?)$'
    pattern_triple = rf'^###\s+{re.escape(section_number)}\s+(.+?)$'
    pattern_quad = rf'^####\s+{re.escape(section_number)}\s+(.+?)$'

    # Try all patterns and collect all matches, then return the longest
    best_content = None
    best_length = 0

    for pattern in [pattern_single, pattern_double, pattern_triple, pattern_quad]:
        i = 0
        while i < len(lines):
            line = lines[i]
            match = re.match(pattern, line)

            if match:
                section_lines = [line]
                i += 1

                # Collect until next section/page
                while i < len(lines):
                    curr = lines[i]

                    # Stop at next section header (1-4 hashes)
                    if re.match(r'^#{1,4}\s+\d+\.\d+', curr):
                        break

                    # Stop at page marker
                    if re.match(r'^## Page \d+', curr):
                        break

                    # Stop at chapter header
                    if re.match(r'^# (CHAPTER|ND-500)', curr):
                        break

                    section_lines.append(curr)
                    i += 1

                # Keep the longest match (detailed content vs TOC)
                content = '\n'.join(section_lines).strip()
                if len(content) > best_length:
                    best_content = content
                    best_length = len(content)

            i += 1

    return best_content

def yaml_has_manual_reference(yaml_path):
    """Check if YAML file already has manual_reference section."""
    with open(yaml_path, 'r', encoding='utf-8') as f:
        content = f.read()
    return 'manual_reference:' in content

def add_manual_to_yaml(yaml_path, section, title, manual_text):
    """Add manual reference to YAML file if it doesn't already exist."""

    # Check if already has manual reference
    if yaml_has_manual_reference(yaml_path):
        return False

    # Append manual reference
    with open(yaml_path, 'a', encoding='utf-8') as f:
        f.write('\n')
        f.write('  manual_reference:\n')
        f.write(f'    section: "{section}"\n')
        f.write(f'    title: "{title}"\n')
        f.write('    content: |\n')

        # Add manual content with proper indentation
        for line in manual_text.split('\n'):
            f.write(f'      {line}\n')

    return True

# Manual mappings with section numbers
MANUAL_MAPPINGS = {
    'neg': ('10.12', 'Negate'),
    'abs': ('10.15', 'Absolute value'),
    'inv': ('10.13', 'Invert'),
    'invc': ('10.14', 'Invert with carry add'),
    'clr': ('10.16', 'Clear register'),
    'incr': ('10.19', 'Increment'),
    'decr': ('10.20', 'Decrement'),
    'and': ('10.21', 'And'),
    'or': ('10.22', 'Or'),
    'xor': ('10.23', 'Exclusive or'),
    'shl': ('10.24', 'Logical shift'),
    'sha': ('10.25', 'Arithmetical shift'),
    'shr': ('10.26', 'Rotational shift'),
    'getb': ('10.27', 'Get bit'),
    'putbi': ('10.28', 'Put bit'),
    'clebi': ('10.29', 'Clear bit'),
    'setbi': ('10.30', 'Set bit'),
    'getbf': ('10.31', 'Get bit field'),
    'putbf': ('10.32', 'Put bit field'),
    'set1': ('10.18', 'Set to one'),
    'stz': ('10.17', 'Store zero'),
    'test': ('10.11', 'Test against zero'),
    'comp': ('10.9', 'Compare'),
    'comp2': ('10.10', 'Compare two operands'),
    'move': ('10.7', 'Move'),
    'swap': ('10.8', 'Swap'),
    'assignto': ('10.1', 'Load'),
    'assignfrom': ('10.4', 'Store'),
    'assignbaseregto': ('10.2', 'Load local base register'),
    'assigntobasereg': ('10.5', 'Store local base register'),
    'assignrecordregto': ('10.3', 'Load record register'),
    'assigntorecordreg': ('10.6', 'Store record register'),

    # Chapter 11 - Arithmetic
    'add': ('11.1', 'Add'),
    'subtract': ('11.2', 'Subtract'),
    'multiply': ('11.3', 'Multiply'),
    'divide': ('11.4', 'Divide'),

    # Chapter 12 - Math functions
    'sin': ('12.5', 'Sine'),
    'cos': ('12.7', 'Cosine'),
    'tan': ('12.9', 'Tangent'),
    'exp': ('12.12', 'Exponential'),
    'sqrt': ('12.4', 'Square root'),

    # Chapter 13 - Control
    'go': ('13.2', 'Unconditional absolute jump'),
    'loop': ('13.4', 'Loop with increment'),
    'loopi': ('13.4', 'Loop with increment'),
    'loopd': ('13.4', 'Loop with increment'),
    'call': ('13.8', 'Call subroutine absolute'),
    'ret': ('13.11', 'Subroutine return'),

    # Chapter 14 - Strings
    'smove': ('14.2', 'String Move'),
    'bmove': ('15.1', 'Block Move and Fill'),

    # Chapter 15 - Misc
    'noop': ('15.13', 'No operation'),
    'freeb': ('15.14', 'Free buddy element'),
    'laddr': ('15.4', 'Load address'),
    'bladdr': ('15.5', 'Load address'),
    'rladdr': ('15.6', 'Load address into base register'),
}

def main():
    script_dir = Path(__file__).parent
    yaml_dir = script_dir / 'docs' / 'instructions' / 'yaml'
    manual_path = script_dir / 'docs' / 'ND-05.009.4 EN ND-500 Reference Manual.md'

    print("=" * 70)
    print("Adding Manual Descriptions Properly (No Duplicates)")
    print("=" * 70)
    print()

    updated = 0
    skipped = 0
    failed = 0

    for filename, (section, title) in MANUAL_MAPPINGS.items():
        yaml_file = yaml_dir / f"{filename}.yaml"

        if not yaml_file.exists():
            print(f"⚠️  File not found: {yaml_file.name}")
            failed += 1
            continue

        # Check if already has manual reference
        if yaml_has_manual_reference(yaml_file):
            print(f"⏭️  Skipped (already has manual): {yaml_file.name}")
            skipped += 1
            continue

        # Extract manual section
        manual_text = extract_manual_section(manual_path, section, title)

        if not manual_text:
            print(f"❌ Manual section not found for: {yaml_file.name} (section {section})")
            failed += 1
            continue

        # Add to YAML
        if add_manual_to_yaml(yaml_file, section, title, manual_text):
            print(f"✓ Added manual to: {yaml_file.name}")
            updated += 1
        else:
            print(f"⚠️  Failed to add manual to: {yaml_file.name}")
            failed += 1

    print()
    print("=" * 70)
    print(f"Updated: {updated} files")
    print(f"Skipped: {skipped} files (already have manual)")
    print(f"Failed: {failed} files")
    print("=" * 70)

if __name__ == '__main__':
    main()
