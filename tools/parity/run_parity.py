#!/usr/bin/env python3
"""
Parity harness for numkit-m.

Runs a function spec against three engines (numkit native, MATLAB R2025b,
Octave 11.1.0), compares output for correctness, measures wall-clock time,
and appends a row to PROGRESS.md.

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
NUMKIT_EXE = ROOT / "build" / "desktop-fast" / "tests" / "smoke" / "Release" / "numkit_smoke.exe"
MATLAB_EXE = "matlab"  # on PATH
OCTAVE_EXE = r"C:\Program Files\GNU Octave\Octave-11.1.0\mingw64\bin\octave-cli.exe"
PROGRESS_MD = ROOT / "PROGRESS.md"
BENCHMARK_MD = ROOT / "BENCHMARK.md"

# Two-point benchmark sizes. A spec's `bench_setup` references the
# variable `N`; the harness defines it to each of these and times the
# call, so BENCHMARK.md shows both small-array (overhead-sensitive) and
# large-array (throughput) ratios vs MATLAB / Octave.
BENCH_SMALL = 1000
BENCH_LARGE = 1000000


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
    # Grouped spec — one engine run validates several functions at once.
    # When non-empty, PROGRESS rows are updated for every name listed
    # here (each row gets the same measurement); `name` then only
    # labels the spec file. Empty → single-function spec keyed by `name`.
    covers: list[str] = field(default_factory=list)
    # Large-array benchmark inputs. The plain `setup`/`expr`/`fingerprint`
    # stay TINY for exact cross-engine correctness; but timing measured on
    # tiny inputs is dominated by per-call interpreter overhead (MATLAB's
    # is large, numkit's is tiny → meaningless 100×+ ratios). When
    # `bench_setup` is non-empty the harness times the call on LARGE data
    # instead, so vs_MATLAB / vs_Octave reflect real throughput. The
    # timed loop runs `bench_expr` (defaults to `expr`) over the variables
    # defined by `bench_setup`. Correctness is unaffected.
    bench_setup: str = ""
    bench_expr: str = ""
    # The two values substituted for `N` in bench_setup — [small, large].
    # Default is element counts (1e3 / 1e6) for vector fns. Image/matrix
    # specs override with side lengths, e.g. [100, 1000] for 100x100 /
    # 1000x1000, and build an N×N input in bench_setup. `bench_note` is
    # free text written into BENCHMARK.md's notes column (e.g. the actual
    # input shape) so the size used is unambiguous per row.
    bench_sizes: list = field(default_factory=lambda: [1000, 1000000])
    bench_note: str = ""
    # Set true on a spec that DEEP-PROBED its function(s) against MATLAB
    # across options / edge branches (not just a basic check). The harness
    # then appends a 🔬 marker to the PROGRESS.md status cell, which it
    # preserves across future runs.
    deep_verified: bool = False

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


def _indent(code: str, n: int) -> str:
    pad = " " * n
    return "\n".join(pad + ln for ln in code.splitlines())


def _indent_stmts(code: str, n: int) -> str:
    """Indent `code` by n spaces, one statement per line.

    Spec `setup` / `expr` are authored as a single `;`-separated line.
    A very long single line trips a numkit parser bug (statements past
    a certain length mis-evaluate), so each `;`-separated statement is
    put on its own line. A `;` inside a `[...]` matrix literal split
    this way stays valid — a newline is also a row separator there.
    """
    return _indent(";\n".join(code.split(";")), n)


def _spec_fn(name: str) -> str:
    """A valid MATLAB identifier for the spec's local function."""
    return "spec_" + re.sub(r"\W", "_", name)


def build_spec_call(spec: Spec) -> str:
    """The script-body line that runs one spec under a thin try.

    The try wraps only the *call* — the spec's own (possibly complex)
    code lives in a separate local function. Keeping the try-body to a
    single call is deliberate: it both contains runtime errors and
    side-steps an interpreter bug where try-wrapping a large inline
    block changes how it evaluates.
    """
    fn = _spec_fn(spec.name)
    return (
        f"fprintf('__SPECBEGIN__ {spec.name}\\n');\n"
        f"try; {fn}(); "
        f"catch ME__; fprintf('__SPECERR__ %s\\n', ME__.message); end\n"
        f"fprintf('__SPECEND__ {spec.name}\\n');\n"
    )


