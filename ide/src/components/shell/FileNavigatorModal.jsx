import React, { useState, useEffect, useCallback, useMemo } from 'react';
import tempFS from '../../temporary';
import localFS from '../../fs/local';

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

function getFileIcon(type, name = '') {
  if (type === 'folder') return '📁';
  if (/\.m$/i.test(name)) return '📄';
  if (/\.(mat|dat|csv|tsv|json)$/i.test(name)) return '📊';
  if (/\.(wav|mp3|ogg|flac|m4a)$/i.test(name)) return '🎵';
  if (/\.(png|jpe?g|bmp|gif|webp|tiff?)$/i.test(name)) return '🖼️';
  return '📝';
}

/**
 * FileNavigatorModal — full-featured file navigator modal matching FigureWindow size.
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
  const [browsePath, setBrowsePath] = useState(currentCwd || '/');
  const [items, setItems] = useState([]);
  const [selectedItem, setSelectedItem] = useState(null);
  const [filterText, setFilterText] = useState('');
  const [sortBy, setSortBy] = useState('name'); // 'name' | 'type' | 'size' | 'modified'
  const [sortAsc, setSortAsc] = useState(true);
  const [loading, setLoading] = useState(false);
  const [creatingFolder, setCreatingFolder] = useState(false);
  const [newFolderName, setNewFolderName] = useState('');

  // Close on Escape
  useEffect(() => {
    const handler = (e) => { if (e.key === 'Escape') onClose?.(); };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [onClose]);

  // Active FS backend
  const activeFS = navFsMode === 'local' ? localFS : tempFS;

  // Load directory contents for browsePath
  const loadDirectory = useCallback(async () => {
    setLoading(true);
    setSelectedItem(null);
    try {
      let rawTree = [];
      if (typeof activeFS.listTree === 'function') {
        rawTree = await activeFS.listTree();
      }

      // Normalize browsePath for search
      let targetPath = browsePath || '/';
      targetPath = targetPath.replace(/\\/g, '/');
      if (!targetPath.startsWith('/')) targetPath = '/' + targetPath;
      if (targetPath.length > 1 && targetPath.endsWith('/')) targetPath = targetPath.slice(0, -1);

      // Find node corresponding to browsePath
      function findChildren(nodes, curRel) {
        if (curRel === targetPath || (targetPath === '/' && curRel === '/')) {
          return nodes;
        }
        for (const n of nodes) {
          const nPath = (n.path || '').replace(/\\/g, '/');
          if (nPath === targetPath) {
            return n.children || [];
          }
          if (n.type === 'folder' && targetPath.startsWith(nPath + '/')) {
            const res = findChildren(n.children || [], nPath);
            if (res) return res;
          }
        }
        return null;
      }

      let childNodes = findChildren(rawTree, '/');
      if (!childNodes) {
        // Fallback: list immediate items matching parent
        childNodes = rawTree.filter((n) => {
          const np = (n.path || '').replace(/\\/g, '/');
          if (targetPath === '/') {
            const segs = np.split('/').filter(Boolean);
            return segs.length === 1;
          }
          if (np.startsWith(targetPath + '/')) {
            const rel = np.slice(targetPath.length + 1);
            return !rel.includes('/');
          }
          return false;
        });
      }

      setItems(Array.isArray(childNodes) ? childNodes : []);
    } catch (err) {
      console.error('[FileNavigatorModal] load error:', err);
      setItems([]);
    } finally {
      setLoading(false);
    }
  }, [activeFS, browsePath]);

  useEffect(() => {
    loadDirectory();
  }, [loadDirectory]);

  // Navigate Up one directory
  const handleNavigateUp = () => {
    let p = browsePath.replace(/\\/g, '/');
    if (p.endsWith('/')) p = p.slice(0, -1);
    const lastSlash = p.lastIndexOf('/');
    if (lastSlash <= 0) {
      setBrowsePath('/');
    } else {
      setBrowsePath(p.slice(0, lastSlash));
    }
  };

  // Double click item
  const handleItemDoubleClick = async (item) => {
    if (item.type === 'folder') {
      setBrowsePath(item.path);
    } else {
      // Open file
      try {
        const content = await activeFS.readFile(item.path);
        onOpenFile?.(item.name, content !== null ? content : '', item.path, navFsMode === 'local' ? 'localFolder' : 'temporary');
        onClose?.();
      } catch (err) {
        console.error('[FileNavigatorModal] open file error:', err);
      }
    }
  };

  const handleSetCurrent = () => {
    let target = browsePath;
    if (selectedItem && selectedItem.type === 'folder') {
      target = selectedItem.path;
    }
    if (onFsModeChange && navFsMode !== fsMode) {
      onFsModeChange(navFsMode);
    }
    onSetCurrentFolder?.(target, navFsMode);
    onClose?.();
  };

  const handleCreateFolder = async () => {
    if (!newFolderName.trim()) {
      setCreatingFolder(false);
      return;
    }
    const folderPath = browsePath === '/' ? `/${newFolderName.trim()}` : `${browsePath}/${newFolderName.trim()}`;
    try {
      await activeFS.mkdir(folderPath);
      setCreatingFolder(false);
      setNewFolderName('');
      loadDirectory();
    } catch (err) {
      console.error('[FileNavigatorModal] mkdir error:', err);
    }
  };

  // Breadcrumbs
  const breadcrumbs = useMemo(() => {
    const parts = (browsePath || '/').split(/[\\/]/).filter(Boolean);
    const crumbs = [{ label: navFsMode === 'local' ? (localMountName || 'Local Root') : 'Root', path: '/' }];
    let acc = '';
    for (const part of parts) {
      acc += '/' + part;
      crumbs.push({ label: part, path: acc });
    }
    return crumbs;
  }, [browsePath, navFsMode, localMountName]);

  // Filter and sort items
  const displayItems = useMemo(() => {
    let list = items;
    if (filterText.trim()) {
      const f = filterText.toLowerCase();
      list = list.filter((i) => (i.name || '').toLowerCase().includes(f));
    }
    return [...list].sort((a, b) => {
      // Folders always first
      if (a.type !== b.type) return a.type === 'folder' ? -1 : 1;
      let cmp = 0;
      if (sortBy === 'name') cmp = (a.name || '').localeCompare(b.name || '');
      else if (sortBy === 'type') cmp = ((a.name || '').split('.').pop() || '').localeCompare((b.name || '').split('.').pop() || '');
      else if (sortBy === 'size') cmp = (a.size || 0) - (b.size || 0);
      else if (sortBy === 'modified') cmp = (a.modified || 0) - (b.modified || 0);
      return sortAsc ? cmp : -cmp;
    });
  }, [items, filterText, sortBy, sortAsc]);

  const isAtRoot = !browsePath || browsePath === '/' || browsePath === '\\';

  return (
    <div className="fw-overlay" onClick={(e) => { if (e.target === e.currentTarget) onClose?.(); }}>
      <div className="fw-window nav-modal-window" role="dialog" aria-modal="true" style={{ width: '85vw', height: '80vh', maxWidth: '1100px' }}>
        
        {/* ── Titlebar ── */}
        <div className="fw-titlebar">
          <div className="fw-title">
            <span style={{ marginRight: '8px' }}>📁</span>
            <span>File Navigator — MATLAB Current Folder</span>
          </div>
          <button className="ve-btn ve-close" onClick={onClose} title="Close (Esc)">✕</button>
        </div>

        {/* ── Toolbar ── */}
        <div className="nav-modal-toolbar">
          <div className="nav-modal-mode-select-wrap">
            <label style={{ fontSize: '12px', color: 'var(--text-muted)', marginRight: '6px' }}>Filesystem:</label>
            <select
              className="cf-mode-select"
              value={navFsMode}
              onChange={(e) => {
                setNavFsMode(e.target.value);
                setBrowsePath('/');
              }}
            >
              <option value="virtual">⚡ Virtual File System (Temporary)</option>
              <option value="local" disabled={!localAvailable}>
                📁 Local File System {localMountName ? `(${localMountName})` : ''}
              </option>
            </select>
          </div>

          <div style={{ width: '1px', height: '20px', background: 'var(--border)', margin: '0 8px' }} />

          <button
            className="cf-btn cf-btn-up"
            onClick={handleNavigateUp}
            disabled={isAtRoot}
            title="Up One Level (..)"
          >
            <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
              <path fillRule="evenodd" d="M8 3.5a.5.5 0 0 1 .5.5v9a.5.5 0 0 1-1 0V4a.5.5 0 0 1 .5-.5z"/>
              <path fillRule="evenodd" d="M7.646 2.146a.5.5 0 0 1 .708 0l4 4a.5.5 0 0 1-.708.708L8 3.207 4.354 6.854a.5.5 0 1 1-.708-.708l4-4z"/>
            </svg>
            <span style={{ marginLeft: '4px', fontSize: '11px' }}>Up</span>
          </button>

          <button
            className="cf-btn"
            onClick={loadDirectory}
            title="Refresh Directory"
            style={{ display: 'inline-flex', alignItems: 'center' }}
          >
            <span style={{ marginRight: '4px' }}>🔄</span> Refresh
          </button>

          <button
            className="cf-btn"
            onClick={() => setCreatingFolder(true)}
            title="Create New Folder"
            style={{ display: 'inline-flex', alignItems: 'center' }}
          >
            <span style={{ marginRight: '4px' }}>➕</span> New Folder
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
          <span style={{ color: 'var(--text-muted)', fontSize: '11px', marginRight: '6px' }}>Path:</span>
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
            <span>New Folder Name:</span>
            <input
              type="text"
              autoFocus
              className="nav-modal-filter-input"
              style={{ width: '220px', marginLeft: '8px' }}
              value={newFolderName}
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
                    style={{ width: '50%' }}
                    onClick={() => { if (sortBy === 'name') setSortAsc(!sortAsc); else { setSortBy('name'); setSortAsc(true); } }}
                  >
                    Name {sortBy === 'name' ? (sortAsc ? '▲' : '▼') : ''}
                  </th>
                  <th
                    style={{ width: '15%' }}
                    onClick={() => { if (sortBy === 'type') setSortAsc(!sortAsc); else { setSortBy('type'); setSortAsc(true); } }}
                  >
                    Type {sortBy === 'type' ? (sortAsc ? '▲' : '▼') : ''}
                  </th>
                  <th
                    style={{ width: '15%', textAlign: 'right' }}
                    onClick={() => { if (sortBy === 'size') setSortAsc(!sortAsc); else { setSortBy('size'); setSortAsc(true); } }}
                  >
                    Size {sortBy === 'size' ? (sortAsc ? '▲' : '▼') : ''}
                  </th>
                  <th
                    style={{ width: '20%', textAlign: 'right' }}
                    onClick={() => { if (sortBy === 'modified') setSortAsc(!sortAsc); else { setSortBy('modified'); setSortAsc(true); } }}
                  >
                    Modified {sortBy === 'modified' ? (sortAsc ? '▲' : '▼') : ''}
                  </th>
                </tr>
              </thead>
              <tbody>
                {!isAtRoot && (
                  <tr className="nav-modal-row" onDoubleClick={handleNavigateUp}>
                    <td colSpan={4} style={{ color: 'var(--text-muted)', cursor: 'pointer' }}>
                      <span style={{ marginRight: '6px' }}>📁</span> .. [Parent Folder]
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
                        <span className="nav-row-icon">{getFileIcon(item.type, item.name)}</span>
                        <span className="nav-row-name">{item.name}</span>
                      </td>
                      <td style={{ color: 'var(--text-muted)', fontSize: '11px' }}>{ext}</td>
                      <td style={{ textAlign: 'right', fontFamily: 'var(--font-mono)', fontSize: '11px' }}>
                        {item.type === 'folder' ? '—' : formatBytes(item.size)}
                      </td>
                      <td style={{ textAlign: 'right', color: 'var(--text-muted)', fontSize: '11px' }}>
                        {formatDate(item.modified)}
                      </td>
                    </tr>
                  );
                })}
                {displayItems.length === 0 && !loading && (
                  <tr>
                    <td colSpan={4} style={{ textAlign: 'center', padding: '30px', color: 'var(--text-muted)' }}>
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
                <div className="nav-info-icon-large">{getFileIcon(selectedItem.type, selectedItem.name)}</div>
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
                      onClick={() => setBrowsePath(selectedItem.path)}
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

        {/* ── Footer ── */}
        <div className="fw-footer nav-modal-footer">
          <div className="nav-modal-footer-cwd">
            <span style={{ color: 'var(--text-muted)', marginRight: '6px' }}>Selected Folder:</span>
            <span style={{ fontFamily: 'var(--font-mono)', fontWeight: 600 }}>
              {selectedItem && selectedItem.type === 'folder' ? selectedItem.path : browsePath}
            </span>
          </div>

          <div style={{ display: 'flex', gap: '8px' }}>
            <button
              className="ve-btn"
              onClick={handleSetCurrent}
              title="Set this folder as the MATLAB/Numkit Current Working Directory"
              style={{ background: 'var(--accent)', color: 'var(--accent-contrast, #fff)' }}
            >
              Select as Current Folder
            </button>
            <button className="ve-btn" onClick={onClose}>Close</button>
          </div>
        </div>
      </div>
    </div>
  );
}
