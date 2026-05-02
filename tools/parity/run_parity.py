#!/usr/bin/env python3
"""
Parity harness for numkit-m.

Runs a function spec against three engines (numkit native, MATLAB R2025b,
Octave 11.1.0), compares output for correctness, measures wall-clock time,
and appends a row to PARITY_PROGRESS.md.

Usage:
    python tools/parity/run_parity.py specs/erf.json
    python tools/parity/run_parity.py --all
    python tools/parity/run_parity.py specs/erf.json --no-matlab   # skip MATLAB
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

# On Windows the default cp1252 console blows up on non-ASCII; force UTF-8.
if sys.stdout.encoding and sys.stdout.encoding.lower() != "utf-8":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent.parent.parent
NUMKIT_EXE = ROOT / "build-desktop-fast" / "Release" / "numkit_example.exe"
MATLAB_EXE = "matlab"  # on PATH
OCTAVE_EXE = r"C:\Program Files\GNU Octave\Octave-11.1.0\mingw64\bin\octave-cli.exe"
PROGRESS_MD = ROOT / "PARITY_PROGRESS.md"


# ───────────────────────────── spec ──────────────────────────────────

@dataclass
class Spec:
    """Single-function parity spec.

    `setup`  — MATLAB-compatible code that defines input variables.
    `expr`   — the call under test, e.g. `y = erf(x);`. Must be MATLAB-
               compatible (we run the same code in all three engines).
    `iters`  — bench loop iterations.
    `tol`    — tolerance for fingerprint comparison (relative for non-tiny
               values, absolute fallback when |v| < 1).
    `fingerprint` — list of MATLAB expressions producing scalars that
                    summarize the result. Default: sum, first, mid, last.
                    Override for non-numeric output (strings, cells).
    `comment` — free-form note carried into the progress row.
    """
    name: str
    namespace: str
    setup: str
    expr: str
    iters: int = 10
    tol: float = 1e-9
    fingerprint: list[str] = field(default_factory=list)
    comment: str = ""

    @classmethod
    def from_json(cls, path: Path) -> "Spec":
        data = json.loads(path.read_text(encoding="utf-8"))
        return cls(**data)


# ────────────────────────── script builder ───────────────────────────

def build_script(spec: Spec, *, timed: bool) -> str:
    """Build a MATLAB/Octave/numkit-compatible script.

    timed=True   → run warmup + iters loop, print elapsed_ms then fingerprint.
    timed=False  → just produce fingerprint (used for correctness reference).
    """
    fp_exprs = spec.fingerprint or [
        f"sum({spec.expr.split('=')[0].strip()}(:))",
        f"{spec.expr.split('=')[0].strip()}(1)",
        f"{spec.expr.split('=')[0].strip()}(end)",
    ]
    fp_print = "\n".join(
        f"fprintf('FP %d %.17g\\n', {i}, double({e}));"
        for i, e in enumerate(fp_exprs)
    )

    if timed:
        return (
            f"{spec.setup}\n"
            f"{spec.expr}\n"  # warmup
            f"t0 = tic;\n"
            f"for kk__ = 1:{spec.iters}\n"
            f"    {spec.expr}\n"
            f"end\n"
            f"elapsed_ms = toc(t0) * 1000.0 / {spec.iters};\n"
            f"fprintf('TIMING %.6f\\n', elapsed_ms);\n"
            f"{fp_print}\n"
        )
    else:
        return f"{spec.setup}\n{spec.expr}\n{fp_print}\n"


# ───────────────────────── engine runners ────────────────────────────

@dataclass
class Result:
    ok: bool
    elapsed_ms: float | None = None
    fingerprint: list[float] = field(default_factory=list)
    raw_stdout: str = ""
    raw_stderr: str = ""
    error: str = ""


def parse_output(out: str) -> tuple[float | None, list[float]]:
    timing = None
    fps: dict[int, float] = {}
    for line in out.splitlines():
        m = re.match(r"^TIMING\s+(\S+)$", line.strip())
        if m:
            timing = float(m.group(1))
        m = re.match(r"^FP\s+(\d+)\s+(\S+)$", line.strip())
        if m:
            fps[int(m.group(1))] = float(m.group(2))
    fp_list = [fps[i] for i in sorted(fps.keys())]
    return timing, fp_list


def run_numkit(spec: Spec, *, timed: bool) -> Result:
    if not NUMKIT_EXE.exists():
        return Result(ok=False, error=f"numkit binary missing: {NUMKIT_EXE}")
    script = build_script(spec, timed=timed)
    with tempfile.NamedTemporaryFile("w", suffix=".m", delete=False, encoding="utf-8") as f:
        f.write(script)
        path = f.name
    try:
        p = subprocess.run([str(NUMKIT_EXE), path], capture_output=True,
                           text=True, timeout=120)
    finally:
        os.unlink(path)
    timing, fp = parse_output(p.stdout)
    return Result(
        ok=(p.returncode == 0 and (not timed or timing is not None) and len(fp) > 0),
        elapsed_ms=timing,
        fingerprint=fp,
        raw_stdout=p.stdout,
        raw_stderr=p.stderr,
        error="" if p.returncode == 0 else f"exit {p.returncode}",
    )


def run_matlab(spec: Spec, *, timed: bool) -> Result:
    script = build_script(spec, timed=timed)
    cmd = [MATLAB_EXE, "-batch", script]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
    timing, fp = parse_output(p.stdout)
    return Result(
        ok=(p.returncode == 0 and (not timed or timing is not None) and len(fp) > 0),
        elapsed_ms=timing,
        fingerprint=fp,
        raw_stdout=p.stdout,
        raw_stderr=p.stderr,
        error="" if p.returncode == 0 else f"exit {p.returncode}",
    )


def run_octave(spec: Spec, *, timed: bool) -> Result:
    script = build_script(spec, timed=timed)
    cmd = [OCTAVE_EXE, "--no-gui", "--eval", script]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
    timing, fp = parse_output(p.stdout)
    return Result(
        ok=(p.returncode == 0 and (not timed or timing is not None) and len(fp) > 0),
        elapsed_ms=timing,
        fingerprint=fp,
        raw_stdout=p.stdout,
        raw_stderr=p.stderr,
        error="" if p.returncode == 0 else f"exit {p.returncode}",
    )


# ──────────────────────── correctness check ──────────────────────────

def fp_close(a: list[float], b: list[float], tol: float) -> bool:
    if len(a) != len(b):
        return False
    for x, y in zip(a, b):
        if x != x and y != y:  # both NaN
            continue
        diff = abs(x - y)
        scale = max(abs(x), abs(y), 1.0)
        if diff > tol * scale:
            return False
    return True


def fp_str(fp: list[float]) -> str:
    return "[" + ", ".join(f"{x:.6g}" for x in fp) + "]"


# ─────────────────────────── log writer ──────────────────────────────

PROGRESS_HEADER = """# numkit-m parity progress

