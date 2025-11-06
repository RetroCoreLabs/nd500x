#!/usr/bin/env python3
"""
Extract instruction descriptions from the ND-500 Reference Manual
and add them to the corresponding YAML files.
"""

import re
import yaml
from pathlib import Path
from collections import defaultdict

def parse_manual_instructions(manual_path):
    """
    Parse the reference manual and extract all instruction descriptions.
    Returns a dict mapping instruction names/mnemonics to their full descriptions.
    """
    with open(manual_path, 'r', encoding='utf-8') as f:
        content = f.read()

    instructions = {}

    # Find all instruction sections (format: # 10.X Title or ## 10.X Title)
    # They're in chapters 10-17
    pattern = r'^#{1,2} (\d+\.\d+)\s+(.+?)$'

    lines = content.split('\n')
    i = 0

    while i < len(lines):
        line = lines[i]
        match = re.match(pattern, line)

        if match:
            section_num = match.group(1)
            title = match.group(2).strip()

            # Check if it's in chapters 10-17 (instruction chapters)
            chapter = int(section_num.split('.')[0])
            if 10 <= chapter <= 17:
                # Extract the instruction description (everything until the next section or page marker)
                description_lines = [line]
                i += 1

                # Collect lines until we hit another section, page marker, or chapter
                while i < len(lines):
                    curr_line = lines[i]

                    # Stop at next section header
                    if re.match(r'^#{1,2} \d+\.\d+', curr_line):
                        break

                    # Stop at page markers
                    if re.match(r'^## Page \d+', curr_line):
                        break

                    # Stop at chapter headers
                    if re.match(r'^# (CHAPTER|ND-500)', curr_line):
                        break

                    description_lines.append(curr_line)
                    i += 1

                # Join and clean up the description
                full_description = '\n'.join(description_lines).strip()

                # Store by title
                key = title.lower().strip()
                instructions[key] = {
                    'section': section_num,
                    'title': title,
                    'description': full_description
                }

                print(f"Found: {section_num} - {title}")

                continue

        i += 1

    return instructions

def find_matching_manual_entry(yaml_data, manual_instructions):
    """
    Find the matching manual entry for a YAML instruction.
    Returns the manual description or None if not found.
    """
    mnemonic = yaml_data.get('instruction', {}).get('mnemonic', '').lower()
    function_name = yaml_data.get('instruction', {}).get('function', '').lower()
    name = yaml_data.get('instruction', {}).get('name', '').lower()

    # Try various matching strategies
    search_terms = [
        function_name,
        name,
        mnemonic
    ]

    # Also try common variations
    if function_name:
        # Try splitting camelCase
        # E.g., "AssignTo" -> "assign to"
        split_name = re.sub(r'([a-z])([A-Z])', r'\1 \2', function_name).lower()
        search_terms.append(split_name)

    # Map some common names
    name_mappings = {
        'assignto': ['load', 'store', 'assign'],
        'assignfrom': ['store', 'assign from'],
        'neg': ['negate'],
        'inv': ['invert'],
        'abs': ['absolute value'],
        'comp': ['compare'],
        'add': ['add'],
        'sub': ['subtract'],
        'mul': ['multiply'],
        'div': ['divide'],
        'and': ['and'],
        'or': ['or'],
        'xor': ['exclusive or'],
        'move': ['move'],
        'swap': ['swap'],
        'test': ['test against zero', 'test'],
        'clr': ['clear register'],
        'incr': ['increment'],
        'decr': ['decrement'],
        'shl': ['logical shift'],
        'sha': ['arithmetical shift'],
        'ret': ['return'],
        'call': ['call'],
        'go': ['go', 'jump', 'branch'],
    }

    # Add mapped terms
    for key in [function_name, name, mnemonic]:
        if key in name_mappings:
            search_terms.extend(name_mappings[key])

    # Search for matches
    best_match = None
    best_score = 0

    for search_term in search_terms:
        if not search_term:
            continue

        for manual_key, manual_data in manual_instructions.items():
            # Simple contains check
            if search_term in manual_key or manual_key in search_term:
                # Calculate match score (longer matches are better)
                score = len(search_term) if search_term in manual_key else len(manual_key)
                if score > best_score:
                    best_score = score
                    best_match = manual_data

    return best_match

def update_yaml_with_manual(yaml_path, manual_instructions):
    """
    Update a YAML file with the corresponding manual description.
    """
    # Read YAML file
    with open(yaml_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Parse YAML
    yaml_data = yaml.safe_load(content)

    if not yaml_data or 'instruction' not in yaml_data:
        return False

    # Find matching manual entry
    manual_entry = find_matching_manual_entry(yaml_data, manual_instructions)

    if not manual_entry:
        print(f"  ⚠️  No manual entry found for {yaml_path.name}")
        return False

    # Add manual description to YAML
    # We'll add it as a new field called 'manual_reference'
    instruction = yaml_data['instruction']

    # Create manual_reference section
    manual_ref = {
        'section': manual_entry['section'],
        'title': manual_entry['title'],
        'content': manual_entry['description']
    }

    # Add to YAML data
    instruction['manual_reference'] = manual_ref

    # Write back to file with manual description as literal block
    # We need to preserve formatting, so we'll do this manually
    with open(yaml_path, 'a', encoding='utf-8') as f:
        f.write('\n')
        f.write('  manual_reference:\n')
        f.write(f'    section: "{manual_entry["section"]}"\n')
        f.write(f'    title: "{manual_entry["title"]}"\n')
        f.write('    content: |\n')

        # Add manual content with proper indentation
        for line in manual_entry['description'].split('\n'):
            f.write(f'      {line}\n')

    print(f"  ✓ Added manual description for {yaml_path.name}")
    return True

def main():
    """Main function"""
    script_dir = Path(__file__).parent
    manual_path = script_dir / 'docs' / 'ND-05.009.4 EN ND-500 Reference Manual.md'
    yaml_dir = script_dir / 'docs' / 'instructions' / 'yaml'

    print("=" * 70)
    print("Adding ND-500 Reference Manual Descriptions to YAML Files")
    print("=" * 70)
    print()

    # Check paths
    if not manual_path.exists():
        print(f"Error: Manual not found at {manual_path}")
        return 1

    if not yaml_dir.exists():
        print(f"Error: YAML directory not found at {yaml_dir}")
        return 1

    # Parse manual
    print("Step 1: Parsing reference manual...")
    print()
    manual_instructions = parse_manual_instructions(manual_path)
    print()
    print(f"Found {len(manual_instructions)} instruction sections in manual")
    print()

    # Get all YAML files
    yaml_files = sorted(yaml_dir.glob('*.yaml'))
    print(f"Step 2: Processing {len(yaml_files)} YAML files...")
    print()

    # Update each YAML file
    success_count = 0
    failed_count = 0

    for yaml_file in yaml_files:
        result = update_yaml_with_manual(yaml_file, manual_instructions)
        if result:
            success_count += 1
        else:
            failed_count += 1

    # Summary
    print()
    print("=" * 70)
    print("Summary:")
    print(f"  ✓ Successfully updated: {success_count} files")
    print(f"  ⚠️  No manual entry found: {failed_count} files")
    print("=" * 70)

    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
