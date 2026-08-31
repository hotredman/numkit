#!/usr/bin/env python3
"""Dual-run harness: real scripts through MATLAB R2025b vs numkit (WASM + native).

  python harness.py harvest [N] [flt]   static candidate selection -> candidates.json
  python harness.py qualify [N] [flt]   MATLAB-verify candidates -> runnable.json
                                        (the committed run corpus — R3)
  python harness.py run [N] [flt]       dual-run runnable.json scripts; compare
                                        workspaces via .mat files (R4) -> reports/
  python harness.py matdiff a.mat b.mat standalone workspace comparator
  python harness.py used-fns <report-json>   list absent fns from a report

Comparison semantics (README "Comparison semantics"): the harness appends
save('<tmp>/out_<engine>.mat') to each run; both .mat files are read by
scipy.io.loadmat (engine-neutral) and numeric variables of the intersection
are compared — class and shape exactly, values at rel 1e-9, NaN==NaN,
±Inf equal to themselves. stdout/exit still drive the parse-error /
runtime-error classes and diagnostics, but the numeric verdict comes from
the workspace; print-format divergences do not count.
"""
import json
import re
import shutil
import subprocess
import sys
import time
from collections import Counter
from datetime import datetime
from pathlib import Path

import numpy as np
from scipy.io import loadmat

HERE = Path(__file__).parent
WORK = HERE / "corpus" / "work"
REPORTS = HERE / "reports"
RUNNABLE = HERE / "runnable.json"
CANDIDATES = HERE / "candidates.json"
TMP = HERE / "corpus" / "tmp"
CATALOG_CACHE = HERE / "corpus" / "catalog.json"
NUMKIT = HERE.parent
WASM_CLI = NUMKIT / "packages" / "numkit" / "bin" / "cli.js"
NATIVE_CLI = NUMKIT / "build" / "desktop-fast" / "apps" / "numkit" / "Release" / "numkit_repl.exe"
MATLAB = Path(r"C:\Program Files\MATLAB\R2025b\bin\matlab.exe")

REL_TOL = 1e-9

BAD_TOKENS = ["input(", "urlread", "system(", "java.", "actxserver", "parfor",
              "gpuArray", "sym(", "sim(", "mex", "waitfor", "pause(", "tic",
              "clock", "now,", "datenum", "websave", "webread", "video",
              "aviread", "VideoReader"]
# Graphics calls are NOT filtered: both engines execute them headless
# (numkit runs its graphics system and emits __FIGURE_DATA__ for the IDE;
# MATLAB -batch creates invisible figures) and the verdict comes from the
# workspace .mat comparison (R4), to which plots are irrelevant. Excluding
# plotting scripts was a stdout-comparison-era restriction.


# ── Static harvest ──────────────────────────────────────────────────────────
# Scripts do NOT need to print anything (R4: the workspace is the result) —
# the empirical MATLAB gate in qualify() is what decides runnability.

def harvest(limit=None, flt=None):
    picked, seen = [], set()
    for repo in sorted(WORK.iterdir()):
        if not repo.is_dir():
            continue
        for m in repo.rglob("*.m"):
            try:
                txt = m.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            # function files may open with a comment block; MATLAB treats the
            # file as a function iff the first non-comment code is `function`
            first_code = re.sub(r"%[^\n]*\n", "", txt.lstrip())
            if first_code.lstrip().startswith("function"):
                continue
            if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*\.m", m.name):
                continue
            if m.stat().st_size > 40_000 or txt.count("\n") > 500:
                continue
            low = txt.lower()
            if any(b.lower() in low for b in BAD_TOKENS):
                continue
            if flt and flt.lower() not in str(m).lower():
                continue
            if m.name in seen:
                continue
            seen.add(m.name)
            picked.append(str(m))
            if limit is not None and len(picked) >= limit:
                break
        if limit is not None and len(picked) >= limit:
            break
    CANDIDATES.write_text(json.dumps(picked, indent=1), encoding="utf-8")
    print(f"harvested {len(picked)} candidates -> {CANDIDATES.name}")
    return picked


# ── Engine runs (each optionally saving its workspace to a .mat) ────────────

