import { useState, useEffect, useRef, useMemo, useCallback } from 'react';
import tempFS from '../temporary';
import localFS from '../fs/local';
import { useTheme, FONT, FONT_UI } from '../theme';

const isMFile = name => name.endsWith('.m');
const isTextFile = name => {
  const exts = ['.m', '.txt', '.md', '.json', '.cpp', '.hpp', '.h', '.c', '.py', '.js', '.ts', '.jsx', '.css', '.html', '.yml', '.yaml', '.cmake', '.sh'];
  return exts.some(e => name.endsWith(e));
};

function rowStyle(isSel, isDir, name, depth, C) {
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
  const C = useTheme();
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
      boxShadow: '0 4px 16px rgba(0,0,0,0.3)', minWidth: 140, padding: '4px 0',
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
  const C = useTheme();
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

function TemporaryBrowser({ onOpenFile, onRefreshKey, isTabUnsaved }) {
  const C = useTheme();
  const [tree, setTree] = useState([]);
  const [expanded, setExpanded] = useState({});
  const [selected, setSelected] = useState(null);
  const [contextMenu, setContextMenu] = useState(null);
  const [creating, setCreating] = useState(null);
  const [renaming, setRenaming] = useState(null);

  const loadTree = useCallback(async () => {
    try { setTree(await tempFS.listTree()); } catch (e) { console.error('[TemporaryFS]', e); }
  }, []);
  useEffect(() => { loadTree(); }, [loadTree, onRefreshKey]);

  const handleFileDoubleClick = useCallback(async (node) => {
    if (node.type === 'file') { const content = await tempFS.readFile(node.path); onOpenFile(node.name, content !== null ? content : '', node.path, 'temporary'); }
  }, [onOpenFile]);

  const handleDuplicate = useCallback(async (node) => {
    if (node.type !== 'file') return;
    const content = await tempFS.readFile(node.path); if (content === null) return;
    const parent = node.path.substring(0, node.path.lastIndexOf('/'));
    const ext = node.name.includes('.') ? node.name.substring(node.name.lastIndexOf('.')) : '';
    const base = ext ? node.name.substring(0, node.name.lastIndexOf('.')) : node.name;
    let copyName = `${base}_copy${ext}`, copyPath = parent ? `${parent}/${copyName}` : `/${copyName}`, counter = 2;
    while (await tempFS.exists(copyPath)) { copyName = `${base}_copy${counter}${ext}`; copyPath = parent ? `${parent}/${copyName}` : `/${copyName}`; counter++; }
    await tempFS.writeFile(copyPath, content); loadTree();
  }, [loadTree]);

  // Import one or more files into `folderPath` ('' = root). Prompts on
  // collisions to keep it safe for the user. Does not touch tabs —
  // if a file that's currently open gets overwritten, the tab keeps
  // its in-memory state until the user closes / reopens it.
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
        if (await tempFS.exists(dest)) {
          const keep = confirm(
            `"${file.name}" already exists in Temporary.\n\nOK — overwrite.\nCancel — keep existing (skip this file).`
          );
          if (!keep) continue;
        }
        const text = await file.text();
        await tempFS.writeFile(dest, text);
        imported++;
      }
      if (imported > 0) {
        if (parent) setExpanded(p => ({ ...p, [parent]: true }));
        loadTree();
      }
    };
    input.click();
  }, [loadTree]);

  // Download one file from Temporary. Always reads the FS-committed
  // copy — if the file is currently open with unsaved edits the user
  // is warned so they can save first. Keeps this view free of
  // tab-state coupling beyond the narrow `isTabUnsaved` predicate.
  const handleDownload = useCallback(async (node) => {
    if (node.type !== 'file') return;
    if (isTabUnsaved && isTabUnsaved(node.path, 'temporary')) {
      const ok = confirm(
        `"${node.name}" has unsaved changes in the editor.\n\nThe download will contain the last saved content. Continue?`
      );
      if (!ok) return;
    }
    const content = await tempFS.readFile(node.path);
    if (content === null) return;
    const blob = new Blob([content], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = node.name;
    a.click();
    URL.revokeObjectURL(url);
  }, [isTabUnsaved]);

  const handleContextMenu = (e, node) => {
    e.preventDefault(); e.stopPropagation();
    const items = [];
    if (node.type === 'folder') {
      items.push({ icon: '📄', label: 'New File', action: () => { setExpanded(p => ({ ...p, [node.path]: true })); setCreating({ parentPath: node.path, type: 'file' }); } });
      items.push({ icon: '📁', label: 'New Folder', action: () => { setExpanded(p => ({ ...p, [node.path]: true })); setCreating({ parentPath: node.path, type: 'folder' }); } });
      items.push({ icon: '📥', label: 'Import file(s) here…', action: () => handleImport(node.path) });
      items.push({ separator: true });
    }
    if (node.type === 'file') {
      items.push({ icon: '📝', label: 'Open in Editor', action: () => handleFileDoubleClick(node) });
      items.push({ icon: '📋', label: 'Duplicate', action: () => handleDuplicate(node) });
      items.push({ icon: '⬇', label: 'Download', action: () => handleDownload(node) });
      items.push({ separator: true });
    }
    items.push({ icon: '✏️', label: 'Rename', action: () => setRenaming(node.path) });
    items.push({ icon: '🗑', label: 'Delete', danger: true, action: async () => { if (confirm(`Delete "${node.name}"?`)) { await tempFS.remove(node.path); loadTree(); } } });
    setContextMenu({ x: e.clientX, y: e.clientY, items });
  };

  const handleRootContextMenu = (e) => {
    e.preventDefault();
    setContextMenu({ x: e.clientX, y: e.clientY, items: [
      { icon: '📄', label: 'New File', action: () => setCreating({ parentPath: '', type: 'file' }) },
      { icon: '📁', label: 'New Folder', action: () => setCreating({ parentPath: '', type: 'folder' }) },
      { icon: '📥', label: 'Import file(s) here…', action: () => handleImport('') },
    ]});
  };

  const handleCreate = async name => {
    if (!name || !creating) { setCreating(null); return; }
    const parent = creating.parentPath || '';
    const path = parent ? `${parent}/${name}` : `/${name}`;
    if (creating.type === 'folder') await tempFS.mkdir(path);
    else { const fn = name.includes('.') ? name : name + '.m'; const fp = parent ? `${parent}/${fn}` : `/${fn}`; await tempFS.writeFile(fp, `% ${fn}\n`); }
    setCreating(null); loadTree();
  };

  const handleRename = async newName => {
    if (!newName || !renaming) { setRenaming(null); return; }
    const parent = renaming.substring(0, renaming.lastIndexOf('/'));
    await tempFS.rename(renaming, `${parent}/${newName}`);
    setRenaming(null); loadTree();
  };

  const renderTree = (nodes, depth = 0) => nodes.map(node => {
    if (renaming === node.path) return <div key={node.path} style={{ padding: '2px 6px', paddingLeft: depth * 14 + 6 }}><InlineInput defaultValue={node.name} onSubmit={handleRename} onCancel={() => setRenaming(null)} /></div>;
    const isDir = node.type === 'folder', isExp = expanded[node.path], isSel = selected === node.path;
    return (
      <div key={node.path}>
        <div onClick={() => isDir ? setExpanded(p => ({ ...p, [node.path]: !p[node.path] })) : setSelected(node.path)}
          onDoubleClick={() => !isDir && handleFileDoubleClick(node)}
          onContextMenu={e => handleContextMenu(e, node)}
          style={rowStyle(isSel, isDir, node.name, depth, C)}
          onMouseEnter={e => { if (!isSel) e.currentTarget.style.background = C.bg3; }}
          onMouseLeave={e => { if (!isSel) e.currentTarget.style.background = 'transparent'; }}>
          {isDir ? <span style={{ fontSize: 9, width: 12, textAlign: 'center', color: C.textMuted }}>{isExp ? '▼' : '▶'}</span>
                 : <span style={{ fontSize: 10, width: 12, textAlign: 'center' }}>📄</span>}
          <span style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{isDir ? '📁 ' : ''}{node.name}</span>
          {!isDir && isMFile(node.name) && <span style={{ fontSize: 7, padding: '0 3px', borderRadius: 2, background: `${C.green}22`, color: C.green }}>M</span>}
        </div>
        {isDir && isExp && (<>
          {creating && creating.parentPath === node.path && <div style={{ padding: '2px 6px', paddingLeft: (depth + 1) * 14 + 6 }}><InlineInput defaultValue="" placeholder={creating.type === 'folder' ? 'folder name' : 'filename.m'} onSubmit={handleCreate} onCancel={() => setCreating(null)} /></div>}
          {node.children && renderTree(node.children, depth + 1)}
        </>)}
      </div>
    );
  });

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{ display: 'flex', gap: 3, padding: '6px 8px', borderBottom: `1px solid ${C.border}`, flexShrink: 0 }}>
        <button onClick={() => setCreating({ parentPath: '', type: 'file' })} title="New file" style={{ padding: '2px 6px', borderRadius: 3, fontSize: 10, background: C.bg2, border: `1px solid ${C.border}`, color: C.textDim, cursor: 'pointer', fontFamily: FONT_UI }}>📄+</button>
        <button onClick={() => setCreating({ parentPath: '', type: 'folder' })} title="New folder" style={{ padding: '2px 6px', borderRadius: 3, fontSize: 10, background: C.bg2, border: `1px solid ${C.border}`, color: C.textDim, cursor: 'pointer', fontFamily: FONT_UI }}>📁+</button>
        <div style={{ flex: 1 }} />
        <button onClick={async () => { if (confirm('Clear all Temporary files?')) { await tempFS.clear(); loadTree(); } }} title="Clear all" style={{ padding: '2px 6px', borderRadius: 3, fontSize: 10, background: C.bg2, border: `1px solid ${C.border}`, color: C.textMuted, cursor: 'pointer', fontFamily: FONT_UI }}>🗑</button>
      </div>
      <div style={{ flex: 1, overflowY: 'auto', padding: '3px 0' }} onContextMenu={handleRootContextMenu}>
        {tree.length === 0 ? <div style={{ padding: 16, textAlign: 'center', color: C.textMuted, fontSize: 10, lineHeight: 1.6 }}>No files yet.<br />Click 📄+ to create a file<br />or right-click to Import.</div> : renderTree(tree)}
        {creating && (creating.parentPath === '' || creating.parentPath === '/') && <div style={{ padding: '2px 6px', paddingLeft: 6 }}><InlineInput defaultValue="" placeholder={creating.type === 'folder' ? 'folder name' : 'filename.m'} onSubmit={handleCreate} onCancel={() => setCreating(null)} /></div>}
      </div>
      {contextMenu && <ContextMenu x={contextMenu.x} y={contextMenu.y} items={contextMenu.items} onClose={() => setContextMenu(null)} />}
    </div>
  );
}