def build_spec_func(spec: Spec, *, engine: str) -> str:
    """The local-function definition that runs one spec's body.

    Each spec is its own function, so its workspace is fully isolated —
    no `clear` and no cross-spec leakage.
    """
    # Auto-fp: use the first assigned output as the fingerprint var.
    # `[y, jj] = f(...)` → strip brackets, take the first name.
    lhs = spec.expr.split("=")[0].strip()
    if lhs.startswith("[") and lhs.endswith("]"):
        first_var = lhs[1:-1].split(",")[0].strip()
    else:
        first_var = lhs
    fp_exprs = spec.fingerprint or [
        f"sum({first_var}(:))",
        f"{first_var}(1)",
        f"{first_var}(end)",
    ]
    fp_print = "\n".join(
        f"    fprintf('FP %d %.17g\\n', {i}, double({e}));"
        for i, e in enumerate(fp_exprs)
    )
    save_dump = ""
    if spec.out_var:
        save_dump = _indent(
            SAVE_DUMP_TEMPLATE.replace("__VAR__", spec.out_var), 4)
    reimport = "    import compat.*\n" if engine == "numkit" else ""

    # Correctness body — always run on the TINY setup+expr (exact
    # cross-engine fingerprint / save comparison).
    correctness = (
        f"{_indent_stmts(spec.setup, 4)}\n"
        f"{_indent_stmts(spec.expr, 4)}\n"
        f"{fp_print}\n"
        f"{save_dump}\n"
    )

    if not spec.bench_setup.strip():
        # No benchmark inputs → correctness only (no timing). Perf for
        # this function shows as "not benched" in BENCHMARK.md until a
        # `bench_setup` is added.
        return (
            f"function {_spec_fn(spec.name)}()\n"
            f"{reimport}"
            f"{correctness}"
            f"end\n"
        )

    # Two-point benchmark. `bench_setup` references N; we define N to each
    # size, rebuild the inputs, and time `bench_expr` (default `expr`).
    bench_expr = spec.bench_expr.strip() or spec.expr

    def timed(size: int, iters: int, label: str) -> str:
        # NB: assign toc() to a variable BEFORE fprintf — numkit's toc(h)
        # returns empty when inlined directly into fprintf's arg list.
        return (
            f"    N = {size};\n"
            f"{_indent_stmts(spec.bench_setup, 4)}\n"
            f"{_indent_stmts(bench_expr, 4)}\n"          # warmup
            f"    t0__ = tic;\n"
            f"    for kk__ = 1:{iters}\n"
            f"{_indent_stmts(bench_expr, 8)}\n"
            f"    end\n"
            f"    elapsed_ms__ = toc(t0__) * 1000.0 / {iters};\n"
            f"    fprintf('{label} %.6f\\n', elapsed_ms__);\n"
        )

    iters_small = max(spec.iters, 50)   # tiny array → many iters for a stable mean
    iters_large = max(spec.iters, 5)
    sizes = (list(spec.bench_sizes) + [BENCH_SMALL, BENCH_LARGE])[:2]
    return (
        f"function {_spec_fn(spec.name)}()\n"
        f"{reimport}"
        f"{correctness}"
        f"{timed(sizes[0], iters_small, 'TIMING_SMALL')}"
        f"{timed(sizes[1], iters_large, 'TIMING_LARGE')}"
        f"end\n"
    )


