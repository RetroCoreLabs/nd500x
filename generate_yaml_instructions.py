#!/usr/bin/env python3
"""
Generate individual YAML files for each unique ND-500 instruction.
Groups all variants of the same instruction into one file.
Reads from docs/instructions/instructions.json and creates YAML files in docs/instructions/yaml/
"""

import json
import os
import sys
from pathlib import Path
from collections import defaultdict

def clean_name_for_filename(name):
    """Convert instruction name to safe filename"""
    # Replace special characters
    replacements = {
        ':=': 'assign_to',
        '=:': 'assign_from',
        'b:=': 'assign_base_to',
        'r:=': 'assign_record_to',
        'b=:': 'assign_to_base',
        'r=:': 'assign_to_record',
        '+': 'plus',
        '-': 'minus',
        '*': 'mult',
        '/': 'div',
        '&': 'and',
        '|': 'or',
        '^': 'xor',
        '~': 'not',
        '<': 'lt',
        '>': 'gt',
        '=': 'eq',
        '!': 'not',
        '@': 'at',
        '#': 'hash',
        '$': 'dollar',
        '%': 'mod',
        ' ': '_',
    }

    result = name.lower()
    for char, replacement in replacements.items():
        result = result.replace(char, replacement)

    # Remove any remaining non-alphanumeric characters
    result = ''.join(c if c.isalnum() or c == '_' else '_' for c in result)

    # Remove consecutive underscores
    while '__' in result:
        result = result.replace('__', '_')

    return result.strip('_')

def parse_prefixes(prefix_str):
    """Parse prefix string to list of prefixes"""
    if not prefix_str or prefix_str == "InstructionPrefixes.NONE":
        return ["NONE"]

    prefixes = []
    for part in prefix_str.split('|'):
        part = part.strip()
        if part.startswith('InstructionPrefixes.'):
            prefix = part.replace('InstructionPrefixes.', '').strip()
            prefixes.append(prefix)

    return prefixes if prefixes else ["NONE"]

def parse_addressing_modes_single(mode_str):
    """Parse a single operand's addressing modes string"""
    if not mode_str:
        return []

    modes = []
    for part in str(mode_str).split('|'):
        part = part.strip()
        if part.startswith('AddressingModes.'):
            mode = part.replace('AddressingModes.', '').strip()
            modes.append(mode)

    return modes

def parse_addressing_modes(modes_list):
    """Parse addressing modes list - returns list of lists (one per operand)"""
    if not modes_list:
        return []

    # If it's a list, parse each operand separately
    if isinstance(modes_list, list):
        return [parse_addressing_modes_single(m) for m in modes_list]
    else:
        # Single operand
        return [parse_addressing_modes_single(modes_list)]

def get_privilege_level(instruction_class, function_name):
    """Determine privilege level based on instruction characteristics"""
    # System instructions require supervisor mode
    system_keywords = ['SYSTEM', 'TRAP', 'MONITOR', 'IO', 'CACHE', 'MMU', 'PRIVILEGED']

    if instruction_class == 'SYSTEM':
        return 'supervisor'

    for keyword in system_keywords:
        if keyword in function_name.upper():
            return 'supervisor'

    return 'user'

