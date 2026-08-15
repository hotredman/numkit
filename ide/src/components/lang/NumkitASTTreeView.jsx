/**
 * NumkitASTTreeView — indented parse-tree inspector (astexplorer-style).
 *
 * Complementary to NumkitASTView (graph layout):
 *   • Graph view = spatial overview of how nodes connect.
 *   • Tree view  = drill-down, depth-first, chevron-collapsible
 *                  inline list. Easier to read deeply nested
 *                  expressions; takes less horizontal space.
 *
 * Both views share `astShared.js` (CATEGORIES, valueText, filters,
 * findActiveAstId, collectCollapsibleIds, eachAstChild) so a new
 * NodeType / category appears in both at once.
 *
 * Each row:
 *   ┌─ chevron (▼/▶, only when node has children)
 *   ├─ type chip (coloured by category)
 *   ├─ optional value (e.g. `'x'`, `3.14`, `+`)
 *   └─ line:col (faded, right side)
 *
 * Click on the row body  → onNavigate(line, col) handoff to editor.
 * Click on chevron       → toggle collapsed subtree.
 *
 * Bidirectional cursor sync: when `cursorLine` matches an AST
 * node's source range, that row gets the `is-active` class and is
 * scrolled into view (`scrollIntoView({block: 'nearest'})`).
 *
 * Props:
 *   source     : string             — the .m text to parse.
 *   engine     : object             — must expose buildAST(text).
 *   cursorLine : number             — optional; line of editor caret
 *                                     for the highlight handoff.
 *   onNavigate : (line, col) => void — optional; called when the
 *                                     user clicks a row body.
 */

import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  CATEGORIES, categoryOf, defaultFilters,
  hasAnyChildren, valueText, eachAstChild,
  findActiveAstId, collectCollapsibleIds,
} from './astShared';

// ── Single tree row (recursive) ─────────────────────────────────────

function ASTTreeRow({
  node, path, depth,
  collapsed, filters, activeId,
  onToggleCollapse, onNavigate, activeRowRef,
}) {
  if (!node) return null;
  const visible = filters[categoryOf(node.type)] !== false;
  const isCollapsed = collapsed.has(path);
  const isActive    = path === activeId;

  // Filtered-out nodes are PASSTHROUGH: don't render them, but emit
  // their children so the subtree stays visible. Children inherit
  // the same depth — they "rise" past the hidden parent.
  if (!visible) {
    const items = [];
    eachAstChild(node, path, (child, childPath) => {
      items.push(
        <ASTTreeRow key={childPath}
                    node={child} path={childPath} depth={depth}
                    collapsed={collapsed} filters={filters} activeId={activeId}
                    onToggleCollapse={onToggleCollapse} onNavigate={onNavigate}
                    activeRowRef={activeRowRef} />
      );
    });
    return items;
  }

  const canCollapse = hasAnyChildren(node);
  const showValue   = valueText(node);
  const className   = `ng-ast-tree-row ng-ast-cat-${categoryOf(node.type)}`
                    + (isActive ? ' is-active' : '');
  const handleToggle = (e) => {
    e.stopPropagation();
    if (canCollapse) onToggleCollapse(path);
  };
  const handleRow = () => {
    if (node.line) onNavigate(node.line, node.col || 1);
  };

  const children = !isCollapsed && (
    <>
      {(() => {
        const items = [];
        eachAstChild(node, path, (child, childPath) => {
          items.push(
            <ASTTreeRow key={childPath}
                        node={child} path={childPath} depth={depth + 1}
                        collapsed={collapsed} filters={filters} activeId={activeId}
                        onToggleCollapse={onToggleCollapse} onNavigate={onNavigate}
                        activeRowRef={activeRowRef} />
          );
        });
        return items;
      })()}
    </>
  );

  return (
    <>
      <div className={className}
           ref={isActive ? activeRowRef : null}
           onClick={handleRow}
           style={{ paddingLeft: 6 + depth * 14 }}
           title={`line ${node.line}:${node.col || 1} — click to jump to source`}>
        <span className={`ng-ast-tree-chev${canCollapse ? '' : ' is-leaf'}`}
              onClick={handleToggle}>
          {canCollapse ? (isCollapsed ? '▶' : '▼') : '·'}
        </span>
        <span className="ng-ast-tree-type">{node.type}</span>
        {showValue && (
          <span className="ng-ast-tree-value">{showValue}</span>
        )}
        <span className="ng-ast-tree-loc">{node.line}:{node.col || 1}</span>
      </div>
      {children}
    </>
  );
}