def build_batch_script(specs: list[Spec], *, engine: str) -> str:
    """A whole chunk of specs as one script for a single engine launch.

    Layout: prelude · one call line per spec · one local function per
    spec (functions last, as MATLAB requires). A *parse* error still
    aborts its chunk — MATLAB compiles the file before running — which
    is why specs are chunked rather than run as one giant script.
    """
    if engine == "numkit":
        # libs/{signal,stats,…} fns are aliased into `compat.<name>`;
        # MATLAB has no compat package so this is numkit-only.
        prelude = "import compat.*\n"
    elif engine == "octave":
        # Octave needs `pkg load` for non-base packages.
        prelude = (
            "try; pkg load signal; end_try_catch\n"
            "try; pkg load statistics; end_try_catch\n"
            "try; pkg load control; end_try_catch\n"
            "try; pkg load image; end_try_catch\n"
        )
    else:
        prelude = ""
    calls = "".join(build_spec_call(s) for s in specs)
    funcs = "\n".join(build_spec_func(s, engine=engine) for s in specs)
    return prelude + calls + "\n" + funcs


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
    ms_small: float | None = None     # TIMING_SMALL (N = BENCH_SMALL)
    ms_large: float | None = None     # TIMING_LARGE (N = BENCH_LARGE)
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


def parse_output(out: str) -> tuple[float | None, float | None, float | None,
                                    list[float], SaveBlock]:
    timing = None
    ts = None      # TIMING_SMALL (N = BENCH_SMALL)
    tl = None      # TIMING_LARGE (N = BENCH_LARGE)
    fps: dict[int, float] = {}
    lines = out.splitlines()
    for line in lines:
        m = re.match(r"^TIMING_SMALL\s+(\S+)$", line.strip())
        if m:
            ts = float(m.group(1))
            continue
        m = re.match(r"^TIMING_LARGE\s+(\S+)$", line.strip())
        if m:
            tl = float(m.group(1))
            continue
        m = re.match(r"^TIMING\s+(\S+)$", line.strip())
        if m:
            timing = float(m.group(1))
        m = re.match(r"^FP\s+(\d+)\s+(\S+)$", line.strip())
        if m:
            tok = m.group(2)
            # Octave prints 'NA' (missing-value) and 'NaN' (not-a-number)
            # as separate strings; MATLAB only prints 'NaN'/'Inf'. Map
            # both Octave forms into NaN so float() doesn't blow up.
            if tok in ("NA", "NaN", "nan"):
                val = float("nan")
            elif tok in ("Inf", "inf"):
                val = float("inf")
            elif tok in ("-Inf", "-inf"):
                val = float("-inf")
            else:
                val = float(tok)
            fps[int(m.group(1))] = val
    fp_list = [fps[i] for i in sorted(fps.keys())]
    sb = parse_save_block(lines)
    return timing, ts, tl, fp_list, sb


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


def parse_batch(text: str) -> dict[str, Result]:
    """Split one engine's combined batch output into a Result per spec,
    delimited by the __SPECBEGIN__ / __SPECEND__ markers."""
    out: dict[str, Result] = {}
    cur: str | None = None
    buf: list[str] = []
    for line in text.splitlines():
        st = line.strip()
        if st.startswith("__SPECBEGIN__ "):
            cur = st[len("__SPECBEGIN__ "):].strip()
            buf = []
        elif st.startswith("__SPECEND__ "):
            if cur is not None:
                chunk = "\n".join(buf)
                err = any(l.strip().startswith("__SPECERR__") for l in buf)
                timing, ts, tl, fp, sb = parse_output(chunk)
                # Correctness no longer depends on a timing line — non-bench
                # specs emit no timing, bench specs emit TIMING_SMALL/LARGE.
                out[cur] = Result(
                    ok=(not err and (len(fp) > 0 or not sb.is_empty())),
                    elapsed_ms=timing, ms_small=ts, ms_large=tl,
                    fingerprint=fp, save=sb,
                    raw_stdout=chunk,
                    error=("spec raised" if err else ""),
                )
            cur, buf = None, []
        elif cur is not None:
            buf.append(line)
    return out


