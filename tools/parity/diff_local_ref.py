"""Diff local MATLAB reference against numkit's PARITY_PROGRESS.md.

Categorizes every MATLAB function as:
  HAVE   — listed in PARITY_PROGRESS.md (regardless of ✅/❌ status)
  MISS   — not listed at all
  SKIP   — explicitly out-of-scope (UI / Java / quantum / OOP / etc.)

Out-of-scope rules apply to the TOC top-level path, not function names.
"""
import argparse
import json
import re
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
# Include MATLAB core + Signal Processing + Stats toolboxes — these
# correspond to our libs/builtin + libs/signal + libs/stats.
TOC_FILES = [
    Path(r"C:/Program Files/MATLAB/R2025b/help/matlab/helpfuncbycat.xml"),
    Path(r"C:/Program Files/MATLAB/R2025b/help/signal/helpfuncbycat.xml"),
    Path(r"C:/Program Files/MATLAB/R2025b/help/stats/helpfuncbycat.xml"),
]
PROGRESS = ROOT / "PARITY_PROGRESS.md"

# ── Out-of-scope ────────────────────────────────────────────────────
# Top-level sections to drop entirely (per project decision: numkit is
# a scientific-compute lib, not an IDE / language interop runtime).
SKIP_TOP = {
    "App Building",
    "External Language Interfaces",
    "Software Development Tools",
    "Environment and Settings",
}
# Sub-sections to drop within otherwise in-scope tops. Indexed by
# (path[2], path[3]) — the toolbox label is path[0] which we don't
# match here (use SKIP_TOOLBOX_SUB for toolbox-specific).
SKIP_SUB = {
    ("Mathematics", "Quantum Computing"),
    ("Programming", "Classes"),
    ("Programming", "Security in MATLAB Code"),
    ("Programming", "Live Scripts and Functions"),
}

# Toolbox-specific drops: (toolbox_label, path[2]) — drop entire
# top-level toolbox section. ML / Code Generation / Simulink are out
# of scope for a pure scientific-compute lib.
SKIP_TOOLBOX_TOP = {
    ("Signal Processing", "AI for Signals"),
    ("Signal Processing", "Code Generation and GPU Support"),
    ("Statistics & ML", "Regression"),
    ("Statistics & ML", "Classification"),
    ("Statistics & ML", "Industrial Statistics"),
    ("Statistics & ML", "Cluster Analysis and Anomaly Detection"),
    ("Statistics & ML", "Dimensionality Reduction and Feature Extraction"),
    ("Statistics & ML", "Simulink and Code Generation"),
    ("Statistics & ML", "ANOVA"),
}
# Function-name prefixes to drop (namespace-style toolbox/ui APIs).
SKIP_PREFIXES = (
    "matlab.ui.", "matlab.app.", "matlab.unittest.",
    "matlab.io.fits.", "matlab.io.hdf4.", "matlab.io.hdf5.",
    "matlab.io.nc.", "matlab.io.parquet.",
    "matlab.automation.", "matlab.system.",
    "matlab.net.", "matlab.web.",
    "py.", "matlab.dotnet.", "matlab.com.",
)


def walk(elem, path, out):
    if elem.tag != "tocitem":
        return
    name_node = elem.find("name")
    if name_node is not None:
        out.append({
            "name": (name_node.text or "").strip(),
            "purpose": elem.findtext("purpose", default="").strip(),
            "path": list(path),
        })
        return
    label = (elem.text or "").strip()
    if not label:
        for c in elem:
            walk(c, path, out)
        return
    new_path = path + [label]
    for c in elem:
        walk(c, new_path, out)


def in_scope(fn) -> bool:
    if any(fn["name"].startswith(p) for p in SKIP_PREFIXES):
        return False
    p = fn["path"]
    # path[0] is toolbox label, path[1] is "Functions" wrapper, path[2]
    # is the top section, path[3] is the sub-section.
    if len(p) >= 3 and p[2] in SKIP_TOP:
        return False
    if len(p) >= 4 and (p[2], p[3]) in SKIP_SUB:
        return False
    if len(p) >= 3 and (p[0], p[2]) in SKIP_TOOLBOX_TOP:
        return False
    return True


def load_numkit_names() -> set[str]:
    text = PROGRESS.read_text(encoding="utf-8")
    names = set()
    for m in re.finditer(r"^\| `([^`]+)`", text, re.MULTILINE):
        names.add(m.group(1).lower())
    return names


