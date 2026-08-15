/**
 * v3 Sidebar — mockup-styled file browser.
 *
 * Single tree component that switches between Temporary FS (in-memory IndexedDB)
 * and Local Folder (File System Access API or Electron native). Mirrors the
 * core operations of the legacy FileBrowser (open, new, rename, delete,
 * import, download) using the mockup's `.sidebar-*` and `.tree-*` CSS classes.
 */
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import tempFS from '../../temporary';
import localFS from '../../fs/local';
import { usePersistedState } from '../../ui-state';

const isMFile = (name) => /\.(m|n)$/i.test(name);

/* ─────────────── icon set (uniform with sidebar header buttons) ─────────── */
const ICON_PROPS = {
  width: 12, height: 12, viewBox: '0 0 12 12',
  fill: 'none', stroke: 'currentColor',
  strokeWidth: 1.3, strokeLinecap: 'round', strokeLinejoin: 'round',
};
const Icons = {
  file:    () => (
    <svg {...ICON_PROPS}><path d="M3 1.5h4L9 3.5V10.5H3z"/><path d="M7 1.5V3.5H9"/></svg>
  ),
  fileNew: () => (
    <svg {...ICON_PROPS}><path d="M3 1.5h4L9 3.5V10.5H3z"/><path d="M7 1.5V3.5H9"/><path d="M6 6v3 M4.5 7.5h3"/></svg>
  ),
  folder:  () => (
    <svg {...ICON_PROPS}><path d="M1.5 3.5h3l1 1h5V10H1.5z"/></svg>
  ),
  folderNew: () => (
    <svg {...ICON_PROPS}><path d="M1.5 3.5h3l1 1h5V10H1.5z"/><path d="M6 6v3 M4.5 7.5h3"/></svg>
  ),
  open:    () => (
    <svg {...ICON_PROPS}><path d="M2 3.5h3l1 1h4V10H2z"/><path d="M2 6h8"/></svg>
  ),
  duplicate: () => (
    <svg {...ICON_PROPS}><rect x="2" y="2" width="6" height="7"/><path d="M4 4.5V11h6V5"/></svg>
  ),
  download: () => (
    <svg {...ICON_PROPS}><path d="M6 1.5V8 M3 5.5l3 3 3-3 M2 10.5h8"/></svg>
  ),
  upload: () => (
    <svg {...ICON_PROPS}><path d="M6 8.5V2 M3 4.5l3-3 3 3 M2 10.5h8"/></svg>
  ),
  rename: () => (
    <svg {...ICON_PROPS}><path d="M2 9.5L8.5 3 10 4.5 3.5 11 2 11z"/><path d="M7 4.5l1.5 1.5"/></svg>
  ),
  trash:  () => (
    <svg {...ICON_PROPS}><path d="M2 3.5h8 M4 3.5V2.5h4v1 M3 3.5L3.5 10.5h5L9 3.5"/></svg>
  ),
  refresh: () => (
    <svg {...ICON_PROPS}><path d="M10 6a4 4 0 1 1-1.17-2.83"/><path d="M10 1.5V4H7.5"/></svg>
  ),
};

/* ─────────────── inline rename input ─────────────── */
function InlineInput({ defaultValue, onSubmit, onCancel, placeholder }) {
  const [val, setVal] = useState(defaultValue || '');
  const ref = useRef(null);
  useEffect(() => { ref.current?.focus(); ref.current?.select(); }, []);
  return (
    <input
      ref={ref}
      value={val}
      onChange={(e) => setVal(e.target.value)}
      onKeyDown={(e) => {
        if (e.key === 'Enter')  onSubmit(val.trim());
        if (e.key === 'Escape') onCancel();
      }}
      onBlur={() => onCancel()}
      placeholder={placeholder || 'filename'}
      className="sidebar-inline-input"
    />
  );
}

/* ─────────────── context menu ─────────────── */
function ContextMenu({ x, y, items, onClose }) {
  const ref = useRef(null);
  useEffect(() => {
    const h = (e) => { if (ref.current && !ref.current.contains(e.target)) onClose(); };
    document.addEventListener('mousedown', h);
    return () => document.removeEventListener('mousedown', h);
  }, [onClose]);
  return (
    <div ref={ref}
      style={{
        position: 'fixed', left: x, top: y, zIndex: 1000,
        background: 'var(--bg-3)', border: '1px solid var(--line)', borderRadius: 5,
        boxShadow: '0 4px 16px rgba(0,0,0,0.35)', minWidth: 160, padding: '4px 0',
        color: 'var(--fg-1)',
      }}>
      {items.map((item, i) => item.separator
        ? <div key={i} style={{ height: 1, background: 'var(--line)', margin: '3px 8px' }} />
        : (
          <div key={i}
            onClick={() => { item.action?.(); onClose(); }}
            style={{
              padding: '5px 12px', fontSize: 11.5,
              color: item.danger ? 'var(--danger)' : 'var(--fg-1)',
              cursor: 'pointer',
              display: 'flex', alignItems: 'center', gap: 8,
              fontFamily: 'var(--font-mono)',
            }}
            onMouseEnter={(e) => (e.currentTarget.style.background = 'var(--bg-4)')}
            onMouseLeave={(e) => (e.currentTarget.style.background = 'transparent')}>
            {item.icon && (
              <span style={{
                width: 14, height: 14,
                display: 'inline-flex', alignItems: 'center', justifyContent: 'center',
                color: item.danger ? 'var(--danger)' : 'var(--fg-2)',
                flexShrink: 0,
              }}>
                {typeof item.icon === 'function' ? item.icon() : item.icon}
              </span>
            )}
            {item.label}
          </div>
        )
      )}
    </div>
  );
}