def _tmpdir(script):
    d = TMP / re.sub(r"[^A-Za-z0-9_.-]", "_", str(Path(script).relative_to(WORK)))[:180]
    if d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True)
    return d


def run_engine(kind, script, mat_out=None):
    """Run `script`; if mat_out is given, save the workspace there after it.

    cwd stays the script's directory so sibling data files resolve (MATLAB
    CLI semantics); the .mat target is absolute so nothing pollutes the
    corpus. numkit kinds run a generated wrapper (`run(...); save(...)`)
    because the CLI executes exactly one file per invocation.
    """
    s = Path(script)
    cwd = str(s.parent)
    t0 = time.perf_counter()
    if kind == "matlab":
        argv = [str(MATLAB), "-batch",
                f"run('{s.as_posix()}')" + (f"; save('{Path(mat_out).as_posix()}')" if mat_out else "")]
    elif kind in ("wasm", "native"):
        if mat_out is None:
            argv = (["node", str(WASM_CLI), s.name] if kind == "wasm"
                    else [str(NATIVE_CLI), s.name])
        else:
            wrap = s.parent / f"__nk_wrap_{kind}.m"
            wrap.write_text(f"run('{s.as_posix()}');\nsave('{Path(mat_out).as_posix()}');\n",
                            encoding="utf-8")
            argv = (["node", str(WASM_CLI), wrap.name] if kind == "wasm"
                    else [str(NATIVE_CLI), wrap.name])
    else:
        raise ValueError(kind)
    to = 180 if kind == "matlab" else 90
    try:
        p = subprocess.run(argv, cwd=cwd, capture_output=True, timeout=to)
        ms = int((time.perf_counter() - t0) * 1000)
        dec = lambda b: (b or b"").decode("utf-8", "replace")
        return {"exit": p.returncode, "out": dec(p.stdout), "err": dec(p.stderr), "ms": ms}
    except subprocess.TimeoutExpired:
        return {"exit": "timeout", "out": "", "err": "", "ms": to * 1000}
    finally:
        if kind in ("wasm", "native") and mat_out is not None:
            wrap.unlink(missing_ok=True)


# ── Workspace comparison (R4) ───────────────────────────────────────────────

def _numeric_kind(arr):
    """Class bucket for comparison: r(eal numeric) / c(omplex) / b(ool).

    Real floats and ints share one bucket: MATLAB R2025b packs integral-
    valued doubles into the smallest integer type when SAVING (v6 and v7
    alike) and promotes back on load — the stored class is unreliable, so
    only shape + values are compared for reals (dtype-level class would
    false-positive on every integral double).
    """
    k = arr.dtype.kind
    if k == "b":
        return "b"
    if k == "c":
        return "c"
    if k in "fiu":
        return "r"
    return None  # char / object (cell) / struct: non-comparable


def _values_close(a, b):
    if a.dtype.kind == "b":
        return bool(np.array_equal(a, b))
    a = np.asarray(a, dtype=np.complex128 if "c" in (a.dtype.kind, b.dtype.kind) else np.float64)
    b = np.asarray(b, dtype=a.dtype)
    both_nan = np.isnan(a) & np.isnan(b)
    close = np.abs(a - b) <= 1e-9 + REL_TOL * np.maximum(np.abs(a), np.abs(b))
    return bool(np.all(close | both_nan))