def load_numkit_status() -> dict[str, str]:
    """Return {name_lower: status} where status is "OK" (✅), "STUB" (❌)
    or "PARTIAL" (⚠️). If a name appears in multiple sections, prefer
    the strongest status."""
    text = PROGRESS.read_text(encoding="utf-8")
    status = {}
    rank = {"OK": 3, "PARTIAL": 2, "STUB": 1}
    for m in re.finditer(r"^\| `([^`]+)` \| ([^ ]+) \|", text, re.MULTILINE):
        name = m.group(1).lower()
        sym = m.group(2)
        st = "OK" if sym == "✅" else ("PARTIAL" if sym == "⚠️" else "STUB")
        if name not in status or rank[st] > rank[status[name]]:
            status[name] = st
    return status


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--show", choices=["miss", "stub", "impl", "all"], default="miss")
    ap.add_argument("--by-section", action="store_true")
    ap.add_argument("--summary", action="store_true")
    args = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")

    matlab_fns = []
    for toc in TOC_FILES:
        if not toc.exists():
            continue
        text = toc.read_text(encoding="utf-8")
        text = re.sub(r"[\x00-\x08\x0b\x0c\x0e-\x1f]", "", text)
        root = ET.fromstring(text)
        # Tag each function with its source toolbox via the toc filename.
        # Stats & signal TOCs use their own top-level "Functions" wrapper,
        # so we tag explicitly here so the output reads "Signal Processing"
        # not "Functions".
        toolbox = toc.parent.name
        prefix_label = {
            "matlab": "MATLAB",
            "signal": "Signal Processing",
            "stats": "Statistics & ML",
        }.get(toolbox, toolbox)
        before = len(matlab_fns)
        for c in root:
            walk(c, [], matlab_fns)
        # Prepend toolbox label as the first path element on new entries.
        for fn in matlab_fns[before:]:
            fn["path"].insert(0, prefix_label)

    nk_status = load_numkit_status()
    classified = []
    for fn in matlab_fns:
        if not in_scope(fn):
            cat = "SKIP"
        else:
            st = nk_status.get(fn["name"].lower())
            if st == "OK":          cat = "IMPL"
            elif st == "PARTIAL":   cat = "PARTIAL"
            elif st == "STUB":      cat = "STUB"
            else:                   cat = "MISS"
        classified.append((cat, fn))

    counts = defaultdict(int)
    for cat, _ in classified:
        counts[cat] += 1
    total = len(classified)

    if args.summary:
        in_scope_total = total - counts['SKIP']
        tracked = counts['IMPL'] + counts['PARTIAL'] + counts['STUB']
        print(f"Total MATLAB functions (MATLAB + Signal + Stats): {total}")
        print(f"  IMPL    ✅ — implemented   : {counts['IMPL']:4d}")
        print(f"  PARTIAL ⚠️ — partial       : {counts['PARTIAL']:4d}")
        print(f"  STUB    ❌ — tracked, todo : {counts['STUB']:4d}")
        print(f"  MISS       — not tracked   : {counts['MISS']:4d}")
        print(f"  SKIP       — out of scope  : {counts['SKIP']:4d}")
        print()
        print(f"In-scope total: {in_scope_total}")
        print(f"  Tracked (any status): {tracked} ({100*tracked/in_scope_total:.1f}%)")
        print(f"  Implemented        : {counts['IMPL']} ({100*counts['IMPL']/in_scope_total:.1f}%)")
        return

    if args.by_section:
        sec = defaultdict(lambda: defaultdict(list))
        for cat, fn in classified:
            tb  = fn["path"][0] if len(fn["path"]) >= 1 else "?"
            top = fn["path"][2] if len(fn["path"]) >= 3 else "(root)"
            sub = fn["path"][3] if len(fn["path"]) >= 4 else "(direct)"
            key = f"[{tb}] {top} / {sub}"
            sec[key][cat].append(fn["name"])
        for key in sorted(sec.keys()):
            d = sec[key]
            in_scope_n = sum(len(d[c]) for c in ("IMPL", "PARTIAL", "STUB", "MISS"))
            if in_scope_n == 0:
                continue
            impl = len(d["IMPL"]); part = len(d["PARTIAL"])
            stub = len(d["STUB"]); miss = len(d["MISS"])
            tracked = impl + part + stub
            pct_tracked = 100 * tracked / in_scope_n
            pct_impl = 100 * impl / in_scope_n
            print(f"\n## {key}")
            print(f"   in-scope={in_scope_n}  IMPL={impl}  PARTIAL={part}  STUB={stub}  MISS={miss}")
            print(f"   tracked={pct_tracked:.0f}%  implemented={pct_impl:.0f}%")
            if args.show in ("miss", "all") and d["MISS"]:
                print("   MISS:", ", ".join(sorted(d["MISS"])))
            if args.show in ("stub", "all") and d["STUB"]:
                print("   STUB:", ", ".join(sorted(d["STUB"])))
            if args.show in ("impl", "all") and d["IMPL"]:
                print("   IMPL:", ", ".join(sorted(d["IMPL"])))
        return

    # default: flat list
    want = args.show.upper()
    for cat, fn in classified:
        if cat != want and want != "ALL":
            continue
        top = fn["path"][1] if len(fn["path"]) >= 2 else ""
        sub = fn["path"][2] if len(fn["path"]) >= 3 else ""
        print(f"{cat}  {top:<25}  {sub:<35}  {fn['name']:30s}  {fn['purpose'][:60]}")


if __name__ == "__main__":
    main()
