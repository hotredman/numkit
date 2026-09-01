#!/usr/bin/env python3
"""build_bugs_site.py — builds a standalone Docsify site for NumKit Bugs & Parity Catalog."""

import os
import re
import sys
import shutil
import argparse
from pathlib import Path

# Ensure UTF-8 output on Windows console
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
  <title>NumKit Bugs &amp; Parity Catalog</title>
  <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1" />
  <meta name="description" content="Catalog of known bugs, defect trackers, and MATLAB parity status for NumKit.">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, minimum-scale=1.0">
  
  <!-- Modern Dark/Light Theme -->
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/docsify-themeable@0/dist/css/theme-simple-dark.css" id="theme-dark">
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/docsify-themeable@0/dist/css/theme-simple.css" id="theme-light" disabled>
  
  <style>
    :root {
      --base-font-size: 15px;
      --theme-color: #38bdf8;
      --sidebar-width: 300px;
      --content-max-width: 65em;
      --code-font-family: 'JetBrains Mono', 'Fira Code', Consolas, monospace;
      --code-font-size: 13.5px;
    }
    
    /* Custom NumKit Badges & Styling */
    .badge-open {
      background: #ef4444;
      color: #ffffff;
      padding: 2px 8px;
      border-radius: 9999px;
      font-size: 12px;
      font-weight: bold;
      display: inline-block;
    }
    .badge-fixed {
      background: #10b981;
      color: #ffffff;
      padding: 2px 8px;
      border-radius: 9999px;
      font-size: 12px;
      font-weight: bold;
      display: inline-block;
    }
    .badge-p0 { background: #dc2626; color: white; padding: 2px 6px; border-radius: 4px; font-size: 11px; }
    .badge-p1 { background: #ea580c; color: white; padding: 2px 6px; border-radius: 4px; font-size: 11px; }
    .badge-p2 { background: #d97706; color: white; padding: 2px 6px; border-radius: 4px; font-size: 11px; }
    .badge-p3 { background: #2563eb; color: white; padding: 2px 6px; border-radius: 4px; font-size: 11px; }

    /* Theme toggle button */
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
    
    /* Code block styling */
    pre[class*="language-"] {
      border-radius: 8px;
      border: 1px solid rgba(255,255,255,0.08);
    }
    
    /* Sidebar enhancements */
    .sidebar-nav > ul > li > a {
      font-weight: 600;
    }
  </style>
</head>
<body>
  <div id="app">Loading NumKit Bug Catalog...</div>
  
  <button class="theme-toggle-btn" id="themeToggle" title="Toggle Dark/Light Mode">🌓</button>

  <script>
    window.$docsify = {
      name: 'NumKit Bugs &amp; Parity',
      repo: 'https://github.com/hotredman/numkit',
      loadSidebar: true,
      loadNavbar: true,
      subMaxLevel: 2,
      auto2top: true,
      search: {
        maxAge: 86400000,
        paths: 'auto',
        placeholder: 'Search bugs, functions, errors...',
        noData: 'No bugs found',
        depth: 3
      },
      copyCode: {
        buttonText: 'Copy',
        errorText: 'Error',
        successText: 'Copied'
      },
      pagination: {
        previousText: 'Previous Bug',
        nextText: 'Next Bug',
        crossChapter: true
      }
    };
  </script>

  <!-- Docsify Core -->
  <script src="https://cdn.jsdelivr.net/npm/docsify@4/lib/docsify.min.js"></script>
  
  <!-- Plugins -->
  <script src="https://cdn.jsdelivr.net/npm/docsify@4/lib/plugins/search.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/docsify-copy-code@2/dist/docsify-copy-code.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/docsify-pagination@2/dist/docsify-pagination.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/docsify/lib/plugins/zoom-image.min.js"></script>

  <!-- Prism.js Syntax Highlighting (MATLAB, C++, Bash, JSON) -->
  <script src="https://cdn.jsdelivr.net/npm/prismjs@1/components/prism-matlab.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/prismjs@1/components/prism-c.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/prismjs@1/components/prism-cpp.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/prismjs@1/components/prism-bash.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/prismjs@1/components/prism-json.min.js"></script>

  <script>
    // Theme toggle logic
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

def parse_bug_metadata(filepath):
    txt = filepath.read_text(encoding='utf-8', errors='replace')
    title = filepath.stem
    severity = 'P2'
    kind = 'bug'
    status = 'OPEN' if 'opened' in filepath.parts else 'FIXED'

    lines = txt.splitlines()
    if lines and lines[0].startswith('#'):
        title = lines[0].lstrip('#').strip()

    m_status = re.search(r'\*\*Status:\*\*\s*([^\n]+)', txt)
    if m_status:
        status_raw = m_status.group(1).strip()
        if 'FIXED' in status_raw:
            status = 'FIXED'
        elif 'OPEN' in status_raw:
            status = 'OPEN'

    m_sev = re.search(r'\*\*Severity:\*\*\s*(P[0-3])', txt)
    if m_sev:
        severity = m_sev.group(1)

    m_kind = re.search(r'\*\*Kind:\*\*\s*(\w+)', txt)
    if m_kind:
        kind = m_kind.group(1)

    return {
        'title': title,
        'severity': severity,
        'kind': kind,
        'status': status,
        'path': filepath
    }

def build_site(out_dir):
    out_dir = Path(out_dir).resolve()
    print(f"Building NumKit Bugs Docsify site in: {out_dir}")
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    # 1. Copy markdown files
    for root, dirs, files in os.walk(BUGS_SRC):
        rel = Path(root).relative_to(BUGS_SRC)
        dest_sub = out_dir / rel
        dest_sub.mkdir(parents=True, exist_ok=True)
        for f in files:
            if f.endswith('.md'):
                shutil.copy2(Path(root) / f, dest_sub / f)

    # 2. Parse opened & closed bugs
    opened_bugs = {}
    for md in sorted((BUGS_SRC / 'opened').glob('*/*.md')):
        ns = md.parent.name
        opened_bugs.setdefault(ns, []).append(parse_bug_metadata(md))

    closed_bugs = {}
    for md in sorted((BUGS_SRC / 'closed').glob('*/*.md')):
        ns = md.parent.name
        closed_bugs.setdefault(ns, []).append(parse_bug_metadata(md))

    total_opened = sum(len(v) for v in opened_bugs.values())
    total_closed = sum(len(v) for v in closed_bugs.values())

    # 3. Generate _sidebar.md
    sidebar = []
    sidebar.append("* 🏠 [**Overview**](README.md)")
    sidebar.append("* 📊 [**Missing Parity Inventory**](missing.md)")
    sidebar.append("")
    sidebar.append(f"* 🔴 **Open Defects ({total_opened})**")
    for ns, bugs in sorted(opened_bugs.items()):
        sidebar.append(f"  * **{ns}** ({len(bugs)})")
        for b in bugs:
            rel_link = f"opened/{ns}/{b['path'].name}"
            clean_title = b['title'].replace('[', '(').replace(']', ')')
            sidebar.append(f"    * [{clean_title}]({rel_link})")

    sidebar.append("")
    sidebar.append(f"* ✅ **Closed & Fixed ({total_closed})**")
    for ns, bugs in sorted(closed_bugs.items()):
        sidebar.append(f"  * **{ns}** ({len(bugs)})")
        for b in bugs:
            rel_link = f"closed/{ns}/{b['path'].name}"
            clean_title = b['title'].replace('[', '(').replace(']', ')')
            sidebar.append(f"    * [{clean_title}]({rel_link})")

    (out_dir / '_sidebar.md').write_text('\n'.join(sidebar), encoding='utf-8')

    # 4. Generate _navbar.md
    navbar = [
        f"* 🔴 [Open ({total_opened})](opened/)",
        f"* ✅ [Closed ({total_closed})](closed/)",
        "* 📊 [Parity Gaps](missing.md)",
        "* 🐙 [GitHub Repo](https://github.com/hotredman/numkit)",
        "* 📚 [Doxygen C++ Docs](https://hotredman.github.io/numkit-doxy/)"
    ]
    (out_dir / '_navbar.md').write_text('\n'.join(navbar), encoding='utf-8')

    # 5. Generate index.html and .nojekyll
    (out_dir / 'index.html').write_text(INDEX_HTML_TEMPLATE, encoding='utf-8')
    (out_dir / '.nojekyll').write_text('', encoding='utf-8')

    print(f"[OK] Generated Docsify site: {total_opened} open bugs, {total_closed} closed bugs.")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Build NumKit Bugs Docsify site.')
    parser.add_argument('--output', '-o', default=str(ROOT / 'build' / 'bugs'), help='Output directory')
    args = parser.parse_args()
    build_site(args.output)
