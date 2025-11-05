#!/usr/bin/env python3
"""
Add assembly code examples to all instruction YAML files.
This script generates context-aware examples based on:
- Instruction category and operation
- Number of operands
- Supported addressing modes
- Data type prefixes
"""

import os
import re
import yaml
from pathlib import Path

# Instruction categories and their typical use cases
CATEGORY_EXAMPLES = {
    'ARITHMETIC': {
        'description': 'Arithmetic operations',
        'contexts': ['calculations', 'counters', 'indexing', 'math']
    },
    'LOGICAL': {
        'description': 'Logical operations',
        'contexts': ['bit manipulation', 'masking', 'flags', 'bitwise operations']
    },
    'MOVE': {
        'description': 'Data movement',
        'contexts': ['loading', 'storing', 'copying', 'transfer']
    },
    'COMPARE': {
        'description': 'Comparison operations',
        'contexts': ['testing', 'conditionals', 'validation', 'bounds checking']
    },
    'SHIFT': {
        'description': 'Shift and rotate operations',
        'contexts': ['multiplication/division by 2', 'bit alignment', 'packing/unpacking']
    },
    'BRANCH': {
        'description': 'Control flow',
        'contexts': ['conditionals', 'loops', 'jumps']
    },
    'CALL': {
        'description': 'Function calls',
        'contexts': ['subroutines', 'procedures', 'function invocation']
    },
    'SYSTEM': {
        'description': 'System operations',
        'contexts': ['privileged operations', 'system control', 'I/O']
    },
    'FLOAT_MATH': {
        'description': 'Floating-point operations',
        'contexts': ['scientific calculations', 'decimals', 'real numbers']
    }
}

# Addressing mode examples
ADDRESSING_MODE_EXAMPLES = {
    'CONSTANT': {
        'syntax': '<value>',
        'example': '42',
        'description': 'Immediate constant value'
    },
    'REGISTER': {
        'syntax': 'Rn',
        'example': 'W2',
        'description': 'Register contents'
    },
    'LOCAL': {
        'syntax': 'B.<displacement>',
        'example': 'B.16',
        'description': 'Stack-relative addressing (local variable)'
    },
    'RECORD': {
        'syntax': 'R.<displacement>',
        'example': 'R.8',
        'description': 'Structure field addressing'
    },
    'ABSOLUTE': {
        'syntax': '<address>',
        'example': '@GLOBAL_VAR',
        'description': 'Direct memory address'
    },
    'PRE_INDEXED': {
        'syntax': '<base>(Rn)',
        'example': 'B.100(W1)',
        'description': 'Base + index addressing (array access)'
    },
    'DESCRIPTOR': {
        'syntax': 'DESC(<operand>)(Rn)',
        'example': 'DESC(STRING)(W1)',
        'description': 'Descriptor-based array access with bounds checking'
    },
    'LOCAL_INDIRECT': {
        'syntax': '@B.<displacement>',
        'example': '@B.20',
        'description': 'Pointer stored on stack'
    },
    'RECORD_INDIRECT': {
        'syntax': '@R.<displacement>',
        'example': '@R.12',
        'description': 'Pointer in structure'
    },
    'ABSOLUTE_INDIRECT': {
        'syntax': '@<address>',
        'example': '@@POINTER',
        'description': 'Global pointer'
    }
}