def run_engine_batch(specs: list[Spec],
                     engine: str) -> tuple[dict[str, Result], str]:
    """Run a whole chunk of specs in ONE engine process.

    Returns (results-by-spec-name, error-string). The error string is
    non-empty only on a whole-process problem (missing binary, timeout,
    non-zero exit); per-spec runtime errors are captured inside each
    Result via the __SPECERR__ marker, and partial results from a
    crashed process are still returned for the specs that completed.
    """
    if engine == "numkit" and not NUMKIT_EXE.exists():
        return {}, f"numkit binary missing: {NUMKIT_EXE}"
    script = build_batch_script(specs, engine=engine)
    path = _write_script(script)
    timeout = max(300, 8 * len(specs))  # one launch covers the chunk
    try:
        if engine == "numkit":
            cmd = [str(NUMKIT_EXE), str(path)]
        elif engine == "matlab":
            cmd = [MATLAB_EXE, "-batch", f"run('{path.as_posix()}')"]
        else:
            cmd = [OCTAVE_EXE, "--no-gui", "--quiet", str(path)]
        try:
            p = subprocess.run(cmd, capture_output=True, text=True,
                               timeout=timeout)
        except subprocess.TimeoutExpired:
            return {}, f"{engine}: chunk timed out after {timeout}s"
    finally:
        path.unlink(missing_ok=True)
    results = parse_batch(p.stdout)
    err = "" if p.returncode == 0 else f"{engine}: exit {p.returncode}"
    return results, err


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
# PROGRESS.md is the live parity map: grouped by MATLAB-doc
# section, one row per function, one ✅/❌/⚠️ status per function.
# Each spec run updates the row(s) for its function in place by
# regex-matching the function name. The same function may appear in
# multiple sections (e.g. interpft is listed under both Interpolation
# and Fourier); every occurrence gets the same fresh measurement.

# PROGRESS.md row — IMPLEMENTATION only (status + correctness + notes).
# Performance lives in BENCHMARK.md.
PROGRESS_ROW_FMT = "| `{name}` | {status} | {correctness} | {comment} |"

# BENCHMARK.md row — per-function perf at two array sizes (1e3 / 1e6) vs
# MATLAB / Octave.  nk_*=numkit per-call mean (ms); ML×/OC× = ref_ms /
# numkit_ms (>1× = numkit faster). "" = not yet benched for that engine.
BENCH_ROW_FMT = ("| `{name}` | {nk_s} | {ml_s} | {oc_s} "
                 "| {nk_l} | {ml_l} | {oc_l} | {notes} |")


def fmt_ms(x: float | None) -> str:
    return "" if x is None else f"{x:.4g}"


def fmt_ratio(num: float | None, den: float | None) -> str:
    if num is None or den is None or den <= 0:
        return ""
    return f"{num / den:.2f}×"


# Match a function's row in PROGRESS.md. Capture the leading
# `| `fn` | status |` so we keep the existing TODO-derived status mark.
def make_row_finder(name: str) -> re.Pattern:
    return re.compile(
        r"^\|\s*`" + re.escape(name) + r"`\s*\|"
        r"\s*(?P<status>[^|]+?)\s*\|"
        r"[^\n]*$",
        re.MULTILINE,
    )


def update_progress_row(*, name: str, nk: Result | None,
                        ml: Result | None, oc: Result | None,
                        correctness: str, comment: str,
                        implemented: bool = False,
                        deep_verified: bool = False) -> int:
    """Update implementation rows in PROGRESS.md for `name` — status +
    correctness + comment only (perf lives in BENCHMARK.md). Appends to a
    Misc section if the function isn't in the TODO-driven layout. A
    deep_verified spec appends a 🔬 marker to the status cell (kept across
    future runs since the status cell is otherwise preserved verbatim)."""
    if not PROGRESS_MD.exists():
        return 0
    text = PROGRESS_MD.read_text(encoding="utf-8")
    rx = make_row_finder(name)
    touched = 0

    def replace(m: re.Match) -> str:
        nonlocal touched
        touched += 1
        # ❌→✅ self-heal when numkit actually ran the fn; ⚠️/✅ preserved.
        cur_status = m.group("status").strip()
        if implemented and cur_status.replace("🔬", "").strip() == "❌":
            cur_status = cur_status.replace("❌", "✅")
        if deep_verified and "🔬" not in cur_status:
            cur_status = cur_status + " 🔬"
        return PROGRESS_ROW_FMT.format(name=name, status=cur_status,
                                       correctness=correctness, comment=comment)

    new_text = rx.sub(replace, text)
    if touched == 0:
        misc = "## Misc / not in TODO"
        if misc not in new_text:
            new_text = new_text.rstrip() + "\n\n" + misc + "\n\n"
            new_text += ("Functions exercised by the harness that don't appear "
                         "in any MATLAB-doc section above.\n\n")
            new_text += "| function | status | correctness | comment |\n"
            new_text += "|---|:---:|:---:|---|\n"
        new_text = new_text.rstrip() + "\n" + PROGRESS_ROW_FMT.format(
            name=name, status="—", correctness=correctness, comment=comment) + "\n"
        touched = 1

    PROGRESS_MD.write_text(new_text, encoding="utf-8")
    return touched


