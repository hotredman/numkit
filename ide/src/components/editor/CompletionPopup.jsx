import { FONT } from '../../theme';
import { BUILTIN_INFO } from './editorHighlight';

// Autocomplete dropdown — anchored at the start of the partial being
// completed (the list lines up with what the user is typing; position uses
// the `ch` unit for column math so it tracks the monospace font width
// without measurement). Pure presentational: the editor owns the item list
// + active index and passes accept / hover callbacks. Renders nothing when
// there is nothing to suggest.
export default function CompletionPopup({ items, anchor, activeIdx, onAccept, onHover, C }) {
  if (!items || items.length === 0) return null;
  return (
    <div style={{
      position: 'absolute',
      left: `calc(8px + ${anchor.col}ch)`,
      top: anchor.line * 20 + 28,    // below the line
      zIndex: 20,
      background: C.bg2,
      border: `1px solid ${C.border}`,
      borderRadius: 3,
      boxShadow: `0 4px 12px ${C.bg0}aa`,
      maxHeight: 240,
      minWidth: 140,
      overflowY: 'auto',
      fontFamily: FONT,
      fontSize: 12,
    }}>
      {items.map((item, i) => {
        const info = BUILTIN_INFO[item];
        // Extract the bit AFTER " — " when present, since the
        // function name itself is already shown on the left.
        const desc = info ? info.split(' — ').slice(1).join(' — ') : '';
        return (
          <div key={item}
               onMouseDown={(e) => { e.preventDefault(); onAccept(item); }}
               onMouseEnter={() => onHover(i)}
               style={{
                 display: 'flex', alignItems: 'baseline', gap: 8,
                 padding: '2px 8px',
                 cursor: 'pointer',
                 background: i === activeIdx ? C.accent : 'transparent',
                 color: i === activeIdx ? C.bg0 : C.text,
               }}>
            <span>{item}</span>
            {desc && (
              <span style={{
                fontSize: 11,
                color: i === activeIdx ? `${C.bg0}cc` : C.textMuted,
                whiteSpace: 'nowrap',
                overflow: 'hidden',
                textOverflow: 'ellipsis',
              }}>{desc}</span>
            )}
          </div>
        );
      })}
    </div>
  );
}
