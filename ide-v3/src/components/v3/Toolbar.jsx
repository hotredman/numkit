/**
 * Top toolbar. Brand mark left, panel pills center, action buttons right.
 *
 * Props are deliberately fine-grained so the parent can wire each button to
 * the existing engine + tab state without re-implementing UI plumbing.
 */
export default function Toolbar({
  panels,
  togglePanel,
  theme,
  onToggleTheme,
  onRun,
  onDebug,
  onStop,
  onSave,
  onClear,
  onReset,
  isDebugging,
  canRun,
}) {
  return (
    <div className="toolbar">
      <div className="brand">
        <span className="brand-mark" aria-label="numkit logo">
          <span className="brand-bracket">[</span>
          <span className="brand-n">n</span>
          <span className="brand-k">k</span>
          <span className="brand-bracket">]</span>
        </span>
        <span className="brand-name">numkit</span>
      </div>

      <div className="toolbar-center">
        <button className={`tool-pill ${panels.explorer ? 'is-active' : ''}`}
          onClick={() => togglePanel('explorer')}>
          <span className="dot dot-blue" />Explorer
        </button>
        <button className={`tool-pill ${panels.editor ? 'is-active' : ''}`}
          onClick={() => togglePanel('editor')}>
          <span className="dot dot-green" />Editor
        </button>
        <button className={`tool-pill ${panels.figures ? 'is-active' : ''}`}
          onClick={() => togglePanel('figures')}>
          <span className="dot dot-amber" />Figures
        </button>
        <button className={`tool-pill ${panels.terminal ? 'is-active' : ''}`}
          onClick={() => togglePanel('terminal')}>
          <span className="dot dot-violet" />Terminal
        </button>
      </div>

      <div className="toolbar-right">
        {!isDebugging && (
          <button className="tool-action tool-run" onClick={onRun} disabled={!canRun}>
            <svg width="10" height="10" viewBox="0 0 10 10"><path d="M2 1.5 L8.5 5 L2 8.5 Z" fill="currentColor"/></svg>
            Run
          </button>
        )}
        {!isDebugging ? (
          <button className="tool-action" onClick={onDebug} disabled={!canRun}>
            <svg width="11" height="11" viewBox="0 0 12 12">
              <circle cx="6" cy="6" r="4" stroke="currentColor" fill="none"/>
              <circle cx="6" cy="6" r="1.5" fill="currentColor"/>
            </svg>
            Debug
          </button>
        ) : (
          <button className="tool-action" onClick={onStop} style={{ color: 'var(--danger)' }}>
            <svg width="11" height="11" viewBox="0 0 12 12">
              <rect x="2" y="2" width="8" height="8" fill="currentColor"/>
            </svg>
            Stop
          </button>
        )}
        <button className="tool-action" onClick={onSave}>
          <svg width="11" height="11" viewBox="0 0 12 12">
            <path d="M2 2h6l2 2v6H2z M4 2v3h4V2 M4 8h4v2H4z" stroke="currentColor" fill="none"/>
          </svg>
          Save
        </button>
        <span className="tool-sep" />
        <button className="tool-action" onClick={onClear}>
          <svg width="11" height="11" viewBox="0 0 12 12">
            <path d="M2 6h8 M5 3v6 M7 3v6" stroke="currentColor" fill="none"/>
          </svg>
          Clear
        </button>
        <button className="tool-action" onClick={onReset}>
          <svg width="11" height="11" viewBox="0 0 12 12">
            <path d="M3 6a3 3 0 1 1 1 2.2L2 10 M2 6V3l1 1" stroke="currentColor" fill="none" strokeLinecap="round"/>
          </svg>
          Reset
        </button>
        <button className="tool-action" onClick={onToggleTheme}
          title={`Switch to ${theme === 'light' ? 'dark' : 'light'} theme`}>
          {theme === 'light' ? (
            <svg width="11" height="11" viewBox="0 0 12 12">
              <path d="M9.5 7.5A4 4 0 0 1 4.5 2.5 4 4 0 1 0 9.5 7.5z" stroke="currentColor" fill="none" strokeLinejoin="round"/>
            </svg>
          ) : (
            <svg width="11" height="11" viewBox="0 0 12 12">
              <circle cx="6" cy="6" r="2.5" stroke="currentColor" fill="none"/>
              <path d="M6 1v1.5 M6 9.5V11 M1 6h1.5 M9.5 6H11 M2.5 2.5l1 1 M8.5 8.5l1 1 M2.5 9.5l1-1 M8.5 3.5l1-1" stroke="currentColor" strokeLinecap="round"/>
            </svg>
          )}
          {theme === 'light' ? 'Dark' : 'Light'}
        </button>
      </div>
    </div>
  );
}