def update_benchmark_row(*, name: str, nk: Result | None,
                         ml: Result | None, oc: Result | None,
                         note: str = "") -> int:
    """Update perf rows in BENCHMARK.md for `name`. Only writes when
    numkit produced two-size timings (a `bench_setup` spec); otherwise
    leaves the existing row untouched so it stays 'not yet benched'.
    `note` (the spec's bench_note) goes into the notes column — used to
    record a non-default input shape, e.g. an image's 100x100 / 1000x1000."""
    if not BENCHMARK_MD.exists():
        return 0
    if not (nk and nk.ok and nk.ms_small is not None and nk.ms_large is not None):
        return 0
    nk_s, nk_l = nk.ms_small, nk.ms_large
    ml_s = ml.ms_small if (ml and ml.ok) else None
    ml_l = ml.ms_large if (ml and ml.ok) else None
    oc_s = oc.ms_small if (oc and oc.ok) else None
    oc_l = oc.ms_large if (oc and oc.ok) else None
    row = BENCH_ROW_FMT.format(
        name=name,
        nk_s=fmt_ms(nk_s), ml_s=fmt_ratio(ml_s, nk_s), oc_s=fmt_ratio(oc_s, nk_s),
        nk_l=fmt_ms(nk_l), ml_l=fmt_ratio(ml_l, nk_l), oc_l=fmt_ratio(oc_l, nk_l),
        notes=note,
    )
    text = BENCHMARK_MD.read_text(encoding="utf-8")
    rx = make_row_finder(name)
    touched = 0

    def replace(m: re.Match) -> str:
        nonlocal touched
        touched += 1
        return row

    new_text = rx.sub(replace, text)
    if touched == 0:
        misc = "## Misc / not in TODO"
        if misc not in new_text:
            new_text = new_text.rstrip() + "\n\n" + misc + "\n\n"
            new_text += ("| function | nk small (ms) | ML× s | OC× s "
                         "| nk large (ms) | ML× l | OC× l | notes |\n")
            new_text += "|---|---:|---:|---:|---:|---:|---:|---|\n"
        new_text = new_text.rstrip() + "\n" + row + "\n"
        touched = 1

    BENCHMARK_MD.write_text(new_text, encoding="utf-8")
    return touched


# ──────────────────────── correctness verdict ─────────────────────────

def evaluate(spec: Spec, nk: Result | None, ml: Result | None,
             oc: Result | None) -> tuple[str, str]:
    """(status, correctness) for one spec from its three engine Results.

    status      — DONE if numkit ran the spec, FAIL otherwise.
    correctness — OK / MISMATCH / N/A vs the reference engine
                  (MATLAB preferred, Octave fallback).
    """
    if nk is None or not nk.ok:
        return "FAIL", "N/A"
    ref = ml if (ml and ml.ok) else (oc if (oc and oc.ok) else None)
    if ref is None:
        return "DONE", "N/A"
    use_save = (spec.out_var and not nk.save.is_empty()
                and not ref.save.is_empty())
    if use_save:
        ok, _ = save_close(nk.save, ref.save, spec.tol)
        return "DONE", ("OK" if ok else "MISMATCH")
    if fp_close(nk.fingerprint, ref.fingerprint, spec.tol):
        return "DONE", "OK"
    return "DONE", "MISMATCH"


# ─────────────────────────────── main ────────────────────────────────

