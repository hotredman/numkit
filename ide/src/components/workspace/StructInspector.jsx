// StructInspector.jsx — MATLAB-style drill-in inspector for struct / cell /
// struct-array variables over engine.inspectPath: breadcrumb nav + field
// list / element table / cell grid, with matrix fields reusing MatrixPanel.
import { useState, useMemo, useEffect, useCallback } from 'react';
import { pathToMatlabLValue, isValidIdentifier } from './inspectorOps';
import ContextMenu from '../ui/ContextMenu';
import { aggregateStats, loadVisibleColumns, saveVisibleColumns } from './valueColumns';
import { useChooser } from './chooser';
import { classify } from '../plot/adapters';
import { MatrixPanel } from './MatrixPanel';
import { EntityBrowser } from './EntityBrowser';

/* ======================================================================== */
/* Struct / cell inspector — MATLAB-style drill-in                          */
/* ======================================================================== */
// Navigable inspector over engine.inspectPath(name, pathStr). The engine
// resolves a ';'-delimited typed path ('' = root) and returns one of:
//   { kind:'struct', rows, cols, numel, fields:[], elems:[[cell]] }
//   { kind:'cell',   rows, cols, elems:[cell] }   (column-major)
//   { kind:'matrix', type, rows, cols, data | truncated }
// where cell = { type, size, summary, drill, label? }.
//
// A single struct (numel 1) renders as a field list; a struct array as an
// element×field table; a cell as an R×C grid; a matrix reuses VirtualTable.
// Double-clicking a `drill` cell pushes a path step; the breadcrumb pops
// back. Mounted with key={variable.name} so a variable swap resets nav.

function InspectorBreadcrumb({ nav, onJump }) {
  return (
    <div className="ve-crumbs">
      {nav.map((c, i) => (
        <span key={i} className="ve-crumb-seg">
          {i > 0 && <span className="ve-crumb-sep">›</span>}
          <button className="ve-crumb" disabled={i === nav.length - 1}
            onClick={() => onJump(i)}>{c.label}</button>
        </span>
      ))}
    </div>
  );
}

// A field name that becomes an inline text input on double-click, for
// renaming. Stops click/double-click propagation so it doesn't trigger
// the row's drill. Enter commits, Esc cancels.
function EditableFieldName({ name, onRename, className, editing: cEditing, setEditing: cSetEditing }) {
  // Editing is optionally controlled: when the parent passes editing /
  // setEditing (so a context-menu "Rename" can start it), use those;
  // otherwise self-manage on double-click (struct-array header usage).
  const [iEditing, iSetEditing] = useState(false);
  const editing = cEditing !== undefined ? cEditing : iEditing;
  const setEditing = cSetEditing || iSetEditing;
  const [val, setVal] = useState(name);
  // Reset the draft whenever edit mode opens (covers the menu-triggered
  // path, which can't pre-seed val like the double-click handler did).
  useEffect(() => { if (editing) setVal(name); }, [editing, name]);
  if (!editing) {
    return (
      <span className={className}
        onDoubleClick={(e) => { e.stopPropagation(); setEditing(true); }}
        title="double-click to rename">{name}</span>
    );
  }
  const commit = () => { setEditing(false); if (val !== name) onRename(name, val); };
  return (
    <input className="ve-rename-input" autoFocus value={val}
      onClick={(e) => e.stopPropagation()}
      onChange={(e) => setVal(e.target.value)}
      onBlur={commit}
      onKeyDown={(e) => {
        if (e.key === 'Enter') { e.preventDefault(); commit(); }
        else if (e.key === 'Escape') { setEditing(false); }
      }} />
  );
}

// Inline "+ new field" control — validates the name as a MATLAB
// identifier and only enables Add when valid. Shared by the single-
// struct list and the struct-array table.
function AddFieldRow({ onAdd }) {
  const [name, setName] = useState('');
  const valid = isValidIdentifier(name);
  const submit = () => { if (valid) { onAdd(name); setName(''); } };
  return (
    <div className="ve-addfield">
      <input className="ve-addfield-input" placeholder="+ new field"
        value={name}
        onChange={(e) => setName(e.target.value)}
        onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); submit(); } }} />
      <button className="ve-addfield-btn" disabled={!valid} onClick={submit}>add</button>
    </div>
  );
}

