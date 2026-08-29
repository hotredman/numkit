#!/usr/bin/env python3
"""Dual-run harness: real scripts through MATLAB R2025b vs numkit (WASM + native).

  python harness.py harvest [N]     select deterministic self-contained scripts
  python harness.py run [N] [flt]   run N candidates (optional path filter)
  python harness.py used-fns <report-json>   list absent fns from a report

Everything runs from corpus/work (the UTF-8-transcoded mirror prepared by
fetch.py). Reports land in reports/<timestamp>.json. Verdict meanings and the
bug-filing workflow: README.md.
"""
import json
import re
import subprocess
import sys
import time
from collections import Counter
from datetime import datetime
from pathlib import Path

HERE = Path(__file__).parent
WORK = HERE / "corpus" / "work"
REPORTS = HERE / "reports"
NUMKIT = HERE.parent
WASM_CLI = NUMKIT / "packages" / "numkit" / "bin" / "cli.js"
NATIVE_CLI = NUMKIT / "build" / "desktop-fast" / "apps" / "numkit" / "Release" / "numkit_repl.exe"
MATLAB = Path(r"C:\Program Files\MATLAB\R2025b\bin\matlab.exe")

BAD_TOKENS = ["input(", "urlread", "system(", "java.", "actxserver", "parfor",
              "gpuArray", "sym(", "sim(", "mex", "waitfor", "pause(", "tic",
              "clock", "now,", "datenum", "websave", "webread", "video",
              "aviread", "VideoReader"]
NEED_OUTPUT = ("disp(", "fprintf(", "sprintf(")


def harvest(limit=60):
    picked, seen = [], set()
    for repo in sorted(WORK.iterdir()):
        if not repo.is_dir():
            continue
        for m in repo.rglob("*.m"):
            try:
                txt = m.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            if txt.lstrip().startswith("function"):
                continue
            if m.stat().st_size > 40_000 or txt.count("\n") > 500:
                continue
            low = txt.lower()
            if any(b.lower() in low for b in BAD_TOKENS):
                continue
            if not any(o in txt for o in NEED_OUTPUT):
                continue
            if m.name in seen:
                continue
            seen.add(m.name)
            picked.append(str(m))
            if len(picked) >= limit:
                break
        if len(picked) >= limit:
            break
    (HERE / "candidates.json").write_text(json.dumps(picked, indent=1))
    print(f"harvested {len(picked)} candidates")


def run_engine(kind, script):
    s = Path(script)
    cwd = str(s.parent)
    t0 = time.perf_counter()
    argv = ([str(MATLAB), "-batch", f"run('{s.as_posix()}')"] if kind == "matlab" else
            ["node", str(WASM_CLI), s.name] if kind == "wasm" else
            [str(NATIVE_CLI), s.name])
    to = 180 if kind == "matlab" else 90
    try:
        p = subprocess.run(argv, cwd=cwd, capture_output=True, timeout=to)
        ms = int((time.perf_counter() - t0) * 1000)
        dec = lambda b: (b or b"").decode("utf-8", "replace")
        return {"exit": p.returncode, "out": dec(p.stdout), "err": dec(p.stderr), "ms": ms}
    except subprocess.TimeoutExpired:
        return {"exit": "timeout", "out": "", "err": "", "ms": to * 1000}


NUM = re.compile(r"-?\d+\.?\d*(?:[eE][+-]?\d+)?")


def normalise(text, script):
    t = (text or "").replace("\r", "")
    t = t.replace(str(Path(script).parent), "<DIR>").replace(Path(script).parent.as_posix(), "<DIR>")
    t = re.sub(r"[ \t]+\n", "\n", t)
    t = re.sub(r"\n{2,}", "\n", t)
    # Console codepages make CJK prose incomparable across engines (MATLAB on
    # a non-matching locale garbles UTF-8 on OUTPUT); fold non-ASCII runs to a
    # single marker so numbers and structure stay strictly compared.
    t = re.sub(r"[^\x20-\x7e]+", " # ", t)
    # Consecutive markers (mojibake runs differ per engine) and column-padding
    # spaces collapse to single tokens.
    t = re.sub(r"(#\s*)+", "# ", t)
    t = re.sub(r"[ \t]{2,}", " ", t)
    return t.strip()