/* ─────────────── tree row + folder ─────────────── */
function TreeRow({ node, depth, expanded, setExpanded, selected, setSelected,
                  onOpenFile, onNavigateFolder, onContextMenu, renaming, onRenameSubmit, onRenameCancel,
                  filter, ops }) {
  const isDir = node.type === 'folder';
  const isExp = !!expanded[node.path];
  const isSel = selected === node.path;
  const [children, setChildren] = useState(node.children || null);
  const [loadingChildren, setLoadingChildren] = useState(false);

  useEffect(() => {
    if (Array.isArray(node.children)) {
      setChildren(node.children);
    }
  }, [node.children]);

  const toggleExpand = async () => {
    const nextExp = !isExp;
    setExpanded((p) => ({ ...p, [node.path]: nextExp }));
    if (nextExp && !children && ops?.listDir) {
      setLoadingChildren(true);
      try {
        const sub = await ops.listDir(node.path);
        setChildren(sub);
      } catch (err) {
        console.warn('[Sidebar] load subfolder error', err);
        setChildren([]);
      } finally {
        setLoadingChildren(false);
      }
    }
  };

  if (renaming === node.path) {
    return (
      <div className="tree-row" style={{ paddingLeft: 8 + depth * 12 }}>
        <InlineInput defaultValue={node.name} onSubmit={onRenameSubmit} onCancel={onRenameCancel} />
      </div>
    );
  }

  // Filter: hide rows that don't match the search (keep folders that have any matching descendant)
  const matches = !filter || node.name.toLowerCase().includes(filter.toLowerCase());
  if (filter && !isDir && !matches) return null;
  if (filter && isDir && !hasMatchingDescendant(node, filter)) return null;

  const onClick = () => {
    setSelected(node.path);
    if (isDir) toggleExpand();
  };
  const onDouble = () => {
    if (isDir && onNavigateFolder) onNavigateFolder(node.path);
    else if (!isDir) onOpenFile?.(node);
  };

  return (
    <div>
      <div className={`tree-row ${isDir ? 'tree-folder' : 'tree-file'} ${isSel ? 'is-active' : ''}`}
        style={{ paddingLeft: isDir ? 8 + depth * 12 : 8 + (depth + 1) * 12 + 4 }}
        onClick={onClick}
        onDoubleClick={onDouble}
        onContextMenu={(e) => onContextMenu(e, node)}>
        {isDir && (
          <svg className="tree-chev" width="9" height="9" viewBox="0 0 9 9"
            style={{ transform: isExp ? 'rotate(90deg)' : 'none' }}>
            <path d="M3 1.5 L6 4.5 L3 7.5" stroke="currentColor" fill="none"
              strokeWidth="1.3" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
        )}
        {isDir ? (
          <svg width="11" height="11" viewBox="0 0 12 12" className="tree-icon">
            <path d="M1 3.5a1 1 0 0 1 1-1h2.5l1 1H10a1 1 0 0 1 1 1V9a1 1 0 0 1-1 1H2a1 1 0 0 1-1-1V3.5z"
              fill="currentColor" opacity="0.7"/>
          </svg>
        ) : (
          <span className="tree-fileglyph">{isMFile(node.name) ? '·n' : '·'}</span>
        )}
        <span className="tree-label">{node.name}</span>
        {loadingChildren && <span style={{ fontSize: 9, opacity: 0.5, marginLeft: 4 }}>…</span>}
      </div>
      {isDir && isExp && Array.isArray(children) && children.map((c) => (
        <TreeRow key={c.path}
          node={c} depth={depth + 1}
          expanded={expanded} setExpanded={setExpanded}
          selected={selected} setSelected={setSelected}
          onOpenFile={onOpenFile} onNavigateFolder={onNavigateFolder} onContextMenu={onContextMenu}
          renaming={renaming} onRenameSubmit={onRenameSubmit} onRenameCancel={onRenameCancel}
          filter={filter} ops={ops} />
      ))}
    </div>
  );
}

function hasMatchingDescendant(node, filter) {
  if (!filter) return true;
  const f = filter.toLowerCase();
  if (node.name.toLowerCase().includes(f)) return true;
  if (Array.isArray(node.children)) {
    return node.children.some((c) => hasMatchingDescendant(c, f));
  }
  return false;
}

/* ─────────────── examples backend (read-only, fetched from /public) ─────── */
//
// Mirrors public/examples/manifest.json into a virtual tree. Files are
// fetched on demand, and on open we mirror the example folder into tempFS
// at /Examples/<Folder>/<file> so sibling .m lookups (helper.m next to
// caller.m) work without addpath.
async function loadExamplesTree() {
  const base = import.meta.env.BASE_URL || '/';
  try {
    const res = await fetch(`${base}examples/manifest.json`);
    if (!res.ok) throw new Error('manifest not found');
    const manifest = await res.json();
    return manifest.folders.map((folder) => ({
      name: folder.name.replace(/_/g, ' '),
      path: `/examples/${folder.name}`,
      type: 'folder',
      children: folder.files.map((f) => ({
        name: f,
        path: `/examples/${folder.name}/${f}`,
        type: 'file',
        _fetchPath: `${base}examples/${folder.name}/${f}`,
      })),
    }));
  } catch (e) {
    console.warn('[Sidebar] examples manifest load failed:', e);
    return [];
  }
}

