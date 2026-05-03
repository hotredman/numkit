"""Parse MATLAB R2025b's local TOC (helpfuncbycat.xml) into a category tree.

The XML is a tree of <tocitem target="..."> elements. Top-level <tocitem>
nodes that have a `target=*.html` link but contain nested <tocitem>s with
<name> children are SECTIONS. Leaf <tocitem>s with a <name>+<purpose> are
FUNCTIONS.

Usage:
    python tools/parity/extract_local_ref.py [--csv]   # default: tree dump
    python tools/parity/extract_local_ref.py --json    # full hierarchy as JSON
"""
import argparse
import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

TOC = Path(r"C:/Program Files/MATLAB/R2025b/help/matlab/helpfuncbycat.xml")


def section_name_from_target(t: str) -> str:
    # "ref/ans.html" -> NOT a section. "entering-commands.html" -> "Entering Commands"
    base = t.replace("ref/", "").replace(".html", "")
    if base.startswith("ref/"):
        return ""
    # Section pages: "language-fundamentals", "matrices-and-arrays" etc.
    return base.replace("-", " ").title()


def walk(elem, path, out):
    """Walk a <tocitem>. `path` is the section breadcrumb."""
    if elem.tag != "tocitem":
        return
    name_node = elem.find("name")
    if name_node is not None:
        # It's a function leaf.
        purpose = elem.findtext("purpose", default="").strip()
        out.append({
            "name": name_node.text.strip() if name_node.text else "",
            "purpose": purpose,
            "path": list(path),
        })
        return
    # Section node: text content of element is the human label.
    label = (elem.text or "").strip()
    if not label:
        # Could happen for the root <toc>; skip mid-walk.
        for child in elem:
            walk(child, path, out)
        return
    new_path = path + [label]
    for child in elem:
        walk(child, new_path, out)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", action="store_true", help="emit JSON list")
    parser.add_argument("--csv", action="store_true", help="emit CSV: top|sub|name|purpose")
    parser.add_argument("--by-section", action="store_true", help="grouped tree dump")
    args = parser.parse_args()

    text = TOC.read_text(encoding="utf-8")
    # Tolerate any stray invalid chars from the proprietary XML.
    text = re.sub(r"[\x00-\x08\x0b\x0c\x0e-\x1f]", "", text)
    root = ET.fromstring(text)

    out = []
    for child in root:
        walk(child, [], out)

    sys.stdout.reconfigure(encoding="utf-8")
    if args.json:
        json.dump(out, sys.stdout, indent=2, ensure_ascii=False)
        return
    if args.csv:
        for f in out:
            top = f["path"][1] if len(f["path"]) >= 2 else ""
            sub = f["path"][2] if len(f["path"]) >= 3 else ""
            print(f'{top}|{sub}|{f["name"]}|{f["purpose"]}')
        return
    if args.by_section:
        # Group by top-level section (path[1]) > sub (path[2]).
        from collections import defaultdict
        tree = defaultdict(lambda: defaultdict(list))
        for f in out:
            top = f["path"][1] if len(f["path"]) >= 2 else "(root)"
            sub = f["path"][2] if len(f["path"]) >= 3 else "(direct)"
            tree[top][sub].append(f)
        for top in sorted(tree.keys()):
            print(f"\n## {top}  ({sum(len(v) for v in tree[top].values())} fns)")
            for sub in sorted(tree[top].keys()):
                items = tree[top][sub]
                print(f"  ### {sub}  ({len(items)} fns)")
                for f in items:
                    print(f"    {f['name']:30s}  {f['purpose'][:80]}")
        return

    # default: counts
    from collections import Counter
    top_counts = Counter()
    for f in out:
        top = f["path"][1] if len(f["path"]) >= 2 else "(root)"
        top_counts[top] += 1
    total = sum(top_counts.values())
    print(f"Total: {total} functions across {len(top_counts)} top-level sections\n")
    for top, n in sorted(top_counts.items(), key=lambda x: -x[1]):
        print(f"  {n:4d}  {top}")


if __name__ == "__main__":
    main()