def matdiff(path_a, path_b):
    """Compare numeric variables of two .mat files.

    Returns (ok, lines): ok=False iff a shared numeric variable diverges in
    class bucket (real/complex/logical — see _numeric_kind), shape, or
    values (rel 1e-9; NaN==NaN). Missing-on-one-side and non-numeric
    variables are reported as info, not failures.
    """
    a = {k: v for k, v in loadmat(str(path_a)).items() if not k.startswith("__")}
    b = {k: v for k, v in loadmat(str(path_b)).items() if not k.startswith("__")}
    lines, ok = [], True
    for name in sorted(set(a) & set(b)):
        ka, kb = _numeric_kind(a[name]), _numeric_kind(b[name])
        if ka is None and kb is None:
            lines.append(f"skip {name}: non-numeric both sides")
            continue
        if ka is None or kb is None:
            lines.append(f"skip {name}: numeric on one side only ({ka} vs {kb})")
            continue
        if ka != kb:
            ok = False
            lines.append(f"DIFF {name}: class {ka} vs {kb}")
            continue
        va, vb = np.asarray(a[name]), np.asarray(b[name])
        if va.shape != vb.shape:
            ok = False
            lines.append(f"DIFF {name}: shape {va.shape} vs {vb.shape}")
            continue
        if not _values_close(va, vb):
            with np.errstate(invalid="ignore"):
                rel = np.nanmax(np.abs(va - vb) / np.maximum(1e-300, np.maximum(np.abs(va), np.abs(vb))))
            lines.append(f"DIFF {name}: values diverge (max rel {rel:.3e})")
            ok = False
    only_a = sorted(set(a) - set(b))
    only_b = sorted(set(b) - set(a))
    if only_a:
        lines.append(f"only-in-{'numkit' if 'numkit' in str(path_a) else 'A'}: {', '.join(only_a[:8])}")
    if only_b:
        lines.append(f"only-in-{'matlab' if 'matlab' in str(path_b) else 'B'}: {', '.join(only_b[:8])}")
    return ok, lines


# ── Verdicts ────────────────────────────────────────────────────────────────

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


def stdout_equal(a, b):
    la, lb = a.splitlines(), b.splitlines()
    if len(la) != len(lb):
        return False
    for ra, rb in zip(la, lb):
        ta, tb = NUM.split(ra), NUM.split(rb)
        na, nb = NUM.findall(ra), NUM.findall(rb)
        if ta != tb or len(na) != len(nb):
            return False
        for x, y in zip(na, nb):
            fx, fy = float(x), float(y)
            if fx != fy and abs(fx - fy) > 1e-9 + 1e-6 * max(abs(fx), abs(fy)):
                return False
    return True


def classify(nk, ml):
    """Exit-code-driven classes; the numeric verdict itself is the matdiff."""
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
    return None, ""  # both exited 0: verdict decided by the workspace diff


# ── Qualify (R3): MATLAB-verified run corpus ───────────────────────────────

def qualify(limit=None, flt=None):
    """MATLAB-verify candidates in PORTIONS; results merge into runnable.json.

    Each run qualifies only the selected slice (path filter = repo dir, e.g.
    `qualify 50 mdadams--`); verified paths replace their old entries, other
    entries persist — portions accumulate instead of restarting the catalog.
    """
    cands = harvest(limit, flt)
    # Merge base: previous portions survive; entries for vanished scripts drop.
    prev = {}
    if RUNNABLE.exists():
        for e in json.loads(RUNNABLE.read_text(encoding="utf-8"))["scripts"]:
            if (HERE / e["path"]).exists():
                prev[e["path"]] = e
    qualified = []
    for i, s in enumerate(cands):
        r = run_engine("matlab", s)  # no save: runnability check only
        rel = str(Path(s).relative_to(HERE))
        if r["exit"] == 0:
            qualified.append({"path": rel, "matlab_ms": r["ms"]})
            prev.pop(rel, None)
        mark = "ok " if r["exit"] == 0 else f"exit={r['exit']}"
        print(f"[{i+1}/{len(cands)}] {Path(s).name:<44} {mark} {r['ms']}ms")
    merged = sorted([{"path": p, **{"matlab_ms": e["matlab_ms"]}} for p, e in prev.items()] + qualified,
                    key=lambda e: e["path"])
    doc = {"qualified": f"{datetime.now():%Y-%m-%dT%H:%M:%S}",
           "scripts": merged}
    if CATALOG_CACHE.exists():
        meta = json.loads(CATALOG_CACHE.read_text(encoding="utf-8")).get("metadata", {})
        doc["catalog_compiled"] = meta.get("compiled_date")
    RUNNABLE.write_text(json.dumps(doc, indent=1, ensure_ascii=False), encoding="utf-8")
    print(f"\nrunnable.json: +{len(qualified)}/{len(cands)} verified this portion, "
          f"{len(merged)} total (catalog {doc.get('catalog_compiled', '?')})")


