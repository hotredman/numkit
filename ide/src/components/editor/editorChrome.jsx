import { FONT } from '../../theme';

// Presentational editor chrome — the line-number gutter and the minimap.
// Both are show-gated static markup whose live state (scroll position,
// canvas pixels) is driven imperatively by SyntaxEditor through the passed
// refs; these components only own the box + styling.

// Line-number gutter — fixed-width column, scroll-synced vertically with the
// editor (the parent writes gutterRef.current.scrollTop). Right-aligned,
// non-interactive. Renders nothing when disabled.
export function EditorGutter({ show, gutterRef, lineNumbers, C }) {
  if (!show) return null;
  return (
    <div style={{ position: 'relative', width: 48, flexShrink: 0,
                  overflow: 'hidden', borderRight: `1px solid ${C.border}`,
                  background: C.bg0 }}>
      <pre ref={gutterRef} aria-hidden="true" style={{
        position: 'absolute', top: 0, left: 0, right: 0, bottom: 0,
        margin: 0, padding: '8px 8px 0 0',
        fontFamily: FONT, fontSize: 13, lineHeight: '20px',
        color: C.textMuted, background: 'transparent',
        overflow: 'hidden', whiteSpace: 'pre',
        pointerEvents: 'none', textAlign: 'right',
        userSelect: 'none',
      }}>{lineNumbers}</pre>
    </div>
  );
}

// Minimap — canvas overview of the whole script; a click jumps the editor
// viewport. SyntaxEditor paints the canvas (sized to its CSS box) in an
// effect and owns the mouse-down → scroll handler. Renders nothing when
// disabled.
export function EditorMinimap({ show, minimapRef, onMouseDown, C }) {
  if (!show) return null;
  return (
    <div style={{ width: 64, flexShrink: 0, position: 'relative',
                  background: C.bg0,
                  borderLeft: `1px solid ${C.border}`,
                  cursor: 'pointer' }}
         onMouseDown={onMouseDown}>
      <canvas ref={minimapRef} style={{ display: 'block', width: '100%', height: '100%' }} />
    </div>
  );
}
