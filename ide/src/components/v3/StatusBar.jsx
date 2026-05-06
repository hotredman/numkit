/**
 * Bottom status bar. All facts come from the parent — this is presentational.
 */
export default function StatusBar({
  engineStatus,
  activeTabName,
  activeTabSource,
  figureCount,
  execTimeMs,
  buildVersion,
  breakpointCount,
  debugLine,
}) {
  return (
    <div className="statusbar">
      <span className="status-pill">{engineStatus === 'ready' ? 'WASM' : 'Demo'}</span>
      {activeTabName && (
        <span className="status-item">
          <i className={`dot ${engineStatus === 'ready' ? 'dot-green' : 'dot-amber'}`} />
          {activeTabName}
        </span>
      )}
      {activeTabSource && (
        <span className="status-item">
          <i className="dot dot-amber" />
          {activeTabSource === 'localFolder' ? 'local folder' : 'temporary'}
        </span>
      )}
      {figureCount > 0 && (
        <span className="status-item">{figureCount} figure{figureCount > 1 ? 's' : ''}</span>
      )}
      {breakpointCount > 0 && (
        <span className="status-item" style={{ color: 'var(--danger)' }}>
          ● {breakpointCount} bp
        </span>
      )}
      {debugLine != null && (
        <span className="status-item" style={{ color: 'var(--warn)' }}>
          ⏸ line {debugLine}
        </span>
      )}
      <span className="status-spacer" />
      {execTimeMs != null && <span className="status-item">{execTimeMs.toFixed(1)}ms</span>}
      {buildVersion && (
        <>
          <span className="status-sep" />
          <span className="status-item" title="numkit-m engine build timestamp">
            build {buildVersion}
          </span>
        </>
      )}
      <span className="status-sep" />
      <span className="status-item">Ctrl+S: save</span>
      <span className="status-sep" />
      <span className="status-item">Tab: autocomplete</span>
      <span className="status-sep" />
      <span className="status-item">↑/↓: history</span>
    </div>
  );
}
