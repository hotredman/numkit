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
BRAND_SRC = ROOT / 'brand'

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
      --content-max-width: 82em;
      --code-font-family: 'JetBrains Mono', 'Fira Code', Consolas, monospace;
      --code-font-size: 13.5px;
    }
    
    /* Sidebar Header & Brand Logo */
    .app-name {
      margin: 20px 16px 14px 18px !important;
      text-align: left !important;
    }
    .app-name-link {
      display: block !important;
    }
    .app-name-link img {
      max-width: 175px !important;
      height: auto !important;
      display: block !important;
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
    .badge-p0 { background: #dc2626; color: white; padding: 2px 7px; border-radius: 4px; font-size: 11px; font-weight: 600; }
    .badge-p1 { background: #ea580c; color: white; padding: 2px 7px; border-radius: 4px; font-size: 11px; font-weight: 600; }
    .badge-p2 { background: #d97706; color: white; padding: 2px 7px; border-radius: 4px; font-size: 11px; font-weight: 600; }
    .badge-p3 { background: #2563eb; color: white; padding: 2px 7px; border-radius: 4px; font-size: 11px; font-weight: 600; }
    .badge-date { background: #334155; color: #94a3b8; padding: 2px 6px; border-radius: 4px; font-size: 11.5px; font-family: monospace; }

    /* Professional Table Styling */
    table {
      width: 100% !important;
      display: table !important;
      border-collapse: collapse;
      margin: 16px 0 20px 0;
      table-layout: auto;
    }
    th {
      background: rgba(30, 41, 59, 0.95) !important;
      color: #e2e8f0 !important;
      font-weight: 600;
      text-align: left;
      padding: 12px 14px;
      border-bottom: 2px solid rgba(255, 255, 255, 0.12);
      font-size: 13.5px;
      letter-spacing: 0.02em;
    }
    td {
      padding: 11px 14px;
      border-bottom: 1px solid rgba(255, 255, 255, 0.06);
      vertical-align: middle;
      font-size: 14px;
    }
    tr:hover td {
      background: rgba(255, 255, 255, 0.02);
    }

    /* Column Width Rules */
    .table-open th:nth-child(1), .table-open td:nth-child(1) { width: 110px; text-align: center; white-space: nowrap; }
    .table-open th:nth-child(2), .table-open td:nth-child(2) { width: 100px; text-align: center; }
    .table-open th:nth-child(3), .table-open td:nth-child(3) { width: auto; font-weight: 500; }
    .table-open th:nth-child(4), .table-open td:nth-child(4) { width: 65px; text-align: center; }
    .table-open th:nth-child(5), .table-open td:nth-child(5) { width: 85px; text-align: center; }

    .table-closed th:nth-child(1), .table-closed td:nth-child(1) { width: 110px; text-align: center; white-space: nowrap; }
    .table-closed th:nth-child(2), .table-closed td:nth-child(2) { width: 100px; text-align: center; }
    .table-closed th:nth-child(3), .table-closed td:nth-child(3) { width: auto; font-weight: 500; }
    .table-closed th:nth-child(4), .table-closed td:nth-child(4) { width: 110px; text-align: center; font-family: monospace; }
    .table-closed th:nth-child(5), .table-closed td:nth-child(5) { width: 65px; text-align: center; }

    td code {
      word-break: break-word !important;
      white-space: normal !important;
      font-size: 12px !important;
      padding: 2px 5px !important;
    }

    /* Table Toolbar Controls (Filter / Search / Page size) */
    .table-toolbar {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      margin: 18px 0 10px 0;
      padding: 10px 14px;
      background: rgba(30, 41, 59, 0.5);
      border: 1px solid rgba(255, 255, 255, 0.08);
      border-radius: 8px;
    }
    .table-toolbar-left, .table-toolbar-right {
      display: flex;
      align-items: center;
      gap: 10px;
      flex-wrap: wrap;
    }
    .table-toolbar input, .table-toolbar select {
      background: #0f172a;
      color: #f8fafc;
      border: 1px solid #334155;
      border-radius: 6px;
      padding: 6px 12px;
      font-size: 13.5px;
      outline: none;
    }
    .table-toolbar input:focus, .table-toolbar select:focus {
      border-color: #38bdf8;
    }
    
    /* Table Pagination Navigation */
    .table-pagination {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      margin: 14px 0 24px 0;
      font-size: 13.5px;
      color: #94a3b8;
    }
    .pagination-buttons {
      display: flex;
      gap: 4px;
    }
    .pagination-btn {
      background: #1e293b;
      color: #cbd5e1;
      border: 1px solid #334155;
      padding: 5px 12px;
      border-radius: 6px;
      cursor: pointer;
      font-size: 13px;
      transition: all 0.15s ease;
    }
    .pagination-btn:hover:not(:disabled) {
      background: #334155;
      color: #f8fafc;
    }
    .pagination-btn.active {
      background: #0284c7;
      color: #ffffff;
      border-color: #38bdf8;
      font-weight: 700;
    }
    .pagination-btn:disabled {
      opacity: 0.4;
      cursor: not-allowed;
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
    // Custom Table Pagination Plugin
    function tablePaginationPlugin(hook, vm) {
      hook.doneEach(function() {
        const tables = document.querySelectorAll('.table-paginated table');
        tables.forEach((table, idx) => {
          const tbody = table.querySelector('tbody');
          if (!tbody) return;
          const rows = Array.from(tbody.querySelectorAll('tr'));
          if (rows.length === 0) return;

          // Extract subsystems for filter dropdown
          const subsystems = new Set();
          rows.forEach(r => {
            const subCell = r.querySelector('td:nth-child(2)');
            if (subCell) {
              const text = subCell.textContent.trim();
              if (text) subsystems.add(text);
            }
          });

          // State
          let currentPage = 1;
          let pageSize = rows.length > 25 ? 25 : 25;
          let searchQuery = '';
          let selectedSubsystem = '';

          // Create toolbar container
          const toolbar = document.createElement('div');
          toolbar.className = 'table-toolbar';

          // Left: Subsystem Filter & Search
          const toolbarLeft = document.createElement('div');
          toolbarLeft.className = 'table-toolbar-left';

          const searchInput = document.createElement('input');
          searchInput.type = 'text';
          searchInput.placeholder = '🔍 Quick filter...';
          searchInput.addEventListener('input', (e) => {
            searchQuery = e.target.value.toLowerCase();
            currentPage = 1;
            render();
          });
          toolbarLeft.appendChild(searchInput);

          if (subsystems.size > 1) {
            const subSelect = document.createElement('select');
            subSelect.innerHTML = '<option value="">All Subsystems (' + rows.length + ')</option>';
            Array.from(subsystems).sort().forEach(s => {
              const count = rows.filter(r => r.querySelector('td:nth-child(2)').textContent.trim() === s).length;
              subSelect.innerHTML += `<option value="${s}">${s} (${count})</option>`;
            });
            subSelect.addEventListener('change', (e) => {
              selectedSubsystem = e.target.value;
              currentPage = 1;
              render();
            });
            toolbarLeft.appendChild(subSelect);
          }

          // Right: Page Size Selector
          const toolbarRight = document.createElement('div');
          toolbarRight.className = 'table-toolbar-right';
          const sizeSelect = document.createElement('select');
          sizeSelect.innerHTML = `
            <option value="25" selected>25 per page</option>
            <option value="50">50 per page</option>
            <option value="100">100 per page</option>
            <option value="all">Show All (${rows.length})</option>
          `;
          sizeSelect.addEventListener('change', (e) => {
            pageSize = e.target.value === 'all' ? Infinity : parseInt(e.target.value, 10);
            currentPage = 1;
            render();
          });
          toolbarRight.appendChild(sizeSelect);

          toolbar.appendChild(toolbarLeft);
          toolbar.appendChild(toolbarRight);
          table.parentNode.insertBefore(toolbar, table);

          // Pagination footer
          const paginationFooter = document.createElement('div');
          paginationFooter.className = 'table-pagination';
          table.parentNode.insertBefore(paginationFooter, table.nextSibling);

          function render() {
            // Filter rows
            const filteredRows = rows.filter(r => {
              const text = r.textContent.toLowerCase();
              const matchesSearch = !searchQuery || text.includes(searchQuery);
              const subCell = r.querySelector('td:nth-child(2)');
              const matchesSub = !selectedSubsystem || (subCell && subCell.textContent.trim() === selectedSubsystem);
              return matchesSearch && matchesSub;
            });

            const total = filteredRows.length;
            const totalPages = pageSize === Infinity ? 1 : Math.max(1, Math.ceil(total / pageSize));
            if (currentPage > totalPages) currentPage = totalPages;

            const startIdx = (currentPage - 1) * (pageSize === Infinity ? total : pageSize);
            const endIdx = pageSize === Infinity ? total : Math.min(startIdx + pageSize, total);

            // Hide all, show current slice
            rows.forEach(r => r.style.display = 'none');
            for (let i = startIdx; i < endIdx; i++) {
              if (filteredRows[i]) filteredRows[i].style.display = '';
            }

            // Update footer
            paginationFooter.innerHTML = '';
            const info = document.createElement('div');
            info.textContent = total === 0 ? 'No matching defects found' : `Showing ${startIdx + 1}–${endIdx} of ${total} defects`;
            paginationFooter.appendChild(info);

            if (totalPages > 1) {
              const btnGroup = document.createElement('div');
              btnGroup.className = 'pagination-buttons';

              const prevBtn = document.createElement('button');
              prevBtn.className = 'pagination-btn';
              prevBtn.textContent = '← Prev';
              prevBtn.disabled = currentPage === 1;
              prevBtn.addEventListener('click', () => { if (currentPage > 1) { currentPage--; render(); } });
              btnGroup.appendChild(prevBtn);

              for (let p = 1; p <= totalPages; p++) {
                if (totalPages > 7 && Math.abs(p - currentPage) > 2 && p !== 1 && p !== totalPages) {
                  if (p === 2 || p === totalPages - 1) {
                    const span = document.createElement('span');
                    span.style.padding = '4px 6px';
                    span.textContent = '...';
                    btnGroup.appendChild(span);
                  }
                  continue;
                }
                const pageBtn = document.createElement('button');
                pageBtn.className = 'pagination-btn' + (p === currentPage ? ' active' : '');
                pageBtn.textContent = p;
                pageBtn.addEventListener('click', () => { currentPage = p; render(); });
                btnGroup.appendChild(pageBtn);
              }

              const nextBtn = document.createElement('button');
              nextBtn.className = 'pagination-btn';
              nextBtn.textContent = 'Next →';
              nextBtn.disabled = currentPage === totalPages;
              nextBtn.addEventListener('click', () => { if (currentPage < totalPages) { currentPage++; render(); } });
              btnGroup.appendChild(nextBtn);

              paginationFooter.appendChild(btnGroup);
            }
          }

          render();
        });
      });
    }

    window.$docsify = {
      name: 'NumKit Bugs &amp; Parity',
      logo: 'assets/numkit-logo-dark.svg',
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
        placeholder: 'Search documentation...',
        noData: 'No results found',
        depth: 3
      },
      copyCode: {
        buttonText: 'Copy',
        errorText: 'Error',
        successText: 'Copied'
      },
      plugins: [
        tablePaginationPlugin
      ]
    };
  </script>

  <script src="https://cdn.jsdelivr.net/npm/docsify@4/lib/docsify.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/docsify@4/lib/plugins/search.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/docsify-copy-code@2/dist/docsify-copy-code.min.js"></script>
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
      const logoImg = document.querySelector('.app-name-link img');
      if (theme === 'light') {
        darkLink.setAttribute('disabled', 'true');
        lightLink.removeAttribute('disabled');
        if (logoImg) logoImg.src = 'assets/numkit-logo-light.svg';
      } else {
        lightLink.setAttribute('disabled', 'true');
        darkLink.removeAttribute('disabled');
        if (logoImg) logoImg.src = 'assets/numkit-logo-dark.svg';
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
            m_fix_info = re.search(r'FIXED\s*\(([^,]+),\s*([0-9]{4}-[0-9]{2}-[0-9]{2})\)', st_raw)
            if m_fix_info:
                commit = m_fix_info.group(1).strip()
                fixed_date = m_fix_info.group(2).strip()
            else:
                m_c = re.search(r'FIXED\s*\(([^)]+)\)', st_raw)
                if m_c:
                    commit = m_c.group(1).strip()

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
        'found_date': found_date,
        'fixed_date': fixed_date,
        'commit': commit,
        'guard': guard,
        'path': md_path
    }

def render_badge(sev):
    return f'<span class="badge-{sev.lower()}">{sev}</span>'

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

    # 2. Copy brand assets (logos)
    assets_dir = out_dir / 'assets'
    assets_dir.mkdir(parents=True, exist_ok=True)
    if (BRAND_SRC / 'numkit-logo-dark.svg').exists():
        shutil.copy2(BRAND_SRC / 'numkit-logo-dark.svg', assets_dir / 'numkit-logo-dark.svg')
    if (BRAND_SRC / 'numkit-logo-light.svg').exists():
        shutil.copy2(BRAND_SRC / 'numkit-logo-light.svg', assets_dir / 'numkit-logo-light.svg')

    # 3. Parse opened and closed bugs
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

    # 4. Parse missing.md
    missing_file = BUGS_SRC / 'missing.md'
    missing_raw = missing_file.read_text(encoding='utf-8', errors='replace') if missing_file.exists() else ''
    missing_count_m = re.search(r'Missing — not implemented\s*\((\d+)\)', missing_raw)
    missing_count = int(missing_count_m.group(1)) if missing_count_m else 839

    # 5. Generate ROOT _sidebar.md
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

    # 6. Generate OPENED _sidebar.md
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

    # 7. Generate CLOSED _sidebar.md
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

    # 8. Generate MISSING _sidebar.md and missing/README.md
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

    if '## ❌ Missing — not implemented' in missing_raw:
        missing_body = missing_raw.split('## ❌ Missing — not implemented')[1]
        missing_body = re.sub(r'^\s*\([0-9]+\)\s*\n+', '', missing_body).strip()
    else:
        missing_body = missing_raw.strip()

    missing_readme = [
        f"# ❌ Missing Functions Backlog ({missing_count})\n",
        "> The complete catalog of MATLAB R2025b functions and toolboxes not yet implemented in NumKit, categorized by domain.\n",
        missing_body
    ]
    (out_dir / 'missing' / 'README.md').write_text('\n'.join(missing_readme), encoding='utf-8')

    # 9. Generate _navbar.md
    navbar = [
        f"* [🔴 Open ({total_opened})](opened/README.md)",
        f"* [✅ Closed ({total_closed})](closed/README.md)",
        f"* [❌ Missing ({missing_count})](missing/README.md)",
        "* [🐙 GitHub](https://github.com/hotredman/numkit)",
        "* [📚 Doxygen](https://hotredman.github.io/numkit-doxy/)"
    ]
    (out_dir / '_navbar.md').write_text('\n'.join(navbar), encoding='utf-8')

    # 10. Generate MAIN README.md (Dashboard with Top-5 items & short clean headers)
    main_readme = []
    main_readme.append("# 🐞 NumKit Defect & MATLAB Parity Catalog\n")
    main_readme.append("Welcome to the structured defect catalog and parity tracking system for **NumKit** (MATLAB/Octave-compatible C++ runtime).\n")

    # Open Bugs Table (Latest 5)
    main_readme.append("## 🔴 Open Defects\n")
    main_readme.append('<div class="table-open">\n')
    main_readme.append("| Found Date | Subsystem | Defect / Function | Sev | Kind |")
    main_readme.append("| :---: | :---: | :--- | :---: | :---: |")
    for b in opened_sorted[:5]:
        clean_title = b['title'].replace('|', '/')
        link = f"[{clean_title}](opened/{b['namespace']}/{b['filename']})"
        f_date = f"`{b['found_date']}`" if b['found_date'] != '-' else '-'
        main_readme.append(f"| {f_date} | **{b['namespace']}** | {link} | {render_badge(b['severity'])} | `{b['kind']}` |")
    main_readme.append('\n</div>\n')
    main_readme.append(f"👉 *[View all {total_opened} open defects in the Registry →](opened/README.md)*\n")

    main_readme.append("\n---\n")

    # Recent Fixes Table (Latest 5)
    main_readme.append("## ✅ Recent Fixes\n")
    main_readme.append('<div class="table-closed">\n')
    main_readme.append("| Fixed Date | Subsystem | Resolved Issue | Commit | Sev |")
    main_readme.append("| :---: | :---: | :--- | :---: | :---: |")
    for b in closed_sorted[:5]:
        clean_title = b['title'].replace('|', '/')
        link = f"[{clean_title}](closed/{b['namespace']}/{b['filename']})"
        fx_date = f"`{b['fixed_date']}`" if b['fixed_date'] != '-' else (f"`{b['found_date']}`" if b['found_date'] != '-' else '-')
        commit_str = f"`{b['commit'][:9]}`" if b['commit'] != '-' else '-'
        main_readme.append(f"| {fx_date} | **{b['namespace']}** | {link} | {commit_str} | {render_badge(b['severity'])} |")
    main_readme.append('\n</div>\n')
    main_readme.append(f"👉 *[View all {total_closed} fixed issues in the complete Archive →](closed/README.md)*\n")

    (out_dir / 'README.md').write_text('\n'.join(main_readme), encoding='utf-8')

    # 11. Generate opened/README.md (with interactive pagination & subsystem filter)
    opened_readme = [
        f"# 🔴 Open Defects Registry ({total_opened})\n",
        "Complete register of all currently active defects and feature gaps.\n",
        '<div class="table-open table-paginated">\n',
        "| Found Date | Subsystem | Defect / Function | Sev | Kind |",
        "| :---: | :---: | :--- | :---: | :---: |"
    ]
    for b in opened_sorted:
        clean_title = b['title'].replace('|', '/')
        link = f"[{clean_title}](opened/{b['namespace']}/{b['filename']})"
        f_date = f"`{b['found_date']}`" if b['found_date'] != '-' else '-'
        opened_readme.append(f"| {f_date} | **{b['namespace']}** | {link} | {render_badge(b['severity'])} | `{b['kind']}` |")
    opened_readme.append('\n</div>\n')
    
    (out_dir / 'opened' / 'README.md').write_text('\n'.join(opened_readme), encoding='utf-8')

    # 12. Generate closed/README.md (with interactive pagination & subsystem filter for 135+ items)
    closed_readme = [
        f"# ✅ Closed &amp; Resolved Defects Registry ({total_closed})\n",
        "Complete historical changelog of all resolved bugs and fixed regressions.\n",
        '<div class="table-closed table-paginated">\n',
        "| Fixed Date | Subsystem | Resolved Issue | Commit | Sev |",
        "| :---: | :---: | :--- | :---: | :---: |"
    ]
    for b in closed_sorted:
        clean_title = b['title'].replace('|', '/')
        link = f"[{clean_title}](closed/{b['namespace']}/{b['filename']})"
        fx_date = f"`{b['fixed_date']}`" if b['fixed_date'] != '-' else (f"`{b['found_date']}`" if b['found_date'] != '-' else '-')
        commit_str = f"`{b['commit'][:9]}`" if b['commit'] != '-' else '-'
        closed_readme.append(f"| {fx_date} | **{b['namespace']}** | {link} | {commit_str} | {render_badge(b['severity'])} |")
    closed_readme.append('\n</div>\n')

    (out_dir / 'closed' / 'README.md').write_text('\n'.join(closed_readme), encoding='utf-8')

    # 13. Generate index.html and .nojekyll
    (out_dir / 'index.html').write_text(INDEX_HTML_TEMPLATE, encoding='utf-8')
    (out_dir / '.nojekyll').write_text('', encoding='utf-8')

    print(f"[OK] Generated Docsify site: {total_opened} open, {total_closed} closed, {missing_count} missing.")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Build NumKit Bugs Docsify site.')
    parser.add_argument('--output', '-o', default=str(ROOT / 'build' / 'bugs'), help='Output directory')
    args = parser.parse_args()
    build_site(args.output)