// ── Filter bar (same chips as graph AST view, plus the actions) ────

function ASTTreeFilterBar({ filters, onChange, onCollapseAll, onExpandAll }) {
  return (
    <div className="ng-ast-filterbar">
      {CATEGORIES.map((cat) => (
        <button key={cat.key}
                className={`ng-ast-chip${filters[cat.key] ? ' ng-ast-chip-on' : ''}`}
                style={{ '--chip-color': cat.color }}
                onClick={() => onChange({ ...filters, [cat.key]: !filters[cat.key] })}
                title={cat.types.join(', ')}>
          {cat.label}
        </button>
      ))}
      <span className="ng-ast-filterbar-sep" />
      <button className="ng-ast-action" onClick={onCollapseAll}
              title="Collapse every node that has children">collapse all</button>
      <button className="ng-ast-action" onClick={onExpandAll}
              title="Expand all collapsed subtrees">expand all</button>
    </div>
  );
}

// ── Component ──────────────────────────────────────────────────────

export default function NumkitASTTreeView({ source, engine, cursorLine, onNavigate }) {
  const [ast, setAst]         = useState(null);
  const [error, setError]     = useState(null);
  const [collapsed, setCollapsed] = useState(() => new Set());
  const [filters, setFilters] = useState(defaultFilters);
  const activeRowRef = useRef(null);

  useEffect(() => {
    if (!engine || typeof engine.buildAST !== 'function') {
      setError('engine.buildAST not available');
      setAst(null);
      return;
    }
    if (!source || source.trim() === '') {
      setAst(null);
      setError(null);
      return;
    }
    let cancelled = false;
    (async () => {
      try {
        const result = await engine.buildAST(source);
        if (cancelled) return;
        if (result && result.error) {
          setError(result.error);
          setAst(null);
        } else {
          setAst(result);
          setError(null);
          setCollapsed(new Set());  // stale IDs on source change
        }
      } catch (err) {
        if (cancelled) return;
        setError(err?.message || String(err));
        setAst(null);
      }
    })();
    return () => { cancelled = true; };
  }, [source, engine]);

  const activeId = useMemo(
    () => findActiveAstId(ast, cursorLine, collapsed, filters),
    [ast, cursorLine, collapsed, filters]);

  // Scroll active row into view when it changes (cursor moved in
  // editor → new AST node became active).
  useEffect(() => {
    if (!activeId || !activeRowRef.current) return;
    activeRowRef.current.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
  }, [activeId]);

  const onToggleCollapse = useCallback((id) => {
    setCollapsed((prev) => {
      const next = new Set(prev);
      if (next.has(id)) next.delete(id);
      else              next.add(id);
      return next;
    });
  }, []);
  const onNavigateSafe = useCallback((line, col) => {
    if (typeof onNavigate === 'function') onNavigate(line, col);
  }, [onNavigate]);

  if (error) {
    return (
      <div className="numkit-ast-tree-view ng-empty">
        <div className="ng-empty-title">AST unavailable</div>
        <div className="ng-empty-msg">{error}</div>
      </div>
    );
  }
  if (!ast) {
    return (
      <div className="numkit-ast-tree-view ng-empty">
        <div className="ng-empty-title">Empty AST</div>
        <div className="ng-empty-msg">
          Load a script in the editor to see its parse tree here.
        </div>
      </div>
    );
  }

  return (
    <div className="numkit-ast-tree-view">
      <ASTTreeFilterBar filters={filters} onChange={setFilters}
                        onCollapseAll={() => setCollapsed(collectCollapsibleIds(ast, filters))}
                        onExpandAll={() => setCollapsed(new Set())} />
      <div className="ng-ast-tree-scroll">
        <ASTTreeRow
          node={ast} path="0" depth={0}
          collapsed={collapsed} filters={filters} activeId={activeId}
          onToggleCollapse={onToggleCollapse} onNavigate={onNavigateSafe}
          activeRowRef={activeRowRef} />
      </div>
    </div>
  );
}
