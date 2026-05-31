/**
 * ValueTable — the shared named-value table behind both the Workspace
 * list view and the struct/cell inspector. Renders a Field/Name · Value ·
 * Size · Class (+ optional stat) table whose visible columns are chosen
 * from a right-click header menu and persisted per `storageKey`.
 *
 * Callers supply the rows and (optionally) a custom name-cell renderer and
 * row click / context-menu handlers — keeping ValueTable agnostic to
 * whether a row is a workspace variable or a struct field.
 */
import { useState, useEffect } from 'react';
import ContextMenu from './ContextMenu';
import {
  VALUE_COLUMNS, loadVisibleColumns, saveVisibleColumns, toggleColumn,
  statValue, fmtStat,
} from './valueColumns';

export default function ValueTable({
  rows,
  nameHeader = 'Name',
  nameCell,            // optional (row) => node for the name cell
  storageKey,
  onRowClick,          // optional (row) => void — fired on a drillable row
  onRowContextMenu,    // optional (row, event) => void — caller renders its menu
  footer,              // optional node rendered under the table
  emptyLabel = '(empty)',
}) {
  const [visible, setVisible] = useState(() => loadVisibleColumns(storageKey));
  const [headMenu, setHeadMenu] = useState(null);   // { x, y } | null

  useEffect(() => { saveVisibleColumns(storageKey, visible); }, [storageKey, visible]);

  const cols = VALUE_COLUMNS.filter((c) => visible.has(c.key));

  const renderCell = (row, c) => {
    if (c.key === 'value') return <td key="value" className="vt-value" title={row.value}>{row.value}</td>;
    if (c.key === 'size')  return <td key="size" className="vt-right vt-muted">{row.size || ''}</td>;
    if (c.key === 'class') return <td key="class" className="vt-class">{row.klass || ''}</td>;
    return <td key={c.key} className="vt-right vt-muted">{fmtStat(statValue(row.stats, c.stat))}</td>;
  };

  return (
    <div className="vt-wrap">
      <table className="vt-table">
        <thead>
          <tr onContextMenu={(e) => { e.preventDefault(); setHeadMenu({ x: e.clientX, y: e.clientY }); }}
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
                ? (e) => { e.preventDefault(); onRowContextMenu(row, e); } : undefined}>
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
        <ContextMenu x={headMenu.x} y={headMenu.y} onClose={() => setHeadMenu(null)} items={[
          { label: 'Select all', keepOpen: true,
            onClick: () => setVisible(new Set(VALUE_COLUMNS.map((c) => c.key))) },
          { label: 'Clear all', keepOpen: true,
            onClick: () => setVisible(new Set()) },
          { separator: true },
          // The name column is always shown (checked + disabled), mirroring
          // MATLAB's locked Field/Name column.
          { label: `✓ ${nameHeader}`, disabled: true },
          ...VALUE_COLUMNS.map((c) => ({
            label: `${visible.has(c.key) ? '✓' : ' '} ${c.label}`,
            keepOpen: true,
            onClick: () => setVisible((prev) => toggleColumn(prev, c.key)),
          })),
        ]} />
      )}
    </div>
  );
}
