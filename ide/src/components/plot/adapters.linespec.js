// adapters.linespec.js — MATLAB line-spec ('r--o') + colour / palette parsing.
// Leaf module shared by the dataset-layer and axes adapters.
export const KIND_PALETTE = ['#7fd99a', '#5fb3d4', '#e9b870', '#9b8cf2', '#e26a6a', '#d4a5e6', '#f2a37e', '#6fcfbf'];

// MATLAB-style line spec parsing: 'r--o' → { color, lineStyle, marker }
//   color   — single char from STYLE_COLOR
//   lineStyle — '-' | '--' | ':' | '-.' (longest-match wins)
//   marker  — 'o' | '+' | '*' | '.' | 'x' | 's' | 'd' | '^' | 'v' | '<' | '>' | 'p' | 'h'
// Order in the spec is free (MATLAB tolerates "or", "ro-", "-or", etc.).
const STYLE_COLOR = { r: '#f07070', g: '#6ee7a0', b: '#60d0f0', k: '#d4d4f0', m: '#e070c0', c: '#60d0f0', y: '#e8d060', w: '#ffffff' };
const STYLE_MARKERS = new Set(['o', '+', '*', '.', 'x', 's', 'd', '^', 'v', '<', '>', 'p', 'h']);
export function parseLineSpec(s) {
  if (!s || typeof s !== 'string') return {};
  // Two style dialects share this slot:
  //   • Classic MATLAB linespec ("r--o", "b:") — single-char color.
  //   • Engine extras ("color=#rrggbb;lineWidth=2") — explicit kv list.
  // Parse as kv first (it's unambiguous because of the '=' sign), then
  // fall back to a left-to-right longest-match tokeniser.
  const out = {};
  if (s.includes('=')) {
    for (const kv of s.split(';')) {
      const [k, v] = kv.split('=');
      if (!k || v == null) continue;
      const key = k.trim(), val = v.trim();
      if (key === 'color') out.color = val;
      else if (key === 'lineWidth' || key === 'linewidth') out.lineWidth = Number(val);
      else if (key === 'fontSize' || key === 'fontsize') out.fontSize = Number(val);
      else if (key === 'fillOpacity' || key === 'fillopacity') out.fillOpacity = Number(val);
      else if (key === 'smoothNormals' || key === 'smoothnormals') out.smoothNormals = (val === '1' || val === 'true');
      else if (key === 'cometAnim' || key === 'cometanim') out.cometAnim = (val === '1' || val === 'true');
      else if (key === 'filled') out.filled = (val === '1' || val === 'true' || val === '');
    }
    return out;
  }
  // Left-to-right scan. Try line-style longest-match first (so '--'
  // beats '-' and '-.' beats '-'/'.'); then color; then marker.
  let i = 0;
  while (i < s.length) {
    const c2 = s.substr(i, 2);
    if (!out.lineStyle && (c2 === '--' || c2 === '-.')) {
      out.lineStyle = c2; i += 2; continue;
    }
    const c = s[i];
    if (!out.lineStyle && (c === '-' || c === ':')) {
      out.lineStyle = c; i += 1; continue;
    }
    if (!out.color && STYLE_COLOR[c]) {
      out.color = STYLE_COLOR[c]; i += 1; continue;
    }
    if (!out.marker && STYLE_MARKERS.has(c)) {
      // '.' is ambiguous (line-style '-.' vs marker '.'). The
      // longest-match for '-.' above already eats the line-style
      // form, so a bare '.' here is unambiguously a marker.
      out.marker = c; i += 1; continue;
    }
    // Unknown char — skip silently (MATLAB also ignores stray chars).
    i += 1;
  }
  return out;
}
