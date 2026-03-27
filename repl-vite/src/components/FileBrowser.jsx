import { useState, useEffect, useRef, useMemo, useCallback } from 'react';
import vfs from '../vfs';
import C, { FONT, FONT_UI } from '../theme';

const isMFile = name => name.endsWith('.m');
const isTextFile = name => {
  const exts = ['.m', '.txt', '.md', '.json', '.cpp', '.hpp', '.h', '.c', '.py', '.js', '.ts', '.jsx', '.css', '.html', '.yml', '.yaml', '.cmake', '.sh'];
  return exts.some(e => name.endsWith(e));
};

function rowStyle(isSel, isDir, name, depth) {
  return {
    display: 'flex', alignItems: 'center', gap: 4,
    padding: '2px 6px', paddingLeft: depth * 14 + 6,
    cursor: 'pointer', fontSize: 11, userSelect: 'none',
    background: isSel ? `${C.accent}15` : 'transparent',
    borderLeft: isSel ? `2px solid ${C.accent}` : '2px solid transparent',
    color: isMFile(name) ? C.green : isDir ? C.accent : C.textDim,
    transition: 'all 0.1s',
  };
}

function ContextMenu({ x, y, items, onClose }) {
  const ref = useRef(null);
  useEffect(() => {
    const handler = e => { if (ref.current && !ref.current.contains(e.target)) onClose(); };
    document.addEventListener('mousedown', handler);
    return () => document.removeEventListener('mousedown', handler);
  }, [onClose]);
  return (
    <div ref={ref} style={{
      position: 'fixed', left: x, top: y, zIndex: 1000,
      background: C.bg3, border: `1px solid ${C.borderHi}`, borderRadius: 5,
      boxShadow: '0 4px 16px rgba(0,0,0,0.5)', minWidth: 140, padding: '4px 0',
    }}>
      {items.map((item, i) => item.separator ? (
        <div key={i} style={{ height: 1, background: C.border, margin: '3px 8px' }} />
      ) : (
        <div key={i} onClick={() => { item.action(); onClose(); }}
          style={{ padding: '5px 12px', fontSize: 11, color: item.danger ? C.red : C.text, cursor: 'pointer' }}
          onMouseEnter={e => e.currentTarget.style.background = C.bg4}
          onMouseLeave={e => e.currentTarget.style.background = 'transparent'}>
          {item.icon && <span style={{ marginRight: 6 }}>{item.icon}</span>}{item.label}
        </div>
      ))}
    </div>
  );
}

function InlineInput({ defaultValue, onSubmit, onCancel, placeholder }) {
  const [val, setVal] = useState(defaultValue || '');
  const ref = useRef(null);
  useEffect(() => { ref.current?.focus(); ref.current?.select(); }, []);
  return (
    <input ref={ref} value={val} onChange={e => setVal(e.target.value)}
      onKeyDown={e => { if (e.key === 'Enter') onSubmit(val.trim()); if (e.key === 'Escape') onCancel(); }}
      onBlur={() => onCancel()} placeholder={placeholder || 'filename.m'}
      style={{ width: '100%', padding: '3px 6px', fontSize: 11, background: C.bg0, border: `1px solid ${C.accent}`, borderRadius: 3, color: C.text, fontFamily: FONT, outline: 'none' }} />
  );
}

