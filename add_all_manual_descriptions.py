#!/usr/bin/env python3
"""
Automatically find and add manual descriptions for ALL 241 instruction YAML files.
Searches the reference manual by instruction name/mnemonic.
"""

import re
import yaml
from pathlib import Path

MANUAL_PATH = "docs/ND-05.009.4 EN ND-500 Reference Manual.md"
YAML_DIR = Path("docs/instructions/yaml")

def extract_all_manual_sections(manual_path):
    """
    Extract ALL sections from the manual with their content.
    Returns dict: {section_number: (title, content)}
    """
    with open(manual_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    sections = {}
    i = 0

    while i < len(lines):
        line = lines[i].strip()

        # Match section headers: # 10.12 Negate, ## 10.21 And, ### 10.13 Invert, #### 15.1 Block Move
        match = re.match(r'^#{1,4}\s+(\d+\.\d+(?:\.\d+)?)\s+(.+)$', line)

        if match:
            section_num = match.group(1)
            title = match.group(2).strip()

            # Collect section content
            section_lines = [lines[i].rstrip()]
            i += 1

            while i < len(lines):
                curr = lines[i].rstrip()

                # Stop at next section
                if re.match(r'^#{1,4}\s+\d+\.\d+', curr):
                    break

                # Stop at page marker
                if re.match(r'^## Page \d+', curr):
                    break

                # Stop at major chapter
                if re.match(r'^# (CHAPTER|ND-500|PART |Appendix)', curr):
                    break

                section_lines.append(curr)
                i += 1

            content = '\n'.join(section_lines).strip()

            # Keep the longest content for each section (avoid TOC entries)
            if section_num not in sections or len(content) > len(sections[section_num][1]):
                sections[section_num] = (title, content)
        else:
            i += 1

    return sections

def search_manual_for_instruction(sections, instruction_name, mnemonic):
    """
    Search manual sections for an instruction by name or mnemonic.
    Returns (section_num, title, content) or None
    """
    # Normalize the instruction name
    name_lower = instruction_name.lower()
    mnemonic_lower = mnemonic.lower() if mnemonic else ""

    best_match = None
    best_score = 0

    for section_num, (title, content) in sections.items():
        title_lower = title.lower()
        content_lower = content.lower()

        score = 0

        # Exact title match (highest priority)
        if name_lower == title_lower or mnemonic_lower == title_lower:
            score = 1000
        # Title contains name
        elif name_lower in title_lower or mnemonic_lower in title_lower:
            score = 100
        # Content mentions the instruction prominently
        elif f"{mnemonic_lower} " in content_lower[:500]:
            score = 50
        # Content mentions it at all
        elif mnemonic_lower in content_lower:
            score = 10

        # Prefer longer, more detailed content
        if score > 0:
            score += len(content) // 100

        if score > best_score:
            best_score = score
            best_match = (section_num, title, content)

    return best_match if best_score >= 10 else None

def yaml_has_manual_reference(yaml_path):
    """Check if YAML already has manual_reference"""
    content = yaml_path.read_text()
    return 'manual_reference:' in content

def add_manual_to_yaml(yaml_path, section, title, content):
    """Add manual reference to YAML file"""
    if yaml_has_manual_reference(yaml_path):
        return False  # Skip if already has it

    manual_yaml = f"""
  manual_reference:
    section: "{section}"
    title: "{title}"
    content: |
"""

    # Indent content by 6 spaces
    indented_content = '\n'.join('      ' + line if line.strip() else ''
                                  for line in content.split('\n'))
    manual_yaml += indented_content

    # Append to file
    with open(yaml_path, 'a', encoding='utf-8') as f:
        f.write('\n' + manual_yaml)

    return True

def get_instruction_info(yaml_path):
    """Extract instruction name and mnemonic from YAML"""
    try:
        with open(yaml_path, 'r', encoding='utf-8') as f:
            data = yaml.safe_load(f)

        name = data.get('instruction', {}).get('name', '')
        mnemonic = data.get('instruction', {}).get('mnemonic', '')
        function = data.get('instruction', {}).get('function', '')

        return name, mnemonic, function
    except:
        # Fallback: use filename
        return yaml_path.stem, yaml_path.stem, yaml_path.stem

def main():
    print("=" * 70)
    print("PROCESSING ALL INSTRUCTION YAML FILES")
    print("=" * 70)
    print()

    # Load all manual sections
    print(f"Loading reference manual: {MANUAL_PATH}")
    sections = extract_all_manual_sections(MANUAL_PATH)
    print(f"Found {len(sections)} sections in manual")
    print()

    # Process each YAML file
    yaml_files = sorted(YAML_DIR.glob("*.yaml"))
    total = len(yaml_files)

    updated = 0
    skipped = 0
    not_found = 0

    not_found_list = []

    for i, yaml_path in enumerate(yaml_files, 1):
        name, mnemonic, function = get_instruction_info(yaml_path)

        # Skip if already has manual
        if yaml_has_manual_reference(yaml_path):
            skipped += 1
            continue

        # Search manual for this instruction
        match = search_manual_for_instruction(sections, name, mnemonic)

        if match:
            section_num, title, content = match
            if add_manual_to_yaml(yaml_path, section_num, title, content):
                print(f"[{i}/{total}] ✓ {yaml_path.name:30s} -> Section {section_num}: {title}")
                updated += 1
        else:
            print(f"[{i}/{total}] ✗ {yaml_path.name:30s} -> NOT FOUND")
            not_found += 1
            not_found_list.append(yaml_path.name)

    print()
    print("=" * 70)
    print(f"COMPLETE:")
    print(f"  Updated: {updated}")
    print(f"  Skipped (already have manual): {skipped}")
    print(f"  Not found: {not_found}")
    print("=" * 70)

    if not_found_list:
        print()
        print("Instructions NOT FOUND in manual:")
        for name in not_found_list[:20]:
            print(f"  - {name}")
        if len(not_found_list) > 20:
            print(f"  ... and {len(not_found_list) - 20} more")

if __name__ == '__main__':
    main()
