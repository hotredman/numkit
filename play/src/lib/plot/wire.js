// Pure wire parsing (rule R5): __FIGURE_DATA__/__FIGURE_CLOSE__
// markers → objects. Framework-free, unit-testable; the engine owns
// the wire format (portions 29–41), this mirrors the IDE's parser.
export function parseFigureWire(text) {
  const figures = [];
  const closed = [];
  for (const line of String(text ?? '').split('\n')) {
    const t = line.trim();
    if (t.startsWith('__FIGURE_DATA__:')) {
      try {
        figures.push(JSON.parse(t.slice('__FIGURE_DATA__:'.length)));
      } catch { /* skip malformed line */ }
    } else if (t.startsWith('__FIGURE_CLOSE__:')) {
      closed.push(parseInt(t.split(':')[1], 10));
    } else if (t === '__FIGURE_CLOSE_ALL__') {
      closed.push('all');
    }
  }
  return { figures, closed };
}

// Manifest JSON → flat control list (same shapes the sidebar renders).
export function flattenControls(manifestJson) {
  if (!manifestJson) return [];
  const m = typeof manifestJson === 'string'
    ? JSON.parse(manifestJson) : manifestJson;
  const out = [];
  for (const g of m.groups ?? []) {
    const controls = [];
    for (const c of g.controls ?? []) {
      controls.push(c.type === 'select'
        ? { kind: 'select', var: c.var, label: c.label ?? c.var,
            options: c.options ?? [],
            value: c.default ?? (c.options?.[0] ?? '') }
        : { kind: 'slider', var: c.var, label: c.label ?? c.var,
            min: c.min ?? 0, max: c.max ?? 100, step: c.step ?? 1,
            unit: c.unit ?? '', value: c.default ?? c.min ?? 0 });
    }
    if (controls.length || g.name)
      out.push({ name: g.name ?? '', controls });
  }
  return out;
}