// ════════════════════════════════════════
// LocalBrowser — root = user workspace
// ════════════════════════════════════════
function LocalBrowser({ onOpenFile, onRefreshKey }) {
  const [tree, setTree] = useState([]);
  const [expanded, setExpanded] = useState({});
  const [selected, setSelected] = useState(null);
  const [contextMenu, setContextMenu] = useState(null);
  const [creating, setCreating] = useState(null);
  const [renaming, setRenaming] = useState(null);

  const loadTree = useCallback(async () => {
    try { setTree(await vfs.listTree()); } catch (e) { console.error('[VFS]', e); }
  }, []);

  useEffect(() => { loadTree(); }, [loadTree, onRefreshKey]);

  const handleFileDoubleClick = useCallback(async (node) => {
    if (node.type === 'file') {
      const content = await vfs.readFile(node.path);
      onOpenFile(node.name, content !== null ? content : '', node.path, 'local');
    }
  }, [onOpenFile]);

  const handleContextMenu = (e, node) => {
    e.preventDefault();
    const items = [];
    if (node.type === 'folder') {
      items.push({ icon: '📄', label: 'New File', action: () => { setExpanded(p => ({ ...p, [node.path]: true })); setCreating({ parentPath: node.path, type: 'file' }); } });
      items.push({ icon: '📁', label: 'New Folder', action: () => { setExpanded(p => ({ ...p, [node.path]: true })); setCreating({ parentPath: node.path, type: 'folder' }); } });
      items.push({ separator: true });
    }
    if (node.type === 'file') {
      items.push({ icon: '📝', label: 'Open in Editor', action: () => handleFileDoubleClick(node) });
      items.push({ separator: true });
    }
    items.push({ icon: '✏️', label: 'Rename', action: () => setRenaming(node.path) });
    items.push({ icon: '🗑', label: 'Delete', danger: true, action: async () => {
      if (confirm(`Delete "${node.name}"?`)) { await vfs.remove(node.path); loadTree(); }
    }});
    setContextMenu({ x: e.clientX, y: e.clientY, items });
  };

  const handleRootContextMenu = (e) => {
    e.preventDefault();
    setContextMenu({
      x: e.clientX, y: e.clientY,
      items: [
        { icon: '📄', label: 'New File', action: () => setCreating({ parentPath: '', type: 'file' }) },
        { icon: '📁', label: 'New Folder', action: () => setCreating({ parentPath: '', type: 'folder' }) },
      ],
    });
  };

  const handleCreate = async name => {
    if (!name || !creating) { setCreating(null); return; }
    const parent = creating.parentPath || '';
    const path = parent ? `${parent}/${name}` : `/${name}`;
    if (creating.type === 'folder') await vfs.mkdir(path);
    else {
      const fileName = name.endsWith('.m') ? name : name;
      const filePath = parent ? `${parent}/${fileName}` : `/${fileName}`;
      await vfs.writeFile(filePath, `% ${fileName}\n`);
    }
    setCreating(null); loadTree();
  };

  const handleRename = async newName => {
    if (!newName || !renaming) { setRenaming(null); return; }
    const parent = renaming.substring(0, renaming.lastIndexOf('/'));
    await vfs.rename(renaming, `${parent}/${newName}`);
    setRenaming(null); loadTree();
  };

  const renderTree = (nodes, depth = 0) => nodes.map(node => {
    if (renaming === node.path) {
      return <div key={node.path} style={{ padding: '2px 6px', paddingLeft: depth * 14 + 6 }}>
        <InlineInput defaultValue={node.name} onSubmit={handleRename} onCancel={() => setRenaming(null)} />
      </div>;
    }
    const isDir = node.type === 'folder';
    const isExp = expanded[node.path];
    const isSel = selected === node.path;
    return (
      <div key={node.path}>
        <div onClick={() => isDir ? setExpanded(p => ({ ...p, [node.path]: !p[node.path] })) : setSelected(node.path)}
          onDoubleClick={() => !isDir && handleFileDoubleClick(node)}
          onContextMenu={e => handleContextMenu(e, node)}
          style={rowStyle(isSel, isDir, node.name, depth)}
          onMouseEnter={e => { if (!isSel) e.currentTarget.style.background = C.bg3; }}
          onMouseLeave={e => { if (!isSel) e.currentTarget.style.background = 'transparent'; }}>
          {isDir ? <span style={{ fontSize: 9, width: 12, textAlign: 'center', color: C.textMuted }}>{isExp ? '▼' : '▶'}</span>
                 : <span style={{ fontSize: 10, width: 12, textAlign: 'center' }}>📄</span>}
          <span style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{isDir ? '📁 ' : ''}{node.name}</span>
          {!isDir && isMFile(node.name) && <span style={{ fontSize: 7, padding: '0 3px', borderRadius: 2, background: `${C.green}22`, color: C.green }}>M</span>}
        </div>
        {isDir && isExp && (
          <>
            {creating && creating.parentPath === node.path && (
              <div style={{ padding: '2px 6px', paddingLeft: (depth + 1) * 14 + 6 }}>
                <InlineInput defaultValue="" placeholder={creating.type === 'folder' ? 'folder name' : 'filename.m'} onSubmit={handleCreate} onCancel={() => setCreating(null)} />
              </div>
            )}
            {node.children && renderTree(node.children, depth + 1)}
          </>
        )}
      </div>
    );
  });

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{ display: 'flex', gap: 3, padding: '6px 8px', borderBottom: `1px solid ${C.border}`, flexShrink: 0 }}>
        <button onClick={() => setCreating({ parentPath: '', type: 'file' })} title="New file"
          style={{ padding: '2px 6px', borderRadius: 3, fontSize: 10, background: C.bg2, border: `1px solid ${C.border}`, color: C.textDim, cursor: 'pointer', fontFamily: FONT_UI }}>📄+</button>
        <button onClick={() => setCreating({ parentPath: '', type: 'folder' })} title="New folder"
          style={{ padding: '2px 6px', borderRadius: 3, fontSize: 10, background: C.bg2, border: `1px solid ${C.border}`, color: C.textDim, cursor: 'pointer', fontFamily: FONT_UI }}>📁+</button>
        <div style={{ flex: 1 }} />
        <button onClick={async () => { if (confirm('Clear all local files?')) { await vfs.clear(); loadTree(); } }} title="Clear all"
          style={{ padding: '2px 6px', borderRadius: 3, fontSize: 10, background: C.bg2, border: `1px solid ${C.border}`, color: C.textMuted, cursor: 'pointer', fontFamily: FONT_UI }}>🗑</button>
      </div>
      <div style={{ flex: 1, overflowY: 'auto', padding: '3px 0' }} onContextMenu={handleRootContextMenu}>
        {tree.length === 0
          ? <div style={{ padding: 16, textAlign: 'center', color: C.textMuted, fontSize: 10, lineHeight: 1.6 }}>
              No files yet.<br />Click 📄+ to create a file<br />or right-click for options.
            </div>
          : renderTree(tree)}
        {creating && (creating.parentPath === '' || creating.parentPath === '/') && (
          <div style={{ padding: '2px 6px', paddingLeft: 6 }}>
            <InlineInput defaultValue="" placeholder={creating.type === 'folder' ? 'folder name' : 'filename.m'} onSubmit={handleCreate} onCancel={() => setCreating(null)} />
          </div>
        )}
      </div>
      {contextMenu && <ContextMenu x={contextMenu.x} y={contextMenu.y} items={contextMenu.items} onClose={() => setContextMenu(null)} />}
    </div>
  );
}

