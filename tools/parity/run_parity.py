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
    # Full-element correctness comparison. If `out_var` is non-empty we
    # dump every element of that variable as a SAVE block AFTER the
    # timed loop, and the harness compares element-by-element (with
    # `tol`) instead of falling back on fingerprints. Numeric / logical
    # / integer outputs go through `double()`; char/string get dumped
    # as text; cells of chars get one element per line. Set on specs
    # whose output is small enough that 1× full-print is acceptable.
    out_var: str = ""

    @classmethod
    def from_json(cls, path: Path) -> "Spec":
        data = json.loads(path.read_text(encoding="utf-8"))
        return cls(**data)


# ────────────────────────── script builder ───────────────────────────

# Inline MATLAB/Octave/numkit-compatible code that prints `<var>` as a
# SAVE block parsed by `parse_save_block` below. Type-dispatches at run
# time so the same snippet works for double / single / int* / logical /
# char / string / cell-of-chars outputs.
SAVE_DUMP_TEMPLATE = r"""
sv__ = __VAR__;
if isnumeric(sv__) || islogical(sv__)
    flat__ = double(sv__(:));
    fprintf('SAVE_NUM_BEGIN %d\n', numel(flat__));
    for i__ = 1:numel(flat__)
        fprintf('%.17g\n', flat__(i__));
    end
    fprintf('SAVE_END\n');
elseif ischar(sv__)
    fprintf('SAVE_CHAR_BEGIN %d\n', numel(sv__));
    fprintf('%s\n', reshape(sv__, 1, []));
    fprintf('SAVE_END\n');
elseif isstring(sv__)
    fprintf('SAVE_STR_BEGIN %d\n', numel(sv__));
    for i__ = 1:numel(sv__)
        fprintf('%s\n', char(sv__(i__)));
    end
    fprintf('SAVE_END\n');
elseif iscell(sv__)
    fprintf('SAVE_CELL_BEGIN %d\n', numel(sv__));
    for i__ = 1:numel(sv__)
        el__ = sv__{i__};
        if ischar(el__) || isstring(el__)
            fprintf('%s\n', char(el__));
        elseif isnumeric(el__) && isscalar(el__)
            fprintf('%.17g\n', double(el__));
        else
            fprintf('?\n');
        end
    end
    fprintf('SAVE_END\n');
else
    fprintf('SAVE_NONE\n');
end
"""


