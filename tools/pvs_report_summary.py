#!/usr/bin/env python3
"""pvs_report_summary.py — converts PVS-Studio analysis results into structured reports in bugs/pvs/.

Usage:
  python tools/pvs_report_summary.py --input <pvs_report.json> [--output-dir bugs/pvs] [--commit]
"""

import os
import sys
import json
import argparse
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

def run_cmd(cmd, cwd=ROOT):
    res = subprocess.run(cmd, cwd=str(cwd), capture_output=True, text=True, encoding='utf-8', errors='replace')
    return res

def get_git_info():
    rev = run_cmd(['git', 'rev-parse', '--short', 'HEAD']).stdout.strip() or 'unknown'
    branch = run_cmd(['git', 'rev-parse', '--abbrev-ref', 'HEAD']).stdout.strip() or 'main'
    return rev, branch

def normalize_path(p):
    try:
        rel = Path(p).resolve().relative_to(ROOT)
        return str(rel).replace('\\', '/')
    except Exception:
        return str(p).replace('\\', '/')

def parse_pvs_json(json_path):
    with open(json_path, 'r', encoding='utf-8', errors='replace') as f:
        data = json.load(f)

    if isinstance(data, dict):
        raw_warnings = data.get('warnings', [])
    elif isinstance(data, list):
        raw_warnings = data
    else:
        raw_warnings = []

    parsed = []
    for w in raw_warnings:
        code = w.get('code', 'V???')
        cwe = w.get('cwe')
        level_raw = w.get('level', 2)
        if isinstance(level_raw, int):
            level_map = {1: 'High', 2: 'Medium', 3: 'Low'}
            severity = level_map.get(level_raw, 'Medium')
        else:
            severity = str(level_raw).capitalize()

        message = w.get('message', '')
        positions = w.get('positions', [])
        if positions:
            pos = positions[0]
            file_path = normalize_path(pos.get('file', ''))
            line = pos.get('line', 0)
            col = pos.get('column', 0)
        else:
            file_path = normalize_path(w.get('file', ''))
            line = w.get('line', 0)
            col = w.get('column', 0)

        # Categorize subsystem
        subsystem = 'other'
        if file_path.startswith('src/core/'):
            subsystem = 'core'
        elif file_path.startswith('src/toolboxes/'):
            parts = file_path.split('/')
            subsystem = parts[2] if len(parts) > 2 else 'toolboxes'
        elif file_path.startswith('src/bundle/'):
            subsystem = 'bundle'
        elif file_path.startswith('src/codegen/'):
            subsystem = 'codegen'
        elif file_path.startswith('wasm/'):
            subsystem = 'wasm'
        elif file_path.startswith('apps/'):
            subsystem = 'apps'

        parsed.append({
            'code': code,
            'cwe': cwe,
            'severity': severity,
            'file': file_path,
            'line': line,
            'column': col,
            'message': message,
            'subsystem': subsystem
        })

    return parsed

