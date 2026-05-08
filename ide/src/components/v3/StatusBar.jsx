/**
 * Bottom status bar. All facts come from the parent — this is presentational
 * with one exception: the live JS-heap probe runs locally on a 2-s timer so
 * a single instance polls performance.memory and the parent doesn't have to
 * thread it through props.
 */
import { useEffect, useState } from 'react';

// Format bytes as "<N> MB" / "<N>.<M> GB" with one decimal for big values.
function fmtBytes(n) {
  if (!Number.isFinite(n) || n <= 0) return '?';
  if (n < 1024 * 1024) return Math.round(n / 1024) + ' KB';
  if (n < 1024 * 1024 * 1024) return Math.round(n / (1024 * 1024)) + ' MB';
  return (n / (1024 * 1024 * 1024)).toFixed(1) + ' GB';
}

function HeapBadge({ outputCount }) {
  // Chromium-only API: performance.memory exposes JS heap stats. Other
  // browsers return undefined → we just don't render the badge there.
  const supported = typeof performance !== 'undefined'
                  && typeof performance.memory === 'object';
  const [stats, setStats] = useState(() => supported
    ? { used: performance.memory.usedJSHeapSize,
        limit: performance.memory.jsHeapSizeLimit }
    : null);

  useEffect(() => {
    if (!supported) return;
    const tick = () => setStats({
      used: performance.memory.usedJSHeapSize,
      limit: performance.memory.jsHeapSizeLimit,
    });
    const id = setInterval(tick, 2000);
    return () => clearInterval(id);
  }, [supported]);

  if (!supported || !stats) return null;
  const pct = stats.limit > 0 ? (stats.used / stats.limit) * 100 : 0;
  // Pre-OOM warning at 75 %, danger at 90 %. The renderer typically dies
  // around 95-99 % when V8 can't satisfy a fresh allocation.
  const color = pct > 90 ? 'var(--danger)' : pct > 75 ? 'var(--warn)' : null;
  const title = `JS heap: ${fmtBytes(stats.used)} / ${fmtBytes(stats.limit)}`
              + ` (${pct.toFixed(1)} %)\nConsole output lines: ${outputCount}`
              + '\nIf this approaches 90 %, run `clc` and `close all` to free memory';
  return (
    <span className="status-item" style={color ? { color, fontWeight: 600 } : null}
      title={title}>
      heap {fmtBytes(stats.used)} ({pct.toFixed(0)} %)
    </span>
  );
}

export default function StatusBar({
  engineStatus,
  activeTabName,
  activeTabSource,
  figureCount,
  outputCount = 0,
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
      <HeapBadge outputCount={outputCount} />
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
