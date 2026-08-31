#!/usr/bin/env python3
"""Comparison groups (R5): run a curated group of .m scripts through numkit
AND MATLAB, print a human-readable divergence report.

  python compare.py --list         show available groups
  python compare.py <group>        dual-run the group
  python compare.py <group> -v     also print per-variable diff lines

Groups live in compare/groups/<name>.txt: one script path (relative to
fieldtest/) per line, optional trailing `# known: <bug-id>` marking a
divergence that is already filed. The runner reports:

  PASS        workspaces match (R4 .mat comparison)
  KNOWN (id)  diverges as filed — bugs/opened/<id>.md
  NEW <what>  divergence NOT in the catalog — this is what to act on

Exit code: 1 if any NEW divergence, 0 otherwise. KNOWN-only runs exit 0
(a filed bug is tracked work, not a surprise).

Reuses harness.py machinery (run_engine / matdiff / classify) — one
comparison implementation, two front-ends.
"""
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE))
import harness as H  # noqa: E402

GROUPS = HERE / "compare" / "groups"


def parse_group(path):
    entries = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        known = None
        if "#" in line:
            head, tail = line.split("#", 1)
            known = tail.split("known:", 1)[1].strip() if "known:" in tail else None
            line = head.strip()
        entries.append((line, known))
    return entries


def verdict_of(script):
    """One dual-run; returns (verdict, detail, diff_lines)."""
    s = str(script)
    d = H._tmpdir(s)
    try:
        wasm = H.run_engine("wasm", s, mat_out=d / "out_wasm.mat")
        ml = H.run_engine("matlab", s, mat_out=d / "out_matlab.mat")
        v, detail = H.classify(wasm, ml)
        lines = []
        if v is None:
            wasm2 = H.run_engine("wasm", s, mat_out=d / "out_wasm2.mat")
            if (d / "out_wasm.mat").exists() and (d / "out_wasm2.mat").exists():
                ok, det = H.matdiff(d / "out_wasm.mat", d / "out_wasm2.mat")
                if not ok:
                    return "nondeterministic", "; ".join(det)[:120], det
            if (d / "out_wasm.mat").exists() and (d / "out_matlab.mat").exists():
                ok, lines = H.matdiff(d / "out_wasm.mat", d / "out_matlab.mat")
                verdict = "pass" if ok else "workspace-mismatch"
                return verdict, "; ".join(lines[:4])[:160], lines
            return "runtime-error", "no workspace .mat produced", lines
        return v, detail, lines
    finally:
        shutil.rmtree(d, ignore_errors=True)


def main():
    argv = [a for a in sys.argv[1:]]
    verbose = "-v" in argv
    argv = [a for a in argv if a != "-v"]
    if not argv or argv[0] == "--list":
        if not GROUPS.exists():
            sys.exit("no groups directory yet")
        for g in sorted(GROUPS.glob("*.txt")):
            n = len(parse_group(g))
            print(f"{g.stem:<24} {n} scripts")
        return
    name = argv[0]
    gfile = GROUPS / f"{name}.txt"
    if not gfile.exists():
        sys.exit(f"no such group: {name} (see --list)")
    entries = parse_group(gfile)
    counts = {"pass": 0, "known": 0, "new": 0}
    print(f"=== group {name}: {len(entries)} scripts ===")
    for i, (rel, known) in enumerate(entries):
        s = HERE / rel
        if not s.exists():
            print(f"[{i+1}] {Path(rel).name:<38} MISSING FILE ({rel})")
            counts["new"] += 1
            continue
        v, detail, lines = verdict_of(s)
        if v == "pass":
            counts["pass"] += 1
            mark = "PASS"
        elif known:
            counts["known"] += 1
            mark = f"KNOWN ({known})"
        else:
            counts["new"] += 1
            mark = "NEW"
        print(f"[{i+1}/{len(entries)}] {Path(rel).name:<38} {mark:<44} "
              f"{v}: {detail.splitlines()[0][:80] if detail else ''}")
        if verbose and lines:
            for l in lines[:10]:
                print(f"      {l}")
    print(f"\n{counts['pass']} pass, {counts['known']} known (filed), {counts['new']} NEW")
    sys.exit(1 if counts["new"] else 0)


if __name__ == "__main__":
    main()
