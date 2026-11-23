#!/usr/bin/env python3
"""Replace named cases of test/nd500-conformance.json with cases written by the generator.

The generator is RetroCore's ComprehensiveTestExporter, run through the RetroCore test
TestND500_DualWriter.DualWrite_ConformanceCases with ND500_CONFORMANCE_OUT and
ND500_CONFORMANCE_CASES set. That writes a JSON array of the named cases. This script puts each
of them in place of the case with the same name in the conformance file (same position), and
fails when a name is not in the conformance file exactly once. Nothing is edited by hand.

Usage: tools/conformance_replace_cases.py GENERATED.json [test/nd500-conformance.json]
"""
import json
import sys


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    generated_path = argv[1]
    target_path = argv[2] if len(argv) > 2 else "test/nd500-conformance.json"

    with open(generated_path, encoding="utf-8-sig") as f:
        generated = json.load(f)
    with open(target_path, encoding="utf-8") as f:
        target = json.load(f)

    for case in generated:
        name = case["name"]
        where = [i for i, t in enumerate(target) if t.get("name") == name]
        if len(where) != 1:
            print(f"case '{name}' occurs {len(where)} times in {target_path}; nothing written")
            return 1
        target[where[0]] = case
        print(f"recorded again: {name}")

    # The layout of the file as it is: indented by 2, no newline at the end. Loading and
    # writing it back unchanged gives the same bytes, so only the replaced cases change.
    with open(target_path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(target, f, indent=2)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