def generate_description(instruction_group):
    """Generate comprehensive instruction description from all variants"""
    # Get common info from first instruction
    first = instruction_group[0]
    mnemonic = first.get('mnemonic', '')
    function_name = first.get('functionName', '')
    class_name = first.get('class', '')
    operand_count = first.get('operandCount', 0)

    desc_parts = []

    # Main operation description
    desc_parts.append(f"{function_name}")
    desc_parts.append("")

    if class_name == 'MOVE':
        if 'AssignTo' in function_name:
            desc_parts.append(f"Load/Assign operation:")
            desc_parts.append(f"Loads value into accumulator/register from operand")
            desc_parts.append(f"Syntax: {mnemonic} <source>")
        elif 'AssignFrom' in function_name:
            desc_parts.append(f"Store operation:")
            desc_parts.append(f"Stores value from accumulator/register to operand")
            desc_parts.append(f"Syntax: {mnemonic} <destination>")
        elif 'AssignBaseRegTo' in function_name:
            desc_parts.append(f"Load Base Register:")
            desc_parts.append(f"B ← <operand>")
        elif 'AssignRecordRegTo' in function_name:
            desc_parts.append(f"Load Record Register:")
            desc_parts.append(f"R ← <operand>")
        elif 'AssignToBaseReg' in function_name:
            desc_parts.append(f"Store to Base Register target:")
            desc_parts.append(f"<operand> ← B")
        elif 'AssignToRecordReg' in function_name:
            desc_parts.append(f"Store to Record Register target:")
            desc_parts.append(f"<operand> ← R")
        elif 'Move' in function_name:
            desc_parts.append(f"Move data between operands:")
            desc_parts.append(f"<dest> ← <source>")
        elif 'Swap' in function_name:
            desc_parts.append(f"Exchange/swap values:")
            desc_parts.append(f"<op1> ↔ <op2>")
    elif class_name == 'ARITHMETIC':
        if 'Neg' in function_name:
            desc_parts.append(f"Negate operation:")
            desc_parts.append(f"Computes two's complement (negation) of operand")
            desc_parts.append(f"Result ← -operand")
        elif 'Abs' in function_name:
            desc_parts.append(f"Absolute value operation:")
            desc_parts.append(f"Result ← |operand|")
        elif 'Add' in function_name:
            desc_parts.append(f"Addition operation:")
            desc_parts.append(f"Result ← op1 + op2")
        elif 'Sub' in function_name:
            desc_parts.append(f"Subtraction operation:")
            desc_parts.append(f"Result ← op1 - op2")
        elif 'Mul' in function_name:
            desc_parts.append(f"Multiplication operation:")
            desc_parts.append(f"Result ← op1 × op2")
        elif 'Div' in function_name:
            desc_parts.append(f"Division operation:")
            desc_parts.append(f"Result ← op1 ÷ op2")
        else:
            desc_parts.append(f"Arithmetic operation")
    elif class_name == 'LOGICAL':
        if 'Inv' in function_name:
            desc_parts.append(f"Invert/NOT operation:")
            desc_parts.append(f"Performs bitwise NOT (one's complement)")
            desc_parts.append(f"Result ← ~operand")
        elif 'And' in function_name:
            desc_parts.append(f"Logical AND operation:")
            desc_parts.append(f"Result ← op1 & op2")
        elif 'Or' in function_name:
            desc_parts.append(f"Logical OR operation:")
            desc_parts.append(f"Result ← op1 | op2")
        elif 'Xor' in function_name:
            desc_parts.append(f"Logical XOR operation:")
            desc_parts.append(f"Result ← op1 ^ op2")
        else:
            desc_parts.append(f"Logical operation")
    elif class_name == 'COMPARE':
        if 'Comp2' in function_name:
            desc_parts.append(f"Compare two operands:")
            desc_parts.append(f"Compares op1 with op2 and sets condition flags")
        elif 'Comp' in function_name:
            desc_parts.append(f"Compare with accumulator:")
            desc_parts.append(f"Compares operand with accumulator and sets flags")
        elif 'Test' in function_name:
            desc_parts.append(f"Test against zero:")
            desc_parts.append(f"Tests operand and sets condition flags")
        else:
            desc_parts.append(f"Comparison operation")
        desc_parts.append(f"Sets Z (zero), N (negative), V (overflow), C (carry) flags")
    elif class_name == 'BRANCH':
        desc_parts.append(f"Branch/Jump operation:")
        desc_parts.append(f"Conditional or unconditional program flow transfer")
    elif class_name == 'CALL':
        desc_parts.append(f"Subroutine call:")
        desc_parts.append(f"Saves return address and transfers control to subroutine")
    elif class_name == 'SHIFT':
        desc_parts.append(f"Shift operation:")
        desc_parts.append(f"Shifts bits left or right")
    elif class_name == 'BITFIELD':
        desc_parts.append(f"Bit field operation:")
        desc_parts.append(f"Manipulates bit fields within operands")
    elif class_name == 'STRING':
        desc_parts.append(f"String operation:")
        desc_parts.append(f"Processes string/array data")
    elif class_name == 'FLOAT_MATH':
        desc_parts.append(f"Floating point operation:")
        desc_parts.append(f"Performs floating-point arithmetic")
    elif class_name == 'CONTROL':
        desc_parts.append(f"Control flow operation")
    elif class_name == 'SYSTEM':
        desc_parts.append(f"System operation (privileged)")
    elif class_name == 'IO':
        desc_parts.append(f"I/O operation:")
        desc_parts.append(f"Performs input/output operations")

    # Add operand count info
    desc_parts.append("")
    if operand_count == 0:
        desc_parts.append("Operands: None (operates on implicit register)")
    elif operand_count == 1:
        desc_parts.append("Operands: 1")
    else:
        desc_parts.append(f"Operands: {operand_count}")

    # Add variant count
    desc_parts.append(f"Variants: {len(instruction_group)} opcode(s)")

    return "\n".join(desc_parts)