def generate_examples_for_instruction(yaml_data):
    """Generate examples based on instruction characteristics."""

    instruction = yaml_data['instruction']
    name = instruction['name']
    mnemonic = instruction['mnemonic']
    category = instruction.get('category', 'ARITHMETIC')
    operand_count = instruction.get('operand_count', 0)
    variants = instruction.get('variants', [])

    examples = []

    # Get first variant for analysis
    if not variants:
        return examples

    first_variant = variants[0]
    prefixes = first_variant.get('prefixes', ['W'])
    operands_list = first_variant.get('operands', [])

    # Example 1: Basic usage with most common prefix (W - word)
    if 'W' in prefixes or 'R_N' in prefixes:
        prefix = 'W1'
        code_parts = [f"{prefix} {mnemonic.upper()}"]

        if operand_count > 0 and operands_list:
            operand_examples = []
            for i, op in enumerate(operands_list[:operand_count]):
                modes = op.get('addressing_modes', ['CONSTANT'])
                # Use different modes for variety
                if i == 0 and 'REGISTER' in modes:
                    operand_examples.append('W2')
                elif i == 0 and 'CONSTANT' in modes:
                    operand_examples.append('10')
                elif i == 1 and 'LOCAL' in modes:
                    operand_examples.append('B.16')
                elif i == 2 and 'RECORD' in modes:
                    operand_examples.append('R.8')
                else:
                    # Default to first available mode
                    if 'CONSTANT' in modes:
                        operand_examples.append(str((i+1) * 10))
                    elif 'REGISTER' in modes:
                        operand_examples.append(f'W{i+2}')
                    elif 'LOCAL' in modes:
                        operand_examples.append(f'B.{(i+1)*8}')

            code_parts.append(', '.join(operand_examples))

        examples.append({
            'description': f'Basic {name} operation using word register',
            'prefix': 'W',
            'code': ' '.join(code_parts),
            'explanation': f'Perform {name} operation on word-sized data'
        })

    # Example 2: Byte operation (if supported)
    if 'BY' in prefixes and operand_count > 0 and operands_list:
        code_parts = ['BY1', mnemonic.upper()]
        operand_examples = []

        for i, op in enumerate(operands_list[:operand_count]):
            modes = op.get('addressing_modes', ['CONSTANT'])
            if 'CONSTANT' in modes:
                operand_examples.append(str(i + 5))
            elif 'LOCAL' in modes:
                operand_examples.append(f'B.{i*4}')

        if operand_examples:
            code_parts.append(', '.join(operand_examples))
            examples.append({
                'description': f'{name.capitalize()} with byte-sized operands',
                'prefix': 'BY',
                'code': ' '.join(code_parts),
                'explanation': f'Byte operation - processes 8-bit values'
            })

    # Example 3: Local variable addressing mode
    if operand_count > 0 and operands_list:
        first_op_modes = operands_list[0].get('addressing_modes', [])
        if 'LOCAL' in first_op_modes:
            code_parts = ['W1', mnemonic.upper()]
            operand_examples = []

            for i, op in enumerate(operands_list[:operand_count]):
                modes = op.get('addressing_modes', [])
                if 'LOCAL' in modes:
                    operand_examples.append(f'B.{(i+1)*16}')
                elif 'CONSTANT' in modes:
                    operand_examples.append('100')

            if operand_examples:
                code_parts.append(', '.join(operand_examples))
                examples.append({
                    'description': f'{name.capitalize()} with local (stack) variables',
                    'addressing_mode': 'LOCAL',
                    'code': ' '.join(code_parts),
                    'explanation': 'Access local variables using B register (stack frame pointer)'
                })

    # Example 4: Array indexing with PRE_INDEXED mode
    if operand_count > 0 and operands_list:
        has_preindexed = any('PRE_INDEXED' in op.get('addressing_modes', []) for op in operands_list)
        if has_preindexed:
            code_parts = ['W1', mnemonic.upper()]

            for i, op in enumerate(operands_list[:operand_count]):
                modes = op.get('addressing_modes', [])
                if 'PRE_INDEXED' in modes:
                    code_parts.append('ARRAY(W2)')
                    break
                elif 'CONSTANT' in modes:
                    code_parts.append('0')

            examples.append({
                'description': f'{name.capitalize()} with array element access',
                'addressing_mode': 'PRE_INDEXED',
                'code': ' '.join(code_parts),
                'explanation': 'Array access: ARRAY(W2) computes address as ARRAY + W2 * element_size'
            })

    # Example 5: Floating-point operation (if supported)
    if 'F' in prefixes:
        code_parts = ['F1', mnemonic.upper()]

        if operand_count > 0 and operands_list:
            operand_examples = []
            for i, op in enumerate(operands_list[:operand_count]):
                modes = op.get('addressing_modes', [])
                if 'REGISTER' in modes:
                    operand_examples.append(f'F{i+2}')
                elif 'CONSTANT' in modes:
                    operand_examples.append('3.14')

            if operand_examples:
                code_parts.append(', '.join(operand_examples))

        examples.append({
            'description': f'{name.capitalize()} with floating-point data',
            'prefix': 'F',
            'code': ' '.join(code_parts),
            'explanation': 'Floating-point operation using F registers (32-bit IEEE 754)'
        })

    # Example 6: Multiple variants demonstration
    if len(variants) > 1:
        # Show different variants
        variant_examples = []
        for i, variant in enumerate(variants[:3]):  # Show first 3 variants
            var_num = variant.get('variant', f'{i+1}/?')
            var_prefixes = variant.get('prefixes', [])

            if var_prefixes:
                prefix = var_prefixes[0]
                if prefix == 'R_N':
                    prefix_str = 'W1'
                else:
                    prefix_str = f'{prefix}1'

                variant_examples.append(f'Variant {var_num}: {prefix_str} {mnemonic.upper()}')

        if variant_examples:
            examples.append({
                'description': f'Different variants of {name} instruction',
                'code': '\\n'.join(variant_examples),
                'explanation': f'The {name} instruction has {len(variants)} variants for different addressing modes and prefixes'
            })

    # Example 7: Real-world scenario based on category
    if category in CATEGORY_EXAMPLES:
        context = CATEGORY_EXAMPLES[category]['contexts'][0]

        if category == 'ARITHMETIC' and operand_count > 0:
            examples.append({
                'description': f'Real-world use: {context}',
                'code': f'; Loop counter increment\\nW1 {mnemonic.upper()} 1\\n; Result in W1',
                'explanation': f'Common use case for {name} in {context}'
            })
        elif category == 'COMPARE' and operand_count > 0:
            examples.append({
                'description': f'Real-world use: bounds checking',
                'code': f'; Check if index < limit\\nW1 {mnemonic.upper()} MAX_SIZE\\n; Sets flags for conditional branch',
                'explanation': f'Using {name} for validation and conditional logic'
            })

    return examples


