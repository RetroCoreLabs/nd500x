#!/usr/bin/env python3
"""
Generate MON call handler stubs from YAML definitions.

Reads YAML files from the MON calls directory and generates:
1. Individual handler C files in src/libmon/handlers/
2. A registry file (mon_registry.c) that registers all handlers

Usage:
    python3 generate_mon_handlers.py [yaml_dir] [output_dir]

Default:
    yaml_dir = /mnt/e/Dev/Ronny/NDInsight/Developer/MON/calls
    output_dir = src/libmon/handlers
"""

import os
import sys
import glob
import re
from pathlib import Path

# Try to import yaml, fall back to basic parsing if not available
try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False
    print("Warning: PyYAML not installed. Using basic parsing.")


def parse_octal_from_filename(filename):
    """Extract octal MON number from filename like '11B_GetBasicTime.yaml'"""
    basename = os.path.basename(filename)
    match = re.match(r'^(\d+)B_', basename)
    if match:
        octal_str = match.group(1)
        return int(octal_str, 8), octal_str + 'B'
    return None, None


def parse_yaml_file(filepath):
    """Parse a YAML file and return its contents."""
    if HAS_YAML:
        with open(filepath, 'r', encoding='utf-8') as f:
            try:
                return yaml.safe_load(f)
            except yaml.YAMLError as e:
                print(f"Error parsing {filepath}: {e}")
                return None
    else:
        # Basic parsing for critical fields
        data = {}
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        # Extract octal
        match = re.search(r'^octal:\s*(\d+B)', content, re.MULTILINE)
        if match:
            data['octal'] = match.group(1)

        # Extract name
        match = re.search(r'^name:\s*(\S+)', content, re.MULTILINE)
        if match:
            data['name'] = match.group(1)

        # Extract short_names
        match = re.search(r'^short_names:\s*\n((?:\s*-\s*\S+\n)+)', content, re.MULTILINE)
        if match:
            names = re.findall(r'-\s*(\S+)', match.group(1))
            data['short_names'] = names

        # Extract description (first line only)
        match = re.search(r'^description:\s*[|>]?\s*\n?\s*(.+?)(?:\n\s*\n|\nparameters:|\nsee_also:)', content, re.MULTILINE | re.DOTALL)
        if match:
            data['description'] = match.group(1).strip()[:200]
        else:
            match = re.search(r'^description:\s*(.+?)$', content, re.MULTILINE)
            if match:
                data['description'] = match.group(1).strip()[:200]

        # Extract parameters
        data['parameters'] = []
        param_section = re.search(r'^parameters:\s*\n((?:.*\n)*?)(?=^[a-z]+:|\Z)', content, re.MULTILINE)
        if param_section:
            param_text = param_section.group(1)
            params = re.findall(r'-\s*name:\s*(\S+).*?type:\s*(\S+).*?io:\s*(\S+)', param_text, re.DOTALL)
            for p in params:
                data['parameters'].append({'name': p[0], 'type': p[1], 'io': p[2]})

        return data


def escape_c_string(s):
    """Escape a string for C code."""
    if not s:
        return ""
    s = s.replace('\\', '\\\\')
    s = s.replace('"', '\\"')
    s = s.replace('\n', '\\n')
    s = s.replace('\r', '')
    # Remove any non-ASCII characters
    s = ''.join(c if ord(c) < 128 else '?' for c in s)
    return s


