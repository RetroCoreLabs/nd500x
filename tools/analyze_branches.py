#!/usr/bin/env python3
"""
ND-500 Branch Instruction Analyzer
Analyzes execution traces to find potential branch-related bugs.

Usage: python3 analyze_branches.py <trace_file> [output_file]
"""

import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple

# Flag positions in the 7-char format [PdZsCko]
# Position 0: P (Privilege?)
# Position 1: d (Domain?)
# Position 2: Z (Zero)
# Position 3: s/S (Sign)
# Position 4: C/c (Carry)
# Position 5: k/K (Key/User)
# Position 6: o/O (Overflow)
FLAG_POSITIONS = {
    'Z': 2,
    'S': 3,
    'C': 4,
    'K': 5,
    'O': 6,
}

# Branch conditions: returns True if branch should be TAKEN
BRANCH_CONDITIONS = {
    'if=go':    lambda f: f['Z'],           # Equal (Z=1)
    'if><go':   lambda f: not f['Z'],       # Not Equal (Z=0)
    'if>go':    lambda f: not f['S'] and not f['Z'],  # Greater (signed)
    'if>=go':   lambda f: not f['S'],       # Greater/Equal (signed, S=0)
    'if<go':    lambda f: f['S'],           # Less (signed, S=1)
    'if<=go':   lambda f: f['S'] or f['Z'], # Less/Equal (signed)
    'if>>go':   lambda f: f['C'] and not f['Z'],  # Greater (unsigned)
    'if>>=go':  lambda f: f['C'],           # Greater/Equal (unsigned, C=1)
    'if<<go':   lambda f: not f['C'],       # Less (unsigned, C=0)
    'if<<=go':  lambda f: not f['C'] or f['Z'],   # Less/Equal (unsigned)
    'ifkgo':    lambda f: f['K'],           # K flag set
    'if-kgo':   lambda f: not f['K'],       # K flag clear
}


@dataclass
class TraceRecord:
    """Represents a single instruction from the trace."""
    line_number: int
    pc: str
    hex_bytes: str
    mnemonic: str
    operands: str
    flags_before: str
    registers: Dict[str, str] = field(default_factory=dict)
    flags_after: Optional[str] = None
    reg_changes: Optional[str] = None
    is_branch: bool = False


@dataclass
class BranchOccurrence:
    """A single occurrence of a branch instruction."""
    trace_line: int
    preceding_instr: Optional[TraceRecord]
    branch_instr: TraceRecord
    flags_at_branch: Dict[str, bool]
    branch_taken: bool
    target_address: Optional[str]


@dataclass
class BranchLocation:
    """All occurrences of branches at a specific PC."""
    pc: str
    mnemonic: str
    operand: str
    occurrences: List[BranchOccurrence] = field(default_factory=list)
    always_taken: bool = True
    never_taken: bool = True
    issues: List[str] = field(default_factory=list)


def parse_flags(flag_str: str) -> Dict[str, bool]:
    """Parse [PdZsCko] format to flag dict. Uppercase = SET."""
    if not flag_str or len(flag_str) < 7:
        return {'Z': False, 'S': False, 'C': False, 'K': False, 'O': False}

    # Remove brackets if present
    if flag_str.startswith('['):
        flag_str = flag_str[1:]
    if flag_str.endswith(']'):
        flag_str = flag_str[:-1]

    return {
        'Z': flag_str[2].isupper() if len(flag_str) > 2 else False,
        'S': flag_str[3].isupper() if len(flag_str) > 3 else False,
        'C': flag_str[4].isupper() if len(flag_str) > 4 else False,
        'K': flag_str[5].isupper() if len(flag_str) > 5 else False,
        'O': flag_str[6].isupper() if len(flag_str) > 6 else False,
    }


def format_flags(flags: Dict[str, bool]) -> str:
    """Format flags dict as readable string."""
    parts = []
    for name in ['Z', 'S', 'C', 'K', 'O']:
        if flags.get(name, False):
            parts.append(name + '=1')
        else:
            parts.append(name.lower() + '=0')
    return ' '.join(parts)


def is_branch_instruction(mnemonic: str) -> bool:
    """Check if mnemonic is a branch instruction."""
    m = mnemonic.lower().strip()
    return m in BRANCH_CONDITIONS