Auto-generated by `tools/parity/run_parity.py`. Append-only journal of
the autonomous parity cycle started 2026-05-03. The TODO list (`MATLAB_PARITY_TODO.md`)
remains the source of truth for "what's missing"; this file is the journal
of work + measurements.

**Columns:**
- `function` — MATLAB-doc name (call path under numkit follows the namespace below)
- `namespace` — `core`, `signal.*`, `stats.*`, …
- `status` — DONE / FAIL / SKIP
- `numkit_ms` / `matlab_ms` / `octave_ms` — single iteration mean (ms)
- `vs_MATLAB` — MATLAB_ms / numkit_ms (>1 means numkit faster)
- `vs_Octave` — same against Octave
- `correctness` — `OK` (fingerprints match within tol), `MISMATCH`, or `N/A` if comparison engine errored
- `comment` — input size + notes (deviations, edge cases, "alias of …")

| function | namespace | status | numkit_ms | matlab_ms | octave_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|---|:---:|---:|---:|---:|---:|---:|:---:|---|
"""


def ensure_progress_md():
    if not PROGRESS_MD.exists():
        PROGRESS_MD.write_text(PROGRESS_HEADER, encoding="utf-8")


def fmt_ms(x: float | None) -> str:
    return "—" if x is None else f"{x:.3f}"


def fmt_ratio(num: float | None, den: float | None) -> str:
    if num is None or den is None or den <= 0:
        return "—"
    return f"{num / den:.2f}×"


def append_row(*, name: str, namespace: str, status: str,
               nk: Result | None, ml: Result | None, oc: Result | None,
               correctness: str, comment: str) -> None:
    ensure_progress_md()
    nk_ms = nk.elapsed_ms if nk else None
    ml_ms = ml.elapsed_ms if ml else None
    oc_ms = oc.elapsed_ms if oc else None
    row = (
        f"| `{name}` | {namespace} | {status} | "
        f"{fmt_ms(nk_ms)} | {fmt_ms(ml_ms)} | {fmt_ms(oc_ms)} | "
        f"{fmt_ratio(ml_ms, nk_ms)} | {fmt_ratio(oc_ms, nk_ms)} | "
        f"{correctness} | {comment} |\n"
    )
    with PROGRESS_MD.open("a", encoding="utf-8") as f:
        f.write(row)


# ─────────────────────────────── main ────────────────────────────────

def run_one(spec_path: Path, *, no_matlab: bool, no_octave: bool, verbose: bool) -> int:
    spec = Spec.from_json(spec_path)
    print(f"\n=== {spec.name} ({spec.namespace}) ===", flush=True)

    nk = run_numkit(spec, timed=True)
    print(f"  numkit:  ok={nk.ok}  ms={fmt_ms(nk.elapsed_ms)}  fp={fp_str(nk.fingerprint)}",
          flush=True)
    if verbose and not nk.ok:
        print("  numkit stderr:", nk.raw_stderr[:500])
        print("  numkit stdout:", nk.raw_stdout[:500])

    ml = None
    if not no_matlab:
        ml = run_matlab(spec, timed=True)
        print(f"  matlab:  ok={ml.ok}  ms={fmt_ms(ml.elapsed_ms)}  fp={fp_str(ml.fingerprint)}",
              flush=True)
        if verbose and not ml.ok:
            print("  matlab stderr:", ml.raw_stderr[:500])
            print("  matlab stdout:", ml.raw_stdout[:500])

    oc = None
    if not no_octave:
        oc = run_octave(spec, timed=True)
        print(f"  octave:  ok={oc.ok}  ms={fmt_ms(oc.elapsed_ms)}  fp={fp_str(oc.fingerprint)}",
              flush=True)
        if verbose and not oc.ok:
            print("  octave stderr:", oc.raw_stderr[:500])
            print("  octave stdout:", oc.raw_stdout[:500])

    if not nk.ok:
        status = "FAIL"
        correctness = "N/A"
    else:
        ref = ml if (ml and ml.ok) else (oc if (oc and oc.ok) else None)
        if ref is None:
            correctness = "N/A"
            status = "DONE"
        elif fp_close(nk.fingerprint, ref.fingerprint, spec.tol):
            correctness = "OK"
            status = "DONE"
        else:
            correctness = "MISMATCH"
            status = "DONE"

    append_row(
        name=spec.name, namespace=spec.namespace, status=status,
        nk=nk, ml=ml, oc=oc, correctness=correctness, comment=spec.comment,
    )
    print(f"  -> status={status}  correctness={correctness}", flush=True)
    return 0 if status == "DONE" and correctness in ("OK", "N/A") else 1


def main():
    p = argparse.ArgumentParser()
    p.add_argument("specs", nargs="*", help="spec JSON path(s)")
    p.add_argument("--all", action="store_true", help="run every spec under tools/parity/specs/")
    p.add_argument("--no-matlab", action="store_true")
    p.add_argument("--no-octave", action="store_true")
    p.add_argument("-v", "--verbose", action="store_true")
    args = p.parse_args()

    targets: list[Path]
    if args.all:
        targets = sorted((Path(__file__).parent / "specs").glob("*.json"))
    else:
        targets = [Path(s) for s in args.specs]
    if not targets:
        p.error("no specs supplied (use --all or pass paths)")

    rc_total = 0
    for spec_path in targets:
        rc = run_one(spec_path,
                     no_matlab=args.no_matlab,
                     no_octave=args.no_octave,
                     verbose=args.verbose)
        if rc != 0:
            rc_total = rc
    return rc_total


if __name__ == "__main__":
    sys.exit(main())