def add_examples_to_yaml_file(yaml_path):
    """Add examples section to a YAML file if it doesn't exist."""

    print(f"Processing: {yaml_path}")

    # Load YAML
    with open(yaml_path, 'r', encoding='utf-8') as f:
        yaml_data = yaml.safe_load(f)

    # Check if examples already exist
    if 'examples' in yaml_data.get('instruction', {}):
        print(f"  → Already has examples, skipping")
        return False

    # Generate examples
    examples = generate_examples_for_instruction(yaml_data)

    if not examples:
        print(f"  → No examples generated")
        return False

    # Read file as text to preserve formatting
    with open(yaml_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Find the manual_reference section end
    # We'll insert examples before manual_reference
    manual_ref_start = content.find('  manual_reference:')

    if manual_ref_start == -1:
        print(f"  → Could not find manual_reference section")
        return False

    # Build examples YAML
    examples_yaml = "  examples:\n"
    for example in examples:
        examples_yaml += "    - description: |\n"
        for line in example['description'].split('\n'):
            examples_yaml += f"        {line}\n"

        if 'variant' in example:
            examples_yaml += f"      variant: \"{example['variant']}\"\n"
        if 'prefix' in example:
            examples_yaml += f"      prefix: \"{example['prefix']}\"\n"
        if 'addressing_mode' in example:
            examples_yaml += f"      addressing_mode: \"{example['addressing_mode']}\"\n"

        examples_yaml += "      code: |\n"
        for line in example['code'].split('\n'):
            examples_yaml += f"        {line}\n"

        if 'explanation' in example:
            examples_yaml += "      explanation: |\n"
            for line in example['explanation'].split('\n'):
                examples_yaml += f"        {line}\n"

        if 'result' in example:
            examples_yaml += "      result: |\n"
            for line in example['result'].split('\n'):
                examples_yaml += f"        {line}\n"

        examples_yaml += "\n"

    # Insert examples before manual_reference
    new_content = content[:manual_ref_start] + examples_yaml + content[manual_ref_start:]

    # Write back
    with open(yaml_path, 'w', encoding='utf-8') as f:
        f.write(new_content)

    print(f"  ✓ Added {len(examples)} examples")
    return True


def main():
    """Process all YAML files in the instructions directory."""

    yaml_dir = Path('/home/user/nd500x/docs/instructions/yaml')

    if not yaml_dir.exists():
        print(f"Error: Directory not found: {yaml_dir}")
        return

    yaml_files = sorted(yaml_dir.glob('*.yaml'))

    if not yaml_files:
        print(f"No YAML files found in {yaml_dir}")
        return

    print(f"Found {len(yaml_files)} YAML files")
    print()

    updated_count = 0
    skipped_count = 0
    error_count = 0

    for yaml_file in yaml_files:
        try:
            if add_examples_to_yaml_file(yaml_file):
                updated_count += 1
            else:
                skipped_count += 1
        except Exception as e:
            print(f"  ✗ Error: {e}")
            error_count += 1

    print()
    print("=" * 60)
    print(f"Summary:")
    print(f"  Updated: {updated_count}")
    print(f"  Skipped: {skipped_count}")
    print(f"  Errors:  {error_count}")
    print(f"  Total:   {len(yaml_files)}")
    print("=" * 60)


if __name__ == '__main__':
    main()
