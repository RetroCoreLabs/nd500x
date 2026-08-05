#!/usr/bin/env python3
"""
Validate ND-500 instruction YAML and JSON files against schemas

Validates:
1. All YAML files in yaml/ directory against instruction_schema.json
2. instructions.json structure and required fields
"""

import json
import sys
import os
from pathlib import Path

try:
    import jsonschema
    JSONSCHEMA_AVAILABLE = True
except ImportError:
    print("⚠️  WARNING: jsonschema module not available")
    print("Install with: pip3 install jsonschema")
    JSONSCHEMA_AVAILABLE = False

try:
    import yaml
    YAML_AVAILABLE = True
except ImportError:
    print("⚠️  WARNING: pyyaml module not available")
    print("Install with: pip3 install pyyaml")
    YAML_AVAILABLE = False


def load_json(filepath):
    """Load JSON file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        return json.load(f)


def load_yaml(filepath):
    """Load YAML file"""
    if not YAML_AVAILABLE:
        return None
    with open(filepath, 'r', encoding='utf-8') as f:
        return yaml.safe_load(f)


def validate_yaml_files(schema_path, yaml_dir):
    """
    Validate all YAML files against schema

    Returns:
        tuple: (total_files, valid_files, errors)
    """
    if not JSONSCHEMA_AVAILABLE or not YAML_AVAILABLE:
        print("\n⚠️  Skipping YAML validation (missing dependencies)")
        return 0, 0, []

    print(f"\nValidating YAML files in {yaml_dir}...")
    print(f"Using schema: {schema_path}")

    # Load schema
    try:
        schema = load_json(schema_path)
    except Exception as e:
        print(f"❌ ERROR: Cannot load schema: {e}")
        return 0, 0, [f"Schema load error: {e}"]

    # Find all YAML files
    yaml_files = sorted(Path(yaml_dir).glob('*.yaml'))
    total = len(yaml_files)

    if total == 0:
        print(f"⚠️  No YAML files found in {yaml_dir}")
        return 0, 0, []

    print(f"Found {total} YAML files")

    valid = 0
    errors = []

    for yaml_file in yaml_files:
        try:
            # Load YAML
            data = load_yaml(yaml_file)

            # Validate against schema
            jsonschema.validate(instance=data, schema=schema)
            valid += 1

        except jsonschema.ValidationError as e:
            error_msg = f"{yaml_file.name}: {e.message} at {'.'.join(str(p) for p in e.path)}"
            errors.append(error_msg)

        except Exception as e:
            error_msg = f"{yaml_file.name}: {type(e).__name__}: {e}"
            errors.append(error_msg)

    return total, valid, errors


def validate_instructions_json(json_path):
    """
    Validate instructions.json structure

    Returns:
        tuple: (is_valid, errors)
    """
    print(f"\nValidating {json_path}...")

    try:
        data = load_json(json_path)
    except Exception as e:
        return False, [f"JSON parse error: {e}"]

    errors = []

    # Check required top-level fields
    required_fields = ['generator', 'architecture', 'totalInstructions', 'instructions']
    for field in required_fields:
        if field not in data:
            errors.append(f"Missing required field: {field}")

    if 'instructions' not in data:
        return False, errors

    instructions = data['instructions']

    # Validate totalInstructions matches actual count
    claimed_total = data.get('totalInstructions', 0)
    actual_total = len(instructions)

    if claimed_total != actual_total:
        errors.append(f"totalInstructions mismatch: claims {claimed_total}, actual {actual_total}")

    # Check instruction entry structure
    required_instruction_fields = ['opcode', 'mnemonic', 'functionName', 'class',
                                    'operandCount', 'variantNumber', 'totalVariants']

    sample_size = min(10, len(instructions))
    print(f"Spot-checking {sample_size} instruction entries for required fields...")

    for i in range(sample_size):
        instr = instructions[i]
        for field in required_instruction_fields:
            if field not in instr:
                errors.append(f"Instruction {i} ({instr.get('mnemonic', 'UNKNOWN')}): missing field '{field}'")

    # Validate opcode format
    print("Checking opcode formats...")
    invalid_opcodes = []
    for instr in instructions[:100]:  # Check first 100
        opcode = instr.get('opcode', '')
        if not (opcode.startswith('0x') and len(opcode) == 6):
            invalid_opcodes.append(f"{instr.get('mnemonic', 'UNKNOWN')}: '{opcode}'")

    if invalid_opcodes:
        errors.append(f"Invalid opcode format (should be 0xXXXX): {len(invalid_opcodes)} found")
        for err in invalid_opcodes[:5]:  # Show first 5
            errors.append(f"  - {err}")

    return len(errors) == 0, errors


def validate_encoding(dirpath):
    """
    Check that all files are UTF-8 encoded

    Returns:
        tuple: (total_files, utf8_files, errors)
    """
    print(f"\nValidating file encodings in {dirpath}...")

    files_to_check = []
    files_to_check.extend(Path(dirpath).glob('*.json'))
    files_to_check.extend(Path(dirpath).glob('*.yaml'))
    files_to_check.extend(Path(dirpath).glob('*.md'))

    total = len(files_to_check)
    utf8_count = 0
    errors = []

    for filepath in files_to_check:
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                f.read()
            utf8_count += 1
        except UnicodeDecodeError as e:
            errors.append(f"{filepath.name}: UTF-8 decode error at byte {e.start}")

    return total, utf8_count, errors


def main():
    print("="*70)
    print("ND-500 Instruction Schema Validation")
    print("="*70)

    base_dir = Path('/home/user/nd500x/docs/instructions')
    schema_path = base_dir / 'instruction_schema.json'
    yaml_dir = base_dir / 'yaml'
    json_path = base_dir / 'instructions.json'

    all_errors = []

    # 1. Validate YAML files
    if yaml_dir.exists():
        total_yaml, valid_yaml, yaml_errors = validate_yaml_files(schema_path, yaml_dir)

        if YAML_AVAILABLE and JSONSCHEMA_AVAILABLE:
            print(f"\nYAML Validation Results:")
            print(f"  Total files: {total_yaml}")
            print(f"  Valid files: {valid_yaml}")
            print(f"  Invalid files: {total_yaml - valid_yaml}")

            if yaml_errors:
                print(f"\n❌ YAML Validation Errors ({len(yaml_errors)}):")
                for err in yaml_errors[:10]:  # Show first 10
                    print(f"  - {err}")
                if len(yaml_errors) > 10:
                    print(f"  ... and {len(yaml_errors) - 10} more")
                all_errors.extend(yaml_errors)
            else:
                print("  ✅ All YAML files valid")
    else:
        print(f"\n⚠️  YAML directory not found: {yaml_dir}")

    # 2. Validate instructions.json
    print("\n" + "="*70)
    is_json_valid, json_errors = validate_instructions_json(json_path)

    if is_json_valid:
        print("✅ instructions.json structure valid")
    else:
        print(f"❌ instructions.json validation failed ({len(json_errors)} errors):")
        for err in json_errors:
            print(f"  - {err}")
        all_errors.extend(json_errors)

    # 3. Validate UTF-8 encoding
    print("\n" + "="*70)
    total_enc, utf8_enc, enc_errors = validate_encoding(base_dir)

    print(f"\nEncoding Validation Results:")
    print(f"  Total files checked: {total_enc}")
    print(f"  UTF-8 compliant: {utf8_enc}")
    print(f"  Encoding errors: {len(enc_errors)}")

    if enc_errors:
        print(f"\n❌ Encoding Errors:")
        for err in enc_errors:
            print(f"  - {err}")
        all_errors.extend(enc_errors)
    else:
        print("  ✅ All files are UTF-8 compliant")

    # Final summary
    print("\n" + "="*70)
    print("VALIDATION SUMMARY")
    print("="*70)

    if all_errors:
        print(f"\n❌ VALIDATION FAILED: {len(all_errors)} total error(s)")
        return 1
    else:
        print("\n✅ VALIDATION PASSED: All checks successful")
        return 0


if __name__ == '__main__':
    sys.exit(main())