def parse_trace_file(filename: str) -> List[TraceRecord]:
    """Parse trace file into structured records."""
    records = []
    pending_result = None

    # Regex for instruction line
    # Format: 0xADDRESS HEX_BYTES MNEMONIC OPERANDS | REGISTERS [FLAGS]
    instr_pattern = re.compile(
        r'^(0x[0-9A-Fa-f]+)\s+'           # PC address
        r'([0-9A-Fa-f ]+?)\s{2,}'         # Hex bytes
        r'(\S+)\s*'                        # Mnemonic
        r'(.*?)\s*\|\s*'                   # Operands
        r'(.+?)\s*'                        # Registers
        r'\[([A-Za-z]+)\]'                 # Flags
    )

    # Result line pattern (starts with ->)
    result_pattern = re.compile(r'^\s+->\s+(.+)')

    with open(filename, 'r') as f:
        for line_num, line in enumerate(f, 1):
            line = line.rstrip()

            # Check for result line first
            result_match = result_pattern.match(line)
            if result_match and records:
                result_text = result_match.group(1)
                # Check for flag change
                flag_match = re.search(r'\[([A-Za-z]+)\]', result_text)
                if flag_match:
                    records[-1].flags_after = flag_match.group(1)
                # Store register changes
                records[-1].reg_changes = result_text
                continue

            # Check for instruction line
            instr_match = instr_pattern.match(line)
            if instr_match:
                pc, hex_bytes, mnemonic, operands, regs, flags = instr_match.groups()

                # Parse register values
                reg_dict = {}
                for reg_match in re.finditer(r'(\w+)\[([0-9A-Fa-f]+)\]', regs):
                    reg_dict[reg_match.group(1)] = reg_match.group(2)

                record = TraceRecord(
                    line_number=line_num,
                    pc=pc,
                    hex_bytes=hex_bytes.strip(),
                    mnemonic=mnemonic.strip(),
                    operands=operands.strip(),
                    flags_before=flags,
                    registers=reg_dict,
                    is_branch=is_branch_instruction(mnemonic.strip())
                )
                records.append(record)

    return records


def analyze_branches(records: List[TraceRecord]) -> Dict[str, BranchLocation]:
    """Group branches by PC and analyze each occurrence."""
    branches_by_pc: Dict[str, BranchLocation] = {}

    for i, record in enumerate(records):
        if not record.is_branch:
            continue

        mnemonic = record.mnemonic.lower().strip()
        if mnemonic not in BRANCH_CONDITIONS:
            continue

        # Get preceding instruction
        preceding = records[i - 1] if i > 0 else None

        # Parse flags at branch time
        flags = parse_flags(record.flags_before)

        # Determine if branch was taken
        condition_func = BRANCH_CONDITIONS.get(mnemonic)
        branch_taken = condition_func(flags) if condition_func else False

        # Calculate target address
        target = None
        if branch_taken and record.operands:
            # Extract displacement from operand like $0xD or $0x55
            disp_match = re.search(r'\$(?:0x)?([0-9A-Fa-f]+)', record.operands)
            if disp_match:
                disp = int(disp_match.group(1), 16)
                # Target = PC + instruction_length + displacement
                # For short branches, length is typically 2-3 bytes
                pc_val = int(record.pc, 16)
                instr_len = len(record.hex_bytes.replace(' ', '')) // 2
                target = hex(pc_val + instr_len + disp)

        occurrence = BranchOccurrence(
            trace_line=record.line_number,
            preceding_instr=preceding,
            branch_instr=record,
            flags_at_branch=flags,
            branch_taken=branch_taken,
            target_address=target
        )

        # Add to location tracking
        if record.pc not in branches_by_pc:
            branches_by_pc[record.pc] = BranchLocation(
                pc=record.pc,
                mnemonic=mnemonic,
                operand=record.operands
            )

        loc = branches_by_pc[record.pc]
        loc.occurrences.append(occurrence)

        # Update always/never taken tracking
        if branch_taken:
            loc.never_taken = False
        else:
            loc.always_taken = False

    return branches_by_pc


