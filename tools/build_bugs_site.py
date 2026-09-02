#!/usr/bin/env python3
"""build_bugs_site.py — builds a standalone Docsify site for NumKit Bugs & Parity Catalog."""

import os
import re
import sys
import shutil
import argparse
from pathlib import Path

if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except AttributeError:
        pass

ROOT = Path(__file__).resolve().parent.parent
BUGS_SRC = ROOT / 'bugs'

INDEX_HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>NumKit Defect &amp; Parity Catalog</title>
  <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1" />
  <meta name="description" content="Structured defect catalog and MATLAB parity tracking for NumKit.">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, minimum-scale=1.0">
  
  <!-- Modern Dark/Light Theme -->
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/docsify-themeable@0/dist/css/theme-simple-dark.css" id="theme-dark">
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/docsify-themeable@0/dist/css/theme-simple.css" id="theme-light" disabled>
  
  <style>
    :root {
      --base-font-size: 15px;
      --theme-color: #38bdf8;
      --sidebar-width: 320px;
      --content-max-width: 72em;
      --code-font-family: 'JetBrains Mono', 'Fira Code', Consolas, monospace;
      --code-font-size: 13.5px;
    }
    
    /* Sidebar list item and icon alignment */
    .sidebar-nav ul {
      margin: 0;
      padding-left: 14px;
    }
    .sidebar-nav li {
      margin: 5px 0 !important;
    }
    .sidebar-nav li > a {
      display: inline-flex !important;
      align-items: center !important;
      gap: 8px !important;
      font-weight: 500;
      line-height: 1.4 !important;
    }
    .sidebar-nav > ul > li > a {
      font-weight: 600 !important;
      font-size: 14.5px;
    }

    /* Custom Badges */
    .badge-open {
      background: #ef4444;
      color: #ffffff;
      padding: 3px 9px;
      border-radius: 9999px;
      font-size: 12px;
      font-weight: 700;
      display: inline-block;
    }
    .badge-fixed {
      background: #10b981;
      color: #ffffff;
      padding: 3px 9px;
      border-radius: 9999px;
      font-size: 12px;
      font-weight: 700;
      display: inline-block;
    }
    .badge-missing {
      background: #ec4899;
      color: #ffffff;
      padding: 3px 9px;
      border-radius: 9999px;
      font-size: 12px;
      font-weight: 700;
      display: inline-block;
    }
    .badge-p0 { background: #dc2626; color: white; padding: 2px 7px; border-radius: 4px; font-size: 11px; font-weight: 600; }
    .badge-p1 { background: #ea580c; color: white; padding: 2px 7px; border-radius: 4px; font-size: 11px; font-weight: 600; }
    .badge-p2 { background: #d97706; color: white; padding: 2px 7px; border-radius: 4px; font-size: 11px; font-weight: 600; }
    .badge-p3 { background: #2563eb; color: white; padding: 2px 7px; border-radius: 4px; font-size: 11px; font-weight: 600; }
    .badge-date { background: #334155; color: #94a3b8; padding: 2px 6px; border-radius: 4px; font-size: 11.5px; font-family: monospace; }

    /* Metric Cards */
    .metric-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(210px, 1fr));
      gap: 16px;
      margin: 20px 0 30px 0;
    }
    .metric-card {
      background: rgba(30, 41, 59, 0.7);
      border: 1px solid rgba(255, 255, 255, 0.08);
      border-radius: 12px;
      padding: 18px 20px;
      box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
      text-decoration: none !important;
      display: block;
      transition: transform 0.2s ease, border-color 0.2s ease;
    }
    .metric-card:hover {
      transform: translateY(-2px);
      border-color: rgba(56, 189, 248, 0.4);
    }
    .metric-card h3 {
      margin: 0 0 6px 0;
      font-size: 12.5px;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      color: #94a3b8;
    }
    .metric-card .value {
      font-size: 28px;
      font-weight: 800;
      color: #f8fafc;
    }

    /* Tables */
    table {
      width: 100% !important;
      display: table !important;
      border-collapse: collapse;
      margin: 20px 0;
    }
    th {
      background: rgba(30, 41, 59, 0.9) !important;
      color: #cbd5e1 !important;
      font-weight: 600;
      text-align: left;
      padding: 10px 14px;
      border-bottom: 2px solid rgba(255, 255, 255, 0.1);
    }
    td {
      padding: 10px 14px;
      border-bottom: 1px solid rgba(255, 255, 255, 0.05);
      vertical-align: middle;
    }

    /* Theme Toggle */
    .theme-toggle-btn {
      position: fixed;
      bottom: 20px;
      right: 20px;
      z-index: 1000;
      background: #1e293b;
      color: #f8fafc;
      border: 1px solid #334155;
      border-radius: 50%;
      width: 42px;
      height: 42px;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      box-shadow: 0 4px 12px rgba(0,0,0,0.3);
      transition: all 0.2s ease;
    }
    .theme-toggle-btn:hover {
      transform: scale(1.08);
      background: #334155;
    }
    
    pre[class*="language-"] {
      border-radius: 8px;
      border: 1px solid rgba(255,255,255,0.08);
    }
  </style>
</head>
<body>
  <div id="app">Loading NumKit Defect Catalog...</div>
  
  <button class="theme-toggle-btn" id="themeToggle" title="Toggle Dark/Light Mode">🌓</button>

  <script>
    window.$docsify = {
      name: 'NumKit Bugs &amp; Parity',
      repo: 'https://github.com/hotredman/numkit',
      loadSidebar: true,
      loadNavbar: true,
      subMaxLevel: 2,
      auto2top: true,
      alias: {
        '/.*/_sidebar.md': '/_sidebar.md',
        '/opened/.*/_sidebar.md': '/opened/_sidebar.md',
        '/closed/.*/_sidebar.md': '/closed/_sidebar.md',
        '/missing/.*/_sidebar.md': '/missing/_sidebar.md'
      },
      search: {
        maxAge: 86400000,
        paths: 'auto',
        placeholder: 'Search defects, functions, errors...',
        noData: 'No defects found',
        depth: 3
      },
      copyCode: {
        buttonText: 'Copy',
        errorText: 'Error',
        successText: 'Copied'
      },
      pagination: {
        previousText: 'Previous',
        nextText: 'Next',
        crossChapter: true
      }
    };
  </script>

  <script src="https://cdn.jsdelivr.net/npm/docsify@4/lib/docsify.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/docsify@4/lib/plugins/search.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/docsify-copy-code@2/dist/docsify-copy-code.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/docsify-pagination@2/dist/docsify-pagination.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/docsify/lib/plugins/zoom-image.min.js"></script>

  <script src="https://cdn.jsdelivr.net/npm/prismjs@1/components/prism-matlab.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/prismjs@1/components/prism-c.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/prismjs@1/components/prism-cpp.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/prismjs@1/components/prism-bash.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/prismjs@1/components/prism-json.min.js"></script>

  <script>
    const darkLink = document.getElementById('theme-dark');
    const lightLink = document.getElementById('theme-light');
    const toggleBtn = document.getElementById('themeToggle');

    let currentTheme = localStorage.getItem('numkit_docs_theme') || 'dark';
    function applyTheme(theme) {
      if (theme === 'light') {
        darkLink.setAttribute('disabled', 'true');
        lightLink.removeAttribute('disabled');
      } else {
        lightLink.setAttribute('disabled', 'true');
        darkLink.removeAttribute('disabled');
      }
      localStorage.setItem('numkit_docs_theme', theme);
    }
    applyTheme(currentTheme);

    toggleBtn.addEventListener('click', () => {
      currentTheme = currentTheme === 'dark' ? 'light' : 'dark';
      applyTheme(currentTheme);
    });
  </script>
</body>
</html>
"""

def parse_bug(md_path):
    txt = md_path.read_text(encoding='utf-8', errors='replace')
    title = md_path.stem
    severity = 'P2'
    kind = 'bug'
    status = 'OPEN' if 'opened' in md_path.parts else 'FIXED'
    found_date = '-'
    fixed_date = '-'
    commit = '-'
    guard = '-'

    lines = txt.splitlines()
    if lines and lines[0].startswith('#'):
        title = lines[0].lstrip('#').strip()

    m_status = re.search(r'\*\*Status:\*\*\s*([^\n]+)', txt)
    if m_status:
        st_raw = m_status.group(1).strip()
        if 'FIXED' in st_raw:
            status = 'FIXED'
            m_fix_info = re.search(r'FIXED\s*\(([^,]+),\s*([0-9]{4}-[0-9]{2}-[0-9]{2})\)', st_raw)
            if m_fix_info:
                commit = m_fix_info.group(1).strip()
                fixed_date = m_fix_info.group(2).strip()
            else:
                m_c = re.search(r'FIXED\s*\(([^)]+)\)', st_raw)
                if m_c:
                    commit = m_c.group(1).strip()
        elif 'OPEN' in st_raw:
            status = 'OPEN'

    m_found = re.search(r'\*\*Found:\*\*\s*([0-9]{4}-[0-9]{2}-[0-9]{2})', txt)
    if m_found:
        found_date = m_found.group(1)

    m_sev = re.search(r'\*\*Severity:\*\*\s*(P[0-3])', txt)
    if m_sev:
        severity = m_sev.group(1)

    m_kind = re.search(r'\*\*Kind:\*\*\s*([\w-]+)', txt)
    if m_kind:
        kind = m_kind.group(1)

    m_guard = re.search(r'\*\*Guard:\*\*\s*`?([A-Za-z0-9_]+)`?', txt)
    if m_guard:
        guard = m_guard.group(1)

    return {
        'title': title,
        'namespace': md_path.parent.name,
        'filename': md_path.name,
        'severity': severity,
        'kind': kind,
        'status': status,
        'found_date': found_date,
        'fixed_date': fixed_date,
        'commit': commit,
        'guard': guard,
        'path': md_path
    }

def render_badge(sev):
    return f'<span class="badge-{sev.lower()}">{sev}</span>'

def render_status(st):
    if st == 'OPEN':
        return '<span class="badge-open">OPEN</span>'
    return '<span class="badge-fixed">FIXED</span>'

def build_site(out_dir):
    out_dir = Path(out_dir).resolve()
    print(f"Building NumKit Bugs Docsify site in: {out_dir}")
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    # 1. Copy raw markdown files
    for root, dirs, files in os.walk(BUGS_SRC):
        rel = Path(root).relative_to(BUGS_SRC)
        dest_sub = out_dir / rel
        dest_sub.mkdir(parents=True, exist_ok=True)
        for f in files:
            if f.endswith('.md'):
                shutil.copy2(Path(root) / f, dest_sub / f)

    # 2. Parse opened and closed bugs
    opened_list = []
    opened_by_ns = {}
    for md in sorted((BUGS_SRC / 'opened').glob('*/*.md')):
        b = parse_bug(md)
        opened_list.append(b)
        opened_by_ns.setdefault(b['namespace'], []).append(b)

    closed_list = []
    closed_by_ns = {}
    for md in sorted((BUGS_SRC / 'closed').glob('*/*.md')):
        b = parse_bug(md)
        closed_list.append(b)
        closed_by_ns.setdefault(b['namespace'], []).append(b)

    # Sort opened by found_date descending (newest first)
    opened_sorted = sorted(opened_list, key=lambda x: (x['found_date'] != '-', x['found_date']), reverse=True)
    # Sort closed by fixed_date descending (newest first)
    closed_sorted = sorted(closed_list, key=lambda x: (x['fixed_date'] != '-', x['fixed_date'], x['found_date']), reverse=True)

    total_opened = len(opened_list)
    total_closed = len(closed_list)
    total_bugs = total_opened + total_closed
    fix_rate = (total_closed / total_bugs * 100) if total_bugs else 0.0

    # 3. Parse missing.md
    missing_file = BUGS_SRC / 'missing.md'
    missing_raw = missing_file.read_text(encoding='utf-8', errors='replace') if missing_file.exists() else ''
    missing_count_m = re.search(r'Missing — not implemented\s*\((\d+)\)', missing_raw)
    missing_count = int(missing_count_m.group(1)) if missing_count_m else 839

    # 4. Generate ROOT _sidebar.md (3 Clean Pillars)
    root_sidebar = [
        "* [🏠 **Overview & Dashboard**](README.md)",
        "",
        f"* [🔴 **Open Defects ({total_opened})**](opened/README.md)",
        f"* [✅ **Fixed Defects ({total_closed})**](closed/README.md)",
        f"* [❌ **Missing Functions ({missing_count})**](missing/README.md)",
        "",
        "---",
        "* [🐙 **GitHub Repository**](https://github.com/hotredman/numkit)",
        "* [📚 **Doxygen C++ Docs**](https://hotredman.github.io/numkit-doxy/)"
    ]
    (out_dir / '_sidebar.md').write_text('\n'.join(root_sidebar), encoding='utf-8')

    # 5. Generate OPENED _sidebar.md
    opened_sidebar = [
        "* [⬅️ **Back to Main Dashboard**](README.md)",
        "",
        f"* [📋 **All Open Defects ({total_opened})**](opened/README.md)",
        "",
        "* 🔴 **By Subsystem / Namespace:**"
    ]
    for ns, bugs in sorted(opened_by_ns.items()):
        opened_sidebar.append(f"  * **{ns}** ({len(bugs)})")
        for b in bugs:
            clean_title = b['title'].replace('[', '(').replace(']', ')')
            opened_sidebar.append(f"    * [{clean_title}](opened/{ns}/{b['filename']})")
    
    opened_sidebar_txt = '\n'.join(opened_sidebar)
    (out_dir / 'opened' / '_sidebar.md').write_text(opened_sidebar_txt, encoding='utf-8')
    for ns in opened_by_ns.keys():
        ns_dir = out_dir / 'opened' / ns
        ns_dir.mkdir(parents=True, exist_ok=True)
        (ns_dir / '_sidebar.md').write_text(opened_sidebar_txt, encoding='utf-8')

    # 6. Generate CLOSED _sidebar.md
    closed_sidebar = [
        "* [⬅️ **Back to Main Dashboard**](README.md)",
        "",
        f"* [📋 **All Fixed Defects ({total_closed})**](closed/README.md)",
        "",
        "* ✅ **By Subsystem / Namespace:**"
    ]
    for ns, bugs in sorted(closed_by_ns.items()):
        closed_sidebar.append(f"  * **{ns}** ({len(bugs)})")
        for b in bugs:
            clean_title = b['title'].replace('[', '(').replace(']', ')')
            closed_sidebar.append(f"    * [{clean_title}](closed/{ns}/{b['filename']})")
    
    closed_sidebar_txt = '\n'.join(closed_sidebar)
    (out_dir / 'closed' / '_sidebar.md').write_text(closed_sidebar_txt, encoding='utf-8')
    for ns in closed_by_ns.keys():
        ns_dir = out_dir / 'closed' / ns
        ns_dir.mkdir(parents=True, exist_ok=True)
        (ns_dir / '_sidebar.md').write_text(closed_sidebar_txt, encoding='utf-8')

    # 7. Generate MISSING _sidebar.md and missing/README.md
    (out_dir / 'missing').mkdir(parents=True, exist_ok=True)
    missing_sidebar = [
        "* [⬅️ **Back to Main Dashboard**](README.md)",
        "",
        f"* [❌ **Missing Functions ({missing_count})**](missing/README.md)",
        "",
        "* 🔍 [🔴 Open Defects](opened/README.md)",
        "* 🔍 [✅ Fixed Defects](closed/README.md)"
    ]
    (out_dir / 'missing' / '_sidebar.md').write_text('\n'.join(missing_sidebar), encoding='utf-8')

    # Extract missing functions content (if split by ## ❌ Missing)
    if '## ❌ Missing — not implemented' in missing_raw:
        missing_body = '## ❌ Missing — not implemented' + missing_raw.split('## ❌ Missing — not implemented')[1]
    else:
        missing_body = missing_raw

    missing_readme = [
        f"# ❌ Missing Functions Backlog ({missing_count})\n",
        "> The complete catalog of MATLAB R2025b functions and toolboxes not yet implemented in NumKit, categorized by domain.\n",
        missing_body
    ]
    (out_dir / 'missing' / 'README.md').write_text('\n'.join(missing_readme), encoding='utf-8')

    # 8. Generate _navbar.md
    navbar = [
        f"* [🔴 Open ({total_opened})](opened/README.md)",
        f"* [✅ Closed ({total_closed})](closed/README.md)",
        f"* [❌ Missing ({missing_count})](missing/README.md)",
        "* [🐙 GitHub](https://github.com/hotredman/numkit)",
        "* [📚 Doxygen](https://hotredman.github.io/numkit-doxy/)"
    ]
    (out_dir / '_navbar.md').write_text('\n'.join(navbar), encoding='utf-8')

    # 9. Generate MAIN README.md (Dashboard with 4 KPI cards and tables)
    main_readme = []
    main_readme.append("# 🐞 NumKit Defect & MATLAB Parity Dashboard\n")
    main_readme.append("Welcome to the structured defect catalog and parity tracking system for **NumKit** (MATLAB/Octave-compatible C++ runtime).\n")
    
    # 4 Metric Cards with links
    main_readme.append('<div class="metric-grid">')
    main_readme.append(f'<a href="#/opened/README" class="metric-card"><h3>Active Open Defects</h3><div class="value" style="color:#ef4444;">{total_opened}</div></a>')
    main_readme.append(f'<a href="#/closed/README" class="metric-card"><h3>Resolved &amp; Fixed</h3><div class="value" style="color:#10b981;">{total_closed}</div></a>')
    main_readme.append(f'<a href="#/closed/README" class="metric-card"><h3>Resolution Rate</h3><div class="value" style="color:#38bdf8;">{fix_rate:.1f}%</div></a>')
    main_readme.append(f'<a href="#/missing/README" class="metric-card"><h3>Missing Backlog</h3><div class="value" style="color:#ec4899;">{missing_count}</div></a>')
    main_readme.append('</div>\n')

    # Open Bugs Table
    main_readme.append(f"## 🔴 Active Open Defects ({total_opened})\n")
    main_readme.append("| Found Date | Subsystem | Defect / Function | Sev | Kind | Status |")
    main_readme.append("| :---: | :---: | :--- | :---: | :---: | :---: |")
    for b in opened_sorted:
        clean_title = b['title'].replace('|', '/')
        link = f"[{clean_title}](opened/{b['namespace']}/{b['filename']})"
        f_date = f"`{b['found_date']}`" if b['found_date'] != '-' else '-'
        main_readme.append(f"| {f_date} | **{b['namespace']}** | {link} | {render_badge(b['severity'])} | `{b['kind']}` | {render_status(b['status'])} |")

    main_readme.append("\n---\n")

    # Recent Fixes Table (Top 20 recently fixed)
    main_readme.append(f"## ✅ Recently Resolved Defects\n")
    main_readme.append("| Fixed Date | Subsystem | Resolved Issue | Commit | Sev | Status |")
    main_readme.append("| :---: | :---: | :--- | :---: | :---: | :---: |")
    for b in closed_sorted[:20]:
        clean_title = b['title'].replace('|', '/')
        link = f"[{clean_title}](closed/{b['namespace']}/{b['filename']})"
        fx_date = f"`{b['fixed_date']}`" if b['fixed_date'] != '-' else (f"`{b['found_date']}`" if b['found_date'] != '-' else '-')
        commit_str = f"`{b['commit'][:9]}`" if b['commit'] != '-' else '-'
        main_readme.append(f"| {fx_date} | **{b['namespace']}** | {link} | {commit_str} | {render_badge(b['severity'])} | {render_status(b['status'])} |")

    main_readme.append(f"\n👉 *[View all {total_closed} fixed issues in the complete Archive →](closed/README.md)*\n")

    (out_dir / 'README.md').write_text('\n'.join(main_readme), encoding='utf-8')

    # 10. Generate opened/README.md
    opened_readme = [
        f"# 🔴 Open Defects Registry ({total_opened})\n",
        "Complete register of all currently active defects and feature gaps.\n",
        "| Found Date | Subsystem | Defect / Function | Sev | Kind | Guard Test |",
        "| :---: | :---: | :--- | :---: | :---: | :--- |"
    ]
    for b in opened_sorted:
        clean_title = b['title'].replace('|', '/')
        link = f"[{clean_title}](opened/{b['namespace']}/{b['filename']})"
        f_date = f"`{b['found_date']}`" if b['found_date'] != '-' else '-'
        guard_str = f"`{b['guard']}`" if b['guard'] != '-' else '-'
        opened_readme.append(f"| {f_date} | **{b['namespace']}** | {link} | {render_badge(b['severity'])} | `{b['kind']}` | {guard_str} |")
    
    (out_dir / 'opened' / 'README.md').write_text('\n'.join(opened_readme), encoding='utf-8')

    # 11. Generate closed/README.md
    closed_readme = [
        f"# ✅ Closed &amp; Resolved Defects Registry ({total_closed})\n",
        "Complete historical changelog of all resolved bugs and fixed regressions.\n",
        "| Fixed Date | Subsystem | Resolved Issue | Commit | Sev | Kind |",
        "| :---: | :---: | :--- | :---: | :---: | :---: |"
    ]
    for b in closed_sorted:
        clean_title = b['title'].replace('|', '/')
        link = f"[{clean_title}](closed/{b['namespace']}/{b['filename']})"
        fx_date = f"`{b['fixed_date']}`" if b['fixed_date'] != '-' else (f"`{b['found_date']}`" if b['found_date'] != '-' else '-')
        commit_str = f"`{b['commit'][:9]}`" if b['commit'] != '-' else '-'
        closed_readme.append(f"| {fx_date} | **{b['namespace']}** | {link} | {commit_str} | {render_badge(b['severity'])} | `{b['kind']}` |")

    (out_dir / 'closed' / 'README.md').write_text('\n'.join(closed_readme), encoding='utf-8')

    # 12. Generate index.html and .nojekyll
    (out_dir / 'index.html').write_text(INDEX_HTML_TEMPLATE, encoding='utf-8')
    (out_dir / '.nojekyll').write_text('', encoding='utf-8')

    print(f"[OK] Generated Docsify site: {total_opened} open, {total_closed} closed, {missing_count} missing.")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Build NumKit Bugs Docsify site.')
    parser.add_argument('--output', '-o', default=str(ROOT / 'build' / 'bugs'), help='Output directory')
    args = parser.parse_args()
    build_site(args.output)