// ════════════════════════════════════════
// ExamplesBrowser — read-only from manifest
// ════════════════════════════════════════
function ExamplesBrowser({ onOpenFile }) {
  const [tree, setTree] = useState([]);
  const [expanded, setExpanded] = useState({});
  const [selected, setSelected] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    (async () => {
      try {
        const base = import.meta.env.BASE_URL || '/';
        const res = await fetch(`${base}examples/manifest.json`);
        if (!res.ok) throw new Error('manifest not found');
        const manifest = await res.json();
        const nodes = manifest.folders.map(folder => ({
          name: folder.name.replace(/_/g, ' '),
          path: `/examples/${folder.name}`,
          type: 'folder',
          children: folder.files.map(f => ({
            name: f, path: `/examples/${folder.name}/${f}`, type: 'file',
            _fetchPath: `${base}examples/${folder.name}/${f}`,
          })),
        }));
        setTree(nodes);
        if (nodes.length > 0) setExpanded({ [nodes[0].path]: true });
      } catch (e) { console.warn('[Examples]', e); setTree([]); }
      finally { setLoading(false); }
    })();
  }, []);

  const handleFileDoubleClick = useCallback(async (node) => {
    if (node.type !== 'file' || !node._fetchPath) return;
    try {
      const res = await fetch(node._fetchPath);
      if (!res.ok) throw new Error('fetch failed');
      const content = await res.text();
      onOpenFile(node.name, content, null, 'examples');
    } catch (e) { console.error('[Examples]', e); }
  }, [onOpenFile]);

  const renderTree = (nodes, depth = 0) => nodes.map(node => {
    const isDir = node.type === 'folder';
    const isExp = expanded[node.path];
    const isSel = selected === node.path;
    return (
      <div key={node.path}>
        <div onClick={() => isDir ? setExpanded(p => ({ ...p, [node.path]: !p[node.path] })) : setSelected(node.path)}
          onDoubleClick={() => !isDir && handleFileDoubleClick(node)}
          style={rowStyle(isSel, isDir, node.name, depth)}
          onMouseEnter={e => { if (!isSel) e.currentTarget.style.background = C.bg3; }}
          onMouseLeave={e => { if (!isSel) e.currentTarget.style.background = 'transparent'; }}>
          {isDir ? <span style={{ fontSize: 9, width: 12, textAlign: 'center', color: C.textMuted }}>{isExp ? '▼' : '▶'}</span>
                 : <span style={{ fontSize: 10, width: 12, textAlign: 'center' }}>📄</span>}
          <span style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{isDir ? '📁 ' : ''}{node.name}</span>
          {!isDir && isMFile(node.name) && <span style={{ fontSize: 7, padding: '0 3px', borderRadius: 2, background: `${C.green}22`, color: C.green }}>M</span>}
        </div>
        {isDir && isExp && node.children && renderTree(node.children, depth + 1)}
      </div>
    );
  });

  if (loading) return <div style={{ padding: 16, textAlign: 'center', color: C.textMuted, fontSize: 11 }}>Loading…</div>;
  return (
    <div style={{ flex: 1, overflowY: 'auto', padding: '3px 0' }}>
      {tree.length === 0
        ? <div style={{ padding: 16, textAlign: 'center', color: C.textMuted, fontSize: 10 }}>No examples found.</div>
        : <><div style={{ padding: '4px 8px', fontSize: 9, color: C.textMuted, fontStyle: 'italic' }}>Double-click to open in editor</div>{renderTree(tree)}</>}
    </div>
  );
}