# ── Dual run with .mat comparison (R4) ──────────────────────────────────────

def run(scripts):
    REPORTS.mkdir(exist_ok=True)
    results = []
    for i, s in enumerate(scripts):
        s = str((HERE / s) if not Path(s).is_absolute() else Path(s))
        rel = str(Path(s).relative_to(HERE))
        d = _tmpdir(s)
        wasm = run_engine("wasm", s, mat_out=d / "out_wasm.mat")
        wasm2 = run_engine("wasm", s, mat_out=d / "out_wasm2.mat")
        det_ok, det_lines = True, ["(probe unavailable)"]
        if (d / "out_wasm.mat").exists() and (d / "out_wasm2.mat").exists():
            det_ok, det_lines = matdiff(d / "out_wasm.mat", d / "out_wasm2.mat")
        native = run_engine("native", s, mat_out=d / "out_native.mat")
        ml = run_engine("matlab", s, mat_out=d / "out_matlab.mat")

        v, detail = classify(wasm, ml)
        mat_ok, mat_lines, stdout_ok = None, [], None
        if v is None:
            stdout_ok = stdout_equal(normalise(wasm["out"], s), normalise(ml["out"], s))
            if not det_ok:
                v, detail = "nondeterministic", "; ".join(det_lines)[:160]
            elif (d / "out_wasm.mat").exists() and (d / "out_matlab.mat").exists():
                mat_ok, mat_lines = matdiff(d / "out_wasm.mat", d / "out_matlab.mat")
                v = "pass" if mat_ok else "workspace-mismatch"
                detail = "; ".join(mat_lines[:6])[:400]
            else:
                v, detail = "runtime-error", "no workspace .mat produced"
        r = {"script": rel, "verdict": v, "detail": detail,
             "wasm_ms": wasm["ms"], "matlab_ms": ml["ms"], "native_ms": native["ms"],
             "stdout_match": stdout_ok,
             "mat_diff": mat_lines[:20],
             "numkit_out": (wasm["out"] or wasm["err"])[:800],
             "matlab_out": (ml["out"] or ml["err"])[:800]}
        results.append(r)
        print(f"[{i+1}/{len(scripts)}] {Path(rel).name:<40} {v:<20} "
              f"wasm={r['wasm_ms']}ms matlab={r['matlab_ms']}ms native={r['native_ms']}ms")
        shutil.rmtree(d, ignore_errors=True)
    out = REPORTS / f"{datetime.now():%Y%m%d-%H%M%S}.json"
    out.write_text(json.dumps(results, indent=1, ensure_ascii=False), encoding="utf-8")
    print(f"\nreport: {out}")
    print("=== SUMMARY ===")
    for k, n in Counter(r["verdict"] for r in results).most_common():
        print(f"{n:>4}  {k}")


def load_runnable(n, flt):
    if not RUNNABLE.exists():
        sys.exit("no runnable.json — run `python harness.py qualify` first (R3)")
    doc = json.loads(RUNNABLE.read_text(encoding="utf-8"))
    scripts = [s["path"] for s in doc["scripts"]]
    if flt:
        scripts = [s for s in scripts if flt.lower() in s.lower()]
    return scripts[:n] if n else scripts


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "run"
    n = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].isdigit() else None
    flt = sys.argv[3] if len(sys.argv) > 3 else None
    if cmd == "harvest":
        harvest(n, flt)
    elif cmd == "qualify":
        qualify(n, flt)
    elif cmd == "run":
        run(load_runnable(n, flt))
    elif cmd == "matdiff":
        ok, lines = matdiff(sys.argv[2], sys.argv[3])
        print("\n".join(lines) or "identical (numeric intersection)")
        sys.exit(0 if ok else 1)
    elif cmd == "used-fns":
        for r in json.loads(Path(sys.argv[2]).read_text(encoding="utf-8")):
            if r["verdict"] == "absent-fn":
                m = re.search(r"function '([^']+)'", r["detail"])
                if m:
                    print(m.group(1))
    else:
        sys.exit(__doc__)
