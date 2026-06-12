#!/usr/bin/env python3
"""Architectural layering guard.

Enforces the acyclic dependency direction established by the layering refactor:

    value/  (L0)    STL only
    fs/     (L0)    STL only
    ops/    (L0.5)  -> value (+ Highway / Threads)
    core/   (L1)    -> value, fs, ops
    math/   (L2)    -> value, fs, ops, math, lang (+ builtin, transitional)
    lang/   (L2)    -> value, fs, ops, math, lang (+ builtin, transitional)
    toolboxes/*  (L2)    -> value, fs, ops, math, lang, sibling toolboxes
    bundle/ (L3)    -> everything

This checker pins value, fs, ops, core and the math/lang COMPUTE: none may
include headers from a layer above them. core MUST stay free of every toolbox
(the original "core pulls libs" defect); math/lang compute MUST stay free of
core / runtime / toolboxes (the point of the C4 numkit::math / numkit::lang
split). The `*_reg.cpp` registration adapters now live in bundle/src/register
(core-coupled CallContext / Engine glue), so they are outside the scanned
compute trees. (The transitional `builtin` allowance is gone — the C4
include-path migration completed; no math/lang/toolbox TU includes a
<numkit/builtin/...> header.) Toolbox purity is
now ALSO enforced: every toolbox compute TU must stay core/runtime-free (their
Engine glue moved to bundle in F; stateful surfaces decoupled via FsContext/
FnHandle). Exempt: each toolbox's library.{cpp,hpp} installer + io's type.cpp
(engine.outputText). The AST→NodeGraph IDE-analysis pass (formerly the `graph`
toolbox) now lives in its own `scriptgraph` layer, honestly core-allowed.

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
    "core":  {"value", "fs", "ops", "core", "figure"},
    # figure (L0.5 header-only): plot/figure session state (FigureManager); uses
    # only ops (decimate) + STL, never core. core + graphics depend on it.
    "figure": {"value", "fs", "ops", "figure"},
    # math/lang (L2 compute, ns numkit::math / numkit::lang after the C4 split)
    # must stay free of core / runtime / toolboxes — the whole point of the split.
    # They may use value/fs/ops and each other (e.g. lang/arrays -> math's private
    # arithmetic/cumsum.hpp reduction kernel, via the shared src -I). The
    # core-coupled `*_reg.cpp` registration glue lives in bundle/src/register and
    # is excluded from the scan. (The transitional `builtin` allowance was dropped
    # once the C4 include-path migration completed — no math/lang TU includes a
    # <numkit/builtin/...> header any more.)
    "math":  {"value", "fs", "ops", "math", "lang"},
    "lang":  {"value", "fs", "ops", "math", "lang"},
    # toolboxes/* (L2 compute) must stay free of core / runtime — all their
    # Engine glue (the `*_reg.cpp` adapters) was relocated to bundle in F, and
    # the stateful surfaces were decoupled (FsContext / FnHandle). They may use
    # value/fs/ops, math/lang compute, graphics, and each other (sibling toolbox
    # names added dynamically below). Two sanctioned core users are exempt
    # per-file in scan(): each toolbox's `library.cpp` installer (registration
    # ABI) and io's `type.cpp` (legitimately Engine& — it writes via
    # engine.outputText).
    "toolboxes": {"value", "fs", "ops", "math", "lang", "graphics"},
    # graphics (L2 service): the plotting library (figure/plot/imshow/…). Now
    # core-free like every other L2 lib — the plotting bodies (plots.cpp) take a
    # GraphicsContext (FigureManager + scratch arena + callBuiltin/callHandle
    # escape hatches), never the Engine. The lone Engine-coupled file is the
    # install hub library.cpp (registration ABI + the generic CallContext→
    # GraphicsContext adapter), exempt per-file in scan() exactly like a toolbox
    # installer. The figure session state lives below core in the figure/ layer.
    # The defining rule: graphics depends on NO toolbox — toolboxes MAY depend on
    # graphics, never the reverse. (graphics→image was broken by routing
    # imshow's file-decode through a by-name `imread` handle resolved at call
    # time, not an image-toolbox include.)
    "graphics": {"value", "fs", "ops", "figure", "graphics"},
    # scriptgraph (L2 IDE-analysis): the offline AST→NodeGraph→JSON pass behind
    # the IDE's `buildScriptGraph` WASM export (data-flow graph view + AST view).
    # It registers NO builtin — it is genuinely core-coupled (it walks core's
    # ast/lexer/parser), so `core` IS allowed here, no per-file exemption needed.
    # Formerly mis-filed as the `graph` toolbox (name clashed with MATLAB
    # graph/digraph + implied a function toolbox); renamed + relocated out of
    # toolboxes/ to its own tier. Nothing depends on it but the wasm bindings.
    "scriptgraph": {"value", "fs", "ops", "core", "scriptgraph"},
}

# Layer -> directories scanned (relative to repo root).
LAYER_DIRS = {
    "value": ["src/value"],
    "fs":    ["src/fs"],
    "ops":   ["src/ops"],
    "core":  ["src/core"],
    "figure": ["src/figure"],
    "math":  ["src/math"],
    "lang":  ["src/lang"],
    "graphics": ["src/graphics"],
    "scriptgraph": ["src/scriptgraph"],
    "toolboxes": ["src/toolboxes"],
}

INCLUDE_RE = re.compile(r'#\s*include\s*<numkit/([a-zA-Z0-9_]+)/')
QUOTED_INCLUDE_RE = re.compile(r'#\s*include\s*"([^"]+)"')
SRC_SUFFIXES = {".cpp", ".hpp", ".h", ".cc", ".cxx", ".ipp"}
HEADER_SUFFIXES = {".hpp", ".h", ".ipp", ".cuh", ".hh", ".hxx"}

# Layers that must be fully self-contained: a quoted `#include "X"` from one of
# these layers must resolve to a header that physically lives inside the layer's
# own tree. A quoted include whose basename isn't found in the layer resolves via
# some external -I — a cross-layer leak. The convention is <numkit/...> for ANY
# cross-module dependency; quoted form is for same-component headers only. This is
# the bug class the angle-include check misses: ops/src/fft_portable.cpp once did
# `#include "dsp_helpers.hpp"`, reaching up into the signal toolbox (+ core),
# invisible because it was a quoted include. math/lang are intentionally excluded
# (their compile uses a broad shared -I + transitional builtin helpers).
STRICT_QUOTED = ("value", "fs", "ops", "core")


def _header_basenames(base: Path) -> set[str]:
    """Every header file basename living under `base` (recursively)."""
    return {h.name for h in base.rglob("*") if h.suffix in HEADER_SUFFIXES}


def scan(repo: Path) -> list[str]:
    violations: list[str] = []
    # Sibling toolbox names — a toolbox may depend on another toolbox's compute
    # (all L2 peers), so they're allowed includes; discovered here rather than
    # hard-coded in ALLOWED.
    tb_root = repo / "src" / "toolboxes"
    toolbox_names = {p.name for p in tb_root.iterdir() if p.is_dir()} if tb_root.is_dir() else set()
    for layer, dirs in LAYER_DIRS.items():
        allowed = ALLOWED[layer]
        if layer == "toolboxes":
            allowed = allowed | toolbox_names
        strict = layer in STRICT_QUOTED
        # For strict layers, the set of header basenames the layer owns — a
        # quoted include resolving outside this set is a cross-layer leak.
        own_headers: set[str] = set()
        if strict:
            for d in dirs:
                base = repo / d
                if base.is_dir():
                    own_headers |= _header_basenames(base)
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
                # Sanctioned core-coupled installers (registration ABI): each
                # toolbox's AND graphics' library.{cpp,hpp}. These define
                # <Lib>Library::install(Engine&); graphics' library.cpp also
                # hosts the generic CallContext→GraphicsContext adapter. The
                # compute TUs around them (toolbox math, graphics' plots.cpp)
                # are scanned and MUST stay core-free.
                if f.name in ("library.cpp", "library.hpp") and layer in (
                    "toolboxes",
                    "graphics",
                ):
                    continue
                # Toolbox-only extra exemption: io's type.cpp (legitimately
                # Engine& — it writes via engine.outputText). Every other toolbox
                # compute TU must stay core/runtime-free. (The former `/graph/`
                # exemption is gone — that AST→NodeGraph analysis pass was renamed
                # and relocated out of toolboxes/ to the `scriptgraph` layer, which
                # is honestly `core`-allowed in ALLOWED, so it needs no exemption.)
                if layer == "toolboxes" and (
                    f.as_posix().endswith("toolboxes/io/src/text/type.cpp")
                ):
                    continue
                rel = f.relative_to(repo).as_posix()
                for n, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                    m = INCLUDE_RE.search(line)
                    if m and m.group(1) not in allowed:
                        violations.append(
                            f"{rel}:{n}: layer '{layer}' must not include "
                            f"<numkit/{m.group(1)}/...>  (allowed: {sorted(allowed)})"
                        )
                    # Quoted-include cross-layer-leak check (strict layers only).
                    if strict:
                        mq = QUOTED_INCLUDE_RE.search(line)
                        if mq:
                            basename = mq.group(1).rsplit("/", 1)[-1]
                            if basename not in own_headers:
                                violations.append(
                                    f"{rel}:{n}: layer '{layer}' quoted-includes "
                                    f'"{mq.group(1)}" — not part of the {layer}/ tree '
                                    f"(cross-layer leak: use <numkit/...> for a cross-module "
                                    f"dependency, or relocate the header into {layer}/)"
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
            "(value/fs/ops/core + math/lang + toolbox compute) must not depend on "
            "a layer above them — math/lang and toolbox compute must stay "
            "core/runtime-free (toolbox installers + io/type.cpp + graph exempt).",
            file=sys.stderr,
        )
        return 1
    print("Layering OK: value, fs, ops, core, figure, math, lang, graphics, scriptgraph, toolboxes respect the dependency direction.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
