import React, { useState, useEffect } from 'react';

/**
 * CurrentFolderBar — MATLAB-style Current Folder strip.
 *
 * Layout:
 * File System [Local/Virtual]  Current Folder  [Up][Browse][ Path Input ]
 */
export default function CurrentFolderBar({
  fsMode = 'virtual', // 'local' | 'virtual'
  onFsModeChange,
  cwd = '/',
  onCwdChange,
  onNavigateUp,
  onOpenNavigator,
  localAvailable = false,
  localMountName = null,
}) {
  const [editingPath, setEditingPath] = useState(cwd);
  const [isEditing, setIsEditing] = useState(false);

  useEffect(() => {
    setEditingPath(cwd);
  }, [cwd]);

  const handleKeyDown = (e) => {
    if (e.key === 'Enter') {
      setIsEditing(false);
      if (editingPath.trim() !== cwd) {
        onCwdChange?.(editingPath.trim());
      }
    } else if (e.key === 'Escape') {
      setIsEditing(false);
      setEditingPath(cwd);
    }
  };

  const isAtRoot = !cwd || cwd === '/' || cwd === '\\' || /^[A-Za-z]:[\\/]?$/.test(cwd);

  return (
    <div className="current-folder-bar">
      {/* 1. File System label */}
      <span className="cf-label">File System:</span>

      {/* 2. [Local/Virtual] selector */}
      <div className="cf-mode-wrapper">
        <select
          className="cf-mode-select"
          value={fsMode}
          onChange={(e) => onFsModeChange?.(e.target.value)}
          title="Select Active Working Filesystem"
        >
          <option value="virtual">Virtual</option>
          <option value="local" disabled={!localAvailable}>
            Local
          </option>
        </select>
      </div>

      <span className="cf-sep" />

      {/* 3. Current Folder label */}
      <span className="cf-label">Current Folder</span>

      {/* 4. [Up] compact button */}
      <button
        className="cf-btn cf-btn-icon-only"
        onClick={onNavigateUp}
        disabled={isAtRoot}
        title="Up One Level (..)"
        aria-label="Up One Level"
      >
        <svg width="13" height="13" viewBox="0 0 16 16" fill="currentColor">
          <path fillRule="evenodd" d="M8 12a.5.5 0 0 0 .5-.5V4.707l2.646 2.647a.5.5 0 0 0 .708-.708l-3.5-3.5a.5.5 0 0 0-.708 0l-3.5 3.5a.5.5 0 1 0 .708.708L7.5 4.707V11.5a.5.5 0 0 0 .5.5z"/>
        </svg>
      </button>

      {/* 5. [Browse] compact button */}
      <button
        className="cf-btn cf-btn-icon-only"
        onClick={onOpenNavigator}
        title="Browse Folders (File Navigator)"
        aria-label="Browse Folders"
      >
        <svg width="13" height="13" viewBox="0 0 16 16" fill="currentColor">
          <path d="M1.5 2A1.5 1.5 0 0 0 0 3.5v9A1.5 1.5 0 0 0 1.5 14h13a1.5 1.5 0 0 0 1.5-1.5v-7A1.5 1.5 0 0 0 14.5 4H7.414l-1.707-1.707A1 1 0 0 0 5 2H1.5zm0 1H5l1.707 1.707A1 1 0 0 0 7.414 5H14.5a.5.5 0 0 1 .5.5v7a.5.5 0 0 1-.5.5h-13a.5.5 0 0 1-.5-.5v-9a.5.5 0 0 1 .5-.5z"/>
        </svg>
      </button>

      {/* 6. [ строка для пути ] */}
      <div className="cf-path-box">
        <svg className="cf-path-icon" width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
          <path d="M.5 3l.04.87a1.99 1.99 0 0 0-.342 1.311l.637 7A2 2 0 0 0 2.826 14H13.174a2 2 0 0 0 1.99-1.819l.637-7A1.99 1.99 0 0 0 15.46 3.87L15.5 3H9.414L7.707 1.293A1 1 0 0 0 7 1H2a2 2 0 0 0-2 2h.5z"/>
        </svg>
        <input
          type="text"
          className="cf-path-input"
          value={editingPath}
          onChange={(e) => setEditingPath(e.target.value)}
          onFocus={() => setIsEditing(true)}
          onBlur={() => {
            setIsEditing(false);
            if (editingPath.trim() !== cwd) onCwdChange?.(editingPath.trim());
          }}
          onKeyDown={handleKeyDown}
          title="Current working path. Press Enter to change directory."
          spellCheck={false}
        />
      </div>
    </div>
  );
}
