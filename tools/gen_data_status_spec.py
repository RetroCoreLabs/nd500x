#!/usr/bin/env python3
"""gen_data_status_spec.py - write DataStatusSpec.cs from the manual text.

SPDX-License-Identifier: MIT
Copyright (c) 2025-2026 Ronny Hansen

Reads the "Data status bits:" list that the ND-500 Reference Manual gives
for each instruction (embedded in docs/instructions/yaml/*.yaml under
instruction.manual_reference) and writes a C# table for RetroCore's
conformance-corpus generators: which of the data status bits Z, C, S, K, O
each instruction changes, for integer and for float data types, and whether
its result depends on a flag it reads.

The manual's rule (ND-05.009.4 section 6.5.1, and again at the start of the
instruction set): "Bits that are set, reset or left unaffected are mentioned
explicitly. All data status bits not mentioned are reset." So a data status
bit is kept only where the list says "Unaffected"; every data status bit the
list does not name is cleared by the instruction.

K is NOT a data status bit: Table 7 lists Z C S O IVO DZ FU FO BO only, and
the manual describes K on its own (section 6.5, "K : Flag"): "There are special
instructions for setting, resetting and testing this condition ... CIND, LIND
and string instructions will always leave a status in K regardless of its
previous value, while descriptor addressing may set but never clear the K
flag." So the reset rule does not reach K: an instruction keeps K unless its
list names K or its Operation writes it.

The generators use it to add flag-preset twins: the same scenario with
Z, C, S, K and O set beforehand. For an "Unaffected" instruction every preset
flag must survive (except one its Operation writes); for every other mapped
instruction the twin must end with exactly the data flags of the clean
original - which tests that unlisted flags really are cleared. Only
instructions whose every line resolves are marked Mapped; anything else gets
no twin until someone adjudicates it.

Reading rules (each non-literal one is recorded in the entry's Note):
  "X -> F"            F is changed (computed) by the instruction.
  "X -> F (integer)"  changed for BY/H/W/BI only; unaffected for F/D.
  "X -> F (float)"    changed for F/D only; unaffected for integers.
  "Unaffected"        no data status bit changes (the Unaffected mask is all of them).
  "All cleared"       every data status bit is cleared (Z C S O); K is not one.
  "-> F0"             read as FO (floating overflow) - a transcription of O as 0.
  "-> 0"              read as O when the line names overflow.
  FU, FO, DZ, BO      trap-status bits, not data flags: not preset, so ignored here.
  code fences, lone backticks and the page footer "Norsk Data ND-05.009.4 EN"
                      are layout, not manual content, and are skipped.
  a list with no resolved line (only layout) is "(empty list)": unmapped, never
                      read as "Unaffected".
  "The instruction ST1:= ..." / "ST1=: ..." remarks and the conditional-jump table
                      introduction, which follow some lists on shared pages, describe
                      other instructions: skipped, and noted.

Usage: tools/gen_data_status_spec.py <output DataStatusSpec.cs>
"""

import glob
import os
import re
import sys

import yaml

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
YAML_DIR = os.path.join(REPO, 'docs', 'instructions', 'yaml')

DATA_FLAGS = ['Z', 'C', 'S', 'K', 'O']          # the bits a twin presets
STATUS_FLAGS = ['Z', 'C', 'S', 'O']              # of those, the Table 7 data status bits
TRAP_STATUS = {'FU', 'FO', 'DZ', 'BO'}
NOISE = re.compile(r'^(`{1,3}\w*|-{2,}|Norsk Data ND-05\.009\.4 EN)$')
# Manual sentences that follow a list on a shared page but describe ANOTHER
# instruction (the special-register transfer pages remark on ST1:= / ST1=:;
# the conditional-jump pages go straight into their table introduction).
OTHER_INSTRUCTION = re.compile(r'^(The instruction ST1[:=]{2} |In the following table all conditional jump)')

# Instructions whose RESULT depends on a data flag they read. Presetting the
# flags would change what they compute, so they get no twin. Each is here
# because of the manual sentence quoted.
READS_FLAGS = {
    'addc': 'manual 11.17 "Add with carry": the carry is added (+ C)',
    'subc': 'manual 11.18 "Subtract with carry": the carry takes part (+ C)',
    'invc': 'manual 10.14 "Invert with carry add": the carry is added (+ C)',
}
READS_FLAGS_PREFIX = ('if', '-if')  # conditional branches and IF K RET test status bits

