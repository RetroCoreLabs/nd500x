#!/usr/bin/env python3
"""
Comprehensive manual description updater with detailed instruction matching.
"""

import re
import yaml
from pathlib import Path

# Comprehensive mapping of YAML function names to manual sections
INSTRUCTION_MAPPINGS = {
    # Register operations - Section 16.9
    'a1get': ('16.9', 'Integer float register communication', 'An:=', 'store A1 register'),
    'a2get': ('16.9', 'Integer float register communication', 'An:=', 'store A2 register'),
    'a3get': ('16.9', 'Integer float register communication', 'An:=', 'store A3 register'),
    'a4get': ('16.9', 'Integer float register communication', 'An:=', 'store A4 register'),
    'a1set': ('16.9', 'Integer float register communication', ':=An', 'load A1 register'),
    'a2set': ('16.9', 'Integer float register communication', ':=An', 'load A2 register'),
    'a3set': ('16.9', 'Integer float register communication', ':=An', 'load A3 register'),
    'a4set': ('16.9', 'Integer float register communication', ':=An', 'load A4 register'),
    'e1get': ('16.9', 'Integer float register communication', 'En:=', 'store E1 register'),
    'e2get': ('16.9', 'Integer float register communication', 'En:=', 'store E2 register'),
    'e3get': ('16.9', 'Integer float register communication', 'En:=', 'store E3 register'),
    'e4get': ('16.9', 'Integer float register communication', 'En:=', 'store E4 register'),
    'e1set': ('16.9', 'Integer float register communication', ':=En', 'load E1 register'),
    'e2set': ('16.9', 'Integer float register communication', ':=En', 'load E2 register'),
    'e3set': ('16.9', 'Integer float register communication', ':=En', 'load E3 register'),
    'e4set': ('16.9', 'Integer float register communication', ':=En', 'load E4 register'),

    # Base and Record register operations
    'assignbaseregto': ('10.2', 'Load local base register', 'B:=', 'load base register'),
    'assigntobasereg': ('10.5', 'Store local base register', 'B=:', 'store base register'),
    'assignrecordregto': ('10.3', 'Load record register', 'R:=', 'load record register'),
    'assigntorecordreg': ('10.6', 'Store record register', 'R=:', 'store record register'),

    # Mathematical functions - Chapter 12
    'acos': ('12.8', 'Arc cosine', 'ACOS', 'arc cosine'),
    'asin': ('12.6', 'Arc sine', 'ASIN', 'arc sine'),
    'atan': ('12.10', 'Arc tangent', 'ATAN', 'arc tangent'),
    'atan2': ('12.11', 'Arc tangent two argument', 'ATAN2', 'arc tangent two args'),
    'alog': ('12.13', 'Natural logarithm', 'ALOG', 'natural logarithm'),
    'alog10': ('12.15', 'Common logarithm', 'ALOG10', 'common logarithm'),
    'alog2': ('12.14', 'Binary logarithm', 'ALOG2', 'binary logarithm'),
    'sqrt': ('12.4', 'Square root', 'SQRT', 'square root'),
    'poly': ('12.3', 'Polynomial', 'POLY', 'polynomial evaluation'),

    # Comparison operations
    'comp2': ('10.10', 'Compare two operands', 'COMP2', 'compare two operands'),

    # Division operations
    'div2': ('11.8', 'Divide two operands', 'DIV2', 'divide two operands'),
    'div3': ('11.12', 'Divide three operands', 'DIV3', 'divide three operands'),
    'div4': ('11.14', 'Divide with remainder to register (modulo)', 'DIV4', 'divide with remainder'),
    'udiv': ('11.16', 'Unsigned divide', 'UDIV', 'unsigned divide'),

    # Multiplication operations
    'mul2': ('11.6', 'Multiply two operands', 'MUL2', 'multiply two operands'),
    'mul3': ('11.10', 'Multiply three operands', 'MUL3', 'multiply three operands'),
    'mul4': ('11.12', 'Multiply four operands', 'MUL4', 'multiply four operands'),
    'umul': ('11.18', 'Unsigned multiply', 'UMUL', 'unsigned multiply'),
    'mulad': ('11.20', 'Sum of Products', 'MULAD', 'multiply and add'),

    # Subtraction operations
    'sub2': ('11.4', 'Subtract two operands', 'SUB2', 'subtract two operands'),
    'sub3': ('11.10', 'Subtract three operands', 'SUB3', 'subtract three operands'),
    'subc': ('11.18', 'Subtract with carry', 'SUBC', 'subtract with carry'),

    # Addition operations
    'add2': ('11.2', 'Add two operands', 'ADD2', 'add two operands'),
    'add3': ('11.9', 'Add three operands', 'ADD3', 'add three operands'),

    # Type conversion
    'biconv': ('Type conversion', 'BI type conversion'),
    'byconv': ('Type conversion', 'BY type conversion'),
    'hconv': ('Type conversion', 'H type conversion'),
    'wconv': ('Type conversion', 'W type conversion'),
    'fconv': ('Type conversion', 'F type conversion'),
    'dconv': ('Type conversion', 'D type conversion'),

    # Special register operations - Section 16.7
    'tosget': ('16.7', 'Load special register', 'TOS', 'get TOS register'),
    'tosset': ('16.7', 'Load special register', 'TOS', 'set TOS register'),
    'llget': ('16.7', 'Load special register', 'LL', 'get LL register'),
    'llset': ('16.7', 'Load special register', 'LL', 'set LL register'),
    'hlget': ('16.7', 'Load special register', 'HL', 'get HL register'),
    'hlset': ('16.7', 'Load special register', 'HL', 'set HL register'),
    'thaget': ('16.7', 'Load special register', 'THA', 'get THA register'),
    'thaset': ('16.7', 'Load special register', 'THA', 'set THA register'),
    'lget': ('16.7', 'Load special register', 'L', 'get L register'),
    'lset': ('16.7', 'Load special register', 'L', 'set L register'),
    'cadget': ('16.7', 'Load special register', 'CAD', 'get CAD register'),
    'cadset': ('16.7', 'Load special register', 'CAD', 'set CAD register'),
    'cedget': ('16.7', 'Load special register', 'CED', 'get CED register'),
    'psget': ('16.7', 'Load special register', 'PS', 'get PS register'),
    'psset': ('16.7', 'Load special register', 'PS', 'set PS register'),
    'st1get': ('16.7', 'Load special register', 'ST1', 'get ST1 register'),
    'st1set': ('16.7', 'Load special register', 'ST1', 'set ST1 register'),

    # Trap enable registers
    'ote1get': ('16.7', 'Load special register', 'OTE1', 'get OTE1 register'),
    'ote1set': ('16.7', 'Load special register', 'OTE1', 'set OTE1 register'),
    'ote2get': ('16.7', 'Load special register', 'OTE2', 'get OTE2 register'),
    'ote2set': ('16.7', 'Load special register', 'OTE2', 'set OTE2 register'),
    'cte1get': ('16.7', 'Load special register', 'CTE1', 'get CTE1 register'),
    'cte2get': ('16.7', 'Load special register', 'CTE2', 'get CTE2 register'),
    'mte1get': ('16.7', 'Load special register', 'MTE1', 'get MTE1 register'),
    'mte2get': ('16.7', 'Load special register', 'MTE2', 'get MTE2 register'),
    'temm1get': ('16.7', 'Load special register', 'TEMM1', 'get TEMM1 register'),
    'temm2get': ('16.7', 'Load special register', 'TEMM2', 'get TEMM2 register'),

    # Control flow
    'callg': ('13.9', 'Call subroutine general', 'CALLG', 'call via general operand'),
    'retb': ('13.11', 'Subroutine return', 'RETB', 'return from subroutine'),
    'retd': ('13.11', 'Subroutine return', 'RETD', 'return from subroutine'),
    'rett': ('13.11', 'Subroutine return', 'RETT', 'return from trap'),
    'retk': ('13.11', 'Subroutine return', 'RETK', 'return and key trap'),
    'retbk': ('13.11', 'Subroutine return', 'RETBK', 'return and key trap'),

    # Entry points
    'entb': ('13.10', 'Subroutine entry points', 'ENTB', 'block entry'),
    'entd': ('13.10', 'Subroutine entry points', 'ENTD', 'domain entry'),
    'entf': ('13.10', 'Subroutine entry points', 'ENTF', 'function entry'),
    'entfn': ('13.10', 'Subroutine entry points', 'ENTFN', 'function entry no locals'),
    'entm': ('13.10', 'Subroutine entry points', 'ENTM', 'monitor entry'),
    'ents': ('13.10', 'Subroutine entry points', 'ENTS', 'subroutine entry'),
    'entsn': ('13.10', 'Subroutine entry points', 'ENTSN', 'subroutine entry no locals'),
    'entt': ('13.10', 'Subroutine entry points', 'ENTT', 'trap entry'),

    # System operations
    'dcc': ('16.10', 'Data cache clear', 'DCC', 'data cache clear'),
    'ddirt': ('16.11', 'DDIRT - Dump Dirty', 'DDIRT', 'dump dirty cache'),
    'dmon': ('16.13', 'Data memory management on', 'DMON', 'MMU on'),
    'dmof': ('16.15', 'Data memory management off', 'DMOF', 'MMU off'),
    'pmon': ('16.17', 'Program memory management on', 'PMON', 'program MMU on'),
    'pmof': ('16.19', 'Program memory management off', 'PMOF', 'program MMU off'),

    # Register block operations
    'sregbl': ('16.27.1', 'SREGBL - Save register block', 'SREGBL', 'save register block'),
    'lregbl': ('16.27.2', 'LREGBL - Load register block', 'LREGBL', 'load register block'),
    'scntxt': ('16.27.3', 'SCNTXT - Save context block', 'SCNTXT', 'save context'),
    'lcntxt': ('16.27.4', 'LCNTXT - Load context block', 'LCNTXT', 'load context'),

    # I/O operations
    'riom': ('16.23', 'Read I/O processor memory', 'RIOM', 'read IOP memory'),
    'wiom': ('16.24', 'Write I/O processor memory', 'WIOM', 'write IOP memory'),
    'rdus': ('16.28', 'REXT - Read from device external to CPU', 'RDUS', 'read device'),
    'wdus': ('16.29', 'WEXT - Write to device external to CPU', 'WDUS', 'write device'),

    # Physical segment operations
    'rphs': ('16.31', 'RPHS - Read from physical segment', 'RPHS', 'read physical segment'),
    'wphs': ('16.32', 'WPHS - Write to physical segment', 'WPHS', 'write physical segment'),

    # Jump/branch operations
    'jumps': ('16.34', 'JUMPS - Call supervisor', 'JUMPS', 'jump to supervisor'),
    'jumpg': ('13.3', 'Jump general', 'JUMPG', 'jump via general operand'),

    # Miscellaneous
    'init': ('13.12', 'Initialize stack', 'INIT', 'initialize stack'),
    'freeb': ('15.14', 'Free buddy element', 'FREEB', 'free buddy memory'),
    'svers': ('16.35', 'SVERS - Store microprogram version', 'SVERS', 'store version'),
    'scpuno': ('16.36', 'SCPUNO - Store CPU number', 'SCPUNO', 'store CPU number'),
    'tutti': ('16.37', 'TUTTI - Test unit', 'TUTTI', 'test unit'),

    # Bit operations
    'getb': ('10.27', 'Get bit', 'GETB', 'get bit'),
    'putbi': ('10.28', 'Put bit', 'PUTBI', 'put bit'),
    'clebi': ('10.29', 'Clear bit', 'CLEBI', 'clear bit'),
    'setbi': ('10.30', 'Set bit', 'SETBI', 'set bit'),
    'getbf': ('10.31', 'Get bit field', 'GETBF', 'get bit field'),
    'putbf': ('10.32', 'Put bit field', 'PUTBF', 'put bit field'),

    # Condition operations
    'tset': ('16.3', 'Test and set', 'TSET', 'test and set'),
    'sete': ('16.5', 'Set bit in trap enable register', 'SETE', 'set trap enable'),
    'clte': ('16.6', 'Clear bit in trap enable register', 'CLTE', 'clear trap enable'),

    # Shift operations
    'shr': ('10.26', 'Rotational shift', 'SHR', 'shift right rotate'),

    # String operations
    'sfill': ('14.8', 'String Fill', 'SFILL', 'string fill'),
    'sfilln': ('14.9', 'String Fill n bytes', 'SFILLN', 'string fill n'),
    'smovn': ('14.7', 'String move m elements', 'SMOVN', 'string move n'),
    'smvwh': ('14.3', 'String move while', 'SMVWH', 'string move while'),
    'smvtu': ('14.4', 'String move until', 'SMVTU', 'string move until'),
    'smvtr': ('14.6', 'String move translated until', 'SMVTR', 'string move translate'),
    'smvun': ('14.5', 'String move until n', 'SMVUN', 'string move until n'),
    'scomp': ('14.10', 'String compare', 'SCOMP', 'string compare'),
    'sloca': ('14.11', 'String locate', 'SLOCA', 'string locate'),
    'smatch': ('14.12', 'String match', 'SMATCH', 'string match'),
    'sscan': ('14.13', 'String scan', 'SSCAN', 'string scan'),
    'sskip': ('14.14', 'String skip', 'SSKIP', 'string skip'),
    'sspan': ('14.15', 'String span', 'SSPAN', 'string span'),
    'sspar': ('14.16', 'String span reverse', 'SSPAR', 'string span reverse'),

    # Set/Clear operations
    'set1': ('10.18', 'Set to one', 'SET1', 'set to one'),
    'setk': ('16.4', 'Set key', 'SETK', 'set key'),
    'clrk': ('16.2', 'Clear key', 'CLRK', 'clear key'),
    'stz': ('10.17', 'Store zero', 'STZ', 'store zero'),

    # Address operations
    'laddr': ('15.4', 'Load address', 'LADDR', 'load address'),
    'bladdr': ('15.5', 'Load address of base', 'BLADDR', 'load base address'),
    'rladdr': ('15.6', 'Load address into base register', 'RLADDR', 'load record address'),
    'phyladr': ('15.7', 'Physical address', 'PHYLADR', 'get physical address'),

    # Index operations
    'lind': ('15.8', 'Load index', 'LIND', 'load index'),
    'cind': ('15.9', 'Calculate Index', 'CIND', 'calculate index'),
    'axi': ('15.10', 'Adjust index', 'AXI', 'adjust index'),
    'ixi': ('15.11', 'Initialize index', 'IXI', 'initialize index'),

    # Memory operations
    'bmove': ('14.1', 'Block Move', 'BMOVE', 'block move'),

    # Conditional branches
    'ifequalgo': ('13.5', 'Conditional branch', 'IF=GO', 'if equal go'),
    'ifnotequalgo': ('13.5', 'Conditional branch', 'IF<>GO', 'if not equal go'),
    'iflessthango': ('13.5', 'Conditional branch', 'IF<GO', 'if less go'),
    'iflessequalgo': ('13.5', 'Conditional branch', 'IF<=GO', 'if less or equal go'),
    'ifgreaterthango': ('13.5', 'Conditional branch', 'IF>GO', 'if greater go'),
    'ifgreaterequalgo': ('13.5', 'Conditional branch', 'IF>=GO', 'if greater or equal go'),
    'ifunsignedlessgo': ('13.5', 'Conditional branch', 'IFU<GO', 'if unsigned less go'),
    'ifunsignedlessequalgo': ('13.5', 'Conditional branch', 'IFU<=GO', 'if unsigned less or equal go'),
    'ifunsignedgreatergo': ('13.5', 'Conditional branch', 'IFU>GO', 'if unsigned greater go'),
    'ifunsignedgreaterequalgo': ('13.5', 'Conditional branch', 'IFU>=GO', 'if unsigned greater or equal go'),
    'ifkeygo': ('13.5', 'Conditional branch', 'IFKGO', 'if key set go'),
    'ifstackgo': ('13.5', 'Conditional branch', 'IFSTGO', 'if stack go'),
    'ifkret': ('13.11', 'Conditional return', 'IFKRET', 'if key return'),
    'ifkgo': ('13.5', 'Conditional branch', 'IFKGO', 'if key go'),
    'ifstgo': ('13.5', 'Conditional branch', 'IFSTGO', 'if stack go'),

    # Misc operations
    'noop': ('15.13', 'No operation', 'NOOP', 'no operation'),
    'bp': ('16.1', 'Break point', 'BP', 'breakpoint'),
    'solo': ('16.1', 'Single operation', 'SOLO', 'single operation'),
}

def main():
    """
    Use the comprehensive mapping to update YAML files.
    """
    script_dir = Path(__file__).parent
    yaml_dir = script_dir / 'docs' / 'instructions' / 'yaml'
    manual_path = script_dir / 'docs' / 'ND-05.009.4 EN ND-500 Reference Manual.md'

    print("=" * 70)
    print("Comprehensive Manual Description Updater")
    print("Using detailed instruction mappings")
    print("=" * 70)
    print()

    # Get all YAML files
    yaml_files = sorted(yaml_dir.glob('*.yaml'))
    updated = 0
    skipped = 0

    for yaml_file in yaml_files:
        filename = yaml_file.stem.lower()

        if filename in INSTRUCTION_MAPPINGS:
            mapping = INSTRUCTION_MAPPINGS[filename]
            print(f"✓ Found mapping for {yaml_file.name}: {mapping[1]}")
            # Here we would add the manual description
            # For now, just count it
            updated += 1
        else:
            print(f"⚠️  No mapping for {yaml_file.name}")
            skipped += 1

    print()
    print("=" * 70)
    print(f"Found mappings for: {updated} files")
    print(f"Still need manual work: {skipped} files")
    print("=" * 70)

    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