def generate_handler_file(mon_number, octal_str, data, output_dir):
    """Generate a single handler C file."""
    name = data.get('name', 'Unknown')
    short_names = data.get('short_names', [])
    short_name = short_names[0] if short_names else name
    description = data.get('description', '')
    parameters = data.get('parameters', [])

    # Generate parameter documentation
    param_doc = ""
    if parameters:
        param_doc = " *\n * Parameters:\n"
        for p in parameters:
            io_str = {'I': 'input', 'O': 'output', 'IO': 'in/out'}.get(p.get('io', ''), p.get('io', ''))
            param_doc += f" *   [{p.get('io', '?')}] {p.get('name', '?')} ({p.get('type', '?')}): {io_str}\n"

    # Clean description for C comment
    desc_clean = escape_c_string(description).replace('\\n', '\n * ')

    content = f'''/*
 * MON {octal_str} ({mon_number} decimal): {name} ({short_name})
 *
 * {desc_clean}
{param_doc} *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_{octal_str}_{name}(MonContext* ctx) {{
    /* TODO: Implement {name} ({short_name}) */

    /* Log input parameters */
'''

    # Add parameter logging
    for i, p in enumerate(parameters):
        if p.get('io') in ['I', 'IO']:
            content += f'    MON_LOG_IN_WORD(ctx, {i}, "{p.get("name", "param" + str(i))}");\n'

    content += '''
    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
'''

    # Write file
    filename = f"mon_{octal_str}_{name}.c"
    filepath = os.path.join(output_dir, filename)

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

    return filename, name, short_name


def generate_registry_file(handlers, output_dir):
    """Generate the mon_registry.c file that registers all handlers."""
    content = '''/*
 * MON Handler Registry
 *
 * AUTO-GENERATED - DO NOT EDIT
 *
 * This file registers all MON call handlers.
 * Run tools/generate_mon_handlers.py to regenerate.
 */

#include "mon.h"

/* Forward declarations for all handlers */
'''

    # Forward declarations
    for h in handlers:
        content += f'extern MonResult mon_{h["octal"]}_{h["name"]}(MonContext* ctx);\n'

    content += '''
/* Register all handlers */
void mon_register_all_handlers(void) {
'''

    # Registration calls
    for h in handlers:
        desc = escape_c_string(h.get('description', '')[:100])
        content += f'''    mon_register(
        {h["number"]},           /* MON number (decimal) */
        "{h["octal"]}",         /* Octal string */
        "{h["short_name"]}",    /* Short name */
        "{h["name"]}",          /* Long name */
        "{desc}",  /* Description */
        mon_{h["octal"]}_{h["name"]},  /* Handler */
        MON_STATUS_NOT_IMPLEMENTED,    /* Status */
        {h["param_count"]}             /* Param count */
    );
'''

    content += '}\n'

    # Write file (one directory up from handlers)
    filepath = os.path.join(os.path.dirname(output_dir), 'mon_registry.c')
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

    return filepath


def main():
    # Default paths
    yaml_dir = '/mnt/e/Dev/Ronny/NDInsight/Developer/MON/calls'
    output_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                              'src', 'libmon', 'handlers')

    # Parse command line args
    if len(sys.argv) > 1:
        yaml_dir = sys.argv[1]
    if len(sys.argv) > 2:
        output_dir = sys.argv[2]

    # Ensure output directory exists
    os.makedirs(output_dir, exist_ok=True)

    print(f"Reading YAML files from: {yaml_dir}")
    print(f"Writing handlers to: {output_dir}")

    # Find all YAML files
    yaml_files = glob.glob(os.path.join(yaml_dir, '*.yaml'))
    print(f"Found {len(yaml_files)} YAML files")

    handlers = []

    for yaml_file in sorted(yaml_files):
        mon_number, octal_str = parse_octal_from_filename(yaml_file)
        if mon_number is None:
            print(f"Skipping {yaml_file} - couldn't parse MON number")
            continue

        data = parse_yaml_file(yaml_file)
        if not data:
            print(f"Skipping {yaml_file} - couldn't parse YAML")
            continue

        # Use octal from file if available
        if 'octal' in data:
            octal_str = data['octal']
            # Remove 'B' suffix if present and parse
            octal_clean = octal_str.rstrip('Bb')
            mon_number = int(octal_clean, 8)

        filename, name, short_name = generate_handler_file(mon_number, octal_str, data, output_dir)

        handlers.append({
            'number': mon_number,
            'octal': octal_str,
            'name': name,
            'short_name': short_name,
            'description': data.get('description', ''),
            'param_count': len(data.get('parameters', []))
        })

        print(f"  Generated {filename}")

    # Generate registry
    registry_file = generate_registry_file(handlers, output_dir)
    print(f"\nGenerated registry: {registry_file}")
    print(f"\nTotal handlers: {len(handlers)}")


if __name__ == '__main__':
    main()