def instruction_group_to_yaml(instruction_group, group_key):
    """Convert a group of instruction variants to YAML string"""
    # Get common info from first instruction
    first = instruction_group[0]
    mnemonic = first.get('mnemonic', '')
    function_name = first.get('functionName', '')
    class_name = first.get('class', '')
    operand_count = first.get('operandCount', 0)
    privilege = get_privilege_level(class_name, function_name)

    description = generate_description(instruction_group)

    # Build YAML content
    yaml_lines = []
    yaml_lines.append("instruction:")
    yaml_lines.append(f'  name: "{mnemonic}"')
    yaml_lines.append(f'  function: "{function_name}"')
    yaml_lines.append(f'  mnemonic: "{mnemonic}"')
    yaml_lines.append(f'  description: |')
    for line in description.split('\n'):
        yaml_lines.append(f'    {line}')
    yaml_lines.append('')
    yaml_lines.append(f'  category: "{class_name}"')
    yaml_lines.append(f'  instruction_class: "{class_name.lower()}"')
    yaml_lines.append(f'  privilege: "{privilege}"')
    yaml_lines.append(f'  operand_count: {operand_count}')
    yaml_lines.append('')

    # Variants section
    yaml_lines.append(f'  variants:')

    # FIX: Use actual index in group, not JSON's variantNumber (which is wrong!)
    total_in_group = len(instruction_group)

    for variant_idx, instruction in enumerate(instruction_group):
        opcode = instruction.get('opcode', '0x0000')
        prefixes = parse_prefixes(instruction.get('prefixes', ''))
        prefix_mask = instruction.get('prefixMask', '0x00')
        allowed_modes = parse_addressing_modes(instruction.get('allowedModes', []))
        operand_templates = instruction.get('operandTemplates', [])
        metadata = instruction.get('metadata', [])

        # Convert hex to binary
        opcode_int = int(opcode, 16)
        opcode_binary = format(opcode_int, '016b')
        # Format binary with underscores for readability
        opcode_binary_formatted = '_'.join([opcode_binary[i:i+4] for i in range(0, 16, 4)])

        # Use actual position in group: 1/12, 2/12, 3/12... NOT 1/3, 1/3, 1/3, 1/3
        yaml_lines.append(f'    - variant: {variant_idx + 1}/{total_in_group}')
        yaml_lines.append(f'      opcode: "{opcode}"')
        yaml_lines.append(f'      opcode_binary: "{opcode_binary_formatted}"')
        yaml_lines.append(f'      opcode_decimal: {opcode_int}')
        yaml_lines.append(f'      prefix_mask: "{prefix_mask}"')
        yaml_lines.append('')

        yaml_lines.append(f'      prefixes:')
        for prefix in prefixes:
            yaml_lines.append(f'        - {prefix}')
        yaml_lines.append('')

        # Structure operands properly (allowed_modes is now list of lists)
        if allowed_modes and any(allowed_modes):
            num_operands = len(allowed_modes)

            # If we have multiple operands, structure them properly
            if num_operands > 0:
                yaml_lines.append(f'      operands:')
                for op_idx in range(num_operands):
                    yaml_lines.append(f'        - operand: {op_idx + 1}')

                    # Addressing modes for this operand
                    if op_idx < len(allowed_modes) and allowed_modes[op_idx]:
                        yaml_lines.append(f'          addressing_modes:')
                        for mode in allowed_modes[op_idx]:
                            yaml_lines.append(f'            - {mode}')

                    # Template for this operand
                    if operand_templates and op_idx < len(operand_templates):
                        yaml_lines.append(f'          template: "{operand_templates[op_idx]}"')

                    # Metadata for this operand
                    if metadata and op_idx < len(metadata):
                        yaml_lines.append(f'          metadata: "{metadata[op_idx]}"')

                    yaml_lines.append('')

    yaml_lines.append('# Generated from nd500-opcodes')
    yaml_lines.append(f'# Instruction group: {group_key}')
    yaml_lines.append(f'# Total variants: {len(instruction_group)}')

    return '\n'.join(yaml_lines) + '\n'