// ════════════════════════════════════════
// GitHubBrowser
// ════════════════════════════════════════
function GitHubBrowser({ onOpenFile, defaultRepo }) {
  const [repoUrl, setRepoUrl] = useState(defaultRepo || '');
  const [tree, setTree] = useState(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');
  const [expanded, setExpanded] = useState({});
  const [selected, setSelected] = useState(null);
  const [branch, setBranch] = useState('main');
  const [branches, setBranches] = useState([]);
  const [repoInfo, setRepoInfo] = useState(null);
  const [previewFile, setPreviewFile] = useState(null);
  const [previewContent, setPreviewContent] = useState('');
  const [previewLoading, setPreviewLoading] = useState(false);

  const parseRepo = url => {
    const c = url.trim().replace(/\/+$/, '').replace(/\.git$/, '');
    let m = c.match(/github\.com\/([^/]+)\/([^/]+)/);
    if (m) return { owner: m[1], repo: m[2] };
    m = c.match(/^([^/\s]+)\/([^/\s]+)$/);
    if (m) return { owner: m[1], repo: m[2] };
    return null;
  };

  const fetchRepo = useCallback(async (urlOverride) => {
    const url = urlOverride || repoUrl;
    const p = parseRepo(url);
    if (!p) { setError('Use: owner/repo'); return; }
    setLoading(true); setError(''); setTree(null); setPreviewFile(null);
    try {
      const infoRes = await fetch(`https://api.github.com/repos/${p.owner}/${p.repo}`);
      if (!infoRes.ok) throw new Error(infoRes.status === 404 ? 'Repository not found' : `API error: ${infoRes.status}`);
      const info = await infoRes.json();
      setRepoInfo(info);
      const brRes = await fetch(`https://api.github.com/repos/${p.owner}/${p.repo}/branches?per_page=20`);
      if (brRes.ok) setBranches((await brRes.json()).map(b => b.name));
      const treeRes = await fetch(`https://api.github.com/repos/${p.owner}/${p.repo}/git/trees/${info.default_branch || 'main'}?recursive=1`);
      if (!treeRes.ok) throw new Error('Failed to fetch file tree');
      setTree((await treeRes.json()).tree || []);
      setBranch(info.default_branch || 'main');
    } catch (err) { setError(err.message); } finally { setLoading(false); }
  }, [repoUrl]);

  useEffect(() => { if (defaultRepo) fetchRepo(defaultRepo); }, []); // eslint-disable-line

  const fetchBranch = async branchName => {
    const p = parseRepo(repoUrl);
    if (!p) return;
    setLoading(true); setError(''); setBranch(branchName); setPreviewFile(null);
    try {
      const treeRes = await fetch(`https://api.github.com/repos/${p.owner}/${p.repo}/git/trees/${branchName}?recursive=1`);
      if (!treeRes.ok) throw new Error('Failed to fetch branch');
      setTree((await treeRes.json()).tree || []);
    } catch (err) { setError(err.message); } finally { setLoading(false); }
  };

  const fetchFileContent = useCallback(async (path) => {
    const p = parseRepo(repoUrl);
    if (!p) return;
    setPreviewLoading(true); setPreviewFile(path);
    try {
      const res = await fetch(`https://api.github.com/repos/${p.owner}/${p.repo}/contents/${path}?ref=${branch}`);
      if (!res.ok) throw new Error('Failed to fetch file');
      const data = await res.json();
      setPreviewContent(data.encoding === 'base64' ? atob(data.content) : data.content || '');
    } catch (e) { setPreviewContent(`Error: ${e.message}`); } finally { setPreviewLoading(false); }
  }, [repoUrl, branch]);

  const handleFileDoubleClick = useCallback(async (path, name) => {
    const p = parseRepo(repoUrl);
    if (!p) return;
    try {
      const res = await fetch(`https://api.github.com/repos/${p.owner}/${p.repo}/contents/${path}?ref=${branch}`);
      if (!res.ok) throw new Error('Failed to fetch file');
      const data = await res.json();
      const content = data.encoding === 'base64' ? atob(data.content) : data.content || '';
      onOpenFile(name, content, null, 'github');
    } catch (e) { console.error('[GitHub]', e); }
  }, [repoUrl, branch, onOpenFile]);

  const nested = useMemo(() => {
    if (!tree) return [];
    const root = { children: {} };
    for (const item of tree) {
      const parts = item.path.split('/');
      let node = root;
      for (let i = 0; i < parts.length; i++) {
        if (!node.children[parts[i]]) {
          node.children[parts[i]] = { name: parts[i], path: parts.slice(0, i + 1).join('/'), type: i === parts.length - 1 ? item.type : 'tree', children: {} };
        }
        node = node.children[parts[i]];
      }
    }
    const flatten = obj => Object.values(obj).sort((a, b) => {
      if (a.type === 'tree' && b.type !== 'tree') return -1;
      if (a.type !== 'tree' && b.type === 'tree') return 1;
      return a.name.localeCompare(b.name);
    }).map(n => ({ ...n, children: n.children ? flatten(n.children) : undefined }));
    return flatten(root.children);
  }, [tree]);

  const renderTree = (nodes, depth = 0) => nodes.map(node => {
    const isDir = node.type === 'tree';
    const isExp = expanded[node.path];
    const isSel = selected === node.path;
    return (
      <div key={node.path}>
        <div onClick={() => isDir ? setExpanded(p => ({ ...p, [node.path]: !p[node.path] })) : (() => { setSelected(node.path); if (isTextFile(node.name)) fetchFileContent(node.path); })()}
          onDoubleClick={() => !isDir && isTextFile(node.name) && handleFileDoubleClick(node.path, node.name)}
          style={rowStyle(isSel, isDir, node.name, depth)}
          onMouseEnter={e => { if (!isSel) e.currentTarget.style.background = C.bg3; }}
          onMouseLeave={e => { if (!isSel) e.currentTarget.style.background = 'transparent'; }}>
          {isDir ? <span style={{ fontSize: 9, width: 12, textAlign: 'center', color: C.textMuted }}>{isExp ? '▼' : '▶'}</span>
                 : <span style={{ fontSize: 10, width: 12, textAlign: 'center' }}>{isMFile(node.name) ? '📄' : '📝'}</span>}
          <span style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{isDir ? '📁 ' : ''}{node.name}</span>
          {!isDir && isMFile(node.name) && <span style={{ fontSize: 7, padding: '0 3px', borderRadius: 2, background: `${C.green}22`, color: C.green }}>M</span>}
        </div>
        {isDir && isExp && node.children && node.children.length > 0 && renderTree(node.children, depth + 1)}
      </div>
    );
  });

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{ padding: '8px 10px', borderBottom: `1px solid ${C.border}`, flexShrink: 0 }}>
        <div style={{ display: 'flex', gap: 4 }}>
          <input value={repoUrl} onChange={e => setRepoUrl(e.target.value)} onKeyDown={e => e.key === 'Enter' && fetchRepo()}
            placeholder="owner/repo" style={{ flex: 1, padding: '5px 8px', borderRadius: 5, fontSize: 11, background: C.bg0, border: `1px solid ${C.border}`, color: C.text, outline: 'none', fontFamily: FONT }} />
          <button onClick={() => fetchRepo()} disabled={loading || !repoUrl.trim()}
            style={{ padding: '5px 10px', borderRadius: 5, fontSize: 10, fontWeight: 600, background: C.accent, color: '#fff', border: 'none', cursor: 'pointer', opacity: loading || !repoUrl.trim() ? 0.5 : 1, fontFamily: FONT }}>
            {loading ? '…' : 'Load'}
          </button>
        </div>
        {branches.length > 0 && (
          <div style={{ marginTop: 5, display: 'flex', alignItems: 'center', gap: 5 }}>
            <span style={{ fontSize: 9, color: C.textMuted }}>Branch:</span>
            <select value={branch} onChange={e => fetchBranch(e.target.value)}
              style={{ flex: 1, padding: '2px 4px', borderRadius: 3, fontSize: 10, background: C.bg0, border: `1px solid ${C.border}`, color: C.text, fontFamily: FONT, cursor: 'pointer' }}>
              {branches.map(b => <option key={b} value={b}>{b}</option>)}
            </select>
          </div>
        )}
        {repoInfo && <div style={{ marginTop: 4, fontSize: 9, color: C.textMuted, display: 'flex', gap: 6 }}>
          <span>⭐ {repoInfo.stargazers_count}</span><span>🍴 {repoInfo.forks_count}</span>
          {repoInfo.language && <span>💻 {repoInfo.language}</span>}
        </div>}
        {error && <div style={{ color: C.red, fontSize: 10, marginTop: 4 }}>{error}</div>}
      </div>
      <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden' }}>
        {loading && !tree && <div style={{ padding: 16, textAlign: 'center', color: C.textMuted, fontSize: 11 }}>Loading…</div>}
        {nested.length > 0 && <><div style={{ padding: '4px 8px', fontSize: 9, color: C.textMuted, fontStyle: 'italic' }}>Double-click to open in editor</div><div style={{ padding: '3px 0' }}>{renderTree(nested)}</div></>}
        {!tree && !loading && <div style={{ padding: 16, textAlign: 'center', color: C.textMuted, fontSize: 10 }}>Enter a GitHub repo to browse.</div>}
      </div>
      {previewFile && (
        <div style={{ borderTop: `1px solid ${C.border}`, flexShrink: 0, maxHeight: '40%', display: 'flex', flexDirection: 'column' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '4px 10px', background: C.bg0, borderBottom: `1px solid ${C.border}` }}>
            <span style={{ fontSize: 10, color: C.text, fontWeight: 600, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{previewFile.split('/').pop()}</span>
            <div style={{ display: 'flex', gap: 3, flexShrink: 0 }}>
              {isTextFile(previewFile) && <button onClick={() => onOpenFile(previewFile.split('/').pop(), previewContent, null, 'github')}
                style={{ padding: '2px 6px', borderRadius: 3, fontSize: 9, fontWeight: 600, background: C.accent, color: '#fff', border: 'none', cursor: 'pointer' }}>Open in Editor</button>}
              <button onClick={() => { setPreviewFile(null); setPreviewContent(''); }}
                style={{ padding: '2px 4px', borderRadius: 3, fontSize: 12, lineHeight: 1, background: 'none', color: C.textMuted, border: 'none', cursor: 'pointer' }}>×</button>
            </div>
          </div>
          <pre style={{ flex: 1, overflowY: 'auto', padding: '6px 10px', margin: 0, fontSize: 10, lineHeight: 1.5, color: C.textDim, background: C.bg0, fontFamily: FONT, whiteSpace: 'pre-wrap', wordBreak: 'break-word' }}>
            {previewLoading ? 'Loading…' : previewContent}
          </pre>
        </div>
      )}
    </div>
  );
}

// ════════════════════════════════════════
// FileBrowser — main export
// ════════════════════════════════════════
export default function FileBrowser({ onOpenFile, defaultGitHubRepo, vfsRefreshKey }) {
  const [source, setSource] = useState('local');
  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{ padding: '7px 10px', borderBottom: `1px solid ${C.border}`, flexShrink: 0, display: 'flex', alignItems: 'center', gap: 6 }}>
        <span style={{ fontSize: 11, fontWeight: 600, color: C.text, fontFamily: FONT_UI, flexShrink: 0 }}>📂</span>
        <select value={source} onChange={e => setSource(e.target.value)}
          style={{ padding: '4px 8px', borderRadius: 4, fontSize: 11, background: C.bg0, border: `1px solid ${C.border}`, color: C.text, fontFamily: FONT_UI, cursor: 'pointer', outline: 'none', flex: 1 }}>
          <option value="local">💾 Local Files</option>
          <option value="examples">📋 Examples</option>
          <option value="github">🐙 GitHub</option>
        </select>
      </div>
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
        {source === 'local' && <LocalBrowser onOpenFile={onOpenFile} onRefreshKey={vfsRefreshKey} />}
        {source === 'examples' && <ExamplesBrowser onOpenFile={onOpenFile} />}
        {source === 'github' && <GitHubBrowser onOpenFile={onOpenFile} defaultRepo={defaultGitHubRepo} />}
      </div>
    </div>
  );
}
