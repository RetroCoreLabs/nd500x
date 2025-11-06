#!/usr/bin/env python3
"""
Validate all instruction YAML files against the JSON Schema.
"""

import os
import json
import yaml
from pathlib import Path
from typing import List, Tuple

try:
    import jsonschema
    from jsonschema import Draft7Validator, ValidationError
except ImportError:
    print("Error: jsonschema library not installed")
    print("Install with: pip3 install jsonschema")
    exit(1)


def load_schema(schema_path: Path) -> dict:
    """Load JSON Schema from file."""
    with open(schema_path, 'r', encoding='utf-8') as f:
        return json.load(f)


def load_yaml_file(yaml_path: Path) -> dict:
    """Load YAML file."""
    with open(yaml_path, 'r', encoding='utf-8') as f:
        return yaml.safe_load(f)


def validate_yaml_against_schema(yaml_data: dict, schema: dict, validator: Draft7Validator) -> Tuple[bool, List[str]]:
    """Validate YAML data against schema. Returns (is_valid, errors)."""

    errors = []

    try:
        validator.validate(yaml_data)
        return True, []
    except ValidationError as e:
        # Collect all validation errors
        for error in validator.iter_errors(yaml_data):
            error_path = '.'.join(str(p) for p in error.path)
            if error_path:
                errors.append(f"  {error_path}: {error.message}")
            else:
                errors.append(f"  {error.message}")

        return False, errors


def validate_file(yaml_path: Path, schema: dict, validator: Draft7Validator) -> Tuple[bool, List[str]]:
    """Validate a single YAML file."""

    try:
        yaml_data = load_yaml_file(yaml_path)
    except Exception as e:
        return False, [f"Failed to load YAML: {e}"]

    return validate_yaml_against_schema(yaml_data, schema, validator)


def check_yaml_structure(yaml_path: Path) -> List[str]:
    """Additional structural checks beyond schema validation."""

    warnings = []

    try:
        yaml_data = load_yaml_file(yaml_path)
    except Exception as e:
        return [f"Failed to load YAML: {e}"]

    instruction = yaml_data.get('instruction', {})

    # Check operand count consistency
    operand_count = instruction.get('operand_count', 0)
    variants = instruction.get('variants', [])

    for i, variant in enumerate(variants):
        variant_num = variant.get('variant', f'{i+1}/?')
        operands = variant.get('operands', [])

        # If operand_count > 0, variants should have operands
        if operand_count > 0 and not operands:
            warnings.append(f"  Variant {variant_num}: operand_count={operand_count} but no operands defined")

        # Check operand numbering
        if operands:
            for j, operand in enumerate(operands):
                expected_num = j + 1
                actual_num = operand.get('operand', -1)
                if actual_num != expected_num:
                    warnings.append(f"  Variant {variant_num}, operand {j}: expected operand={expected_num}, got {actual_num}")

    # Check variant numbering
    for i, variant in enumerate(variants):
        variant_str = variant.get('variant', '')
        match = re.match(r'^(\d+)/(\d+)$', variant_str)
        if match:
            current = int(match.group(1))
            total = int(match.group(2))

            expected_current = i + 1
            expected_total = len(variants)

            if current != expected_current:
                warnings.append(f"  Variant {variant_str}: expected variant number {expected_current}, got {current}")

            if total != expected_total:
                warnings.append(f"  Variant {variant_str}: expected total {expected_total}, got {total}")

    # Check if examples exist
    if 'examples' not in instruction:
        warnings.append("  No examples section found")

    # Check manual_reference
    if 'manual_reference' not in instruction:
        warnings.append("  No manual_reference section found")

    return warnings


def main():
    """Validate all YAML files."""

    # Paths
    schema_path = Path('/home/user/nd500x/docs/instructions/instruction_schema.json')
    yaml_dir = Path('/home/user/nd500x/docs/instructions/yaml')

    if not schema_path.exists():
        print(f"Error: Schema not found: {schema_path}")
        return 1

    if not yaml_dir.exists():
        print(f"Error: YAML directory not found: {yaml_dir}")
        return 1

    # Load schema
    print("Loading JSON Schema...")
    try:
        schema = load_schema(schema_path)
        validator = Draft7Validator(schema)
    except Exception as e:
        print(f"Error loading schema: {e}")
        return 1

    print(f"✓ Schema loaded: {schema_path.name}")
    print()

    # Find all YAML files
    yaml_files = sorted(yaml_dir.glob('*.yaml'))

    if not yaml_files:
        print(f"No YAML files found in {yaml_dir}")
        return 1

    print(f"Found {len(yaml_files)} YAML files to validate")
    print("=" * 70)
    print()

    # Validate each file
    valid_count = 0
    invalid_count = 0
    warning_count = 0

    for yaml_file in yaml_files:
        is_valid, errors = validate_file(yaml_file, schema, validator)

        if is_valid:
            # Check for warnings
            warnings = check_yaml_structure(yaml_file)

            if warnings:
                print(f"⚠ {yaml_file.name} - Valid with warnings:")
                for warning in warnings:
                    print(warning)
                print()
                warning_count += 1
            else:
                print(f"✓ {yaml_file.name}")
                valid_count += 1
        else:
            print(f"✗ {yaml_file.name} - VALIDATION FAILED:")
            for error in errors:
                print(error)
            print()
            invalid_count += 1

    # Summary
    print()
    print("=" * 70)
    print("VALIDATION SUMMARY")
    print("=" * 70)
    print(f"Total files:       {len(yaml_files)}")
    print(f"Valid:             {valid_count} ✓")
    print(f"Valid w/ warnings: {warning_count} ⚠")
    print(f"Invalid:           {invalid_count} ✗")
    print("=" * 70)

    if invalid_count > 0:
        print()
        print(f"⚠ {invalid_count} files failed validation!")
        return 1
    elif warning_count > 0:
        print()
        print(f"⚠ {warning_count} files have warnings")
        return 0
    else:
        print()
        print("✓ All files valid!")
        return 0


if __name__ == '__main__':
    import re
    exit(main())