def main():
    """Main function to generate YAML files"""
    # Paths
    script_dir = Path(__file__).parent
    json_file = script_dir / 'docs' / 'instructions' / 'instructions.json'
    output_dir = script_dir / 'docs' / 'instructions' / 'yaml'

    # Check if JSON file exists
    if not json_file.exists():
        print(f"Error: {json_file} not found")
        return 1

    # Create output directory
    output_dir.mkdir(exist_ok=True, parents=True)

    # Read JSON file
    print(f"Reading {json_file}...")
    with open(json_file, 'r') as f:
        data = json.load(f)

    instructions = data.get('instructions', [])
    total_instructions = len(instructions)

    print(f"Found {total_instructions} instruction variants")
    print(f"Grouping by function name...")

    # Group instructions by function name
    instruction_groups = defaultdict(list)
    for instruction in instructions:
        function_name = instruction.get('functionName', 'Unknown')
        instruction_groups[function_name].append(instruction)

    unique_count = len(instruction_groups)
    print(f"Found {unique_count} unique instructions")
    print(f"Generating YAML files in {output_dir}...")

    # Generate YAML files
    for group_idx, (function_name, group) in enumerate(sorted(instruction_groups.items()), 1):
        # Create filename from function name
        safe_name = clean_name_for_filename(function_name)
        filename = f"{safe_name}.yaml"
        filepath = output_dir / filename

        # Generate YAML content
        yaml_content = instruction_group_to_yaml(group, function_name)

        # Write file
        with open(filepath, 'w') as f:
            f.write(yaml_content)

        # Progress indicator
        if group_idx % 25 == 0:
            print(f"  Generated {group_idx}/{unique_count} files...")

    print(f"\nSuccess! Generated {unique_count} YAML files in {output_dir}")
    print(f"\nSummary:")
    print(f"  - Total instruction variants: {total_instructions}")
    print(f"  - Unique instructions: {unique_count}")
    print(f"  - YAML files created: {unique_count}")
    print(f"\nExample files:")
    yaml_files = list(output_dir.glob('*.yaml'))
    for i, yaml_file in enumerate(sorted(yaml_files)[:5]):
        print(f"  - {yaml_file.name}")

    return 0

if __name__ == '__main__':
    sys.exit(main())
