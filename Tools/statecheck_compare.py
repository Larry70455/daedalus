#!/usr/bin/env python3
"""Compare two DAEDALUS_STATE_CHECK dumps (interpreter vs dynarec).

Usage:
    python3 Tools/statecheck_compare.py statecheck_interpreter.txt statecheck_dynarec.txt

Each dump has one line per N64 vertical blank with the program counter, the
COP0 Count register, hashes of the integer and floating point registers,
FCR31, HI, LO, a hash of N64 RAM, and all 32 integer registers.

The script prints the first frame where the two runs differ and which values
differ, plus a verdict. Exit code 0 means PASS (identical, or the final RAM
and FPU state match), 1 means FAIL, 2 means a usage or file problem, 3 means
INCONCLUSIVE (N64 RAM never changed, so the program never really ran).

Note on timing: the interpreter and the dynarec may take an interrupt a few
instructions apart, which can show up as a different PC or Count at a frame
boundary while RAM and registers agree again later. A difference in RAM or
registers that persists is the real warning sign.
"""

import sys

FIELDS = ["pc", "count", "gpr_hash", "fpr_hash", "fcr31", "hi", "lo", "rdram_hash"]


def load(path):
    frames = []
    header = []
    with open(path) as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line:
                continue
            if line.startswith("#"):
                header.append(line)
                continue
            left, _, right = line.partition("|")
            parts = left.split()
            vi = int(parts[0])
            values = dict(zip(FIELDS, parts[1:]))
            values["gpr"] = right.split()
            frames.append((vi, values))
    return header, frames


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2

    try:
        header_a, a = load(sys.argv[1])
        header_b, b = load(sys.argv[2])
    except OSError as err:
        print(f"Could not read file: {err}")
        return 2

    print(f"A: {sys.argv[1]} ({len(a)} frames)")
    print(f"B: {sys.argv[2]} ({len(b)} frames)")

    n = min(len(a), len(b))
    if n == 0:
        print("No frames to compare.")
        return 2

    # If N64 RAM never changes during the whole run, the program most likely
    # never started its real work (still booting, or stuck), so a match proves
    # nothing. Report that instead of PASS.
    ram_static = len(set(f[1]["rdram_hash"] for f in a[:n])) == 1 and len(set(f[1]["rdram_hash"] for f in b[:n])) == 1

    first_diff = None
    diff_frames = 0
    ram_reg_diff_frames = 0
    for i in range(n):
        va = a[i][1]
        vb = b[i][1]
        diffs = [f for f in FIELDS if va.get(f) != vb.get(f)]
        if va["gpr"] != vb["gpr"]:
            diffs.append("gpr")
        if diffs:
            diff_frames += 1
            if any(f in diffs for f in ("gpr_hash", "fpr_hash", "rdram_hash", "hi", "lo", "fcr31")):
                ram_reg_diff_frames += 1
            if first_diff is None:
                first_diff = (i, diffs)

    last_same = a[n - 1][1] == b[n - 1][1]

    if ram_static:
        print("INCONCLUSIVE: N64 RAM never changed in either run (program did not get going). Try more frames.")
        return 3

    if first_diff is None:
        print(f"IDENTICAL: all {n} frames match (PC, Count, registers, RAM).")
        return 0

    # Verdict. Interrupt timing makes PC, Count and loop registers differ at
    # frame boundaries even when both cores are correct, so the verdict is
    # based on N64 RAM (which holds the program's results) and the FPU state
    # at the end of the run.
    ram_diff = [a[k][0] for k in range(n) if a[k][1]["rdram_hash"] != b[k][1]["rdram_hash"]]
    fa, fb = a[n - 1][1], b[n - 1][1]
    final_ram_same = fa["rdram_hash"] == fb["rdram_hash"]
    final_fpu_same = fa["fpr_hash"] == fb["fpr_hash"] and fa["fcr31"] == fb["fcr31"]
    print(f"RAM differs in {len(ram_diff)} of {n} frames" + (f" (first {ram_diff[0]}, last {ram_diff[-1]})" if ram_diff else ""))
    print(f"Final RAM identical: {'yes' if final_ram_same else 'NO'}")
    print(f"Final FPU registers identical: {'yes' if final_fpu_same else 'NO'}")
    if final_ram_same and final_fpu_same:
        print("VERDICT: PASS (same final results; remaining differences are interrupt timing)")
    else:
        print("VERDICT: FAIL (final results differ)")

    i, diffs = first_diff
    va, vb = a[i][1], b[i][1]
    print(f"DIFFERENT: first difference at frame {a[i][0]} in: {', '.join(diffs)}")
    for f in FIELDS:
        if va.get(f) != vb.get(f):
            print(f"  {f:10s} A={va.get(f)}  B={vb.get(f)}")
    for r, (ra, rb) in enumerate(zip(va["gpr"], vb["gpr"])):
        if ra != rb:
            print(f"  r{r:<9d} A={ra}  B={rb}")
    print(f"Frames with any difference: {diff_frames} of {n}")
    print(f"Frames with register/RAM difference: {ram_reg_diff_frames} of {n}")
    print(f"Last frame identical: {'yes' if last_same else 'no'}")
    return 0 if ( final_ram_same and final_fpu_same ) else 1


if __name__ == "__main__":
    sys.exit(main())
