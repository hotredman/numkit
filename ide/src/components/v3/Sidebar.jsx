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
              padding: '5px 12px', fontSize: 11,
              color: item.danger ? 'var(--danger)' : 'var(--fg-1)',
              cursor: 'pointer',
            }}
            onMouseEnter={(e) => (e.currentTarget.style.background = 'var(--bg-4)')}
            onMouseLeave={(e) => (e.currentTarget.style.background = 'transparent')}>
            {item.icon && <span style={{ marginRight: 6 }}>{item.icon}</span>}{item.label}
          </div>
        )
      )}
    </div>
  );
}

/* ─────────────── tree row + folder ─────────────── */
function TreeRow({ node, depth, expanded, setExpanded, selected, setSelected,
                  onOpenFile, onContextMenu, renaming, onRenameSubmit, onRenameCancel,
                  filter }) {
  const isDir = node.type === 'folder';
  const isExp = !!expanded[node.path];
  const isSel = selected === node.path;

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
    if (isDir) setExpanded((p) => ({ ...p, [node.path]: !p[node.path] }));
    else setSelected(node.path);
  };
  const onDouble = () => { if (!isDir) onOpenFile?.(node); };

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
      </div>
      {isDir && isExp && Array.isArray(node.children) && node.children.map((c) => (
        <TreeRow key={c.path}
          node={c} depth={depth + 1}
          expanded={expanded} setExpanded={setExpanded}
          selected={selected} setSelected={setSelected}
          onOpenFile={onOpenFile} onContextMenu={onContextMenu}
          renaming={renaming} onRenameSubmit={onRenameSubmit} onRenameCancel={onRenameCancel}
          filter={filter} />
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

async function openExample(node, tree, vfsAdapters) {
  if (node.type !== 'file' || !node._fetchPath) return null;
  const res = await fetch(node._fetchPath);
  if (!res.ok) throw new Error('fetch failed');
  const content = await res.text();

  // Mirror folder into tempFS at /Examples/<Folder>/<file> so sibling
  // .m lookup works for multi-file examples.
  let vfsPath = null;
  const m = node.path.match(/^\/examples\/([^/]+)\/(.+)$/);
  if (m && vfsAdapters?.temp) {
    const [, folder, fname] = m;
    const folderNode = tree.find((n) => n.path === `/examples/${folder}`);
    const siblings = folderNode?.children?.filter((c) => c.type === 'file') || [];
    await Promise.all(siblings.map(async (sib) => {
      const sibVfsPath = `/Examples/${folder}/${sib.name}`;
      if (sib.name === fname) return;
      if (vfsAdapters.temp.exists(sibVfsPath)) return;
      try {
        const sr = await fetch(sib._fetchPath);
        if (sr.ok) vfsAdapters.temp.writeFile(sibVfsPath, await sr.text());
      } catch { /* per-file fetch failure tolerated */ }
    }));
    vfsPath = `/Examples/${folder}/${fname}`;
    vfsAdapters.temp.writeFile(vfsPath, content);
  }
  return { content, vfsPath };
}

/* ─────────────── source-specific operations ─────────────── */
function makeOps(source) {
  const fs = source === 'localFolder' ? localFS : tempFS;
  return {
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
  onOpenFile,
  vfsRefreshKey,
  isTabUnsaved,
  onLocalMount,
  vfsAdapters,
}) {
  const localAvailable = typeof localFS !== 'undefined' && localFS.isAvailable?.();
  const [source, setSource] = usePersistedState('numkit.ide.sidebar.source', 'examples');
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
  const ops = useMemo(() => makeOps(source === 'examples' ? 'temporary' : source), [source]);

  const loadTree = useCallback(async () => {
    try {
      if (isExamples) setTree(await loadExamplesTree());
      else setTree(await ops.listTree());
    } catch (e) { console.error('[Sidebar] listTree failed', e); }
  }, [ops, isExamples]);

  // Reload on source change + on external write signal
  useEffect(() => { loadTree(); }, [loadTree, vfsRefreshKey]);

  // Restore Local Folder mount on mount
  useEffect(() => {
    if (source !== 'localFolder' || !localAvailable) return;
    let cancelled = false;
    (async () => {
      try {
        const name = await localFS.reconnect();
        if (cancelled) return;
        if (name) {
          setLocalMountName(name);
          setLocalStatus('connected');
          await loadTree();
          if (onLocalMount) await onLocalMount();
        }
      } catch (e) {
        if (!cancelled) console.warn('[Sidebar] reconnect failed', e);
      }
    })();
    return () => { cancelled = true; };
  }, [source, localAvailable, loadTree, onLocalMount]);

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
        await loadTree();
        if (onLocalMount) await onLocalMount();
      } else {
        setLocalStatus(localMountName ? 'connected' : 'idle');
      }
    } catch (e) {
      console.error('[Sidebar] pickDirectory failed', e);
      setLocalStatus('denied');
    }
  }, [localMountName, loadTree, onLocalMount]);

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
        if (r) onOpenFile?.(node.name, r.content, r.vfsPath, 'examples');
      } catch (e) { console.error('[Sidebar] openExample', e); }
      return;
    }
    const content = await ops.readFile(node.path);
    onOpenFile?.(node.name, content !== null ? content : '', node.path, source);
  }, [ops, onOpenFile, source, isExamples, tree, vfsAdapters]);

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
        items.push({ icon: '📝', label: 'Open in Editor', action: () => handleFileOpen(node) });
        items.push({ separator: true });
      }
      items.push({ icon: '🔄', label: 'Refresh', action: () => loadTree() });
      setContextMenu({ x: e.clientX, y: e.clientY, items });
      return;
    }
    const items = [];
    if (node.type === 'folder') {
      items.push({ icon: '📄', label: 'New file…',
        action: () => { setExpanded((p) => ({ ...p, [node.path]: true })); setCreating({ parentPath: node.path, type: 'file' }); } });
      items.push({ icon: '📁', label: 'New folder…',
        action: () => { setExpanded((p) => ({ ...p, [node.path]: true })); setCreating({ parentPath: node.path, type: 'folder' }); } });
      items.push({ icon: '📥', label: 'Import file(s) here…',
        action: () => handleImport(node.path) });
      items.push({ separator: true });
    } else {
      items.push({ icon: '📝', label: 'Open in Editor', action: () => handleFileOpen(node) });
      items.push({ icon: '📋', label: 'Duplicate', action: () => handleDuplicate(node) });
      items.push({ icon: '⬇',  label: 'Download',   action: () => handleDownload(node) });
      items.push({ separator: true });
    }
    items.push({ icon: '✏', label: 'Rename', action: () => setRenaming(node.path) });
    items.push({ icon: '🗑', label: 'Delete', danger: true, action: () => handleDelete(node) });
    items.push({ separator: true });
    items.push({ icon: '🔄', label: 'Refresh', action: () => loadTree() });
    setContextMenu({ x: e.clientX, y: e.clientY, items });
  }, [isExamples, setExpanded, handleImport, handleFileOpen, handleDuplicate, handleDownload, handleDelete, loadTree]);

  const handleRootContextMenu = useCallback((e) => {
    e.preventDefault();
    if (isExamples) {
      // Examples folder is read-only on disk, but the user may want to
      // re-pull the manifest if it was edited externally.
      setContextMenu({
        x: e.clientX, y: e.clientY,
        items: [{ icon: '🔄', label: 'Refresh', action: () => loadTree() }],
      });
      return;
    }
    setContextMenu({
      x: e.clientX, y: e.clientY,
      items: [
        { icon: '📄', label: 'New file…',         action: () => setCreating({ parentPath: '', type: 'file' }) },
        { icon: '📁', label: 'New folder…',       action: () => setCreating({ parentPath: '', type: 'folder' }) },
        { icon: '📥', label: 'Import file(s)…',   action: () => handleImport('') },
        { separator: true },
        { icon: '🔄', label: 'Refresh',           action: () => loadTree() },
      ],
    });
  }, [isExamples, handleImport, loadTree]);

  /* ─── render ─── */
  const isLocalUnmounted = source === 'localFolder' && localStatus !== 'connected';

  return (
    <aside className="sidebar">
      {/* Source picker + new-file button */}
      <div className="sidebar-head">
        <select className="ws-picker"
          value={source}
          onChange={(e) => switchSource(e.target.value)}>
          <option value="examples">Examples</option>
          <option value="temporary">Temporary</option>
          {localAvailable && <option value="localFolder">Local Folder</option>}
        </select>
        {!isExamples && (
          <button className="sidebar-icon" title="New file"
            onClick={() => setCreating({ parentPath: '', type: 'file' })}>
            <svg width="11" height="11" viewBox="0 0 12 12">
              <path d="M6 2v8M2 6h8" stroke="currentColor" strokeWidth="1.3" strokeLinecap="round"/>
            </svg>
          </button>
        )}
      </div>

      {/* Search */}
      <div className="sidebar-search">
        <svg width="10" height="10" viewBox="0 0 12 12">
          <circle cx="5" cy="5" r="3.2" stroke="currentColor" fill="none"/>
          <path d="M7.4 7.4L10 10" stroke="currentColor"/>
        </svg>
        {filter ? (
          <input
            value={filter}
            onChange={(e) => setFilter(e.target.value)}
            placeholder="filter files…"
            spellCheck={false}
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

      {/* Local Folder mount info */}
      {source === 'localFolder' && localStatus === 'connected' && localMountName && (
        <div style={{
          padding: '4px 12px', fontSize: 10, color: 'var(--fg-3)',
          display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        }}>
          <span title={localMountName} style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
            📁 {localMountName}
          </span>
          <button onClick={handleUnmount} title="Unmount"
            style={{
              background: 'transparent', border: 'none', color: 'var(--fg-3)',
              cursor: 'pointer', padding: '0 4px', fontSize: 11,
            }}>×</button>
        </div>
      )}

      {/* Tree */}
      {!isLocalUnmounted && (
        <div className="sidebar-tree" onContextMenu={handleRootContextMenu}>
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
              onOpenFile={handleFileOpen} onContextMenu={handleContextMenu}
              renaming={renaming}
              onRenameSubmit={handleRename}
              onRenameCancel={() => setRenaming(null)}
              filter={filter.trim()}
            />
          ))}
          {creating && (creating.parentPath === '' || creating.parentPath === '/') && (
            <div className="tree-row" style={{ paddingLeft: 16 }}>
              <InlineInput defaultValue=""
                placeholder={creating.type === 'folder' ? 'folder name' : 'filename.m'}
                onSubmit={handleCreate}
                onCancel={() => setCreating(null)} />
            </div>
          )}
        </div>
      )}

      {contextMenu && (
        <ContextMenu x={contextMenu.x} y={contextMenu.y}
          items={contextMenu.items} onClose={() => setContextMenu(null)} />
      )}
    </aside>
  );
}
