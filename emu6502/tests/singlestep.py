#!/usr/bin/env python3
"""Run the SingleStepTests 6502 vectors through tests/singlestep.

Usage:  python3 tests/singlestep.py DIR [OPCODE ...]

DIR holds the files 00.json .. ff.json from
https://github.com/SingleStepTests/65x02/tree/main/6502/v1
Give opcodes in hex (a9 eb 9c ...) to run only those.

The 12 JAM opcodes are skipped: the real chip keeps putting addresses
on the bus while it is locked up, which the test vectors record, but
the emulator simply stops with PC on the JAM.
"""
import json
import os
import subprocess
import sys

JAM = {0x02, 0x12, 0x22, 0x32, 0x42, 0x52, 0x62, 0x72, 0x92, 0xB2, 0xD2, 0xF2}
RUNNER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "singlestep")


def state(s):
    out = [s["pc"], s["s"], s["a"], s["x"], s["y"], s["p"], len(s["ram"])]
    for addr, value in s["ram"]:
        out += [addr, value]
    return out


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    folder = sys.argv[1]
    wanted = [int(a, 16) for a in sys.argv[2:]] or range(256)
    bad = []
    for op in wanted:
        if op in JAM:
            continue
        with open(os.path.join(folder, "%02x.json" % op)) as f:
            tests = json.load(f)
        lines = []
        for t in tests:
            nums = state(t["initial"]) + state(t["final"]) + [len(t["cycles"])]
            lines.append(t["name"].replace(" ", "_") + " " + " ".join(map(str, nums)))
        result = subprocess.run([RUNNER], input="\n".join(lines) + "\n",
                                capture_output=True, text=True)
        print("$%02X: %s" % (op, result.stdout.strip().replace("\n", "\n     ")))
        if result.returncode != 0:
            bad.append(op)
    if bad:
        print("FAILED opcodes: " + " ".join("$%02X" % op for op in bad))
        sys.exit(1)
    print("All opcodes pass.")


if __name__ == "__main__":
    main()