// ── Local Folder: real disk access via File System Access API ──────
//
// Only rendered when localFS.isAvailable() (Chromium-family browsers
// and the Electron desktop shell). Mirrors TemporaryBrowser's
// operations over a real directory the user picks through the native
// folder-picker dialog. The directory handle is cached in IndexedDB
// so reopening the IDE restores the tree after a permission refresh.
function LocalFolderBrowser({ onOpenFile, isTabUnsaved }) {
  const C = useTheme();
  const [tree, setTree] = useState([]);
  const [mountName, setMountName] = useState(null);
  const [status, setStatus] = useState('idle'); // idle | connecting | connected | denied
  const [error, setError] = useState(null);
  const [expanded, setExpanded] = useState({});
  const [selected, setSelected] = useState(null);
  const [contextMenu, setContextMenu] = useState(null);
  const [creating, setCreating] = useState(null);
  const [renaming, setRenaming] = useState(null);

  const loadTree = useCallback(async () => {
    try { setTree(await localFS.listTree()); } catch (e) { console.error('[LocalFS]', e); setError(String(e?.message || e)); }
  }, []);

  // On mount, try to silently reconnect to the previously-picked folder.
  useEffect(() => {
    let cancelled = false;
    (async () => {
      try {
        const name = await localFS.reconnect();
        if (cancelled) return;
        if (name) {
          setMountName(name);
          setStatus('connected');
          await loadTree();
        }
      } catch (e) {
        if (!cancelled) setError(String(e?.message || e));
      }
    })();
    return () => { cancelled = true; };
  }, [loadTree]);

  const handlePick = useCallback(async () => {
    setError(null);
    setStatus('connecting');
    try {
      const name = await localFS.pickDirectory();
      if (name) {
        setMountName(name);
        setStatus('connected');
        await loadTree();
      } else {
        setStatus(mountName ? 'connected' : 'idle');
      }
    } catch (e) {
      setError(String(e?.message || e));
      setStatus('denied');
    }
  }, [mountName, loadTree]);

  const handleDisconnect = useCallback(async () => {
    if (!confirm('Unmount this folder? Your files on disk are not affected.')) return;
    await localFS.disconnect();
    setMountName(null);
    setStatus('idle');
    setTree([]);
    setExpanded({});
    setSelected(null);
  }, []);

  const handleFileDoubleClick = useCallback(async (node) => {
    if (node.type === 'file') {
      const content = await localFS.readFile(node.path);
      onOpenFile(node.name, content !== null ? content : '', node.path, 'localFolder');
    }
  }, [onOpenFile]);

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
        if (await localFS.exists(dest)) {
          const ok = confirm(`"${file.name}" already exists on disk.\n\nOK — overwrite.\nCancel — keep existing (skip this file).`);
          if (!ok) continue;
        }
        const text = await file.text();
        await localFS.writeFile(dest, text);
        imported++;
      }
      if (imported > 0) {
        if (parent) setExpanded(p => ({ ...p, [parent]: true }));
        loadTree();
      }
    };
    input.click();
  }, [loadTree]);

  const handleDownload = useCallback(async (node) => {
    if (node.type !== 'file') return;
    if (isTabUnsaved && isTabUnsaved(node.path, 'localFolder')) {
      const ok = confirm(
        `"${node.name}" has unsaved changes in the editor.\n\nThe download will contain the on-disk content. Continue?`
      );
      if (!ok) return;
    }
    const content = await localFS.readFile(node.path);
    if (content === null) return;
    const blob = new Blob([content], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = node.name;
    a.click();
    URL.revokeObjectURL(url);
  }, [isTabUnsaved]);

  // Whether the mounted backend can launch the OS file manager.
  // True only in the Electron desktop shell (native preload bridge);
  // web Chromium has FSA handles with no underlying OS path.
  const canReveal = useMemo(() => localFS.supportsReveal(), []);

  const handleContextMenu = (e, node) => {
    e.preventDefault(); e.stopPropagation();
    const items = [];
    if (node.type === 'folder') {
      items.push({ icon: '📄', label: 'New File', action: () => { setExpanded(p => ({ ...p, [node.path]: true })); setCreating({ parentPath: node.path, type: 'file' }); } });
      items.push({ icon: '📁', label: 'New Folder', action: () => { setExpanded(p => ({ ...p, [node.path]: true })); setCreating({ parentPath: node.path, type: 'folder' }); } });
      items.push({ icon: '📥', label: 'Import file(s) here…', action: () => handleImport(node.path) });
      if (canReveal) items.push({ icon: '📂', label: 'Reveal in Explorer', action: async () => {
        try { await localFS.revealInExplorer(node.path); }
        catch (err) { alert('Reveal failed: ' + (err?.message || err)); }
      }});
      items.push({ separator: true });
    }
    if (node.type === 'file') {
      items.push({ icon: '📝', label: 'Open in Editor', action: () => handleFileDoubleClick(node) });
      items.push({ icon: '⬇', label: 'Download', action: () => handleDownload(node) });
      if (canReveal) items.push({ icon: '📂', label: 'Reveal in Explorer', action: async () => {
        try { await localFS.showItemInFolder(node.path); }
        catch (err) { alert('Reveal failed: ' + (err?.message || err)); }
      }});
      items.push({ separator: true });
    }
    items.push({ icon: '✏️', label: 'Rename', action: () => setRenaming(node.path) });
    items.push({ icon: '🗑', label: 'Delete', danger: true, action: async () => {
      if (!confirm(`Delete "${node.name}" on disk?`)) return;
      try { await localFS.remove(node.path); loadTree(); } catch (e) { alert('Delete failed: ' + (e?.message || e)); }
    }});
    setContextMenu({ x: e.clientX, y: e.clientY, items });
  };

  const handleRootContextMenu = (e) => {
    if (status !== 'connected') return;
    e.preventDefault();
    const items = [
      { icon: '📄', label: 'New File', action: () => setCreating({ parentPath: '', type: 'file' }) },
      { icon: '📁', label: 'New Folder', action: () => setCreating({ parentPath: '', type: 'folder' }) },
      { icon: '📥', label: 'Import file(s) here…', action: () => handleImport('') },
    ];
    if (canReveal) items.push({ icon: '📂', label: 'Reveal in Explorer', action: async () => {
      try { await localFS.revealInExplorer(''); }
      catch (err) { alert('Reveal failed: ' + (err?.message || err)); }
    }});
    setContextMenu({ x: e.clientX, y: e.clientY, items });
  };

  const handleCreate = async name => {
    if (!name || !creating) { setCreating(null); return; }
    const parent = creating.parentPath || '';
    const path = parent ? `${parent}/${name}` : `/${name}`;
    try {
      if (creating.type === 'folder') await localFS.mkdir(path);
      else { const fn = name.includes('.') ? name : name + '.m'; const fp = parent ? `${parent}/${fn}` : `/${fn}`; await localFS.writeFile(fp, `% ${fn}\n`); }
    } catch (e) { alert('Create failed: ' + (e?.message || e)); }
    setCreating(null); loadTree();
  };

  const handleRename = async newName => {
    if (!newName || !renaming) { setRenaming(null); return; }
    const parent = renaming.substring(0, renaming.lastIndexOf('/'));
    try { await localFS.rename(renaming, `${parent}/${newName}`); }
    catch (e) { alert('Rename failed: ' + (e?.message || e)); }
    setRenaming(null); loadTree();
  };

  const renderTree = (nodes, depth = 0) => nodes.map(node => {
    if (renaming === node.path) return <div key={node.path} style={{ padding: '2px 6px', paddingLeft: depth * 14 + 6 }}><InlineInput defaultValue={node.name} onSubmit={handleRename} onCancel={() => setRenaming(null)} /></div>;
    const isDir = node.type === 'folder', isExp = expanded[node.path], isSel = selected === node.path;
    return (
      <div key={node.path}>
        <div onClick={() => isDir ? setExpanded(p => ({ ...p, [node.path]: !p[node.path] })) : setSelected(node.path)}
          onDoubleClick={() => !isDir && handleFileDoubleClick(node)}
          onContextMenu={e => handleContextMenu(e, node)}
          style={rowStyle(isSel, isDir, node.name, depth, C)}
          onMouseEnter={e => { if (!isSel) e.currentTarget.style.background = C.bg3; }}
          onMouseLeave={e => { if (!isSel) e.currentTarget.style.background = 'transparent'; }}>
          {isDir ? <span style={{ fontSize: 9, width: 12, textAlign: 'center', color: C.textMuted }}>{isExp ? '▼' : '▶'}</span>
                 : <span style={{ fontSize: 10, width: 12, textAlign: 'center' }}>📄</span>}
          <span style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{isDir ? '📁 ' : ''}{node.name}</span>
          {!isDir && isMFile(node.name) && <span style={{ fontSize: 7, padding: '0 3px', borderRadius: 2, background: `${C.green}22`, color: C.green }}>M</span>}
        </div>
        {isDir && isExp && (<>
          {creating && creating.parentPath === node.path && <div style={{ padding: '2px 6px', paddingLeft: (depth + 1) * 14 + 6 }}><InlineInput defaultValue="" placeholder={creating.type === 'folder' ? 'folder name' : 'filename.m'} onSubmit={handleCreate} onCancel={() => setCreating(null)} /></div>}
          {node.children && renderTree(node.children, depth + 1)}
        </>)}
      </div>
    );
  });

  // ── States ──
  if (status === 'idle') {
    return (
      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', padding: 16, textAlign: 'center', color: C.textDim, fontSize: 11, gap: 10 }}>
        <div style={{ fontSize: 11, lineHeight: 1.6 }}>Pick a folder on your disk.<br />Files are read and saved in place.</div>
        <button onClick={handlePick} style={{ padding: '6px 12px', borderRadius: 4, fontSize: 11, background: C.accent, color: '#fff', border: 'none', cursor: 'pointer', fontFamily: FONT_UI, fontWeight: 600 }}>📂 Open Folder…</button>
      </div>
    );
  }
  if (status === 'denied') {
    return (
      <div style={{ padding: 14, fontSize: 11, color: C.textDim, lineHeight: 1.5 }}>
        <div style={{ color: C.red, marginBottom: 8 }}>Permission denied.</div>
        {error && <div style={{ marginBottom: 8, color: C.textMuted, fontFamily: FONT, fontSize: 10 }}>{error}</div>}
        <button onClick={handlePick} style={{ padding: '5px 10px', borderRadius: 4, fontSize: 11, background: C.bg2, color: C.text, border: `1px solid ${C.border}`, cursor: 'pointer', fontFamily: FONT_UI }}>🔄 Try again</button>
      </div>
    );
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{ display: 'flex', gap: 3, padding: '6px 8px', borderBottom: `1px solid ${C.border}`, flexShrink: 0, alignItems: 'center' }}>
        <span title={mountName} style={{ flex: 1, fontSize: 10, color: C.textDim, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', fontFamily: FONT_UI }}>💾 {mountName}</span>
        <button onClick={() => setCreating({ parentPath: '', type: 'file' })} title="New file" style={{ padding: '2px 6px', borderRadius: 3, fontSize: 10, background: C.bg2, border: `1px solid ${C.border}`, color: C.textDim, cursor: 'pointer', fontFamily: FONT_UI }}>📄+</button>
        <button onClick={() => setCreating({ parentPath: '', type: 'folder' })} title="New folder" style={{ padding: '2px 6px', borderRadius: 3, fontSize: 10, background: C.bg2, border: `1px solid ${C.border}`, color: C.textDim, cursor: 'pointer', fontFamily: FONT_UI }}>📁+</button>
        {canReveal && <button onClick={async () => { try { await localFS.revealInExplorer(''); } catch (err) { alert('Reveal failed: ' + (err?.message || err)); } }} title="Reveal current folder in Explorer" style={{ padding: '2px 6px', borderRadius: 3, fontSize: 10, background: C.bg2, border: `1px solid ${C.border}`, color: C.textDim, cursor: 'pointer', fontFamily: FONT_UI }}>📂</button>}
        <button onClick={handlePick} title="Change folder" style={{ padding: '2px 6px', borderRadius: 3, fontSize: 10, background: C.bg2, border: `1px solid ${C.border}`, color: C.textDim, cursor: 'pointer', fontFamily: FONT_UI }}>🔄</button>
        <button onClick={handleDisconnect} title="Unmount folder" style={{ padding: '2px 6px', borderRadius: 3, fontSize: 10, background: C.bg2, border: `1px solid ${C.border}`, color: C.textMuted, cursor: 'pointer', fontFamily: FONT_UI }}>✕</button>
      </div>
      <div style={{ flex: 1, overflowY: 'auto', padding: '3px 0' }} onContextMenu={handleRootContextMenu}>
        {tree.length === 0 ? <div style={{ padding: 16, textAlign: 'center', color: C.textMuted, fontSize: 10, lineHeight: 1.6 }}>Empty folder.<br />Right-click to add files.</div> : renderTree(tree)}
        {creating && (creating.parentPath === '' || creating.parentPath === '/') && <div style={{ padding: '2px 6px', paddingLeft: 6 }}><InlineInput defaultValue="" placeholder={creating.type === 'folder' ? 'folder name' : 'filename.m'} onSubmit={handleCreate} onCancel={() => setCreating(null)} /></div>}
      </div>
      {contextMenu && <ContextMenu x={contextMenu.x} y={contextMenu.y} items={contextMenu.items} onClose={() => setContextMenu(null)} />}
    </div>
  );
}

function ExamplesBrowser({ onOpenFile }) {
  const C = useTheme();
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
          name: folder.name.replace(/_/g, ' '), path: `/examples/${folder.name}`, type: 'folder',
          children: folder.files.map(f => ({ name: f, path: `/examples/${folder.name}/${f}`, type: 'file', _fetchPath: `${base}examples/${folder.name}/${f}` })),
        }));
        setTree(nodes);
        if (nodes.length > 0) setExpanded({ [nodes[0].path]: true });
      } catch (e) { console.warn('[Examples]', e); setTree([]); }
      finally { setLoading(false); }
    })();
  }, []);

  const handleFileDoubleClick = useCallback(async (node) => {
    if (node.type !== 'file' || !node._fetchPath) return;
    try { const res = await fetch(node._fetchPath); if (!res.ok) throw new Error('fetch failed'); const content = await res.text(); onOpenFile(node.name, content, null, 'examples'); } catch (e) { console.error('[Examples]', e); }
  }, [onOpenFile]);

  const renderTree = (nodes, depth = 0) => nodes.map(node => {
    const isDir = node.type === 'folder', isExp = expanded[node.path], isSel = selected === node.path;
    return (
      <div key={node.path}>
        <div onClick={() => isDir ? setExpanded(p => ({ ...p, [node.path]: !p[node.path] })) : setSelected(node.path)}
          onDoubleClick={() => !isDir && handleFileDoubleClick(node)}
          style={rowStyle(isSel, isDir, node.name, depth, C)}
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
      {tree.length === 0 ? <div style={{ padding: 16, textAlign: 'center', color: C.textMuted, fontSize: 10 }}>No examples found.</div>
        : <><div style={{ padding: '4px 8px', fontSize: 9, color: C.textMuted, fontStyle: 'italic' }}>Double-click to open in editor</div>{renderTree(tree)}</>}
    </div>
  );
}

function GitHubBrowser({ onOpenFile, defaultRepo }) {
  const C = useTheme();
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

  const parseRepo = url => { const c = url.trim().replace(/\/+$/, '').replace(/\.git$/, ''); let m = c.match(/github\.com\/([^/]+)\/([^/]+)/); if (m) return { owner: m[1], repo: m[2] }; m = c.match(/^([^/\s]+)\/([^/\s]+)$/); if (m) return { owner: m[1], repo: m[2] }; return null; };

  const fetchRepo = useCallback(async (urlOverride) => {
    const url = urlOverride || repoUrl; const p = parseRepo(url);
    if (!p) { setError('Use: owner/repo'); return; }
    setLoading(true); setError(''); setTree(null); setPreviewFile(null);
    try {
      const infoRes = await fetch(`https://api.github.com/repos/${p.owner}/${p.repo}`);
      if (!infoRes.ok) throw new Error(infoRes.status === 404 ? 'Repository not found' : `API error: ${infoRes.status}`);
      const info = await infoRes.json(); setRepoInfo(info);
      const brRes = await fetch(`https://api.github.com/repos/${p.owner}/${p.repo}/branches?per_page=20`);
      if (brRes.ok) setBranches((await brRes.json()).map(b => b.name));
      const treeRes = await fetch(`https://api.github.com/repos/${p.owner}/${p.repo}/git/trees/${info.default_branch || 'main'}?recursive=1`);
      if (!treeRes.ok) throw new Error('Failed to fetch file tree');
      setTree((await treeRes.json()).tree || []); setBranch(info.default_branch || 'main');
    } catch (err) { setError(err.message); } finally { setLoading(false); }
  }, [repoUrl]);

  useEffect(() => { if (defaultRepo) fetchRepo(defaultRepo); }, []); // eslint-disable-line

  const fetchBranch = async branchName => {
    const p = parseRepo(repoUrl); if (!p) return;
    setLoading(true); setError(''); setBranch(branchName); setPreviewFile(null);
    try { const r = await fetch(`https://api.github.com/repos/${p.owner}/${p.repo}/git/trees/${branchName}?recursive=1`); if (!r.ok) throw new Error('Failed'); setTree((await r.json()).tree || []); } catch (err) { setError(err.message); } finally { setLoading(false); }
  };

  const fetchFileContent = useCallback(async (path) => {
    const p = parseRepo(repoUrl); if (!p) return; setPreviewLoading(true); setPreviewFile(path);
    try { const r = await fetch(`https://api.github.com/repos/${p.owner}/${p.repo}/contents/${path}?ref=${branch}`); if (!r.ok) throw new Error('Failed'); const d = await r.json(); setPreviewContent(d.encoding === 'base64' ? atob(d.content) : d.content || ''); } catch (e) { setPreviewContent(`Error: ${e.message}`); } finally { setPreviewLoading(false); }
  }, [repoUrl, branch]);

  const handleFileDoubleClick = useCallback(async (path, name) => {
    const p = parseRepo(repoUrl); if (!p) return;
    try { const r = await fetch(`https://api.github.com/repos/${p.owner}/${p.repo}/contents/${path}?ref=${branch}`); if (!r.ok) throw new Error('Failed'); const d = await r.json(); onOpenFile(name, d.encoding === 'base64' ? atob(d.content) : d.content || '', null, 'github'); } catch (e) { console.error('[GitHub]', e); }
  }, [repoUrl, branch, onOpenFile]);

  const nested = useMemo(() => {
    if (!tree) return [];
    const root = { children: {} };
    for (const item of tree) { const parts = item.path.split('/'); let node = root; for (let i = 0; i < parts.length; i++) { if (!node.children[parts[i]]) node.children[parts[i]] = { name: parts[i], path: parts.slice(0, i + 1).join('/'), type: i === parts.length - 1 ? item.type : 'tree', children: {} }; node = node.children[parts[i]]; } }
    const flatten = obj => Object.values(obj).sort((a, b) => { if (a.type === 'tree' && b.type !== 'tree') return -1; if (a.type !== 'tree' && b.type === 'tree') return 1; return a.name.localeCompare(b.name); }).map(n => ({ ...n, children: n.children ? flatten(n.children) : undefined }));
    return flatten(root.children);
  }, [tree]);

  const renderTree = (nodes, depth = 0) => nodes.map(node => {
    const isDir = node.type === 'tree', isExp = expanded[node.path], isSel = selected === node.path;
    return (
      <div key={node.path}>
        <div onClick={() => isDir ? setExpanded(p => ({ ...p, [node.path]: !p[node.path] })) : (() => { setSelected(node.path); if (isTextFile(node.name)) fetchFileContent(node.path); })()}
          onDoubleClick={() => !isDir && isTextFile(node.name) && handleFileDoubleClick(node.path, node.name)}
          style={rowStyle(isSel, isDir, node.name, depth, C)}
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
        {repoInfo && <div style={{ marginTop: 4, fontSize: 9, color: C.textMuted, display: 'flex', gap: 6 }}><span>⭐ {repoInfo.stargazers_count}</span><span>🍴 {repoInfo.forks_count}</span>{repoInfo.language && <span>💻 {repoInfo.language}</span>}</div>}
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

export default function FileBrowser({ onOpenFile, defaultGitHubRepo, vfsRefreshKey, isTabUnsaved }) {
  const C = useTheme();
  const [source, setSource] = useState('temporary');
  // File System Access API presence is fixed per-browser-session.
  // Firefox / Safari report false and the "Local Folder" option is
  // simply not offered.
  const hasLocalFolder = useMemo(() => localFS.isAvailable(), []);
  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{ padding: '7px 10px', borderBottom: `1px solid ${C.border}`, flexShrink: 0, display: 'flex', alignItems: 'center', gap: 6 }}>
        <span style={{ fontSize: 11, fontWeight: 600, color: C.text, fontFamily: FONT_UI, flexShrink: 0 }}>📂</span>
        <select value={source} onChange={e => setSource(e.target.value)}
          style={{ padding: '4px 8px', borderRadius: 4, fontSize: 11, background: C.bg0, border: `1px solid ${C.border}`, color: C.text, fontFamily: FONT_UI, cursor: 'pointer', outline: 'none', flex: 1 }}>
          <option value="temporary">📌 Temporary</option>
          {hasLocalFolder && <option value="localFolder">💾 Local Folder</option>}
          <option value="examples">📋 Examples</option>
          <option value="github">🐙 GitHub</option>
        </select>
      </div>
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
        {source === 'temporary' && <TemporaryBrowser onOpenFile={onOpenFile} onRefreshKey={vfsRefreshKey} isTabUnsaved={isTabUnsaved} />}
        {source === 'localFolder' && hasLocalFolder && <LocalFolderBrowser onOpenFile={onOpenFile} isTabUnsaved={isTabUnsaved} />}
        {source === 'examples' && <ExamplesBrowser onOpenFile={onOpenFile} />}
        {source === 'github' && <GitHubBrowser onOpenFile={onOpenFile} defaultRepo={defaultGitHubRepo} />}
      </div>
    </div>
  );
}