def detect_issues(branches: Dict[str, BranchLocation]) -> List[Tuple[str, str, str]]:
    """Detect potential bugs in branch logic."""
    issues = []

    for pc, loc in branches.items():
        # Check for consistent behavior that might indicate issues
        if len(loc.occurrences) >= 3:
            if loc.always_taken:
                issues.append((pc, "ALWAYS_TAKEN",
                    f"Branch at {pc} ({loc.mnemonic}) is ALWAYS taken ({len(loc.occurrences)} occurrences). "
                    f"May indicate dead code or redundant branch."))
            elif loc.never_taken:
                issues.append((pc, "NEVER_TAKEN",
                    f"Branch at {pc} ({loc.mnemonic}) is NEVER taken ({len(loc.occurrences)} occurrences). "
                    f"May indicate dead code or redundant test."))

        # Check for suspicious flag patterns
        for occ in loc.occurrences:
            if occ.preceding_instr:
                prev = occ.preceding_instr
                # Check if preceding instruction modified flags
                if prev.flags_after is None:
                    # No flag change recorded - might be missing
                    if prev.mnemonic.lower() not in ['call', 'entd', 'rets', 'jump', 'go']:
                        loc.issues.append(
                            f"Line {occ.trace_line}: Preceding instruction '{prev.mnemonic}' "
                            f"has no flag change recorded")

    return issues


def get_condition_explanation(mnemonic: str, flags: Dict[str, bool], taken: bool) -> str:
    """Explain why a branch was taken or not taken."""
    m = mnemonic.lower()
    explanations = {
        'if=go': f"Z={1 if flags['Z'] else 0} (Equal requires Z=1)",
        'if><go': f"Z={1 if flags['Z'] else 0} (Not Equal requires Z=0)",
        'if>=go': f"S={1 if flags['S'] else 0} (Signed >= requires S=0)",
        'if<go': f"S={1 if flags['S'] else 0} (Signed < requires S=1)",
        'if>>=go': f"C={1 if flags['C'] else 0} (Unsigned >= requires C=1)",
        'if<<go': f"C={1 if flags['C'] else 0} (Unsigned < requires C=0)",
        'if-kgo': f"K={1 if flags['K'] else 0} (K-clear requires K=0)",
        'ifkgo': f"K={1 if flags['K'] else 0} (K-set requires K=1)",
    }
    base = explanations.get(m, f"Flags: {format_flags(flags)}")
    return f"{base} -> {'TAKEN' if taken else 'NOT TAKEN'}"