def fuzzy_equal(a, b):
    la, lb = a.splitlines(), b.splitlines()
    if len(la) != len(lb):
        return False, f"line-count {len(la)} vs {len(lb)}"
    for i, (ra, rb) in enumerate(zip(la, lb)):
        ta, tb = NUM.split(ra), NUM.split(rb)
        na, nb = NUM.findall(ra), NUM.findall(rb)
        if ta != tb:
            return False, f"line {i+1} text differs: {ra[:60]!r} vs {rb[:60]!r}"
        if len(na) != len(nb):
            return False, f"line {i+1} numeric count {len(na)} vs {len(nb)}"
        for x, y in zip(na, nb):
            fx, fy = float(x), float(y)
            if fx != fy and abs(fx - fy) > 1e-9 + 1e-6 * max(abs(fx), abs(fy)):
                return False, f"line {i+1} value {x} vs {y}"
    return True, ""


def classify(nk, ml):
    if ml["exit"] == "timeout":
        return "matlab-timeout", ""
    if nk["exit"] == "timeout":
        return "numkit-hang", ""
    nk_err, ml_err = nk["exit"] != 0, ml["exit"] != 0
    if ml_err:
        detail = (ml["err"] or ml["out"]).strip()[:120]
        if nk_err:
            return "both-error", detail
        return "matlab-only-error", detail
    if nk_err:
        msg = (nk["err"] or nk["out"]).strip()
        if "undefined function" in msg.lower():
            return "absent-fn", msg[:160]
        if "parse error" in msg.lower():
            return "parse-error", msg[:160]
        if "not supported" in msg.lower():
            return "unsupported", msg[:160]
        return "runtime-error", msg[:160]
    eq, why = fuzzy_equal(normalise(nk["out"], nk.get("script", "")),
                          normalise(ml["out"], ""))
    return ("pass" if eq else "output-mismatch"), why


def run(cands):
    REPORTS.mkdir(exist_ok=True)
    results = []
    for i, s in enumerate(cands):
        rel = str(Path(s).relative_to(HERE))
        wasm = run_engine("wasm", s)
        wasm["script"] = s
        if (run_engine("wasm", s)["out"] or "") != (wasm["out"] or ""):
            v, d = "nondeterministic", ""
        else:
            ml = run_engine("matlab", s)
            native = run_engine("native", s)
            v, d = classify(wasm, ml)
        r = {"script": rel, "verdict": v, "detail": d,
             "wasm_ms": wasm["ms"],
             "matlab_ms": ml["ms"] if v != "nondeterministic" else None,
             "native_ms": native["ms"] if v != "nondeterministic" else None,
             "numkit_out": (wasm["out"] or wasm["err"])[:800],
             "matlab_out": (ml["out"] or ml["err"])[:800] if v != "nondeterministic" else ""}
        results.append(r)
        print(f"[{i+1}/{len(cands)}] {Path(rel).name:<40} {v:<18} "
              f"wasm={r['wasm_ms']}ms matlab={r['matlab_ms']}ms native={r['native_ms']}ms")
    out = REPORTS / f"{datetime.now():%Y%m%d-%H%M%S}.json"
    out.write_text(json.dumps(results, indent=1, ensure_ascii=False))
    print(f"\nreport: {out}")
    print("=== SUMMARY ===")
    for k, n in Counter(r["verdict"] for r in results).most_common():
        print(f"{n:>4}  {k}")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "run"
    if cmd == "harvest":
        harvest(int(sys.argv[2]) if len(sys.argv) > 2 else 60)
    elif cmd == "used-fns":
        for r in json.loads(Path(sys.argv[2]).read_text(encoding="utf-8")):
            if r["verdict"] == "absent-fn":
                m = re.search(r"function '([^']+)'", r["detail"])
                if m:
                    print(m.group(1))
    else:
        cands = json.loads((HERE / "candidates.json").read_text())
        if len(sys.argv) > 3:
            cands = [c for c in cands if sys.argv[3].lower() in c.lower()]
        run(cands[: int(sys.argv[2]) if len(sys.argv) > 2 else 25])