// Extensions that must be mirrored byte-for-byte (imread / audioread /
// load read these through the binary VFS hook; a text round-trip would
// corrupt any byte ≥ 0x80).
const BINARY_EXAMPLE_EXT = /\.(png|jpe?g|gif|bmp|tga|tiff?|webp|psd|hdr|pic|pgm|ppm|pnm|wav|mp3|m4a|ogg|flac|mat)$/i;

// Fetch one example file and mirror it into the adapter at `vfsPath`,
// picking the binary or text channel by extension. Returns the text
// content for text files, or null for binary files.
async function mirrorExampleFile(adapter, fetchPath, vfsPath) {
  const r = await fetch(fetchPath);
  if (!r.ok) throw new Error('fetch failed');
  if (BINARY_EXAMPLE_EXT.test(vfsPath) && typeof adapter.writeFileBytes === 'function') {
    const buf = await r.arrayBuffer();
    adapter.writeFileBytes(vfsPath, new Uint8Array(buf));
    return null;
  }
  const text = await r.text();
  adapter.writeFile(vfsPath, text);
  return text;
}

async function openExample(node, tree, vfsAdapters) {
  if (node.type !== 'file' || !node._fetchPath) return null;
  const isBinary = BINARY_EXAMPLE_EXT.test(node.name || node.path);

  const fname = node.name || 'example.m';
  const scriptBaseName = fname.replace(/\.[^/.]+$/, '');
  const targetRelDir = `/examples/${scriptBaseName}`;
  const vfsPath = `${targetRelDir}/${fname}`;
  let content = null;

  const tempBackend = vfsAdapters?.temp || tempFS;
  if (tempBackend) {
    try {
      if (typeof tempBackend.mkdir === 'function') await tempBackend.mkdir(targetRelDir);
    } catch { /* ignore */ }

    // Check for sibling files in the same example category folder
    const m = (node.path || '').match(/^\/examples\/([^/]+)\/(.+)$/);
    if (m) {
      const [, folder] = m;
      const folderNode = tree.find((n) => n.path === `/examples/${folder}`);
      const siblings = folderNode?.children?.filter((c) => c.type === 'file') || [];
      await Promise.all(siblings.map(async (sib) => {
        if (sib.name === fname) return;
        const sibVfsPath = `${targetRelDir}/${sib.name}`;
        try {
          if (tempBackend.exists && tempBackend.exists(sibVfsPath)) return;
          await mirrorExampleFile(tempBackend, sib._fetchPath, sibVfsPath);
        } catch { /* tolerate */ }
      }));
    }

    try {
      content = await mirrorExampleFile(tempBackend, node._fetchPath, vfsPath);
    } catch (e) {
      console.warn('[openExample] mirror failed, fetching directly:', e);
      const res = await fetch(node._fetchPath);
      if (res.ok) content = isBinary ? null : await res.text();
    }
  } else {
    const res = await fetch(node._fetchPath);
    if (!res.ok) throw new Error('fetch failed');
    content = isBinary ? null : await res.text();
  }

  return { content, vfsPath, isBinary };
}

/* ─────────────── GitHub backend ─────────────── */
//
// Browses a public GitHub repo via the REST API. No auth — relies on the
// 60 req/hour anonymous limit, which is plenty for one user clicking through
// a tree. The repo URL, branch, and which folders are expanded are persisted
// per-source so reopening the IDE restores the last view.

function parseGhRepo(url) {
  if (!url) return null;
  const c = url.trim().replace(/\/+$/, '').replace(/\.git$/, '');
  let m = c.match(/github\.com\/([^/]+)\/([^/]+)/);
  if (m) return { owner: m[1], repo: m[2] };
  m = c.match(/^([^/\s]+)\/([^/\s]+)$/);
  if (m) return { owner: m[1], repo: m[2] };
  return null;
}

async function ghFetchTree(owner, repo, ref) {
  const meta = await fetch(`https://api.github.com/repos/${owner}/${repo}`).then((r) => r.json());
  const branch = ref || meta.default_branch || 'main';
  const treeResp = await fetch(`https://api.github.com/repos/${owner}/${repo}/git/trees/${branch}?recursive=1`);
  if (!treeResp.ok) throw new Error(`tree ${treeResp.status}`);
  const data = await treeResp.json();
  return { meta, branch, flat: data.tree || [] };
}

async function ghFetchBranches(owner, repo) {
  const r = await fetch(`https://api.github.com/repos/${owner}/${repo}/branches?per_page=30`);
  return r.ok ? (await r.json()).map((b) => b.name) : [];
}

async function ghFetchFile(owner, repo, branch, path) {
  const r = await fetch(`https://api.github.com/repos/${owner}/${repo}/contents/${path}?ref=${branch}`);
  if (!r.ok) throw new Error(`file ${r.status}`);
  const d = await r.json();
  return d.encoding === 'base64' ? atob(d.content) : (d.content || '');
}

// Build a hierarchical tree from a flat GitHub tree array.
function buildGhTree(flat) {
  const root = { children: {} };
  for (const it of flat) {
    const parts = it.path.split('/');
    let cur = root;
    for (let i = 0; i < parts.length; i++) {
      const seg = parts[i];
      if (!cur.children[seg]) cur.children[seg] = {
        name: seg,
        path: parts.slice(0, i + 1).join('/'),
        type: i === parts.length - 1 ? (it.type === 'tree' ? 'folder' : 'file') : 'folder',
        children: {},
      };
      cur = cur.children[seg];
    }
  }
  const flatten = (obj) => Object.values(obj)
    .sort((a, b) => {
      if (a.type === 'folder' && b.type !== 'folder') return -1;
      if (a.type !== 'folder' && b.type === 'folder') return 1;
      return a.name.localeCompare(b.name);
    })
    .map((n) => ({ ...n, children: n.children ? flatten(n.children) : undefined }));
  return flatten(root.children);
}