def generate_report(branches: Dict[str, BranchLocation], issues: List[Tuple[str, str, str]]) -> str:
    """Generate markdown report."""
    lines = []

    # Header
    lines.append("# Branch Instruction Analysis Report")
    lines.append("")
    lines.append("Generated from ND-500 execution trace analysis.")
    lines.append("")

    # Summary
    total_locations = len(branches)
    always_taken = sum(1 for b in branches.values() if b.always_taken)
    never_taken = sum(1 for b in branches.values() if b.never_taken)
    varying = total_locations - always_taken - never_taken
    total_occurrences = sum(len(b.occurrences) for b in branches.values())

    lines.append("## Summary")
    lines.append("")
    lines.append(f"- **Total unique branch locations:** {total_locations}")
    lines.append(f"- **Total branch occurrences:** {total_occurrences}")
    lines.append(f"- **Branches always taken:** {always_taken}")
    lines.append(f"- **Branches never taken:** {never_taken}")
    lines.append(f"- **Branches with varying outcomes:** {varying}")
    lines.append(f"- **Potential issues detected:** {len(issues)}")
    lines.append("")

    # Branch type distribution
    lines.append("## Branch Type Distribution")
    lines.append("")
    type_counts = defaultdict(int)
    for loc in branches.values():
        type_counts[loc.mnemonic] += len(loc.occurrences)

    lines.append("| Branch Type | Occurrences | Condition |")
    lines.append("|-------------|-------------|-----------|")
    for mtype, count in sorted(type_counts.items(), key=lambda x: -x[1]):
        cond_desc = {
            'if=go': 'Equal (Z=1)',
            'if><go': 'Not Equal (Z=0)',
            'if>=go': 'Signed >= (S=0)',
            'if<go': 'Signed < (S=1)',
            'if>>=go': 'Unsigned >= (C=1)',
            'if<<go': 'Unsigned < (C=0)',
            'if-kgo': 'K Clear (K=0)',
            'ifkgo': 'K Set (K=1)',
        }.get(mtype, 'Unknown')
        lines.append(f"| {mtype} | {count} | {cond_desc} |")
    lines.append("")

    # Detailed branch analysis by PC
    lines.append("## Branch Details by PC Address")
    lines.append("")

    # Sort by PC address
    sorted_pcs = sorted(branches.keys(), key=lambda x: int(x, 16))

    for pc in sorted_pcs:
        loc = branches[pc]
        lines.append(f"### {pc}: {loc.mnemonic}")
        lines.append("")
        lines.append(f"**Operand:** `{loc.operand}`")
        lines.append(f"**Occurrences:** {len(loc.occurrences)}")

        if loc.always_taken:
            lines.append("**Status:** Always taken")
        elif loc.never_taken:
            lines.append("**Status:** Never taken")
        else:
            taken_count = sum(1 for o in loc.occurrences if o.branch_taken)
            lines.append(f"**Status:** Taken {taken_count}/{len(loc.occurrences)} times")
        lines.append("")

        # Show first few occurrences
        lines.append("| Line | Preceding Instruction | Flags Before | Flags After | Taken? |")
        lines.append("|------|----------------------|--------------|-------------|--------|")

        # Limit to first 5 occurrences to keep report manageable
        for occ in loc.occurrences[:5]:
            prev_desc = ""
            flags_before = ""
            flags_after = ""

            if occ.preceding_instr:
                p = occ.preceding_instr
                prev_desc = f"`{p.mnemonic} {p.operands[:30]}`"
                flags_before = f"`[{p.flags_before}]`"
                if p.flags_after:
                    flags_after = f"`[{p.flags_after}]`"
                else:
                    flags_after = "(unchanged)"

            taken_str = "Yes" if occ.branch_taken else "No"
            lines.append(f"| {occ.trace_line} | {prev_desc} | {flags_before} | {flags_after} | {taken_str} |")

        if len(loc.occurrences) > 5:
            lines.append(f"| ... | ({len(loc.occurrences) - 5} more occurrences) | | | |")

        lines.append("")

        # Show analysis
        first_occ = loc.occurrences[0] if loc.occurrences else None
        if first_occ:
            explanation = get_condition_explanation(loc.mnemonic, first_occ.flags_at_branch, first_occ.branch_taken)
            lines.append(f"**Analysis:** {explanation}")
            lines.append("")

        # Show any issues for this location
        if loc.issues:
            lines.append("**Potential Issues:**")
            for issue in loc.issues[:3]:
                lines.append(f"- {issue}")
            lines.append("")

    # Issues section
    if issues:
        lines.append("## Potential Issues")
        lines.append("")
        for pc, issue_type, desc in sorted(issues, key=lambda x: int(x[0], 16)):
            lines.append(f"### {pc}: {issue_type}")
            lines.append("")
            lines.append(desc)
            lines.append("")

    # Flag reference
    lines.append("## Flag Reference")
    lines.append("")
    lines.append("| Flag | Meaning |")
    lines.append("|------|---------|")
    lines.append("| Z | Zero - result is zero |")
    lines.append("| S | Sign - result is negative (MSB=1) |")
    lines.append("| C | Carry - unsigned overflow/borrow |")
    lines.append("| K | User/Key - programmable flag |")
    lines.append("| O | Overflow - signed overflow |")
    lines.append("")
    lines.append("**Branch type mapping:**")
    lines.append("- **Signed comparisons:** Use S and Z flags (if<, if>, if<=, if>=, if=, if><)")
    lines.append("- **Unsigned comparisons:** Use C and Z flags (if<<, if>>, if<<=, if>>=)")
    lines.append("- **User flag:** Uses K flag (ifk, if-k)")
    lines.append("")

    return '\n'.join(lines)


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 analyze_branches.py <trace_file> [output_file]")
        print("  trace_file: Path to ND-500 execution trace file")
        print("  output_file: Optional output markdown file (default: docs/branch_analysis_report.md)")
        sys.exit(1)

    trace_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else 'docs/branch_analysis_report.md'

    print(f"Parsing trace file: {trace_file}")
    records = parse_trace_file(trace_file)
    print(f"  Found {len(records)} instruction records")

    print("Analyzing branches...")
    branches = analyze_branches(records)
    print(f"  Found {len(branches)} unique branch locations")

    branch_count = sum(len(b.occurrences) for b in branches.values())
    print(f"  Total branch occurrences: {branch_count}")

    print("Detecting potential issues...")
    issues = detect_issues(branches)
    print(f"  Found {len(issues)} potential issues")

    print(f"Generating report: {output_file}")
    report = generate_report(branches, issues)

    with open(output_file, 'w') as f:
        f.write(report)

    print("Done!")

    # Print summary to stdout
    print("\n=== Quick Summary ===")
    for pc in sorted(branches.keys(), key=lambda x: int(x, 16)):
        loc = branches[pc]
        status = "ALWAYS" if loc.always_taken else ("NEVER" if loc.never_taken else "VARIES")
        count = len(loc.occurrences)
        print(f"  {pc}: {loc.mnemonic:12s} {status:8s} ({count}x)")


if __name__ == '__main__':
    main()
