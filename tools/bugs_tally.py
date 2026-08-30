#!/usr/bin/env python3
"""
tools/bugs_tally.py — Dynamically scan bugs/opened and bugs/closed and report status.
"""

import os
import re

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BUGS_DIR = os.path.join(REPO_ROOT, "bugs")
OPENED_DIR = os.path.join(BUGS_DIR, "opened")
CLOSED_DIR = os.path.join(BUGS_DIR, "closed")

def parse_bug_file(filepath):
    info = {
        "path": filepath,
        "relpath": os.path.relpath(filepath, BUGS_DIR).replace("\\", "/"),
        "title": "",
        "status": "UNKNOWN",
        "severity": "P2",
        "kind": "bug",
        "symptom": ""
    }
    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()

    lines = content.splitlines()
    if lines:
        info["title"] = lines[0].lstrip("# ").strip()

    m_sev = re.search(r"\*\*Severity:\*\*\s*(P[0-3])", content, re.IGNORECASE)
    if m_sev:
        info["severity"] = m_sev.group(1).upper()

    m_kind = re.search(r"\*\*Kind:\*\*\s*([a-zA-Z0-9_-]+)", content, re.IGNORECASE)
    if m_kind:
        info["kind"] = m_kind.group(1).lower()

    m_sym = re.search(r"## Symptom\s*\n+([^\n#]+)", content)
    if m_sym:
        info["symptom"] = m_sym.group(1).strip()

    return info

def scan_bugs():
    opened = []
    if os.path.exists(OPENED_DIR):
        for root, _, files in os.walk(OPENED_DIR):
            for f in sorted(files):
                if f.endswith(".md"):
                    opened.append(parse_bug_file(os.path.join(root, f)))

    closed = []
    if os.path.exists(CLOSED_DIR):
        for root, _, files in os.walk(CLOSED_DIR):
            for f in sorted(files):
                if f.endswith(".md"):
                    closed.append(parse_bug_file(os.path.join(root, f)))

    print(f"\n" + "=" * 70)
    print(f"  NumKit Bug Catalog Tally: {len(closed)} Fixed, {len(opened)} Open (Total: {len(closed) + len(opened)})")
    print("=" * 70 + "\n")

    if opened:
        print(f"[OPEN] ({len(opened)}):")
        print(f"{'Kind':<15} {'Sev':<5} {'Path':<45} {'Title'}")
        print("-" * 100)
        for b in sorted(opened, key=lambda x: (x["kind"], x["severity"], x["relpath"])):
            print(f"{b['kind']:<15} {b['severity']:<5} {b['relpath']:<45} {b['title']}")
        print()

    print(f"[FIXED] ({len(closed)}): Archived in bugs/closed/\n")

if __name__ == "__main__":
    scan_bugs()