/* GitHub browser pane — replaces the regular tree UI when source === 'github'. */
function GitHubBrowser({ onOpenFile }) {
  const [repoUrl, setRepoUrl] = usePersistedState('numkit.ide.fb.github.repoUrl', '');
  const [branch, setBranch]   = usePersistedState('numkit.ide.fb.github.branch', '');
  const [tree, setTree]       = useState([]);
  const [branches, setBranches] = useState([]);
  const [meta, setMeta]       = useState(null);
  const [loading, setLoading] = useState(false);
  const [error, setError]     = useState('');
  const [expanded, setExpanded] = usePersistedState('numkit.ide.fb.github.expanded', {});
  const [filter, setFilter]   = useState('');

  const load = useCallback(async (urlOverride, branchOverride) => {
    const url = urlOverride ?? repoUrl;
    const p = parseGhRepo(url);
    if (!p) { setError('Use: owner/repo'); return; }
    setLoading(true); setError('');
    try {
      const { meta: m, branch: br, flat } = await ghFetchTree(p.owner, p.repo, branchOverride);
      setMeta(m);
      setBranch(br);
      setTree(buildGhTree(flat));
      const brs = await ghFetchBranches(p.owner, p.repo);
      setBranches(brs);
    } catch (e) {
      setError(String(e?.message || e));
      setTree([]); setBranches([]); setMeta(null);
    } finally {
      setLoading(false);
    }
  }, [repoUrl, setBranch]);

  // Auto-load on mount if a repo is already remembered
  useEffect(() => {
    if (repoUrl && !tree.length && !loading) load(repoUrl, branch || undefined);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const handleOpen = useCallback(async (node) => {
    if (node.type !== 'file') return;
    const p = parseGhRepo(repoUrl);
    if (!p) return;
    try {
      const content = await ghFetchFile(p.owner, p.repo, branch, node.path);
      onOpenFile?.(node.name, content, null, 'github');
    } catch (e) {
      console.error('[Sidebar] gh fetch file', e);
    }
  }, [repoUrl, branch, onOpenFile]);

  return (
    <>
      <div style={{ padding: '8px 10px', borderBottom: '1px solid var(--line-soft)' }}>
        <div style={{ display: 'flex', gap: 6 }}>
          <input value={repoUrl}
            onChange={(e) => setRepoUrl(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && load()}
            placeholder="owner/repo"
            style={{
              flex: 1, padding: '4px 8px', borderRadius: 4, fontSize: 11,
              background: 'var(--bg-0)', color: 'var(--fg-0)',
              border: '1px solid var(--line)',
              fontFamily: 'var(--font-mono)', outline: 'none',
            }} />
          <button onClick={() => load()}
            disabled={loading || !repoUrl.trim()}
            style={{
              padding: '4px 10px', borderRadius: 4, fontSize: 11, fontWeight: 600,
              background: 'var(--accent)', color: '#fff', border: 'none',
              cursor: loading ? 'default' : 'pointer',
              opacity: loading || !repoUrl.trim() ? 0.45 : 1,
            }}>{loading ? '…' : 'Load'}</button>
        </div>
        {branches.length > 0 && (
          <div style={{ marginTop: 6, display: 'flex', alignItems: 'center', gap: 6 }}>
            <span style={{ fontSize: 10, color: 'var(--fg-3)' }}>Branch:</span>
            <select value={branch}
              onChange={(e) => load(repoUrl, e.target.value)}
              style={{
                flex: 1, padding: '2px 4px', borderRadius: 3, fontSize: 11,
                background: 'var(--bg-0)', color: 'var(--fg-0)',
                border: '1px solid var(--line)',
                fontFamily: 'var(--font-mono)',
              }}>
              {branches.map((b) => <option key={b} value={b}>{b}</option>)}
            </select>
          </div>
        )}
        {meta && (
          <div style={{
            marginTop: 4, fontSize: 10, color: 'var(--fg-3)',
            display: 'flex', gap: 8, fontFamily: 'var(--font-mono)',
          }}>
            <span>★ {meta.stargazers_count}</span>
            <span>⑂ {meta.forks_count}</span>
            {meta.language && <span>{meta.language}</span>}
          </div>
        )}
        {error && (
          <div style={{ color: 'var(--danger)', fontSize: 10, marginTop: 4, fontFamily: 'var(--font-mono)' }}>{error}</div>
        )}
      </div>
      <div className="sidebar-search">
        <svg width="10" height="10" viewBox="0 0 12 12">
          <circle cx="5" cy="5" r="3.2" stroke="currentColor" fill="none"/>
          <path d="M7.4 7.4L10 10" stroke="currentColor"/>
        </svg>
        {filter ? (
          <input value={filter}
            onChange={(e) => setFilter(e.target.value)}
            placeholder="filter files…" spellCheck={false}
            style={{
              flex: 1, background: 'transparent', border: 'none', outline: 'none',
              color: 'inherit', font: 'inherit', padding: 0,
            }}/>
        ) : (
          <span className="sidebar-search-hint" onClick={() => setFilter(' ')}>
            Double-click to open file
          </span>
        )}
        {filter && (
          <button onClick={() => setFilter('')} title="Clear"
            style={{
              background: 'transparent', border: 'none',
              color: 'var(--fg-3)', cursor: 'pointer', padding: 0,
              fontSize: 13, lineHeight: 1,
            }}>×</button>
        )}
      </div>
      <div className="sidebar-tree">
        {loading && tree.length === 0 && (
          <div style={{ padding: 16, textAlign: 'center', color: 'var(--fg-3)', fontSize: 11 }}>Loading…</div>
        )}
        {!loading && tree.length === 0 && !error && (
          <div style={{ padding: 16, textAlign: 'center', color: 'var(--fg-3)', fontSize: 11, lineHeight: 1.5 }}>
            Enter a GitHub repo<br/>(e.g. <code>owner/repo</code>) and press Load.
          </div>
        )}
        {tree.map((node) => (
          <TreeRow key={node.path}
            node={node} depth={0}
            expanded={expanded} setExpanded={setExpanded}
            selected={null} setSelected={() => {}}
            onOpenFile={handleOpen}
            onContextMenu={() => {}}
            renaming={null}
            onRenameSubmit={() => {}}
            onRenameCancel={() => {}}
            filter={filter.trim()}
          />
        ))}
      </div>
    </>
  );
}

/* ─────────────── source-specific operations ─────────────── */
function makeOps(source) {
  const fs = source === 'localFolder' ? localFS : tempFS;
  return {
    listDir: (p) => (typeof fs.listDir === 'function' ? fs.listDir(p) : fs.listTree()),
    listTree: () => fs.listTree(),
    readFile: (p) => fs.readFile(p),
    writeFile: (p, c) => fs.writeFile(p, c),
    exists: (p) => fs.exists(p),
    remove: (p) => fs.remove(p),
    rename: (a, b) => fs.rename(a, b),
    mkdir: (p) => fs.mkdir(p),
  };
}

/* ─────────────── full sidebar component ─────────────── */
export default function Sidebar({
  fsMode = 'virtual',
  onFsModeChange,
  cwd = '/',
  onCwdChange,
  onNavigateUp,
  onOpenFile,
  vfsRefreshKey,
  isTabUnsaved,
  onLocalMount,
  vfsAdapters,
}) {
  const localAvailable = typeof localFS !== 'undefined' && localFS.isAvailable?.();
  const [rawSource, setSource] = usePersistedState('numkit.ide.sidebar.source', 'fs');
  const source = (rawSource === 'localFolder' || rawSource === 'temporary') ? 'fs' : rawSource;
  const [tree, setTree] = useState([]);
  const [expanded, setExpanded] = usePersistedState(`numkit.ide.fb.expanded.${source}`, {});
  const [selected, setSelected] = useState(null);
  const [contextMenu, setContextMenu] = useState(null);
  const [creating, setCreating] = useState(null);
  const [renaming, setRenaming] = useState(null);
  const [filter, setFilter] = useState('');
  const [localMountName, setLocalMountName] = useState(null);
  const [localStatus, setLocalStatus] = useState('idle'); // idle|connecting|connected|denied

  const isExamples = source === 'examples';
  const isGithub   = source === 'github';
  const isLocal    = source === 'fs' && fsMode === 'local';
  const isLocalUnmounted = isLocal && localStatus !== 'connected';

  const currentRelDir = useMemo(() => {
    if (source !== 'fs') return '';
    if (isLocal) {
      const lRoot = localFS.root?.() || '';
      if (cwd && lRoot && cwd.startsWith(lRoot)) {
        let rel = cwd.slice(lRoot.length).replace(/\\/g, '/');
        if (!rel.startsWith('/')) rel = '/' + rel;
        return rel === '/' ? '' : rel;
      }
      return '';
    }
    const vRel = (cwd || '/').replace(/\\/g, '/');
    return vRel === '/' ? '' : vRel;
  }, [source, isLocal, cwd]);

  const isAtRoot = useMemo(() => {
    if (source !== 'fs') return true;
    if (isLocal) {
      const clean = (cwd || '').trim().replace(/\\/g, '/');
      if (!clean || clean === '/' || /^[A-Za-z]:\/?$/.test(clean)) return true;
      return false;
    }
    return !currentRelDir || currentRelDir === '/' || currentRelDir === '';
  }, [source, isLocal, cwd, currentRelDir]);

  const ops = useMemo(() =>
    makeOps(isLocal ? 'localFolder' : 'temporary'),
  [isLocal]);

  const loadTree = useCallback(async () => {
    try {
      if (isExamples) {
        setTree(await loadExamplesTree());
      } else if (isGithub) {
        /* GitHubBrowser owns its own tree state */
      } else {
        if (source === 'fs') {
          const entries = await ops.listDir(currentRelDir || '/');
          setTree(Array.isArray(entries) ? entries : []);
        } else {
          const raw = await ops.listTree();
          setTree(Array.isArray(raw) ? raw : []);
        }
      }
    } catch (e) { console.error('[Sidebar] loadTree failed', e); }
  }, [ops, isExamples, isGithub, source, currentRelDir]);

  // Reload on source change, fsMode change, cwd change, external write signal
  useEffect(() => { loadTree(); }, [loadTree, vfsRefreshKey, fsMode, cwd]);

  // Restore Local Folder mount on mount
  useEffect(() => {
    if (!isLocal || !localAvailable) return;
    let cancelled = false;
    (async () => {
      try {
        const name = await localFS.reconnect();
        if (cancelled) return;
        if (name) {
          setLocalMountName(name);
          setLocalStatus('connected');
          const root = localFS.root?.();
          if (root && (!cwd || cwd === '/')) {
            onCwdChange?.(root);
          }
          await loadTree();
          if (onLocalMount) await onLocalMount();
        }
      } catch (e) {
        if (!cancelled) console.warn('[Sidebar] reconnect failed', e);
      }
    })();
    return () => { cancelled = true; };
  }, [isLocal, localAvailable, loadTree, onLocalMount, cwd, onCwdChange]);

  /* ─── source switch ─── */
  const switchSource = useCallback((next) => {
    setSource(next);
    setSelected(null);
    setRenaming(null);
    setCreating(null);
    setFilter('');
  }, [setSource]);

  /* ─── local folder pick / disconnect ─── */
  const handlePickLocal = useCallback(async () => {
    setLocalStatus('connecting');
    try {
      const name = await localFS.pickDirectory();
      if (name) {
        setLocalMountName(name);
        setLocalStatus('connected');
        const root = localFS.root?.();
        if (root) onCwdChange?.(root);
        await loadTree();
        if (onLocalMount) await onLocalMount();
      } else {
        setLocalStatus(localMountName ? 'connected' : 'idle');
      }
    } catch (e) {
      console.error('[Sidebar] pickDirectory failed', e);
      setLocalStatus('denied');
    }
  }, [localMountName, loadTree, onLocalMount, onCwdChange]);

  const handleUnmount = useCallback(async () => {
    if (!confirm('Unmount this folder? Your files on disk are not affected.')) return;
    await localFS.disconnect();
    setLocalMountName(null);
    setLocalStatus('idle');
    setTree([]);
  }, []);

  /* ─── file ops ─── */
  const handleFileOpen = useCallback(async (node) => {
    if (node.type !== 'file') return;
    if (isExamples) {
      try {
        const r = await openExample(node, tree, vfsAdapters);
        // Binary examples (images/audio) are mirrored into tempFS for
        // imread/audioread but aren't text — don't load them in the editor.
        if (r && !r.isBinary) onOpenFile?.(node.name, r.content, r.vfsPath, 'examples');
      } catch (e) { console.error('[Sidebar] openExample', e); }
      return;
    }
    const content = await ops.readFile(node.path);
    onOpenFile?.(node.name, content !== null ? content : '', node.path, isLocal ? 'localFolder' : 'temporary');
  }, [ops, onOpenFile, isLocal, isExamples, tree, vfsAdapters]);

  const handleCreate = useCallback(async (name) => {
    if (!name || !creating) { setCreating(null); return; }
    const parent = creating.parentPath || '';
    if (creating.type === 'folder') {
      const path = parent ? `${parent}/${name}` : `/${name}`;
      await ops.mkdir(path);
    } else {
      const fn = name.includes('.') ? name : name + '.m';
      const fp = parent ? `${parent}/${fn}` : `/${fn}`;
      await ops.writeFile(fp, `% ${fn}\n`);
    }
    setCreating(null);
    if (parent) setExpanded((p) => ({ ...p, [parent]: true }));
    loadTree();
  }, [creating, ops, loadTree, setExpanded]);

  const handleRename = useCallback(async (newName) => {
    if (!newName || !renaming) { setRenaming(null); return; }
    const parent = renaming.substring(0, renaming.lastIndexOf('/'));
    await ops.rename(renaming, `${parent}/${newName}`);
    setRenaming(null);
    loadTree();
  }, [renaming, ops, loadTree]);

  const handleDelete = useCallback(async (node) => {
    if (!confirm(`Delete "${node.name}"?`)) return;
    await ops.remove(node.path);
    loadTree();
  }, [ops, loadTree]);

  const handleDuplicate = useCallback(async (node) => {
    if (node.type !== 'file') return;
    const content = await ops.readFile(node.path);
    if (content === null) return;
    const parent = node.path.substring(0, node.path.lastIndexOf('/'));
    const dot = node.name.lastIndexOf('.');
    const ext = dot >= 0 ? node.name.substring(dot) : '';
    const base = dot >= 0 ? node.name.substring(0, dot) : node.name;
    let copyName = `${base}_copy${ext}`;
    let copyPath = parent ? `${parent}/${copyName}` : `/${copyName}`;
    let counter = 2;
    while (await ops.exists(copyPath)) {
      copyName = `${base}_copy${counter}${ext}`;
      copyPath = parent ? `${parent}/${copyName}` : `/${copyName}`;
      counter++;
    }
    await ops.writeFile(copyPath, content);
    loadTree();
  }, [ops, loadTree]);

  const handleDownload = useCallback(async (node) => {
    if (node.type !== 'file') return;
    if (isTabUnsaved && isTabUnsaved(node.path, source)) {
      const ok = confirm(`"${node.name}" has unsaved changes. Continue with last saved version?`);
      if (!ok) return;
    }
    const content = await ops.readFile(node.path);
    if (content === null) return;
    const blob = new Blob([content], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url; a.download = node.name; a.click();
    URL.revokeObjectURL(url);
  }, [ops, isTabUnsaved, source]);

  const handleImport = useCallback((folderPath) => {
    const input = document.createElement('input');
    input.type = 'file';
    input.multiple = true;
    input.onchange = async (e) => {
      const files = Array.from(e.target.files || []);
      if (!files.length) return;
      const parent = folderPath || '';
      let imported = 0;
      for (const file of files) {
        const dest = parent ? `${parent}/${file.name}` : `/${file.name}`;
        if (await ops.exists(dest)) {
          const ok = confirm(
            `"${file.name}" already exists.\n\nOK — overwrite.\nCancel — skip this file.`
          );
          if (!ok) continue;
        }
        const text = await file.text();
        await ops.writeFile(dest, text);
        imported++;
      }
      if (imported > 0) {
        if (parent) setExpanded((p) => ({ ...p, [parent]: true }));
        loadTree();
      }
    };
    input.click();
  }, [ops, loadTree, setExpanded]);

  /* ─── context menu ─── */
  const handleContextMenu = useCallback((e, node) => {
    e.preventDefault();
    e.stopPropagation();
    // Examples are read-only — only "Open" / "Refresh" make sense.
    if (isExamples) {
      const items = [];
      if (node.type === 'file') {
        items.push({ icon: Icons.open, label: 'Open in Editor', action: () => handleFileOpen(node) });
        items.push({ separator: true });
      }
      items.push({ icon: Icons.refresh, label: 'Refresh', action: () => loadTree() });
      setContextMenu({ x: e.clientX, y: e.clientY, items });
      return;
    }
    const items = [];
    if (node.type === 'folder') {
      items.push({ icon: Icons.fileNew, label: 'New file…',
        action: () => { setExpanded((p) => ({ ...p, [node.path]: true })); setCreating({ parentPath: node.path, type: 'file' }); } });
      items.push({ icon: Icons.folderNew, label: 'New folder…',
        action: () => { setExpanded((p) => ({ ...p, [node.path]: true })); setCreating({ parentPath: node.path, type: 'folder' }); } });
      items.push({ icon: Icons.upload, label: 'Import file(s) here…',
        action: () => handleImport(node.path) });
      items.push({ separator: true });
    } else {
      items.push({ icon: Icons.open,      label: 'Open in Editor', action: () => handleFileOpen(node) });
      items.push({ icon: Icons.duplicate, label: 'Duplicate',      action: () => handleDuplicate(node) });
      items.push({ icon: Icons.download,  label: 'Download',       action: () => handleDownload(node) });
      items.push({ separator: true });
    }
    items.push({ icon: Icons.rename, label: 'Rename', action: () => setRenaming(node.path) });
    items.push({ icon: Icons.trash, label: 'Delete', danger: true, action: () => handleDelete(node) });
    items.push({ separator: true });
    items.push({ icon: Icons.refresh, label: 'Refresh', action: () => loadTree() });
    setContextMenu({ x: e.clientX, y: e.clientY, items });
  }, [isExamples, setExpanded, handleImport, handleFileOpen, handleDuplicate, handleDownload, handleDelete, loadTree]);

  /* ─── root context menu (empty area click) ─── */
  const handleRootContextMenu = useCallback((e) => {
    e.preventDefault();
    if (isExamples) {
      setContextMenu({
        x: e.clientX, y: e.clientY,
        items: [
          { icon: Icons.refresh, label: 'Refresh', action: () => loadTree() },
        ],
      });
      return;
    }
    setContextMenu({
      x: e.clientX, y: e.clientY,
      items: [
        { icon: Icons.fileNew,   label: 'New file…',       action: () => setCreating({ parentPath: currentRelDir, type: 'file' }) },
        { icon: Icons.folderNew, label: 'New folder…',     action: () => setCreating({ parentPath: currentRelDir, type: 'folder' }) },
        { icon: Icons.upload,    label: 'Import file(s)…', action: () => handleImport(currentRelDir) },
        { separator: true },
        { icon: Icons.refresh,   label: 'Refresh',         action: () => loadTree() },
      ],
    });
  }, [isExamples, handleImport, loadTree, currentRelDir]);

  /* ─── render ─── */
  return (
    <aside className="sidebar">
      {/* Source picker. Row 1 = combo + refresh (refresh pairs with the
          combo since it acts on whatever source is selected). Row 2 =
          the mutating actions (new file / new folder / open-folder),
          shown only for writable sources. */}
      <div className="sidebar-head">
        <div className="sidebar-head-row">
          <select className="ws-picker"
            value={source}
            onChange={(e) => switchSource(e.target.value)}>
            <option value="fs">File System</option>
            <option value="examples">Examples</option>
            <option value="github">GitHub</option>
          </select>
          {/* Always-visible refresh — picks up changes made on disk by
              other tools (mainly the real-disk Local Folder backend, but
              cheap enough to keep for every source). Stays on the combo
              row. */}
          <button className="sidebar-icon" title="Refresh tree"
            onClick={() => loadTree()}>
            {Icons.refresh()}
          </button>
        </div>

        {/* Mutating actions — new file + new folder work in both
            Temporary and Local Folder (ops routes to the active fs);
            open-folder is Local-only. Hidden for read-only sources
            (Examples / GitHub). */}
        {!isExamples && !isGithub && (
          <div className="sidebar-head-row sidebar-head-actions">
            <button className="sidebar-icon" title="New file"
              onClick={() => setCreating({ parentPath: currentRelDir, type: 'file' })}>
              {Icons.fileNew()}
            </button>
            <button className="sidebar-icon" title="New folder"
              onClick={() => setCreating({ parentPath: currentRelDir, type: 'folder' })}>
              {Icons.folderNew()}
            </button>
            {/* Local Folder only — open the OS folder-picker to (re)mount
                a directory. Reuses handlePickLocal (same as first-mount),
                so subsequent picks just switch the root. */}
            {isLocal && localAvailable && (
              <button className="sidebar-icon" title="Open folder…"
                onClick={handlePickLocal}>
                <svg width="13" height="13" viewBox="0 0 14 14" fill="none">
                  <path d="M1.5 4.5h4l1.2 1.5h5.8v6a1 1 0 0 1-1 1H2.5a1 1 0 0 1-1-1V4.5z"
                    stroke="currentColor" strokeWidth="1.2" strokeLinejoin="round"/>
                  <path d="M1.5 4.5V3a1 1 0 0 1 1-1h2.7a1 1 0 0 1 .7.3l1 1h5.6a1 1 0 0 1 1 1v1.2"
                    stroke="currentColor" strokeWidth="1.2" strokeLinejoin="round"/>
                </svg>
              </button>
            )}
          </div>
        )}
      </div>

      {/* GitHub source — owns its own search/tree below */}
      {isGithub && (
        <GitHubBrowser onOpenFile={onOpenFile} />
      )}

      {!isGithub && (
        <>
      {/* Search */}
      <div className="sidebar-search">
        <svg width="10" height="10" viewBox="0 0 12 12">
          <circle cx="5" cy="5" r="3.2" stroke="currentColor" fill="none"/>
          <path d="M7.4 7.4L10 10" stroke="currentColor"/>
        </svg>
        <input
          value={filter}
          onChange={(e) => setFilter(e.target.value)}
          placeholder="Filter files…"
          spellCheck={false}
          style={{
            flex: 1, background: 'transparent', border: 'none', outline: 'none',
            color: 'inherit', font: 'inherit', padding: '0 4px', fontSize: 11,
          }}
        />
        {filter && (
          <button
            onClick={() => setFilter('')}
            title="Clear filter"
            style={{
              background: 'transparent', border: 'none',
              color: 'var(--fg-3)', cursor: 'pointer', padding: 0,
              fontSize: 13, lineHeight: 1,
            }}>×</button>
        )}
      </div>

      {/* Local Folder mount prompt */}
      {isLocalUnmounted && (
        <div style={{ padding: 12, textAlign: 'center' }}>
          <button onClick={handlePickLocal}
            style={{
              padding: '6px 14px', borderRadius: 6, fontSize: 11,
              background: 'var(--accent)', color: '#fff',
              border: 'none', cursor: 'pointer',
            }}>
            {localStatus === 'connecting' ? 'Picking…' : 'Choose folder…'}
          </button>
          {localStatus === 'denied' && (
            <div style={{ marginTop: 8, fontSize: 10, color: 'var(--danger)' }}>
              Permission denied
            </div>
          )}
        </div>
      )}

      {/* Tree */}
      {!isLocalUnmounted && (
        <div className="sidebar-tree" onContextMenu={handleRootContextMenu}>
          {source === 'fs' && !isAtRoot && (
            <div
              className={`tree-row tree-folder ${selected === '..' ? 'is-active' : ''}`}
              style={{ paddingLeft: 8, color: 'var(--fg-2)', cursor: 'pointer', userSelect: 'none' }}
              onClick={() => setSelected('..')}
              onDoubleClick={onNavigateUp}
              title="Parent folder (..)"
            >
              <svg width="11" height="11" viewBox="0 0 12 12" className="tree-icon" style={{ opacity: 0.7 }}>
                <path d="M1 3.5a1 1 0 0 1 1-1h2.5l1 1H10a1 1 0 0 1 1 1V9a1 1 0 0 1-1 1H2a1 1 0 0 1-1-1V3.5z"
                  fill="currentColor"/>
              </svg>
              <span className="tree-label" style={{ fontWeight: 600, letterSpacing: '1px' }}>..</span>
            </div>
          )}
          {tree.length === 0 && !creating && (
            <div style={{
              padding: 16, textAlign: 'center',
              color: 'var(--fg-3)', fontSize: 10, lineHeight: 1.6,
            }}>
              No files yet.<br/>
              Click + or right-click here.
            </div>
          )}
          {tree.map((node) => (
            <TreeRow key={node.path}
              node={node} depth={0}
              expanded={expanded} setExpanded={setExpanded}
              selected={selected} setSelected={setSelected}
              onOpenFile={handleFileOpen}
              onNavigateFolder={(folderPath) => {
                if (isLocal) {
                  const lRoot = localFS.root?.() || '';
                  const cleanRel = folderPath.startsWith('/') ? folderPath.slice(1) : folderPath;
                  const fullPath = lRoot ? (lRoot.endsWith('\\') || lRoot.endsWith('/') ? `${lRoot}${cleanRel.replace(/\//g, '\\')}` : `${lRoot}\\${cleanRel.replace(/\//g, '\\')}`) : folderPath;
                  onCwdChange?.(fullPath);
                } else {
                  onCwdChange?.(folderPath);
                }
              }}
              onContextMenu={handleContextMenu}
              renaming={renaming}
              onRenameSubmit={handleRename}
              onRenameCancel={() => setRenaming(null)}
              filter={filter.trim()}
              ops={ops}
            />
          ))}
          {creating && (creating.parentPath === currentRelDir || creating.parentPath === '' || creating.parentPath === '/') && (
            <div className="tree-row" style={{ paddingLeft: 16 }}>
              <InlineInput defaultValue=""
                placeholder={creating.type === 'folder' ? 'folder name' : 'filename.m'}
                onSubmit={handleCreate}
                onCancel={() => setCreating(null)} />
            </div>
          )}
        </div>
      )}
        </>
      )}

      {contextMenu && (
        <ContextMenu x={contextMenu.x} y={contextMenu.y}
          items={contextMenu.items} onClose={() => setContextMenu(null)} />
      )}
    </aside>
  );
}
