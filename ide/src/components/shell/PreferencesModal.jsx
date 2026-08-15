/**
 * PreferencesModal.jsx — IDE settings modal.
 *
 * Visual language mirrors FigureWindow (fw-overlay + fw-window +
 * fw-titlebar + ve-btn / ve-close) so it fits the existing design
 * system without new primitives.
 *
 * Props:
 *   onClose()  — called when the modal should be dismissed (Cancel /
 *                overlay click / ×).  The caller unmounts the component.
 */
import { useState, useEffect, useCallback } from 'react';
import { loadSettings, saveSettings, DEFAULT_SETTINGS } from '../../settings';
import ModalWindow from '../ui/ModalWindow';

/** True when running inside Electron (nativeFS IPC bridge present). */
const isElectron = typeof window !== 'undefined' && typeof window.nativeFS !== 'undefined';

export default function PreferencesModal({ onClose }) {
  const [activeTab, setActiveTab] = useState('general');
  // Local draft — only written to storage on explicit [Save].
  const [draft, setDraft] = useState(() => loadSettings());

  // Resolved paths from main.js (what will actually be used at spawn time).
  // Populated asynchronously via IPC; null means "still loading".
  const [resolved, setResolved] = useState(null);

  // Load resolved paths from main.js on mount & auto-fill empty draft fields.
  useEffect(() => {
    if (!isElectron || typeof window.nativeFS.resolveSettings !== 'function') return;
    window.nativeFS.resolveSettings().then((res) => {
      setResolved(res);
      if (res) {
        setDraft((prev) => {
          const next = { ...prev };
          let changed = false;
          if ((!next.interpreterPath || !/[\\/]/.test(next.interpreterPath)) && res.interpreterPath) {
            next.interpreterPath = res.interpreterPath;
            changed = true;
          }
          if ((!next.codegenPath || !/[\\/]/.test(next.codegenPath)) && res.codegenPath) {
            next.codegenPath = res.codegenPath;
            changed = true;
          }
          return changed ? next : prev;
        });
      }
    }).catch(() => {});
  }, []);

  // Close on Escape
  useEffect(() => {
    const handler = (e) => { if (e.key === 'Escape') onClose(); };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [onClose]);

  const set = useCallback((key, value) => {
    setDraft((prev) => ({ ...prev, [key]: value }));
  }, []);

  const handlePickFile = useCallback(async (key, title) => {
    if (!isElectron) return;
    const picked = await window.nativeFS.pickFile({ title });
    if (picked) set(key, picked);
  }, [set]);

  const handleSave = useCallback(() => {
    saveSettings(draft);
    // Sync to Electron main process so it uses the new paths immediately.
    if (isElectron && typeof window.nativeFS.updateSettings === 'function') {
      window.nativeFS.updateSettings(draft);
    }
    window.dispatchEvent(new CustomEvent('settings-updated', { detail: draft }));
    onClose();
  }, [draft, onClose]);

  const handleRestoreDefaults = useCallback(() => {
    setDraft({ ...DEFAULT_SETTINGS });
  }, []);

  function ToggleRow({ label, settingKey, hint }) {
    const checked = !!draft[settingKey];
    return (
      <div className="prefs-row">
        <div className="prefs-row-left">
          <div className="prefs-label">{label}</div>
          {hint && <div className="prefs-hint">{hint}</div>}
        </div>
        <div className="prefs-row-right">
          <input
            type="checkbox"
            checked={checked}
            onChange={(e) => set(settingKey, e.target.checked)}
            style={{ width: '18px', height: '18px', cursor: 'pointer' }}
          />
        </div>
      </div>
    );
  }

  // ── row helper ──────────────────────────────────────────────────────
  function PathRow({ label, settingKey, hint, pickTitle }) {
    const storedValue = draft[settingKey];
    const resolvedValue = resolved?.[settingKey];

    // Determine status icon:
    //  • green check  — field is set explicitly by the user
    //  • blue dot     — field is empty but auto-detected next to IDE
    //  • grey dot     — field is empty, will use bare PATH lookup
    let statusIcon = null;
    if (resolved) {
      if (storedValue) {
        statusIcon = <span className="prefs-status prefs-status-explicit" title="Configured path">✓</span>;
      } else if (resolvedValue && /[\\/]/.test(resolvedValue)) {
        // resolvedValue contains a directory separator → found next to IDE
        statusIcon = <span className="prefs-status prefs-status-auto" title="Auto-detected path">⊙</span>;
      } else {
        statusIcon = <span className="prefs-status prefs-status-path" title="Will look up in PATH">~</span>;
      }
    }

    return (
      <div className="prefs-row">
        <div className="prefs-row-header">
          <label className="prefs-row-label" htmlFor={`prefs-${settingKey}`}>{label}</label>
          {statusIcon}
        </div>
        <div className="prefs-input-row">
          <input
            id={`prefs-${settingKey}`}
            className="prefs-input"
            type="text"
            value={storedValue || ''}
            placeholder="(empty = auto-detect or PATH)"
            onChange={(e) => set(settingKey, e.target.value)}
            spellCheck={false}
            autoComplete="off"
          />
          {isElectron && (
            <button
              className="ve-btn prefs-pick-btn"
              onClick={() => handlePickFile(settingKey, pickTitle)}
              title={pickTitle}
            >
              Pick…
            </button>
          )}
        </div>
        <span className="prefs-hint">{hint}</span>
      </div>
    );
  }

  return (
    <ModalWindow
      onClose={onClose}
      tag={{
        label: '⚙ settings',
        color: 'var(--accent-2)',
        bg: 'rgba(116,185,255,0.10)',
        border: 'rgba(116,185,255,0.25)',
      }}
      title="Settings"
      width="min(880px, 92vw)"
      height="min(640px, 85vh)"
      className="prefs-window"
      footer={(
        <div className="prefs-footer" style={{ width: '100%' }}>
          <button
            className="ve-btn prefs-restore-btn"
            onClick={handleRestoreDefaults}
            title="Reset all paths to defaults (empty = auto-detect)"
          >
            Restore defaults
          </button>
          <div className="prefs-footer-spacer" />
          <button className="ve-btn" onClick={onClose}>Cancel</button>
          <button
            className="ve-btn prefs-save-btn"
            onClick={handleSave}
          >
            Save
          </button>
        </div>
      )}
    >
      {/* ── main layout (sidebar + pane) ── */}
      <div className="prefs-layout">
        {/* Left Sidebar */}
        <div className="prefs-sidebar">
          <button
            className={`prefs-sidebar-nav ${activeTab === 'general' ? 'is-active' : ''}`}
            onClick={() => setActiveTab('general')}
          >
            ⚙ General
          </button>
          <button
            className={`prefs-sidebar-nav ${activeTab === 'editor' ? 'is-active' : ''}`}
            onClick={() => setActiveTab('editor')}
          >
            📝 Editor
          </button>
          <button
            className={`prefs-sidebar-nav ${activeTab === 'terminal' ? 'is-active' : ''}`}
            onClick={() => setActiveTab('terminal')}
          >
            🖥️ Terminal
          </button>
          <button
            className={`prefs-sidebar-nav ${activeTab === 'appearance' ? 'is-active' : ''}`}
            onClick={() => setActiveTab('appearance')}
          >
            🎨 Appearance
          </button>
        </div>

        {/* Right Main Content Pane */}
        <div className="prefs-main-pane">
          {activeTab === 'general' && (
            <div className="prefs-body">
              <div>
                <div className="prefs-section-title">External Tools & Executables</div>
                <div className="prefs-section-divider" />

                <PathRow
                  label="Interpreter — numkit_repl.exe"
                  settingKey="interpreterPath"
                  hint="Auto-detects numkit_repl.exe in directory of numkit_ide.exe, then falls back to PATH"
                  pickTitle="Select numkit_repl executable"
                />
                <PathRow
                  label="Code Generator — numkit_codegen.exe"
                  settingKey="codegenPath"
                  hint="Auto-detects numkit_codegen.exe in directory of numkit_ide.exe, then falls back to PATH. Used by Build & Run."
                  pickTitle="Select numkit_codegen executable"
                />
                <PathRow
                  label="C++ Compiler — NUMKIT_CXX override"
                  settingKey="cxxPath"
                  hint="Overrides the NUMKIT_CXX environment variable when running Build & Run. Leave empty to use the env var or build-time default."
                  pickTitle="Select C++ compiler executable"
                />
                <ToggleRow
                  label="MATLAB Compatibility Mode (Implicit compat.*)"
                  settingKey="matlabCompatibility"
                  hint="Automatically imports compat.* so you don't have to write import compat.* in every script. Survives 'clear all'."
                />
              </div>

              {isElectron && !resolved && (
                <div className="prefs-hint" style={{ fontStyle: 'italic' }}>
                  Resolving paths…
                </div>
              )}

              {!isElectron && (
                <div className="prefs-hint prefs-browser-note">
                  ⚠ Running in browser mode — [Pick] file dialogs require the Electron desktop app.
                  You can still type paths manually.
                </div>
              )}
            </div>
          )}

          {activeTab === 'editor' && (
            <div className="prefs-body">
              <div className="prefs-section-title">Editor Settings</div>
              <div className="prefs-section-divider" />
              <div className="prefs-hint">
                Font size, tab width, indentation, and linting options will be configured here.
              </div>
            </div>
          )}

          {activeTab === 'terminal' && (
            <div className="prefs-body">
              <div className="prefs-section-title">Terminal & REPL Settings</div>
              <div className="prefs-section-divider" />
              <div className="prefs-hint">
                REPL buffer size, history persistence, and shell environment options will be configured here.
              </div>
            </div>
          )}

          {activeTab === 'appearance' && (
            <div className="prefs-body">
              <div className="prefs-section-title">Appearance & Theme</div>
              <div className="prefs-section-divider" />
              <div className="prefs-hint">
                Theme selection (Dark/Light/High-Contrast) and UI scaling options will be configured here.
              </div>
              <div style={{ marginTop: '20px' }}>
                <div className="prefs-section-title">Plot Settings</div>
                <div className="prefs-section-divider" />
                <div className="prefs-row">
                  <div className="prefs-row-header">
                    <label className="prefs-row-label">Plot Aspect Ratio</label>
                  </div>
                  <div className="prefs-input-row">
                    <select
                      className="prefs-input"
                      value={draft.plotAspectRatio || '16:9'}
                      onChange={(e) => set('plotAspectRatio', e.target.value)}
                      style={{ cursor: 'pointer', fontFamily: 'var(--font-mono)' }}
                    >
                      <option value="4:3">4:3</option>
                      <option value="16:9">16:9</option>
                      <option value="16:10">16:10</option>
                    </select>
                  </div>
                  <span className="prefs-hint">
                    Base aspect ratio for inline plots and the Figures panel previews.
                  </span>
                </div>
              </div>
            </div>
          )}
        </div>
      </div>
    </ModalWindow>
  );
}