# Instructions that write a data flag in their Operation although their Data
# status list does not name it. The flag is added to both masks.
OPERATION_WRITES = {
    'clrk': ('K', 'Operation: "0 -> K bit of status register"'),
    'setk': ('K', 'Operation: "1 -> K bit of status register"'),
    'cind': ('K', 'Operation: "... then 1->K illegal index trap condition else 0->K"'),
    'lind': ('K', 'Operation: "... then 1->K illegal index trap condition else 0->K"'),
}

# Where the B30 microcode, decoded and verified, contradicts the manual's list
# for one data type. Ronny's standing ruling: verified microcode wins over the
# manual. Value: {data type: (flags kept, evidence)}.
MICROCODE_KEEPS = {
    'getbf': {'W': (set(DATA_FLAGS),
                    'microcode: WORD GETBF ends GETBFW 000506 -> BFW_END 003431 -> 003436 '
                    '(ST,LOAD + G,OOPS) with no ST,SAVA, so Z/S/C/O are unchanged '
                    '(nd500x fdd6d78, raw decode with the microword CpuND5000)')},
}

# Instructions where the microword CpuND5000 sweep contradicts the manual's list
# and nobody has yet traced the microcode to settle it. No twin until then.
OPEN_ADJUDICATION = {
    'ixi': 'OPEN: ND5000 microcode sweep 2026-09-18 - 48 IXI flag-preset twins (opcodes 0xFCC8, '
           '0xFCCC, 0xFCD0) end with the preset O still set, where the manual lists '
           '"overflow -> O". The IXI_SPEC path ends in IXISPEC_5 with ST,ACCA ("save and '
           'accumulate": O is ORed on) after an ST,SAVA; needs a per-microword trace',
}

# Instructions that move the whole status register: no twin can be right
# without modelling the transfer itself.
WHOLE_STATUS = {
    'st1:=': 'manual: "The instruction ST1:= will load the data status bits from the operand."',
    'st1=:': 'manual: "ST1=: store 1st status register" - the stored value is the flags',
}


def strings(obj):
    if isinstance(obj, str):
        yield obj
    elif isinstance(obj, dict):
        for v in obj.values():
            yield from strings(v)
    elif isinstance(obj, list):
        for v in obj:
            yield from strings(v)


def data_status_lines(ins):
    """The manual's Data status bits list for one instruction, or None."""
    text = next((s for s in strings(ins.get('manual_reference', {}))
                 if re.search(r'Data status bits?', s, re.I)), None)
    if text is None:
        return None
    after = re.split(r'\*{0,2}Data status bits?:?\*{0,2}', text, maxsplit=1, flags=re.I)[1]
    lines = []
    for raw in after.split('\n'):
        s = raw.strip()
        if NOISE.match(s.strip('`').strip() or '`'):
            continue   # layout (fences, stray backticks, page footer) never starts or ends the list
        if not s:
            if lines:
                break
            continue
        if (s.startswith('**') or s.startswith('#') or s.startswith('`Example')) and lines:
            break
        lines.append(s)
    return lines


