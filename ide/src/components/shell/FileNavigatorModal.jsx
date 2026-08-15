import React, { useState, useEffect, useCallback, useMemo } from 'react';
import tempFS from '../../temporary';
import localFS from '../../fs/local';
import ModalWindow from '../ui/ModalWindow';

function formatBytes(bytes) {
  if (bytes === undefined || bytes === null || isNaN(bytes)) return '—';
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

function formatDate(ts) {
  if (!ts) return '—';
  try {
    const d = new Date(ts);
    return d.toLocaleString(undefined, {
      year: 'numeric', month: 'short', day: 'numeric',
      hour: '2-digit', minute: '2-digit'
    });
  } catch {
    return '—';
  }
}

/* ─────────────── monochrome vector icons (consistent 16×16 grid) ─────────────── */
const ICON_BASE = {
  fill: 'none',
  stroke: 'currentColor',
  strokeWidth: 1.25,
  strokeLinecap: 'round',
  strokeLinejoin: 'round',
};

const NavIcons = {
  folder: ({ size = 14, className = '' }) => (
    <svg width={size} height={size} viewBox="0 0 16 16" {...ICON_BASE} className={className}>
      <path d="M1.5 3.5h4l1.5 2H14.5a1 1 0 0 1 1 1v6.5a1 1 0 0 1-1 1H1.5a1 1 0 0 1-1-1v-7.5a1 1 0 0 1 1-1z" />
    </svg>
  ),
  parentFolder: ({ size = 14, className = '' }) => (
    <svg width={size} height={size} viewBox="0 0 16 16" {...ICON_BASE} className={className}>
      <path d="M1.5 3.5h4l1.5 2H14.5a1 1 0 0 1 1 1v6.5a1 1 0 0 1-1 1H1.5a1 1 0 0 1-1-1v-7.5a1 1 0 0 1 1-1z" />
      <path d="M8 11.5V7.5M6 9.5l2-2 2 2" />
    </svg>
  ),
  script: ({ size = 14, className = '' }) => (
    <svg width={size} height={size} viewBox="0 0 16 16" {...ICON_BASE} className={className}>
      <path d="M3 2h7l3.5 3.5V14a1 1 0 0 1-1 1H3a1 1 0 0 1-1-1V3a1 1 0 0 1 1-1z" />
      <path d="M10 2v4h3.5" />
      <path d="M5 8.5h6M5 11.5h4" />
    </svg>
  ),
  data: ({ size = 14, className = '' }) => (
    <svg width={size} height={size} viewBox="0 0 16 16" {...ICON_BASE} className={className}>
      <rect x="2.5" y="2.5" width="11" height="11" rx="1.5" />
      <path d="M2.5 6.5h11M6.5 6.5v7M10.5 6.5v7" />
    </svg>
  ),
  media: ({ size = 14, className = '' }) => (
    <svg width={size} height={size} viewBox="0 0 16 16" {...ICON_BASE} className={className}>
      <rect x="2.5" y="2.5" width="11" height="11" rx="1.5" />
      <circle cx="5.5" cy="5.5" r="1.25" />
      <path d="M13.5 11l-3.5-3.5-6 6" />
    </svg>
  ),
  audio: ({ size = 14, className = '' }) => (
    <svg width={size} height={size} viewBox="0 0 16 16" {...ICON_BASE} className={className}>
      <path d="M5.5 11.5V3.5l7-1.5v8" />
      <circle cx="4" cy="11.5" r="1.75" />
      <circle cx="11" cy="10" r="1.75" />
    </svg>
  ),
  file: ({ size = 14, className = '' }) => (
    <svg width={size} height={size} viewBox="0 0 16 16" {...ICON_BASE} className={className}>
      <path d="M3 2h7l3.5 3.5V14a1 1 0 0 1-1 1H3a1 1 0 0 1-1-1V3a1 1 0 0 1 1-1z" />
      <path d="M10 2v4h3.5" />
    </svg>
  ),
  refresh: ({ size = 12 }) => (
    <svg width={size} height={size} viewBox="0 0 16 16" {...ICON_BASE}>
      <path d="M13.5 8A5.5 5.5 0 1 1 8 2.5c2 0 3.8.9 4.9 2.4" />
      <path d="M13 2v3.5H9.5" />
    </svg>
  ),
  newFolder: ({ size = 12 }) => (
    <svg width={size} height={size} viewBox="0 0 16 16" {...ICON_BASE}>
      <path d="M1.5 3.5h4l1.5 2H14.5a1 1 0 0 1 1 1v6.5a1 1 0 0 1-1 1H1.5a1 1 0 0 1-1-1v-7.5a1 1 0 0 1 1-1z" />
      <path d="M8 7.5v4M6 9.5h4" />
    </svg>
  ),
};

function renderFileIcon(type, name = '', size = 14) {
  if (type === 'folder') return <NavIcons.folder size={size} />;
  if (/\.(m|n)$/i.test(name)) return <NavIcons.script size={size} />;
  if (/\.(mat|dat|csv|tsv|json)$/i.test(name)) return <NavIcons.data size={size} />;
  if (/\.(wav|mp3|ogg|flac|m4a)$/i.test(name)) return <NavIcons.audio size={size} />;
  if (/\.(png|jpe?g|bmp|gif|webp|tiff?|svg)$/i.test(name)) return <NavIcons.media size={size} />;
  return <NavIcons.file size={size} />;
}

/**
 * FileNavigatorModal — Unified full-featured file navigator modal matching FigureWindow & VariableEditor.
 */
export default function FileNavigatorModal({
  onClose,
  fsMode = 'virtual',
  onFsModeChange,
  currentCwd = '/',
  onSetCurrentFolder,
  onOpenFile,
  localAvailable = false,
  localMountName = null,
}) {
  const [navFsMode, setNavFsMode] = useState(fsMode);
  const [browsePath, setBrowsePath] = useState(() => {
    if (fsMode === 'local') {
      return currentCwd || (typeof localFS !== 'undefined' && localFS.root?.()) || 'C:\\';
    }
    return currentCwd || '/';
  });
  const [items, setItems] = useState([]);
  const [selectedItem, setSelectedItem] = useState(null);
  const [filterText, setFilterText] = useState('');
  const [sortBy, setSortBy] = useState('name'); // 'name' | 'type' | 'size' | 'modified'
  const [sortAsc, setSortAsc] = useState(true);
  const [loading, setLoading] = useState(false);
  const [creatingFolder, setCreatingFolder] = useState(false);
  const [newFolderName, setNewFolderName] = useState('');

  const activeFS = navFsMode === 'local' ? localFS : tempFS;

  // Load directory contents for browsePath
  const loadDirectory = useCallback(async () => {
    setLoading(true);
    setSelectedItem(null);
    try {
      if (navFsMode === 'local') {
        if (typeof localFS.setRootPath === 'function' && browsePath && (/^[A-Za-z]:/.test(browsePath) || browsePath.startsWith('/'))) {
          localFS.setRootPath(browsePath);
        }
        const entries = await localFS.listDir?.(browsePath || '');
        if (Array.isArray(entries)) {
          setItems(entries.map((e) => ({
            name: e.name,
            type: e.type === 'directory' ? 'folder' : (e.type || 'file'),
            size: e.size,
            modified: e.modified || e.mtime,
            path: e.path || (browsePath.endsWith('\\') || browsePath.endsWith('/') ? `${browsePath}${e.name}` : `${browsePath}\\${e.name}`),
          })));
        } else {
          setItems([]);
        }
      } else {
        const entries = await tempFS.listDir?.(browsePath || '/');
        if (Array.isArray(entries)) {
          setItems(entries.map((e) => ({
            name: e.name,
            type: e.type === 'directory' ? 'folder' : (e.type || 'file'),
            size: e.size,
            modified: e.modified || e.mtime,
            path: e.path || (browsePath.endsWith('/') ? `${browsePath}${e.name}` : `${browsePath}/${e.name}`),
          })));
        } else {
          setItems([]);
        }
      }
    } catch (err) {
      console.warn('[FileNavigatorModal] load error:', err);
      setItems([]);
    } finally {
      setLoading(false);
    }
  }, [navFsMode, browsePath]);

  useEffect(() => {
    loadDirectory();
  }, [loadDirectory]);

  // Navigate Up
  const handleNavigateUp = useCallback(() => {
    if (navFsMode === 'local') {
      const isWin = /^[A-Za-z]:/.test(browsePath) || browsePath.includes('\\');
      if (isWin) {
        let norm = browsePath.replace(/\//g, '\\');
        while (norm.endsWith('\\') && norm.length > 3) norm = norm.slice(0, -1);
        const lastSlash = norm.lastIndexOf('\\');
        if (lastSlash < 0 || norm.length <= 3) {
          return;
        }
        if (lastSlash === 2) {
          setBrowsePath(norm.slice(0, 3));
          return;
        }
        setBrowsePath(norm.slice(0, lastSlash));
        return;
      }
    }
    const clean = browsePath.replace(/\/+$/, '');
    const lastSlash = clean.lastIndexOf('/');
    if (lastSlash <= 0) {
      setBrowsePath('/');
    } else {
      setBrowsePath(clean.slice(0, lastSlash));
    }
  }, [navFsMode, browsePath]);

  // Double click item
  const handleItemDoubleClick = useCallback(async (item) => {
    if (!item) return;
    if (item.type === 'folder') {
      if (navFsMode === 'local') {
        const isWin = /^[A-Za-z]:/.test(browsePath) || browsePath.includes('\\');
        const sep = isWin ? '\\' : '/';
        const newPath = browsePath.endsWith(sep) || (isWin && /^[A-Za-z]:\\?$/.test(browsePath))
          ? `${browsePath.replace(/\\?$/, sep)}${item.name}`
          : `${browsePath}${sep}${item.name}`;
        setBrowsePath(newPath);
      } else {
        const newPath = item.path || (browsePath.endsWith('/') ? `${browsePath}${item.name}` : `${browsePath}/${item.name}`);
        setBrowsePath(newPath);
      }
    } else {
      // Open file
      try {
        let content = '';
        if (navFsMode === 'local') {
          content = await localFS.readFile?.(item.path || item.name);
        } else {
          content = await tempFS.readFile?.(item.path || item.name);
        }
        onOpenFile?.(item.name, content, item.path, navFsMode === 'local' ? 'localFolder' : 'temporary');
        onClose?.();
      } catch (err) {
        console.warn('[FileNavigatorModal] Open file failed:', err);
      }
    }
  }, [browsePath, navFsMode, onOpenFile, onClose]);

  // Set as current folder
  const handleSetCurrent = useCallback(() => {
    const targetFolder = selectedItem && selectedItem.type === 'folder'
      ? selectedItem.path
      : browsePath;
    if (navFsMode !== fsMode) {
      onFsModeChange?.(navFsMode);
    }
    onSetCurrentFolder?.(targetFolder);
    onClose?.();
  }, [selectedItem, browsePath, navFsMode, fsMode, onFsModeChange, onSetCurrentFolder, onClose]);

  // Create folder
  const handleCreateFolder = useCallback(async () => {
    if (!newFolderName.trim()) return;
    const folderName = newFolderName.trim();
    try {
      if (navFsMode === 'local') {
        const isWin = /^[A-Za-z]:/.test(browsePath) || browsePath.includes('\\');
        const sep = isWin ? '\\' : '/';
        const full = browsePath.endsWith(sep) ? `${browsePath}${folderName}` : `${browsePath}${sep}${folderName}`;
        await localFS.createDir?.(full);
      } else {
        const full = browsePath.endsWith('/') ? `${browsePath}${folderName}` : `${browsePath}/${folderName}`;
        await tempFS.createDir?.(full);
      }
      setNewFolderName('');
      setCreatingFolder(false);
      loadDirectory();
    } catch (err) {
      alert(`Create folder failed: ${err?.message || err}`);
    }
  }, [newFolderName, navFsMode, browsePath, loadDirectory]);

  // Breadcrumbs
  const breadcrumbs = useMemo(() => {
    if (navFsMode === 'local') {
      const isWin = /^[A-Za-z]:/.test(browsePath) || browsePath.includes('\\');
      if (isWin) {
        const parts = browsePath.split(/[\\/]+/).filter(Boolean);
        const crumbs = [];
        let acc = '';
        parts.forEach((p, idx) => {
          if (idx === 0) {
            acc = p.includes(':') ? `${p}\\` : p;
            crumbs.push({ label: p.toUpperCase(), path: acc });
          } else {
            acc = acc.endsWith('\\') ? `${acc}${p}` : `${acc}\\${p}`;
            crumbs.push({ label: p, path: acc });
          }
        });
        return crumbs;
      }
    }
    const parts = browsePath.split('/').filter(Boolean);
    const crumbs = [{ label: 'Root', path: '/' }];
    let acc = '';
    parts.forEach((p) => {
      acc += '/' + p;
      crumbs.push({ label: p, path: acc });
    });
    return crumbs;
  }, [navFsMode, browsePath]);

  // Filtered and sorted items
  const displayItems = useMemo(() => {
    let list = items;
    if (filterText.trim()) {
      const f = filterText.toLowerCase();
      list = list.filter((i) => (i.name || '').toLowerCase().includes(f));
    }
    return [...list].sort((a, b) => {
      // Folders always grouped first
      if (a.type !== b.type) return a.type === 'folder' ? -1 : 1;
      let cmp = 0;
      if (sortBy === 'name') {
        cmp = (a.name || '').localeCompare(b.name || '', undefined, { numeric: true, sensitivity: 'base' });
      } else if (sortBy === 'type') {
        const extA = a.type === 'folder' ? 'folder' : ((a.name || '').split('.').pop() || '');
        const extB = b.type === 'folder' ? 'folder' : ((b.name || '').split('.').pop() || '');
        cmp = extA.localeCompare(extB, undefined, { numeric: true, sensitivity: 'base' }) || (a.name || '').localeCompare(b.name || '');
      } else if (sortBy === 'size') {
        const sizeA = typeof a.size === 'number' ? a.size : -1;
        const sizeB = typeof b.size === 'number' ? b.size : -1;
        cmp = sizeA - sizeB || (a.name || '').localeCompare(b.name || '');
      } else if (sortBy === 'modified') {
        const modA = typeof a.modified === 'number' ? a.modified : (a.modified ? new Date(a.modified).getTime() : 0);
        const modB = typeof b.modified === 'number' ? b.modified : (b.modified ? new Date(b.modified).getTime() : 0);
        cmp = modA - modB || (a.name || '').localeCompare(b.name || '');
      }
      return sortAsc ? cmp : -cmp;
    });
  }, [items, filterText, sortBy, sortAsc]);

  const isAtRoot = useMemo(() => {
    if (navFsMode === 'local') {
      return !browsePath || browsePath === '/' || browsePath === '\\' || /^[A-Za-z]:[\\/]?$/.test(browsePath);
    }
    return !browsePath || browsePath === '/' || browsePath === '';
  }, [navFsMode, browsePath]);

  const handleSort = (column) => {
    if (sortBy === column) {
      setSortAsc(!sortAsc);
    } else {
      setSortBy(column);
      setSortAsc(true);
    }
  };

  return (
    <ModalWindow
      onClose={onClose}
      title="Explorer"
      width="min(1200px, 94vw)"
      height="min(780px, 90vh)"
      className="nav-modal-window"
      footer={(
        <>
          <div className="nav-modal-footer-cwd">
            <span style={{ color: 'var(--fg-3)', marginRight: '6px' }}>Selected Folder:</span>
            <span style={{ fontFamily: 'var(--font-mono)', fontWeight: 600, color: 'var(--fg-0)' }}>
              {selectedItem && selectedItem.type === 'folder' ? selectedItem.path : browsePath}
            </span>
          </div>

          <div style={{ display: 'flex', gap: '8px' }}>
            <button
              className="ve-btn"
              onClick={handleSetCurrent}
              title="Set this folder as the MATLAB/Numkit Current Working Directory"
              style={{ background: 'var(--accent)', color: 'var(--accent-contrast, #fff)', fontWeight: 600 }}
            >
              Select as Current Folder
            </button>
            <button className="ve-btn" onClick={onClose}>Close</button>
          </div>
        </>
      )}
    >
      {/* ── Toolbar ── */}
      <div className="nav-modal-toolbar">
        <div className="nav-modal-mode-select-wrap">
          <label style={{ fontSize: '12px', color: 'var(--fg-3)', marginRight: '6px' }}>File System:</label>
          <select
            className="cf-mode-select"
            value={navFsMode}
            onChange={(e) => {
              const nextMode = e.target.value;
              setNavFsMode(nextMode);
              if (nextMode === 'local') {
                setBrowsePath((typeof localFS !== 'undefined' && localFS.root?.()) || 'C:\\');
              } else {
                setBrowsePath('/');
              }
            }}
          >
            <option value="virtual">Virtual</option>
            <option value="local" disabled={!localAvailable}>
              Local
            </option>
          </select>
        </div>

        <button
          className="cf-btn"
          onClick={loadDirectory}
          title="Refresh Directory"
          style={{ display: 'inline-flex', alignItems: 'center', gap: '4px', marginLeft: '8px' }}
        >
          <NavIcons.refresh />
          <span>Refresh</span>
        </button>

        <button
          className="cf-btn"
          onClick={() => setCreatingFolder(true)}
          title="Create New Folder"
          style={{ display: 'inline-flex', alignItems: 'center', gap: '4px' }}
        >
          <NavIcons.newFolder />
          <span>New Folder</span>
        </button>

        <div style={{ flex: 1 }} />

        <div className="nav-modal-filter-wrap">
          <input
            type="text"
            className="nav-modal-filter-input"
            placeholder="Search files…"
            value={filterText}
            onChange={(e) => setFilterText(e.target.value)}
          />
        </div>
      </div>

      {/* ── Breadcrumbs Bar ── */}
      <div className="nav-modal-breadcrumbs">
        <span style={{ color: 'var(--fg-3)', fontSize: '11px', marginRight: '6px' }}>Path:</span>
        {breadcrumbs.map((crumb, idx) => (
          <React.Fragment key={crumb.path}>
            {idx > 0 && <span className="nav-breadcrumb-sep">/</span>}
            <button
              className={`nav-breadcrumb-btn ${crumb.path === browsePath ? 'is-active' : ''}`}
              onClick={() => setBrowsePath(crumb.path)}
            >
              {crumb.label}
            </button>
          </React.Fragment>
        ))}
      </div>

      {/* ── New Folder inline prompt ── */}
      {creatingFolder && (
        <div className="nav-modal-new-folder-bar">
          <span style={{ marginRight: '8px' }}>New folder name:</span>
          <input
            type="text"
            className="cf-path-input"
            style={{ width: '220px', padding: '3px 8px', background: 'var(--bg-1)', border: '1px solid var(--line)' }}
            value={newFolderName}
            autoFocus
            onChange={(e) => setNewFolderName(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === 'Enter') handleCreateFolder();
              if (e.key === 'Escape') setCreatingFolder(false);
            }}
          />
          <button className="ve-btn" style={{ marginLeft: '6px' }} onClick={handleCreateFolder}>Create</button>
          <button className="ve-btn" style={{ marginLeft: '4px' }} onClick={() => setCreatingFolder(false)}>Cancel</button>
        </div>
      )}

      {/* ── Main Body (Table + Info Sidebar) ── */}
      <div className="nav-modal-body">
        {/* Table */}
        <div className="nav-modal-table-container">
          <table className="nav-modal-table">
            <thead>
              <tr>
                <th
                  style={{ width: '45%', cursor: 'pointer' }}
                  onClick={() => handleSort('name')}
                  title="Sort by Name"
                >
                  Name {sortBy === 'name' ? (sortAsc ? '▲' : '▼') : ''}
                </th>
                <th
                  style={{ width: '15%', cursor: 'pointer' }}
                  onClick={() => handleSort('type')}
                  title="Sort by Type"
                >
                  Type {sortBy === 'type' ? (sortAsc ? '▲' : '▼') : ''}
                </th>
                <th
                  style={{ width: '18%', textAlign: 'right', cursor: 'pointer' }}
                  onClick={() => handleSort('size')}
                  title="Sort by Size"
                >
                  Size {sortBy === 'size' ? (sortAsc ? '▲' : '▼') : ''}
                </th>
                <th
                  style={{ width: '22%', textAlign: 'right', cursor: 'pointer' }}
                  onClick={() => handleSort('modified')}
                  title="Sort by Modified Date"
                >
                  Modified {sortBy === 'modified' ? (sortAsc ? '▲' : '▼') : ''}
                </th>
              </tr>
            </thead>
            <tbody>
              {!isAtRoot && (
                <tr className="nav-modal-row" onDoubleClick={handleNavigateUp}>
                  <td colSpan={4} style={{ color: 'var(--fg-3)', cursor: 'pointer' }}>
                    <span className="nav-row-icon" style={{ display: 'inline-flex', alignItems: 'center', verticalAlign: 'middle', marginRight: 6, color: 'var(--fg-3)' }}>
                      <NavIcons.parentFolder size={14} />
                    </span>
                    ..
                  </td>
                </tr>
              )}
              {displayItems.map((item) => {
                const isSelected = selectedItem?.path === item.path;
                const ext = item.type === 'folder' ? 'Folder' : (item.name.split('.').pop()?.toUpperCase() || 'File');
                return (
                  <tr
                    key={item.path}
                    className={`nav-modal-row ${isSelected ? 'is-selected' : ''}`}
                    onClick={() => setSelectedItem(item)}
                    onDoubleClick={() => handleItemDoubleClick(item)}
                  >
                    <td>
                      <span className="nav-row-icon" style={{ display: 'inline-flex', alignItems: 'center', verticalAlign: 'middle', marginRight: 6, color: 'var(--fg-2)' }}>
                        {renderFileIcon(item.type, item.name, 14)}
                      </span>
                      <span className="nav-row-name">{item.name}</span>
                    </td>
                    <td style={{ color: 'var(--fg-2)', fontSize: '11px' }}>{ext}</td>
                    <td style={{ textAlign: 'right', fontFamily: 'var(--font-mono)', fontSize: '11px' }}>
                      {item.type === 'folder' ? '—' : formatBytes(item.size)}
                    </td>
                    <td style={{ textAlign: 'right', color: 'var(--fg-3)', fontSize: '11px' }}>
                      {formatDate(item.modified)}
                    </td>
                  </tr>
                );
              })}
              {displayItems.length === 0 && !loading && (
                <tr>
                  <td colSpan={4} style={{ textAlign: 'center', padding: '30px', color: 'var(--fg-3)' }}>
                    Folder is empty
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>

        {/* Info Sidebar */}
        <div className="nav-modal-info-panel">
          <div className="nav-info-title">File Details</div>
          {selectedItem ? (
            <div className="nav-info-content">
              <div className="nav-info-icon-large" style={{ display: 'flex', justifyContent: 'center', padding: '16px 0', color: 'var(--fg-2)' }}>
                {renderFileIcon(selectedItem.type, selectedItem.name, 44)}
              </div>
              <div className="nav-info-name">{selectedItem.name}</div>
              <div className="nav-info-prop">
                <span className="nav-prop-label">Type:</span>
                <span>{selectedItem.type === 'folder' ? 'Folder' : 'File'}</span>
              </div>
              <div className="nav-info-prop">
                <span className="nav-prop-label">Path:</span>
                <span className="nav-prop-val-break">{selectedItem.path}</span>
              </div>
              {selectedItem.size !== undefined && (
                <div className="nav-info-prop">
                  <span className="nav-prop-label">Size:</span>
                  <span>{formatBytes(selectedItem.size)}</span>
                </div>
              )}
              {selectedItem.modified && (
                <div className="nav-info-prop">
                  <span className="nav-prop-label">Modified:</span>
                  <span>{formatDate(selectedItem.modified)}</span>
                </div>
              )}

              <div style={{ marginTop: '20px', display: 'flex', flexDirection: 'column', gap: '8px' }}>
                {selectedItem.type === 'file' && (
                  <button
                    className="ve-btn"
                    style={{ width: '100%', justifyContent: 'center' }}
                    onClick={() => handleItemDoubleClick(selectedItem)}
                  >
                    Open in Editor
                  </button>
                )}
                {selectedItem.type === 'folder' && (
                  <button
                    className="ve-btn"
                    style={{ width: '100%', justifyContent: 'center' }}
                    onClick={() => {
                      if (navFsMode === 'local') {
                        const isWin = /^[A-Za-z]:/.test(browsePath) || browsePath.includes('\\');
                        const sep = isWin ? '\\' : '/';
                        const newPath = browsePath.endsWith(sep) || (isWin && /^[A-Za-z]:\\?$/.test(browsePath))
                          ? `${browsePath.replace(/\\?$/, sep)}${selectedItem.name}`
                          : `${browsePath}${sep}${selectedItem.name}`;
                        setBrowsePath(newPath);
                      } else {
                        setBrowsePath(selectedItem.path);
                      }
                    }}
                  >
                    Enter Folder
                  </button>
                )}
                {typeof activeFS.revealInExplorer === 'function' && (
                  <button
                    className="ve-btn"
                    style={{ width: '100%', justifyContent: 'center' }}
                    onClick={() => activeFS.revealInExplorer(selectedItem.path)}
                  >
                    Reveal in Explorer
                  </button>
                )}
              </div>
            </div>
          ) : (
            <div className="nav-info-empty">Select a file or folder to view details</div>
          )}
        </div>
      </div>
    </ModalWindow>
  );
}
