/**
 * ValueTable — the shared named-value table behind both the Workspace
 * list view and the struct/cell inspector. Renders a Field/Name · Value ·
 * Size · Class (+ optional stat) table whose visible columns are chosen
 * from the shared chooser (header right-click here, plus an optional
 * toolbar button the caller wires through the same state).
 *
 * Column visibility is controlled when the caller passes `visible` /
 * `setVisible` (e.g. the Workspace, which also shows a toolbar chooser
 * button on the same state); otherwise ValueTable self-manages it,
 * persisting per `storageKey` (the struct inspector path).
 *
 * Callers supply the rows and (optionally) a custom name-cell renderer and
 * row click / context-menu handlers — keeping ValueTable agnostic to
 * whether a row is a workspace variable or a struct field.
 */
import { useState, useEffect } from 'react';
import ContextMenu from '../ui/ContextMenu';
import {
  VALUE_COLUMNS, loadVisibleColumns, saveVisibleColumns, buildChooserItems,
  statValue, fmtStat, fmtBytes,
} from './valueColumns';

export default function ValueTable({
  rows,
  nameHeader = 'Name',
  nameCell,            // optional (row) => node for the name cell
  storageKey,
  visible: cVisible,       // optional controlled column set
  setVisible: cSetVisible, // optional controlled setter
  onRowClick,          // optional (row) => void — fired on a drillable row
  onRowContextMenu,    // optional (row, event) => void — caller renders its menu
  footer,              // optional node rendered under the table
  emptyLabel = '(empty)',
}) {
  const controlled = cSetVisible != null;
  // Internal state is always declared (hooks can't be conditional) but is
  // unused / not persisted when the caller controls the column set.
  const [iVisible, iSetVisible] = useState(() => controlled ? new Set() : loadVisibleColumns(storageKey));
  const visible = controlled ? cVisible : iVisible;
  const setVisible = controlled ? cSetVisible : iSetVisible;
  useEffect(() => {
    if (!controlled) saveVisibleColumns(storageKey, iVisible);
  }, [controlled, storageKey, iVisible]);

  const [headMenu, setHeadMenu] = useState(null);   // { x, y } | null
  const cols = VALUE_COLUMNS.filter((c) => visible.has(c.key));

  const renderCell = (row, c) => {
    if (c.key === 'value') return <td key="value" className="vt-value" title={row.value}>{row.value}</td>;
    if (c.key === 'size')  return <td key="size" className="vt-right vt-muted">{row.size || ''}</td>;
    if (c.key === 'bytes') return <td key="bytes" className="vt-right vt-muted">{fmtBytes(row.bytes)}</td>;
    if (c.key === 'class') return <td key="class" className="vt-class">{row.klass || ''}</td>;
    return <td key={c.key} className="vt-right vt-muted">{fmtStat(statValue(row.stats, c.stat))}</td>;
  };

  return (
    <div className="vt-wrap">
      <table className="vt-table">
        <thead>
          <tr onContextMenu={(e) => { e.preventDefault(); e.stopPropagation(); setHeadMenu({ x: e.clientX, y: e.clientY }); }}
            title="right-click to choose columns">
            <th>{nameHeader}</th>
            {cols.map((c) => (
              <th key={c.key} className={c.align === 'right' ? 'vt-right' : ''}>{c.label}</th>
            ))}
          </tr>
        </thead>
        <tbody>
          {rows.map((row) => (
            <tr key={row.key}
              className={row.drill ? 'is-drillable' : ''}
              onClick={onRowClick && row.drill ? () => onRowClick(row) : undefined}
              onContextMenu={onRowContextMenu
                ? (e) => { e.preventDefault(); e.stopPropagation(); onRowContextMenu(row, e); } : undefined}>
              <td className="vt-name">{nameCell ? nameCell(row) : row.name}</td>
              {cols.map((c) => renderCell(row, c))}
            </tr>
          ))}
          {rows.length === 0 && (
            <tr><td colSpan={cols.length + 1} className="ve-struct-empty">{emptyLabel}</td></tr>
          )}
        </tbody>
      </table>
      {footer}
      {headMenu && (
        <ContextMenu x={headMenu.x} y={headMenu.y} onClose={() => setHeadMenu(null)}
          items={buildChooserItems({
            defs: VALUE_COLUMNS, visible, setVisible, lockedLabel: nameHeader,
          })} />
      )}
    </div>
  );
}