def read_entry(mnemonic, lines):
    """Resolve the manual lines into affected-flag masks and notes."""
    affected_int, affected_float = set(), set()
    notes, unmapped = [], []
    stated = 0
    unaffected = False
    for line in lines:
        body = re.sub(r'^-\s*', '', line).strip().strip('`').strip()
        if not body or NOISE.match(body):
            continue
        if OTHER_INSTRUCTION.match(body) and mnemonic not in WHOLE_STATUS:
            notes.append('skipped a remark about another instruction: "%s..."' % body[:40])
            continue
        if re.fullmatch(r'Unaffected\.?', body, re.I):
            stated += 1
            unaffected = True
            continue
        if re.fullmatch(r'All cleared\.?', body, re.I):
            # "All" data status bits - K is not one (Table 7), so it is kept.
            affected_int.update(STATUS_FLAGS)
            affected_float.update(STATUS_FLAGS)
            stated += 1
            continue
        m = re.search(r'->\s*([A-Za-z0-9]{1,3})\b\s*(\(([^)]*)\))?\s*$', body)
        if not m:
            unmapped.append(body)
            continue
        flag, qual = m.group(1), (m.group(3) or '').strip().lower()
        if flag == 'F0':
            notes.append('"%s" read as FO' % body)
            flag = 'FO'
        elif flag == '0' and 'overflow' in body.lower():
            notes.append('"%s" read as O' % body)
            flag = 'O'
        stated += 1
        if flag in TRAP_STATUS:
            continue
        if flag not in DATA_FLAGS:
            unmapped.append(body)
            continue
        if qual in ('', ):
            affected_int.add(flag)
            affected_float.add(flag)
        elif 'integer' in qual:
            affected_int.add(flag)
        elif 'float' in qual:
            affected_float.add(flag)
        else:
            unmapped.append(body)
    if not stated and not unmapped:
        unmapped.append('(empty list)')
    return affected_int, affected_float, unaffected, notes, unmapped