def build_script(spec: Spec, *, timed: bool) -> str:
    """Build a MATLAB/Octave/numkit-compatible script.

    timed=True   → run warmup + iters loop, print elapsed_ms,
                   fingerprints, and (if out_var is set) a full SAVE
                   block of the output variable.
    timed=False  → just produce fingerprints (correctness reference).
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

    save_dump = SAVE_DUMP_TEMPLATE.replace("__VAR__", spec.out_var) if spec.out_var else ""

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
            f"{save_dump}\n"
        )
    else:
        return f"{spec.setup}\n{spec.expr}\n{fp_print}\n{save_dump}\n"


# ───────────────────────── engine runners ────────────────────────────

@dataclass
class SaveBlock:
    """Parsed SAVE_* block from one engine. `kind` is num/char/str/cell.

    For num: items is list[float]. For char: items is one str (the whole
    matrix as text). For str/cell: items is list[str].
    """
    kind: str = ""
    items: list = field(default_factory=list)

    def is_empty(self) -> bool:
        return self.kind == "" or (self.kind != "char" and len(self.items) == 0)


@dataclass
class Result:
    ok: bool
    elapsed_ms: float | None = None
    fingerprint: list[float] = field(default_factory=list)
    save: SaveBlock = field(default_factory=SaveBlock)
    raw_stdout: str = ""
    raw_stderr: str = ""
    error: str = ""


def parse_save_block(lines: list[str]) -> SaveBlock:
    """Walk `lines` from index 0 looking for SAVE_*_BEGIN..SAVE_END."""
    sb = SaveBlock()
    n = len(lines)
    i = 0
    while i < n:
        line = lines[i].strip()
        m = re.match(r"^SAVE_NUM_BEGIN\s+(\d+)$", line)
        if m:
            count = int(m.group(1))
            sb.kind = "num"
            sb.items = []
            for j in range(i + 1, min(i + 1 + count, n)):
                try:
                    sb.items.append(float(lines[j].strip()))
                except ValueError:
                    break
            return sb
        m = re.match(r"^SAVE_CHAR_BEGIN\s+(\d+)$", line)
        if m:
            sb.kind = "char"
            sb.items = [lines[i + 1] if i + 1 < n else ""]
            return sb
        m = re.match(r"^SAVE_(STR|CELL)_BEGIN\s+(\d+)$", line)
        if m:
            count = int(m.group(2))
            sb.kind = "str" if m.group(1) == "STR" else "cell"
            sb.items = []
            for j in range(i + 1, min(i + 1 + count, n)):
                sb.items.append(lines[j])
            return sb
        i += 1
    return sb


def parse_output(out: str) -> tuple[float | None, list[float], SaveBlock]:
    timing = None
    fps: dict[int, float] = {}
    lines = out.splitlines()
    for line in lines:
        m = re.match(r"^TIMING\s+(\S+)$", line.strip())
        if m:
            timing = float(m.group(1))
        m = re.match(r"^FP\s+(\d+)\s+(\S+)$", line.strip())
        if m:
            fps[int(m.group(1))] = float(m.group(2))
    fp_list = [fps[i] for i in sorted(fps.keys())]
    sb = parse_save_block(lines)
    return timing, fp_list, sb


def run_numkit(spec: Spec, *, timed: bool) -> Result:
    if not NUMKIT_EXE.exists():
        return Result(ok=False, error=f"numkit binary missing: {NUMKIT_EXE}")
    # numkit-only: many libs/{signal,stats} fns are also aliased into
    # `compat.<name>` so `import compat.*` brings them flat. MATLAB has
    # no compat package, so we inject this only for the numkit run.
    script = "import compat.*\n" + build_script(spec, timed=timed)
    with tempfile.NamedTemporaryFile("w", suffix=".m", delete=False, encoding="utf-8") as f:
        f.write(script)
        path = f.name
    try:
        p = subprocess.run([str(NUMKIT_EXE), path], capture_output=True,
                           text=True, timeout=120)
    finally:
        os.unlink(path)
    timing, fp, sb = parse_output(p.stdout)
    return Result(
        ok=(p.returncode == 0 and (not timed or timing is not None) and len(fp) > 0),
        elapsed_ms=timing,
        fingerprint=fp,
        save=sb,
        raw_stdout=p.stdout,
        raw_stderr=p.stderr,
        error="" if p.returncode == 0 else f"exit {p.returncode}",
    )


def _write_script(script: str) -> Path:
    """Write a temp .m file. Caller deletes when done.

    We persist scripts on disk because passing `"`-quoted MATLAB code
    inline through Windows argv mangles double quotes (the script
    `["a","b"]` arrives at the engine as `[a,b]`). Files keep them
    intact.
    """
    f = tempfile.NamedTemporaryFile("w", suffix=".m", delete=False, encoding="utf-8")
    f.write(script)
    f.close()
    return Path(f.name)


def run_matlab(spec: Spec, *, timed: bool) -> Result:
    script = build_script(spec, timed=timed)
    path = _write_script(script)
    try:
        # matlab -batch <script-name-without-extension>.
        # We pass the file's directory via --addpath equivalent; cleaner to
        # cd into it via run() / matlab's working dir.
        cmd = [MATLAB_EXE, "-batch", f"run('{path.as_posix()}')"]
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
    finally:
        path.unlink(missing_ok=True)
    timing, fp, sb = parse_output(p.stdout)
    return Result(
        ok=(p.returncode == 0 and (not timed or timing is not None) and len(fp) > 0),
        elapsed_ms=timing,
        fingerprint=fp,
        save=sb,
        raw_stdout=p.stdout,
        raw_stderr=p.stderr,
        error="" if p.returncode == 0 else f"exit {p.returncode}",
    )


def run_octave(spec: Spec, *, timed: bool) -> Result:
    script = build_script(spec, timed=timed)
    path = _write_script(script)
    try:
        cmd = [OCTAVE_EXE, "--no-gui", "--quiet", str(path)]
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
    finally:
        path.unlink(missing_ok=True)
    timing, fp, sb = parse_output(p.stdout)
    return Result(
        ok=(p.returncode == 0 and (not timed or timing is not None) and len(fp) > 0),
        elapsed_ms=timing,
        fingerprint=fp,
        save=sb,
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


def save_close(a: SaveBlock, b: SaveBlock, tol: float) -> tuple[bool, str]:
    """Element-wise compare two SAVE blocks. Returns (ok, reason)."""
    if a.kind != b.kind:
        return False, f"kind mismatch: {a.kind} vs {b.kind}"
    if a.kind == "num":
        if len(a.items) != len(b.items):
            return False, f"length mismatch: {len(a.items)} vs {len(b.items)}"
        worst = 0.0
        worst_idx = -1
        for i, (x, y) in enumerate(zip(a.items, b.items)):
            if x != x and y != y:
                continue
            diff = abs(x - y)
            scale = max(abs(x), abs(y), 1.0)
            rel = diff / scale
            if rel > worst:
                worst = rel
                worst_idx = i
            if rel > tol:
                return False, (f"elem {i}: {x:.17g} vs {y:.17g} "
                               f"(rel={rel:.3g} > tol={tol:.3g})")
        return True, f"max rel diff {worst:.3g} at idx {worst_idx}"
    if a.kind == "char":
        ok = a.items == b.items
        return ok, "" if ok else f"char text differs: {a.items[0][:40]!r} vs {b.items[0][:40]!r}"
    if a.kind in ("str", "cell"):
        if len(a.items) != len(b.items):
            return False, f"length mismatch: {len(a.items)} vs {len(b.items)}"
        for i, (x, y) in enumerate(zip(a.items, b.items)):
            if x != y:
                return False, f"elem {i}: {x!r} vs {y!r}"
        return True, ""
    return False, f"unknown kind {a.kind}"


# ─────────────────────────── log writer ──────────────────────────────
#
# PARITY_PROGRESS.md is the live parity map: grouped by MATLAB-doc
# section, one row per function, one ✅/❌/⚠️ status per function.
# Each spec run updates the row(s) for its function in place by
# regex-matching the function name. The same function may appear in
# multiple sections (e.g. interpft is listed under both Interpolation
# and Fourier); every occurrence gets the same fresh measurement.

# New row format (no raw matlab_ms / octave_ms columns — only ratios):
#   | `fn` | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
ROW_FMT = "| `{name}` | {status} | {nk_ms} | {vs_M} | {vs_O} | {correctness} | {comment} |"


def fmt_ms(x: float | None) -> str:
    return "" if x is None else f"{x:.3f}"


def fmt_ratio(num: float | None, den: float | None) -> str:
    if num is None or den is None or den <= 0:
        return ""
    return f"{num / den:.2f}×"


# Match a function's row in PARITY_PROGRESS.md. Capture the leading
# `| `fn` | status |` so we keep the existing TODO-derived status mark.
def make_row_finder(name: str) -> re.Pattern:
    return re.compile(
        r"^\|\s*`" + re.escape(name) + r"`\s*\|"
        r"\s*(?P<status>[^|]+?)\s*\|"
        r"[^\n]*$",
        re.MULTILINE,
    )


def update_row(*, name: str, nk: Result | None,
               ml: Result | None, oc: Result | None,
               correctness: str, comment: str) -> int:
    """Update every row in PARITY_PROGRESS.md whose function-name matches
    `name`. Returns the number of rows touched. If the function isn't in
    the journal yet (new entry not yet added by regenerate_progress.py),
    we append a row to a "Misc / unclassified" section at EOF."""
    if not PROGRESS_MD.exists():
        # No file yet — leave it; the user is expected to run
        # regenerate_progress.py first to seed it from the TODO.
        return 0

    nk_ms = nk.elapsed_ms if (nk and nk.ok) else None
    ml_ms = ml.elapsed_ms if (ml and ml.ok) else None
    oc_ms = oc.elapsed_ms if (oc and oc.ok) else None

    text = PROGRESS_MD.read_text(encoding="utf-8")
    rx = make_row_finder(name)

    touched = 0

    def replace(m: re.Match) -> str:
        nonlocal touched
        touched += 1
        return ROW_FMT.format(
            name=name,
            status=m.group("status").strip(),  # preserve TODO ✅/❌/⚠️
            nk_ms=fmt_ms(nk_ms),
            vs_M=fmt_ratio(ml_ms, nk_ms),
            vs_O=fmt_ratio(oc_ms, nk_ms),
            correctness=correctness,
            comment=comment,
        )

    new_text = rx.sub(replace, text)

    if touched == 0:
        # Function not yet registered in the TODO-driven layout. Append
        # to a "Misc / new" section at EOF so we don't lose the data.
        misc_marker = "## Misc / not in TODO"
        if misc_marker not in new_text:
            new_text = new_text.rstrip() + "\n\n" + misc_marker + "\n\n"
            new_text += "Functions benched by the harness that don't appear in any of the MATLAB-doc sections above. Move them into a real section if they correspond to a documented MATLAB function.\n\n"
            new_text += "| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |\n"
            new_text += "|---|:---:|---:|---:|---:|:---:|---|\n"
        new_text = new_text.rstrip() + "\n" + ROW_FMT.format(
            name=name,
            status="—",
            nk_ms=fmt_ms(nk_ms),
            vs_M=fmt_ratio(ml_ms, nk_ms),
            vs_O=fmt_ratio(oc_ms, nk_ms),
            correctness=correctness,
            comment=comment,
        ) + "\n"
        touched = 1

    PROGRESS_MD.write_text(new_text, encoding="utf-8")
    return touched


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
        else:
            # Prefer element-wise SAVE-block comparison when both engines
            # produced one (= spec.out_var is set). Fall back to
            # fingerprint comparison otherwise.
            use_save = (spec.out_var
                        and not nk.save.is_empty()
                        and not ref.save.is_empty())
            if use_save:
                ok, why = save_close(nk.save, ref.save, spec.tol)
                if ok:
                    correctness = "OK"
                    if verbose and why:
                        print(f"  save-compare: {why}", flush=True)
                else:
                    correctness = "MISMATCH"
                    print(f"  save-mismatch: {why}", flush=True)
                status = "DONE"
            elif fp_close(nk.fingerprint, ref.fingerprint, spec.tol):
                correctness = "OK"
                status = "DONE"
            else:
                correctness = "MISMATCH"
                status = "DONE"

    n = update_row(
        name=spec.name,
        nk=nk, ml=ml, oc=oc,
        correctness=correctness, comment=spec.comment,
    )
    print(f"  -> status={status}  correctness={correctness}  rows updated: {n}",
          flush=True)
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