def run_chunk(specs: list[Spec], *, no_matlab: bool, no_octave: bool,
              verbose: bool) -> tuple[int, dict[str, int]]:
    """Run one chunk — a single launch per engine — then score and log
    every spec. Returns (rc, tally); rc is 1 if any spec failed or
    mismatched."""
    nk_res, nk_err = run_engine_batch(specs, "numkit")
    ml_res, ml_err = (({}, "") if no_matlab
                      else run_engine_batch(specs, "matlab"))
    oc_res, oc_err = (({}, "") if no_octave
                      else run_engine_batch(specs, "octave"))
    for err in (nk_err, ml_err, oc_err):
        if err:
            print(f"  ! {err}", flush=True)

    rc = 0
    tally = {"OK": 0, "MISMATCH": 0, "FAIL": 0, "N/A": 0}
    for spec in specs:
        nk = nk_res.get(spec.name)
        ml = ml_res.get(spec.name)
        oc = oc_res.get(spec.name)
        status, correctness = evaluate(spec, nk, ml, oc)
        if status == "FAIL":
            tally["FAIL"] += 1
            rc = 1
        else:
            tally[correctness] += 1
            if correctness == "MISMATCH":
                rc = 1
        # Only refresh a row when numkit actually ran the spec. On any
        # numkit failure (engine crash, parse error, runtime throw)
        # leave the row intact — don't overwrite a previously-recorded
        # OK with a transient N/A.
        rows = 0
        if nk is not None and nk.ok:
            for nm in (spec.covers or [spec.name]):
                rows += update_progress_row(
                    name=nm, nk=nk, ml=ml, oc=oc,
                    correctness=correctness, comment=spec.comment,
                    implemented=nk.ok, deep_verified=spec.deep_verified)
                update_benchmark_row(name=nm, nk=nk, ml=ml, oc=oc,
                                     note=spec.bench_note)
        flag = "" if (status == "DONE"
                      and correctness in ("OK", "N/A")) else "  <<"
        print(f"  {spec.name:<34} {status:<5} {correctness:<9} "
              f"rows={rows}{flag}", flush=True)
        if verbose and nk is not None and not nk.ok:
            print(f"      numkit error: {nk.error}", flush=True)
    return rc, tally


def main():
    p = argparse.ArgumentParser()
    p.add_argument("specs", nargs="*", help="spec JSON path(s)")
    p.add_argument("--all", action="store_true",
                   help="run every spec under tools/parity/specs/")
    p.add_argument("--no-matlab", action="store_true")
    p.add_argument("--no-octave", action="store_true")
    p.add_argument("--chunk", type=int, default=100,
                   help="specs per engine launch (default 100)")
    p.add_argument("-v", "--verbose", action="store_true")
    args = p.parse_args()

    if args.all:
        targets = sorted((Path(__file__).parent / "specs").glob("*.json"))
    else:
        targets = [Path(s) for s in args.specs]
    if not targets:
        p.error("no specs supplied (use --all or pass paths)")

    specs: list[Spec] = []
    for sp in targets:
        try:
            specs.append(Spec.from_json(sp))
        except Exception as e:  # a malformed spec must not abort the run
            print(f"  SKIP {sp.name}: bad spec ({e})")
    if not specs:
        return 1

    chunk = max(1, args.chunk)
    nchunks = (len(specs) + chunk - 1) // chunk
    print(f"{len(specs)} spec(s) · chunk={chunk} · "
          f"{nchunks} launch(es) per engine", flush=True)

    rc = 0
    total = {"OK": 0, "MISMATCH": 0, "FAIL": 0, "N/A": 0}
    for ci in range(nchunks):
        batch = specs[ci * chunk:(ci + 1) * chunk]
        print(f"\n── chunk {ci + 1}/{nchunks} ({len(batch)} specs) ──",
              flush=True)
        crc, tally = run_chunk(batch, no_matlab=args.no_matlab,
                               no_octave=args.no_octave,
                               verbose=args.verbose)
        rc = rc or crc
        for k in total:
            total[k] += tally[k]

    print(f"\nsummary: OK={total['OK']}  MISMATCH={total['MISMATCH']}  "
          f"FAIL={total['FAIL']}  N/A={total['N/A']}", flush=True)
    return rc


if __name__ == "__main__":
    sys.exit(main())