def cs_string(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def mask(flags):
    names = [f for f in DATA_FLAGS if f in flags]
    return ' | '.join(names) if names else '0'


def main():
    if len(sys.argv) != 2:
        sys.exit('usage: gen_data_status_spec.py <output DataStatusSpec.cs>')
    entries = []
    for path in sorted(glob.glob(os.path.join(YAML_DIR, '*.yaml'))):
        with open(path, encoding='utf-8') as f:
            ins = yaml.safe_load(f)['instruction']
        mnemonic = ins['mnemonic']
        lines = data_status_lines(ins)
        reads = READS_FLAGS.get(mnemonic)
        if reads is None and mnemonic.startswith(READS_FLAGS_PREFIX):
            reads = 'conditional instruction: its action depends on status bits it tests'
        if lines is None:
            entries.append((mnemonic, os.path.basename(path), False, reads, set(), set(), set(),
                            {}, [], 'manual gives no Data status bits list'))
            continue
        ai, af, said_unaffected, notes, unmapped = read_entry(mnemonic, lines)
        kept = set(DATA_FLAGS) if said_unaffected else {'K'}   # K: not a data status bit
        if 'K' in ai or 'K' in af:
            kept.discard('K')                                    # the list says K is written
        if mnemonic in OPERATION_WRITES:
            flag, quote = OPERATION_WRITES[mnemonic]
            ai.add(flag)
            af.add(flag)
            kept.discard(flag)
            notes.append('%s also changed per %s' % (flag, quote))
        per_type = {}
        for dtype, (flags, evidence) in MICROCODE_KEEPS.get(mnemonic, {}).items():
            per_type[dtype] = flags
            notes.append('%s keeps %s per %s' % (dtype, ' '.join(sorted(flags)), evidence))
        mapped = not unmapped
        if mnemonic in WHOLE_STATUS:
            mapped = False
            notes.append('no twin: ' + WHOLE_STATUS[mnemonic])
        if mnemonic in OPEN_ADJUDICATION:
            mapped = False
            notes.append('no twin: ' + OPEN_ADJUDICATION[mnemonic])
        note = '; '.join(notes + ['unresolved: "%s"' % u for u in unmapped])
        entries.append((mnemonic, os.path.basename(path), mapped, reads, ai, af, kept, per_type,
                        lines, note))

    out = []
    w = out.append
    w('//')
    w('// SPDX-License-Identifier: MIT')
    w('// Copyright (c) 1985-2026 Ronny Hansen')
    w('// RetroCore Labs -- https://github.com/RetroCoreLabs')
    w('// RetroCore -- Emulating yesterday\'s technology with today\'s code')
    w('//')
    w('// GENERATED by nd500x tools/gen_data_status_spec.py from the manual text in')
    w('// nd500x docs/instructions/yaml/*.yaml. Do not edit by hand; regenerate.')
    w('//')
    w('using System.Collections.Generic;')
    w('')
    w('namespace Emulated.Tests.ND500.Validation')
    w('{')
    w('    /// <summary>')
    w('    /// What the ND-500 Reference Manual says each instruction does to the data status')
    w('    /// bits Z(5) C(6) S(7) K(8) O(9), from its "Data status bits:" list.')
    w('    /// </summary>')
    w('    /// <remarks>')
    w('    /// The manual (ND-05.009.4 section 6.5.1): "Bits that are set, reset or left unaffected')
    w('    /// are mentioned explicitly. All data status bits not mentioned are reset." K is not a data')
    w('    /// status bit (Table 7) and is kept unless the list names it or the Operation writes it.')
    w('    /// So Unaffected holds the only flags an instruction keeps; AffectedInteger/AffectedFloat are')
    w('    /// the flags its list names, and it clears every other. ManualLines carries ND\'s own lines')
    w('    /// verbatim so every mask can be checked against the source words; Note records any')
    w('    /// reading that is not literal. Mapped is false when a line did not resolve or the')
    w('    /// manual gives no list; ReadsFlags is non-null when the result depends on a flag it reads.')
    w('    /// </remarks>')
    w('    public static class DataStatusSpec')
    w('    {')
    w('        public const uint Z = 1u << 5;')
    w('        public const uint C = 1u << 6;')
    w('        public const uint S = 1u << 7;')
    w('        public const uint K = 1u << 8;')
    w('        public const uint O = 1u << 9;')
    w('        /// <summary>The data status bits a flag-preset twin sets beforehand.</summary>')
    w('        public const uint DataFlags = Z | C | S | K | O;')
    w('')
    w('        /// <summary>One instruction\'s documented data-status behaviour.</summary>')
    w('        public sealed class Entry')
    w('        {')
    w('            public string Mnemonic;')
    w('            public string YamlFile;')
    w('            public bool Mapped;')
    w('            public string ReadsFlags;')
    w('            public uint AffectedInteger;')
    w('            public uint AffectedFloat;')
    w('            public uint Unaffected;')
    w('            /// <summary>Per data type, where verified microcode overrides Unaffected.</summary>')
    w('            public Dictionary<string, uint> UnaffectedByDataType;')
    w('            public string[] ManualLines;')
    w('            public string Note;')
    w('        }')
    w('')
    w('        public static readonly Dictionary<string, Entry> ByMnemonic = new Dictionary<string, Entry>')
    w('        {')
    for mn, yf, mapped, reads, ai, af, kept, per_type, lines, note in entries:
        w('            [%s] = new Entry' % cs_string(mn))
        w('            {')
        w('                Mnemonic = %s,' % cs_string(mn))
        w('                YamlFile = %s,' % cs_string(yf))
        w('                Mapped = %s,' % ('true' if mapped else 'false'))
        w('                ReadsFlags = %s,' % (cs_string(reads) if reads else 'null'))
        w('                AffectedInteger = %s,' % mask(ai))
        w('                AffectedFloat = %s,' % mask(af))
        w('                Unaffected = %s,' % mask(kept))
        if per_type:
            w('                UnaffectedByDataType = new Dictionary<string, uint> { %s },' %
              ', '.join('[%s] = %s' % (cs_string(t), mask(f)) for t, f in sorted(per_type.items())))
        w('                ManualLines = new string[] { %s },' % ', '.join(cs_string(l) for l in lines))
        w('                Note = %s,' % cs_string(note))
        w('            },')
    w('        };')
    w('    }')
    w('}')
    text = '\n'.join(out) + '\n'
    with open(sys.argv[1], 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    n_mapped = sum(1 for e in entries if e[2])
    n_reads = sum(1 for e in entries if e[3])
    n_twin = sum(1 for e in entries if e[2] and not e[3])
    print('%d instructions: %d mapped, %d read flags, %d eligible for twins' %
          (len(entries), n_mapped, n_reads, n_twin))
    n_kept = sum(1 for e in entries if e[2] and not e[3] and e[6] & {'Z', 'C', 'S', 'O'})
    print('  of which %d keep their data status bits ("Unaffected"), %d clear the unlisted ones'
          % (n_kept, n_twin - n_kept))
    for e in entries:
        if not e[2]:
            print('  unmapped %-10s %s' % (e[0], e[9][:110]))


if __name__ == '__main__':
    main()
