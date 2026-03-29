import CHEAT_SHEET from "../data/cheatsheet";
import { useTheme } from "../theme";

export default function Reference() {
  const C = useTheme();
  return (
    <div style={{ padding: 10, display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(200px, 1fr))", gap: 8, alignContent: "start", overflowY: "auto", flex: 1 }}>
      {CHEAT_SHEET.map(section => (
        <div key={section.title} style={{ background: C.bg2, borderRadius: 6, padding: 10, border: `1px solid ${C.border}` }}>
          <div style={{ fontSize: 10, fontWeight: 700, color: C.accent, marginBottom: 6, textTransform: "uppercase", letterSpacing: 0.8 }}>{section.title}</div>
          {section.items.map((item, i) => (
            <div key={i} style={{ display: "flex", justifyContent: "space-between", gap: 6, marginBottom: 3 }}>
              <code style={{ fontSize: 10, color: C.green, whiteSpace: "nowrap" }}>{item.code}</code>
              <span style={{ fontSize: 9, color: C.textMuted, textAlign: "right" }}>{item.desc}</span>
            </div>
          ))}
        </div>
      ))}
    </div>
  );
}