def generate_markdown(warnings, git_rev, git_branch):
    high = [w for w in warnings if w['severity'] == 'High']
    med = [w for w in warnings if w['severity'] == 'Medium']
    low = [w for w in warnings if w['severity'] == 'Low']

    now_str = datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M UTC')

    md = []
    md.append("# PVS-Studio Static Analysis Report\n")
    md.append(f"> **Commit:** [`{git_rev}`] (branch `{git_branch}`) | **Updated:** {now_str}\n")
    md.append("## Summary\n")
    md.append("| Severity | Count | Description |")
    md.append("| :--- | :---: | :--- |")
    md.append(f"| 🔴 **High** | **{len(high)}** | Critical issues: null dereference, buffer overflow, UB |")
    md.append(f"| 🟡 **Medium** | **{len(med)}** | Suspicious logic, potential leaks, unhandled branches |")
    md.append(f"| 🟢 **Low** | **{len(low)}** | Code style, minor performance tips, informational |")
    md.append(f"| **Total** | **{len(warnings)}** | |\n")

    # By subsystem
    subsystems = sorted(set(w['subsystem'] for w in warnings))
    if subsystems:
        md.append("### Warnings by Subsystem\n")
        md.append("| Subsystem | High | Medium | Low | Total |")
        md.append("| :--- | :---: | :---: | :---: | :---: |")
        for sub in subsystems:
            s_high = sum(1 for w in high if w['subsystem'] == sub)
            s_med = sum(1 for w in med if w['subsystem'] == sub)
            s_low = sum(1 for w in low if w['subsystem'] == sub)
            s_tot = s_high + s_med + s_low
            md.append(f"| `{sub}` | {s_high} | {s_med} | {s_low} | **{s_tot}** |")
        md.append("\n---\n")

    # Detailed High Warnings
    if high:
        md.append("## 🔴 High Severity Warnings\n")
        md.append("| Rule | CWE | Location | Message |")
        md.append("| :--- | :--- | :--- | :--- |")
        for w in high:
            cwe_str = f"[CWE-{w['cwe']}](https://cwe.mitre.org/data/definitions/{w['cwe']}.html)" if w['cwe'] else "-"
            loc = f"[`{w['file']}:{w['line']}`](../../{w['file']}#L{w['line']})"
            rule_url = f"https://pvs-studio.com/en/docs/warnings/{w['code'].lower()}/"
            rule_link = f"[{w['code']}]({rule_url})"
            msg = w['message'].replace('|', '&#124;')
            md.append(f"| {rule_link} | {cwe_str} | {loc} | {msg} |")
        md.append("\n")

    # Detailed Medium Warnings
    if med:
        md.append("## 🟡 Medium Severity Warnings\n")
        md.append("| Rule | CWE | Location | Message |")
        md.append("| :--- | :--- | :--- | :--- |")
        for w in med:
            cwe_str = f"[CWE-{w['cwe']}](https://cwe.mitre.org/data/definitions/{w['cwe']}.html)" if w['cwe'] else "-"
            loc = f"[`{w['file']}:{w['line']}`](../../{w['file']}#L{w['line']})"
            rule_url = f"https://pvs-studio.com/en/docs/warnings/{w['code'].lower()}/"
            rule_link = f"[{w['code']}]({rule_url})"
            msg = w['message'].replace('|', '&#124;')
            md.append(f"| {rule_link} | {cwe_str} | {loc} | {msg} |")
        md.append("\n")

    # Collapsed Low Warnings
    if low:
        md.append("<details>")
        md.append(f"<summary><b>🟢 Low Severity Warnings ({len(low)})</b></summary>\n")
        md.append("| Rule | Location | Message |")
        md.append("| :--- | :--- | :--- |")
        for w in low:
            loc = f"`{w['file']}:{w['line']}`"
            rule_url = f"https://pvs-studio.com/en/docs/warnings/{w['code'].lower()}/"
            rule_link = f"[{w['code']}]({rule_url})"
            msg = w['message'].replace('|', '&#124;')
            md.append(f"| {rule_link} | {loc} | {msg} |")
        md.append("\n</details>\n")

    return '\n'.join(md)

def main():
    parser = argparse.ArgumentParser(description='Convert PVS-Studio report to bugs/pvs/ summary.')
    parser.add_argument('--input', required=True, help='Path to PVS JSON report')
    parser.add_argument('--output-dir', default=str(ROOT / 'bugs' / 'pvs'), help='Output directory')
    parser.add_argument('--commit', action='store_true', help='Commit updated report if clean')
    args = parser.parse_args()

    input_path = Path(args.input)
    if not input_path.exists():
        print(f"ERROR: Input file not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    warnings = parse_pvs_json(input_path)
    git_rev, git_branch = get_git_info()

    summary_md = generate_markdown(warnings, git_rev, git_branch)

    # Write summary.md
    summary_path = out_dir / 'summary.md'
    summary_path.write_text(summary_md, encoding='utf-8')

    # Copy / save latest.json
    latest_json_path = out_dir / 'latest.json'
    latest_json_path.write_text(json.dumps(warnings, indent=2, ensure_ascii=False), encoding='utf-8')

    high_count = sum(1 for w in warnings if w['severity'] == 'High')
    med_count = sum(1 for w in warnings if w['severity'] == 'Medium')
    low_count = sum(1 for w in warnings if w['severity'] == 'Low')

    print(f"\n=======================================================")
    print(f"  PVS-Studio Summary Generated: {summary_path}")
    print(f"  High: {high_count} | Medium: {med_count} | Low: {low_count} | Total: {len(warnings)}")
    print(f"=======================================================\n")

    if args.commit:
        # Check if working tree is clean outside bugs/pvs
        st = run_cmd(['git', 'status', '--porcelain'])
        lines = [l.strip() for l in st.stdout.splitlines() if l.strip()]
        dirty_other = [l for l in lines if not ('bugs/pvs' in l or 'bugs\\pvs' in l)]
        if dirty_other:
            print("WARNING: Working tree has unstaged changes outside bugs/pvs/. Skipping auto-commit:", file=sys.stderr)
            for d in dirty_other:
                print(f"  {d}", file=sys.stderr)
            print("\nTo commit manually: git add bugs/pvs && git commit -m \"chore(audit): update PVS-Studio report\"")
        else:
            run_cmd(['git', 'add', str(out_dir)])
            commit_msg = f"chore(audit): update PVS-Studio report (High: {high_count}, Med: {med_count})"
            res = run_cmd(['git', 'commit', '-m', commit_msg])
            if res.returncode == 0:
                print("[SUCCESS] Committed report to git.")
            else:
                print(res.stdout or res.stderr)
    else:
        print("To commit the report:\n  git add bugs/pvs && git commit -m \"chore(audit): update PVS-Studio report\"\n")

if __name__ == '__main__':
    main()
