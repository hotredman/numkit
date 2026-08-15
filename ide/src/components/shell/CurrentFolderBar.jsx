import React, { useState, useEffect } from 'react';

/**
 * CurrentFolderBar — MATLAB-style Current Folder strip.
 *
 * Placed prominently across the top of the IDE workspace.
 * Features:
 *  - Mode Combo: switch between Local File System and Virtual File System (Temporary).
 *  - Up navigation button: jump to parent folder.
 *  - Interactive path input: view and enter working directory.
 *  - Browse / File Navigator button: opens the File Navigator modal.
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
      <div className="cf-label" title="Current Working Directory (MATLAB pwd)">
        <span className="cf-title">Current Folder:</span>
      </div>

      {/* Mode Selector (Combo) */}
      <div className="cf-mode-wrapper">
        <select
          className="cf-mode-select"
          value={fsMode}
          onChange={(e) => onFsModeChange?.(e.target.value)}
          title="Select Active Working Filesystem"
        >
          <option value="virtual">⚡ Virtual File System (Temporary)</option>
          <option value="local" disabled={!localAvailable}>
            📁 Local File System {localMountName ? `(${localMountName})` : ''}
          </option>
        </select>
      </div>

      <span className="cf-sep" />

      {/* Up Button */}
      <button
        className="cf-btn cf-btn-up"
        onClick={onNavigateUp}
        disabled={isAtRoot}
        title="Up One Level (..)"
      >
        <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor">
          <path fillRule="evenodd" d="M8 3.5a.5.5 0 0 1 .5.5v9a.5.5 0 0 1-1 0V4a.5.5 0 0 1 .5-.5z"/>
          <path fillRule="evenodd" d="M7.646 2.146a.5.5 0 0 1 .708 0l4 4a.5.5 0 0 1-.708.708L8 3.207 4.354 6.854a.5.5 0 1 1-.708-.708l4-4z"/>
        </svg>
      </button>

      {/* Path Display / Input */}
      <div className="cf-path-box">
        <span className="cf-path-icon">
          {fsMode === 'local' ? '📁' : '⚡'}
        </span>
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

      {/* Browse / Navigator Modal Button */}
      <button
        className="cf-btn cf-btn-browse"
        onClick={onOpenNavigator}
        title="Open File Navigator"
      >
        <svg width="13" height="13" viewBox="0 0 16 16" fill="currentColor" style={{ marginRight: '5px' }}>
          <path d="M.5 3l.04.87a1.99 1.99 0 0 0-.342 1.311l.637 7A2 2 0 0 0 2.826 14H9v-1H2.826a1 1 0 0 1-.995-.91l-.637-7A1 1 0 0 1 2.19 4h11.62a1 1 0 0 1 .996 1.09l-.637 7a1 1 0 0 1-.995.91H11v1h2.174a2 2 0 0 0 1.99-1.819l.637-7A1.99 1.99 0 0 0 15.46 3.87L15.5 3H9.414L7.707 1.293A1 1 0 0 0 7 1H2a2 2 0 0 0-2 2h.5z"/>
        </svg>
        Browse…
      </button>
    </div>
  );
}
