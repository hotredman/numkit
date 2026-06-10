#!/usr/bin/env python3
"""Architectural layering guard.

Enforces the acyclic dependency direction established by the layering refactor:

    value/  (L0)    STL only
    fs/     (L0)    STL only
    ops/    (L0.5)  -> value (+ Highway / Threads)
    core/   (L1)    -> value, fs, ops
    math/   (L2)    -> value, fs, ops, math, lang (+ builtin, transitional)
    lang/   (L2)    -> value, fs, ops, math, lang (+ builtin, transitional)
    toolboxes/*  (L2)    -> value, fs, ops, core         (toolboxes; not pinned yet)
    bundle/ (L3)    -> everything

This checker pins value, fs, ops, core and the math/lang COMPUTE: none may
include headers from a layer above them. core MUST stay free of every toolbox
(the original "core pulls libs" defect); math/lang compute MUST stay free of
core / runtime / toolboxes (the point of the C4 numkit::math / numkit::lang
split). The `*_reg.cpp` registration adapters in math/lang/src are core-coupled
glue (CallContext / Engine) bound for the bundle layer in C5, so they are
exempt. `builtin` is transitionally allowed in math/lang (old-path forwarding
stubs + library.hpp Error) until the include-path migration. Toolbox purity is
enforced later.

Scans each guarded layer's include/ + src/ trees for `#include <numkit/X/...>`
and fails if X is not in that layer's allowed set. Exit 0 = clean, 1 = a
forbidden edge was introduced.

Run:  python tools/check_layering.py            (from repo root)
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# numkit/<prefix> each guarded layer is allowed to include.
ALLOWED = {
    "value": {"value"},
    "fs":    {"fs"},
    "ops":   {"value", "ops"},
    "core":  {"value", "fs", "ops", "core"},
    # math/lang (L2 compute, ns numkit::math / numkit::lang after the C4 split)
    # must stay free of core / runtime / toolboxes — the whole point of the split.
    # They may use value/fs/ops, each other (e.g. lang/arrays -> math/arithmetic
    # cumsum), and — transitionally — `builtin` (the old-path forwarding stubs +
    # library.hpp Error live there until the include-path migration). The
    # core-coupled `*_reg.cpp` registration glue is excluded from the scan (it
    # lives in math/lang/src by locality but belongs to bundle; relocated in C5).
    "math":  {"value", "fs", "ops", "math", "lang", "builtin"},
    "lang":  {"value", "fs", "ops", "math", "lang", "builtin"},
}

# Layer -> directories scanned (relative to repo root).
LAYER_DIRS = {
    "value": ["value"],
    "fs":    ["fs"],
    "ops":   ["ops"],
    "core":  ["core"],
    "math":  ["math"],
    "lang":  ["lang"],
}

INCLUDE_RE = re.compile(r'#\s*include\s*<numkit/([a-zA-Z0-9_]+)/')
SRC_SUFFIXES = {".cpp", ".hpp", ".h", ".cc", ".cxx", ".ipp"}


def scan(repo: Path) -> list[str]:
    violations: list[str] = []
    for layer, dirs in LAYER_DIRS.items():
        allowed = ALLOWED[layer]
        for d in dirs:
            base = repo / d
            if not base.is_dir():
                continue
            for f in base.rglob("*"):
                # Library code only — test/benchmark executables link the full
                # aggregate (incl. toolboxes) and may include anything.
                if f.suffix not in SRC_SUFFIXES:
                    continue
                if {"build", "tests", "test", "benchmarks", "benchmark"} & set(f.parts):
                    continue
                # *_reg.cpp adapters are core-coupled registration glue
                # (CallContext / Engine). They sit in math/lang/src by locality
                # but conceptually belong to the bundle layer (relocated in C5),
                # so they are exempt from the compute-layer purity check.
                if f.name.endswith("_reg.cpp"):
                    continue
                for n, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                    m = INCLUDE_RE.search(line)
                    if m and m.group(1) not in allowed:
                        rel = f.relative_to(repo).as_posix()
                        violations.append(
                            f"{rel}:{n}: layer '{layer}' must not include "
                            f"<numkit/{m.group(1)}/...>  (allowed: {sorted(allowed)})"
                        )
    return violations


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    violations = scan(repo)
    if violations:
        print("Layering violations found:\n", file=sys.stderr)
        for v in violations:
            print("  " + v, file=sys.stderr)
        print(
            f"\n{len(violations)} forbidden include(s). The guarded layers "
            "(value/fs/ops/core + math/lang compute) must not depend on a layer "
            "above them — in particular math/lang must stay core/runtime/toolbox-free.",
            file=sys.stderr,
        )
        return 1
    print("Layering OK: value, fs, ops, core, math, lang respect the dependency direction.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
