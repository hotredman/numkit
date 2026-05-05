import { useTheme, FONT } from "../theme";

export default function Workspace({ variables }) {
  const C = useTheme();
  const entries = Object.entries(variables);

  // Variables can be either structured ({ type, size, preview }) from WASM getVars(),
  // or plain JS values from fallback engine. Detect and handle both.
  const isStructured = v => v && typeof v === 'object' && !Array.isArray(v) && 'type' in v;

  const getType = v => {
    if (isStructured(v)) return v.type || 'double';
    if (Array.isArray(v)) return "double";
    if (typeof v === "string") return "char";
    if (typeof v === "object" && v !== null) return "struct";
    return "double";
  };

  const getSize = v => {
    if (isStructured(v)) return v.size || '1x1';
    if (Array.isArray(v)) {
      if (v.length && Array.isArray(v[0])) return `${v.length}x${v[0].length}`;
      return `1x${v.length}`;
    }
    if (typeof v === "string") return `1x${v.length}`;
    return "1x1";
  };

  const getPreview = v => {
    if (isStructured(v)) {
      const p = v.preview;
      if (p === null || p === undefined) return `[${v.size} ${v.type}]`;
      if (typeof p === 'number') {
        return Number.isInteger(p) ? String(p) : p.toFixed(6);
      }
      if (Array.isArray(p)) {
        const flat = p.flat();
        if (flat.length <= 6) return `[${flat.map(x => typeof x === 'number' ? (Number.isInteger(x) ? x : x.toFixed(3)) : x).join(', ')}]`;
        return `[${flat.slice(0, 4).map(x => typeof x === 'number' ? (Number.isInteger(x) ? x : x.toFixed(3)) : x).join(', ')}, ...]`;
      }
      if (typeof p === 'boolean') return p ? 'true' : 'false';
      return String(p);
    }
    // Fallback engine (plain values)
    if (Array.isArray(v)) {
      const f = v.flat();
      if (f.length <= 6) return `[${f.map(x => typeof x === "number" ? (Number.isInteger(x) ? x : x.toFixed(3)) : x).join(", ")}]`;
      return `[${f.slice(0, 4).map(x => typeof x === "number" ? (Number.isInteger(x) ? x : x.toFixed(3)) : x).join(", ")}, ...]`;
    }
    if (typeof v === "string") return `'${v}'`;
    if (typeof v === "object" && v !== null) return `{${Object.keys(v).join(", ")}}`;
    if (typeof v === "number") return Number.isInteger(v) ? String(v) : v.toFixed(6);
    return String(v);
  };

  const typeColors = {
    double: C.cyan, complex: C.purple || C.accent, logical: C.green,
    char: C.yellow, string: C.yellow, cell: C.orange,
    struct: C.orange, func_handle: C.textDim,
  };

  return (
    <div style={{ flex: 1, overflowY: "auto", padding: 8 }}>
      {!entries.length ? (
        <div style={{ color: C.textMuted, fontSize: 11, padding: 16, textAlign: "center", lineHeight: 1.6 }}>
          No variables in workspace.<br />Run some code to see variables here.
        </div>
      ) : (
        <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(180px, 1fr))", gap: 4 }}>
          {entries.map(([name, val]) => {
            const type = getType(val);
            const size = getSize(val);
            return (
              <div key={name} style={{ padding: "6px 8px", borderRadius: 5, background: C.bg2, border: `1px solid ${C.border}`, overflow: "hidden" }}>
                <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 2 }}>
                  <span style={{ fontSize: 11, fontWeight: 600, color: C.text, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>{name}</span>
                  <div style={{ display: "flex", gap: 4, alignItems: "center", flexShrink: 0 }}>
                    <span style={{ fontSize: 8, color: C.textMuted }}>{size}</span>
                    <span style={{ fontSize: 8, padding: "0 4px", borderRadius: 3, background: `${typeColors[type] || C.textDim}22`, color: typeColors[type] || C.textDim }}>{type}</span>
                  </div>
                </div>
                <div style={{ fontSize: 9, color: C.textDim, fontFamily: FONT, whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }}>{getPreview(val)}</div>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
}