// Single struct (numel 1): vertical field list. Click a drillable field
// to descend; × deletes a field; the add-row appends a new one.
function StructFieldList({ payload, onDrill, onAddField, onDeleteField,
                          onRenameField, onDuplicateField, onInsertField }) {
  const cells = payload.elems[0] || [];
  const [menu, setMenu] = useState(null);          // { x, y, name, drill }
  const [renaming, setRenaming] = useState(null);  // field name in rename mode
  // Columns shared with the Workspace (same key); view/sort are per the
  // struct context so it can differ from the Workspace's layout.
  const [cols, setCols] = useChooser('numkit.ide.valuecols', loadVisibleColumns, saveVisibleColumns);
  const open = (name) => onDrill([{ k: 'f', name, label: `.${name}` }]);
  const rows = payload.fields.map((name, f) => {
    const cell = cells[f] || {};
    return {
      key: name, name, value: cell.summary, size: cell.size,
      klass: cell.type, kind: classify(cell.size, cell.type),
      bytes: cell.bytes, stats: cell.stats || null, drill: !!cell.drill,
    };
  });
  const nameCell = (row) => (
    <EditableFieldName name={row.name} onRename={onRenameField}
      editing={renaming === row.name}
      setEditing={(v) => setRenaming(v ? row.name : null)} />
  );
  return (
    <>
      <EntityBrowser
        rows={rows}
        nameHeader="Field"
        countNoun="field"
        defaultView="list"
        viewKey="numkit.ide.struct.view" sortKey="numkit.ide.struct.sort"
        cols={cols} setCols={setCols}
        nameCell={nameCell}
        onOpen={(row) => open(row.name)}
        onRowContextMenu={(row, e) =>
          setMenu({ x: e.clientX, y: e.clientY, name: row.name, drill: row.drill })}
        onAreaContextMenu={(e) => {
          // Right-click anywhere in the table area (not on a row — those
          // stop propagation): the field-agnostic menu (Insert field).
          e.preventDefault();
          setMenu({ x: e.clientX, y: e.clientY, name: null, drill: false });
        }}
      />
      {menu && (
        <ContextMenu x={menu.x} y={menu.y} onClose={() => setMenu(null)} items={
          menu.name
            ? [
              { label: 'Open',         disabled: !menu.drill, onClick: () => open(menu.name) },
              { label: 'Rename',       onClick: () => setRenaming(menu.name) },
              { label: 'Duplicate',    onClick: () => onDuplicateField(menu.name) },
              { label: 'Insert field', onClick: () => onInsertField() },
              { separator: true },
              { label: 'Delete',       onClick: () => onDeleteField(menu.name) },
            ]
            : [
              // Empty table area: no field target → just add a new one.
              { label: 'Insert field', onClick: () => onInsertField() },
            ]
        } />
      )}
    </>
  );
}

// Struct array: rows = elements (1),(2),…; cols = fields. Click a
// drillable cell to open s(e).field; × on a column header deletes that
// field across all elements; the add-row appends a field column.
function StructArrayTable({ payload, onDrill, onAddField, onDeleteField, onRenameField }) {
  return (
    <div className="ve-arr-wrap">
      <table className="ve-arr-table">
        <thead>
          <tr>
            <th className="ve-arr-corner">{payload.rows}×{payload.cols}</th>
            {payload.fields.map((f) => (
              <th key={f}>
                <EditableFieldName name={f} onRename={onRenameField} />
                <button className="ve-arr-delcol" title="delete field"
                  onClick={() => onDeleteField(f)}>×</button>
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {payload.elems.map((row, e) => (
            <tr key={e}>
              <th className="ve-arr-rowhead">({e + 1})</th>
              {row.map((cell, f) => (
                <td key={f}
                  className={cell.drill ? 'is-drillable' : ''}
                  title={cell.summary}
                  onClick={cell.drill
                    ? () => onDrill([
                        { k: 'e', idx: e },
                        { k: 'f', name: payload.fields[f], label: `(${e + 1}).${payload.fields[f]}` },
                      ]) : undefined}>
                  {cell.summary}
                </td>
              ))}
            </tr>
          ))}
        </tbody>
      </table>
      <AddFieldRow onAdd={onAddField} />
    </div>
  );
}

// Cell array: R×C grid of element previews (column-major linear order).
function CellGrid({ payload, onDrill }) {
  const { rows, cols, elems } = payload;
  return (
    <div className="ve-arr-wrap">
      <table className="ve-arr-table">
        <tbody>
          {Array.from({ length: rows }, (_, r) => (
            <tr key={r}>
              <th className="ve-arr-rowhead">{r + 1}</th>
              {Array.from({ length: cols }, (_, c) => {
                const i = c * rows + r;             // column-major
                const cell = elems[i] || {};
                return (
                  <td key={c}
                    className={cell.drill ? 'is-drillable' : ''}
                    title={cell.summary}
                    onClick={cell.drill
                      ? () => onDrill([{ k: 'c', idx: i, label: cell.label || `{${r + 1},${c + 1}}` }])
                      : undefined}>
                    {cell.summary}
                  </td>
                );
              })}
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

// Drilled-into matrix — render through the shared MatrixPanel so a
// struct/cell matrix field gets the same polished toolbar (notation /
// precision / heatmap / inline-plot / stats / copy-csv) as a top-level
// matrix variable. Data comes inline from the inspect payload.
//   name    — breadcrumb leaf label, shown in the address bar
//   onBack  — Esc / back to the parent path
//   onCommit(r,c,rhs) — write-back (Phase B); absent → read-only
function DrilledMatrix({ payload, name, onBack, onCommit }) {
  const data = payload.data || [];
  // Full stat set (min/max/mean/median/mode/var/std/n) over the inline
  // data, for the StatsBar — mirrors the engine's getVarStatsJSON.
  const stats = useMemo(() => aggregateStats(data.flat()), [data]);
  if (payload.truncated) {
    return (
      <div className="ve-struct-empty">
        Field too large to display inline ({payload.rows}×{payload.cols}).
      </div>
    );
  }
  const getCellValue = (r, c) => data[r]?.[c];
  const getSlice = (axis, idx) =>
    axis === 'col' ? data.map((row) => row[idx]) : (data[idx] || []).slice();
  return (
    <MatrixPanel
      rows={payload.rows} cols={payload.cols} name={name} type={payload.type}
      getCellValue={getCellValue} getSlice={getSlice} stats={stats}
      readOnly={!onCommit} onCommit={onCommit} onEscape={onBack} onSave={null}
    />
  );
}

export function StructInspector({ variable, engine }) {
  const [nav, setNav] = useState([{ label: variable.name, path: '' }]);
  const [payload, setPayload] = useState(null);
  const [error, setError] = useState(null);
  const [refreshTick, setRefreshTick] = useState(0);  // bump → refetch after a write
  const cur = nav[nav.length - 1];

  useEffect(() => {
    setError(null);
    if (!engine || typeof engine.inspectPath !== 'function') {
      setError('struct inspection needs a full WASM rebuild (binding missing)');
      setPayload(null);
      return;
    }
    const p = engine.inspectPath(variable.name, cur.path);
    if (p === null) {
      setError('struct inspection needs a full WASM rebuild (binding missing)');
      setPayload(null);
    } else if (p.error) {
      setError(p.error);
      setPayload(null);
    } else {
      setPayload(p);
    }
  }, [variable.name, engine, cur.path, refreshTick]);

  // Write-back for an edited matrix-field cell. Builds a MATLAB lvalue
  // from the current path (e.g. `car.engine.data(2,5)`) and runs the
  // same engine.execute assignment as the top-level editor — no new
  // engine API. Refetch the path afterwards to show the new value.
  const commitFieldCell = useCallback((r, c, rhs) => {
    if (!engine || typeof engine.execute !== 'function') return;
    const lvalue = pathToMatlabLValue(variable.name, cur.path);
    try {
      engine.execute(`${lvalue}(${r + 1},${c + 1}) = ${rhs};`);
    } catch (e) {
      console.warn('[StructInspector] field write-back failed:', e);
    }
    setRefreshTick((t) => t + 1);
  }, [engine, variable.name, cur.path]);

  // Add a field to the struct at the current path. For a struct array,
  // assign via element 1 — MATLAB then adds the field (empty) to every
  // element. New fields default to [] so the user can drill in and fill.
  const addField = useCallback((fieldName) => {
    if (!engine?.execute || !isValidIdentifier(fieldName)) return;
    const lvalue = pathToMatlabLValue(variable.name, cur.path);
    const target = (payload && payload.numel > 1) ? `${lvalue}(1)` : lvalue;
    try {
      engine.execute(`${target}.${fieldName} = [];`);
    } catch (e) {
      console.warn('[StructInspector] add-field failed:', e);
    }
    setRefreshTick((t) => t + 1);
  }, [engine, variable.name, cur.path, payload]);

  // Remove a field via rmfield (drops it from every struct-array element).
  const deleteField = useCallback((fieldName) => {
    if (!engine?.execute || !isValidIdentifier(fieldName)) return;
    const lvalue = pathToMatlabLValue(variable.name, cur.path);
    try {
      engine.execute(`${lvalue} = rmfield(${lvalue}, '${fieldName}');`);
    } catch (e) {
      console.warn('[StructInspector] delete-field failed:', e);
    }
    setRefreshTick((t) => t + 1);
  }, [engine, variable.name, cur.path]);

  // Duplicate a field to a fresh, collision-free "<name>_copy" name. The
  // `[lvalue.copy] = lvalue.name` bracket form works for a single struct
  // and distributes the per-element CSL across a struct array. Names are
  // identifiers, so the expression is injection-safe.
  const duplicateField = useCallback((fieldName) => {
    if (!engine?.execute || !isValidIdentifier(fieldName)) return;
    const lvalue = pathToMatlabLValue(variable.name, cur.path);
    const existing = new Set(payload?.fields || []);
    let copy = `${fieldName}_copy`;
    for (let i = 2; existing.has(copy); i++) copy = `${fieldName}_copy${i}`;
    try {
      engine.execute(`[${lvalue}.${copy}] = ${lvalue}.${fieldName};`);
    } catch (e) {
      console.warn('[StructInspector] duplicate-field failed:', e);
    }
    setRefreshTick((t) => t + 1);
  }, [engine, variable.name, cur.path, payload]);

  // Insert a fresh empty field with a collision-free default name
  // ("unnamed", "unnamed1", …) — mirrors MATLAB's context-menu Insert.
  const insertField = useCallback(() => {
    const existing = new Set(payload?.fields || []);
    let name = 'unnamed';
    for (let i = 1; existing.has(name); i++) name = `unnamed${i}`;
    addField(name);
  }, [payload, addField]);

  // Rename = copy the field's value to the new name, drop the old, then
  // re-pin the field order so the renamed field keeps its original slot.
  // The `[lvalue.new] = lvalue.old` bracket form works for a single
  // struct and distributes the per-element CSL across a struct array.
  // Without the final orderfields() the new field would land at the END
  // (copy-append semantics); the 2-arg orderfields(s, {names...}) restores
  // position. Guards: valid identifier, no-op on same name, refuse to
  // clobber an existing field.
  const renameField = useCallback((oldName, newName) => {
    if (!engine?.execute || !isValidIdentifier(newName)) return;
    if (newName === oldName) return;
    if (payload?.fields?.includes(newName)) return;   // collision
    const lvalue = pathToMatlabLValue(variable.name, cur.path);
    // Desired order = current fields with oldName swapped to newName in
    // place. Field names are identifiers, so the {'a','b',...} cell
    // literal is injection-safe.
    const order = (payload?.fields || []).map((f) => (f === oldName ? newName : f));
    const orderCell = '{' + order.map((f) => `'${f}'`).join(',') + '}';
    try {
      let expr = `[${lvalue}.${newName}] = ${lvalue}.${oldName}; `
               + `${lvalue} = rmfield(${lvalue}, '${oldName}');`;
      if (order.length) expr += ` ${lvalue} = orderfields(${lvalue}, ${orderCell});`;
      engine.execute(expr);
    } catch (e) {
      console.warn('[StructInspector] rename-field failed:', e);
    }
    setRefreshTick((t) => t + 1);
  }, [engine, variable.name, cur.path, payload]);

  // Push a path: steps is an array of { k, name?/idx?, label }. Only the
  // last step's label becomes the breadcrumb segment (e.g. drilling into
  // a struct-array cell is two steps `e:`+`f:` but one crumb "(2).field").
  const drill = useCallback((steps) => {
    const parts = steps.map((s) => (s.k === 'f' ? `f:${s.name}` : `${s.k}:${s.idx}`));
    setNav((prev) => {
      const base = prev[prev.length - 1].path;
      const newPath = (base ? `${base};` : '') + parts.join(';');
      return [...prev, { label: steps[steps.length - 1].label, path: newPath }];
    });
  }, []);
  const jump = useCallback((i) => setNav((prev) => prev.slice(0, i + 1)), []);

  let body;
  if (error) body = <div className="ve-struct-empty">{error}</div>;
  else if (!payload) body = <div className="ve-struct-empty">loading…</div>;
  else if (payload.kind === 'struct') {
    body = payload.numel === 1
      ? <StructFieldList payload={payload} onDrill={drill}
          onAddField={addField} onDeleteField={deleteField} onRenameField={renameField}
          onDuplicateField={duplicateField} onInsertField={insertField} />
      : <StructArrayTable payload={payload} onDrill={drill}
          onAddField={addField} onDeleteField={deleteField} onRenameField={renameField} />;
  } else if (payload.kind === 'cell') {
    body = <CellGrid payload={payload} onDrill={drill} />;
  } else if (payload.kind === 'matrix') {
    const leaf = (cur.label || variable.name).replace(/^\./, '');
    body = (
      <DrilledMatrix
        payload={payload}
        name={leaf}
        onBack={nav.length > 1 ? () => jump(nav.length - 2) : undefined}
        onCommit={commitFieldCell}
      />
    );
  } else {
    body = <div className="ve-struct-empty">unsupported value</div>;
  }

  return (
    <div className="ve-inspector">
      <InspectorBreadcrumb nav={nav} onJump={jump} />
      <div className="ve-inspector-body">{body}</div>
    </div>
  );
}
