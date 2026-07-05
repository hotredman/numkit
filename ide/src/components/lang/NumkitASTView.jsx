/**
 * NumkitASTView — literal parse-tree inspector.
 *
 * Complements NumkitGraphView (data-flow IR) by showing the RAW AST
 * the parser produced. Every token construct visible: BINARY_OP,
 * NUMBER_LITERAL, IDENTIFIER, BLOCK, etc. Useful for debugging the
 * parser/lowering and as an educational view ("how does MATLAB
 * syntax parse?").
 *
 * Pipeline (same measure-then-layout as the graph view):
 *   1. engine.buildAST(source) → recursive JSON of the parse tree.
 *   2. flattenAST() converts that tree to a flat list of React Flow
 *      nodes/edges, honoring two user controls:
 *        - filters: per-category visibility (Literals, Operators,
 *          Control flow, …). Filtered-out nodes become TRANSPARENT
 *          PASSTHROUGHS — their children re-attach to the nearest
 *          visible ancestor in the rendered tree.
 *        - collapsed: Set<nodeId> of subtrees the user has folded
 *          away. Stable path-based IDs (`0/c1/b0/body/c2`) so the
 *          same AST position keeps its collapsed state across
 *          filter toggles.
 *   3. ReactFlow renders the unmeasured nodes at (0,0) with
 *      opacity:0 → useNodesInitialized fires → DOM-measured
 *      width/height feed ELK's `mrtree` algorithm → positions
 *      applied → opacity:1.
 *
 * Click semantics:
 *   - Click the chevron (▼/▶) on a node → toggle collapse.
 *   - Click the node body          → navigate to the source line
 *                                    via the onNavigate prop.
 *
 * Props:
 *   source     : string           — .m text to parse.
 *   engine     : object           — must expose buildAST(text).
 *   onNavigate : (line, col) => void  — optional; called when the
 *                user clicks a node body.
 */

import { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState } from 'react';
import ReactFlow, {
  Background,
  Controls,
  MiniMap,
  ReactFlowProvider,
  Handle,
  Position,
  useNodesInitialized,
  useReactFlow,
  useStoreApi,
} from 'reactflow';
import ELK from 'elkjs/lib/elk.bundled.js';
import 'reactflow/dist/style.css';

import { useFitOnResize } from './useFitOnResize';
import {
  CATEGORIES, categoryOf, categoryColor, defaultFilters,
  hasAnyChildren, valueText, eachAstChild,
  findActiveAstId, collectCollapsibleIds,
} from './astShared';

const elk = new ELK();

// ── AST tree → flat React Flow nodes/edges ────────────────────────

// helpers (CATEGORIES, hasAnyChildren, valueText, findActiveAstId,
// collectCollapsibleIds, eachAstChild) live in `./astShared` so the
// tree view (NumkitASTTreeView) consumes the exact same taxonomy.

function flattenAST(astRoot, collapsedSet, filters, activeId) {
  if (!astRoot) return { nodes: [], edges: [] };
  const rfNodes = [];
  const rfEdges = [];
  function allowed(node) {
    if (!node) return false;
    const cat = categoryOf(node.type);
    return filters[cat] !== false;
  }
  function visit(node, path, displayParentId) {
    if (!node) return;
    const isShown = allowed(node);
    const id = path;
    const collapsed = collapsedSet.has(id);
    if (isShown) {
      rfNodes.push({
        id,
        type: 'ast',
        position: { x: 0, y: 0 },
        data: {
          astType:   node.type,
          line:      node.line,
          col:       node.col,
          value:     valueText(node),
          paramNames:  node.paramNames  || [],
          returnNames: node.returnNames || [],
          category:    categoryOf(node.type),
          hasChildren: hasAnyChildren(node),
          collapsed,
          active:    id === activeId,
        },
      });
      if (displayParentId != null) {
        rfEdges.push({
          id:     `${displayParentId}->${id}`,
          source: displayParentId,
          target: id,
          sourceHandle: 'child-out',
          targetHandle: 'parent-in',
          type: 'default',
          className: 'ng-ast-edge',
        });
      }
      if (collapsed) return;  // hide subtree
    }
    // Recurse with the appropriate parent context: shown nodes are
    // their children's parent; filtered-out nodes pass their parent
    // through (children "rise" past them in the rendered tree).
    const parentForChildren = isShown ? id : displayParentId;
    eachAstChild(node, path, (child, childPath) => {
      visit(child, childPath, parentForChildren);
    });
  }
  visit(astRoot, '0', null);
  return { nodes: rfNodes, edges: rfEdges };
}

// ── AST node renderer ─────────────────────────────────────────────

const ASTViewContext = createContext({
  onToggleCollapse: () => {},
  onNavigate:       () => {},
});

function ASTNodeCard({ id, data }) {
  const { onToggleCollapse, onNavigate } = useContext(ASTViewContext);
  const handleToggle = (e) => {
    e.stopPropagation();   // don't trigger body click
    onToggleCollapse(id);
  };
  const handleBody = () => {
    if (data.line) onNavigate(data.line, data.col || 1);
  };
  const classes = `ng-ast-node ng-ast-cat-${data.category}`
                + (data.active ? ' ng-ast-node-active' : '');
  return (
    <div className={classes}
         onClick={handleBody}
         title={`line ${data.line}:${data.col} — click to jump to source`}>
      <Handle type="target" position={Position.Top}    id="parent-in" />
      <Handle type="source" position={Position.Bottom} id="child-out" />
      <span className="ng-ast-type">{data.astType}</span>
      {data.value && <span className="ng-ast-value">{data.value}</span>}
      {data.hasChildren && (
        <button className="ng-ast-toggle"
                onClick={handleToggle}
                title={data.collapsed ? 'Expand subtree' : 'Collapse subtree'}>
          {data.collapsed ? '▶' : '▼'}
        </button>
      )}
    </div>
  );
}

const nodeTypes = { ast: ASTNodeCard };

// ── Filter bar ────────────────────────────────────────────────────

function ASTFilterBar({ filters, onChange, onCollapseAll, onExpandAll }) {
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
      <button className="ng-ast-action"
              onClick={onCollapseAll}
              title="Collapse every node that has children">collapse all</button>
      <button className="ng-ast-action"
              onClick={onExpandAll}
              title="Expand all collapsed subtrees">expand all</button>
    </div>
  );
}

// ── ELK layout (mrtree, top-down) ─────────────────────────────────

function buildElkAST(rfNodes, rfEdges, sizeById) {
  return {
    id: 'root',
    layoutOptions: {
      // `layered` handles forests (multiple disconnected roots) and
      // arbitrary DAGs robustly — important here because the user
      // may filter out the root BLOCK node, leaving the script's
      // top-level statements as a forest. `mrtree` is tree-only and
      // silently fails on disconnected inputs.
      'elk.algorithm':                              'layered',
      'elk.direction':                              'DOWN',
      'elk.spacing.nodeNode':                       '20',
      'elk.layered.spacing.nodeNodeBetweenLayers':  '40',
    },
    children: rfNodes.map((n) => {
      const m = sizeById[n.id];
      return { id: n.id, width: m?.width ?? 120, height: m?.height ?? 28 };
    }),
    edges: rfEdges.map((e) => ({
      id: e.id,
      sources: [e.source],
      targets: [e.target],
    })),
  };
}

function applyElkPositions(elkResult, rfNodes) {
  const posById = new Map();
  for (const c of elkResult.children || []) {
    posById.set(c.id, { x: c.x || 0, y: c.y || 0 });
  }
  return rfNodes.map((n) => ({
    ...n,
    position: posById.get(n.id) || { x: 0, y: 0 },
  }));
}

// ── Inner component ───────────────────────────────────────────────

function NumkitASTViewInner({ source, engine, onNavigate, cursorLine }) {
  const [ast, setAst]         = useState(null);
  const [error, setError]     = useState(null);
  const [collapsed, setCollapsed] = useState(() => new Set());
  const [filters, setFilters] = useState(defaultFilters);
  const [nodes, setNodes]     = useState([]);
  const [edges, setEdges]     = useState([]);
  const [laidOut, setLaidOut] = useState(false);
  const tokenRef = useRef(0);
  const nodesInitialized = useNodesInitialized();
  const reactFlow        = useReactFlow();
  const storeApi         = useStoreApi();

  // Force React Flow to (re)measure every node whenever the node set
  // changes. Without this the AST pane renders blank (opacity 0) for large
  // trees: RF measures each node exactly once, when its ResizeObserver first
  // observes it, and that initial callback can fire before RF has recorded
  // its container element — so updateNodeDimensions bails. The static AST
  // nodes never resize again, so RF never retries, `useNodesInitialized()`
  // stays false forever, and Phase 2 (ELK layout → opacity 1) never runs.
  //
  // Must run on EVERY change, not just the first: Phase 2 re-emits fresh node
  // objects carrying their ELK positions, which resets RF's per-node
  // measurement again. Several delayed passes cover the window in which the
  // node DOM elements settle after a `setNodes` (a single pass can miss
  // late-rendered nodes, leaving them stuck hidden). `updateNodeDimensions`
  // only writes RF's internal store, so it never feeds back into `nodes`.
  // (`useUpdateNodeInternals` is unreliable here — it rAF-defers and
  // re-queries each node by id, landing on the stale pre-measure state;
  // driving the store action synchronously over the live elements does not.)
  useEffect(() => {
    if (nodes.length === 0) return undefined;
    const remeasure = () => {
      const { domNode, updateNodeDimensions } = storeApi.getState();
      if (!domNode) return;
      const updates = [...domNode.querySelectorAll('.react-flow__node')].map((el) => ({
        id: el.getAttribute('data-id'),
        nodeElement: el,
        forceUpdate: true,
      }));
      if (updates.length) updateNodeDimensions(updates);
    };
    const timers = [0, 60, 140, 280, 500].map((ms) => setTimeout(remeasure, ms));
    return () => timers.forEach(clearTimeout);
  }, [nodes, storeApi]);

  // Re-fit when the pane resizes — `fitView` only fits once (see
  // useFitOnResize). Without this a sibling-pane / Figures / window
  // resize leaves the tree clipped and not flush to the pane edges.
  const containerRef = useRef(null);
  useFitOnResize(containerRef, reactFlow, laidOut);

  // Lower source → AST JSON.
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
    const result = engine.buildAST(source);
    if (result && result.error) {
      setError(result.error);
      setAst(null);
    } else {
      setAst(result);
      setError(null);
      // Reset collapsed set on a source change — old IDs may have
      // become stale.
      setCollapsed(new Set());
    }
  }, [source, engine]);

  // Pick the deepest visible AST node containing the editor caret —
  // used to highlight that node + auto-center the view on it.
  const activeId = useMemo(
    () => findActiveAstId(ast, cursorLine, collapsed, filters),
    [ast, cursorLine, collapsed, filters]);

  // Rebuild flat node list on AST / filters / collapsed / active change.
  // flattenAST returns { nodes, edges } — destructure with rename so
  // we can pass them as `nodes` / `edges` to <ReactFlow> further down
  // without shadowing the local `nodes` state.
  const { nodes: flatNodes, edges: flatEdges } = useMemo(() => {
    return flattenAST(ast, collapsed, filters, activeId);
  }, [ast, collapsed, filters, activeId]);

  // Phase 1 — push unmeasured nodes to RF for DOM measurement.
  useEffect(() => {
    tokenRef.current += 1;
    setLaidOut(false);
    setNodes(flatNodes);
    setEdges(flatEdges);
  }, [flatNodes, flatEdges]);

  // Phase 2 — once measured, ELK layout.
  useEffect(() => {
    if (!nodesInitialized || nodes.length === 0 || laidOut) return;
    const token = tokenRef.current;
    const sizeById = {};
    for (const rf of reactFlow.getNodes()) {
      if (rf.width != null && rf.height != null) {
        sizeById[rf.id] = { width: rf.width, height: rf.height };
      }
    }
    elk.layout(buildElkAST(nodes, edges, sizeById)).then((res) => {
      if (tokenRef.current !== token) return;
      setNodes((prev) => applyElkPositions(res, prev));
      setLaidOut(true);
    }).catch((err) => {
      if (tokenRef.current !== token) return;
      // eslint-disable-next-line no-console
      console.error('ELK AST layout failed', err);
      setError(`Layout failed: ${err.message || err}`);
    });
  }, [nodesInitialized, nodes, edges, laidOut, reactFlow]);

  // Auto-pan to the active node when it changes (cursor moved in
  // editor, AST highlights a new node). Only after the layout is
  // applied so positions are valid; preserves the user's current
  // zoom level (only the center shifts).
  useEffect(() => {
    if (!laidOut || !activeId) return;
    const n = nodes.find((x) => x.id === activeId);
    if (!n) return;
    const w = n.width  || 100;
    const h = n.height || 30;
    reactFlow.setCenter(
      n.position.x + w / 2,
      n.position.y + h / 2,
      { zoom: reactFlow.getZoom(), duration: 300 },
    );
  }, [activeId, laidOut, nodes, reactFlow]);

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
      <div className="numkit-ast-view ng-empty">
        <div className="ng-empty-title">AST unavailable</div>
        <div className="ng-empty-msg">{error}</div>
      </div>
    );
  }
  if (!ast) {
    return (
      <div className="numkit-ast-view ng-empty">
        <div className="ng-empty-title">Empty AST</div>
        <div className="ng-empty-msg">
          Load a script in the editor to see its parse tree here.
        </div>
      </div>
    );
  }

  return (
    <ASTViewContext.Provider value={{ onToggleCollapse, onNavigate: onNavigateSafe }}>
      <div className="numkit-ast-view" ref={containerRef}>
        <ASTFilterBar filters={filters} onChange={setFilters}
                      onCollapseAll={() => setCollapsed(collectCollapsibleIds(ast, filters))}
                      onExpandAll={() => setCollapsed(new Set())} />
        <ReactFlow
          nodes={nodes}
          edges={edges}
          nodeTypes={nodeTypes}
          defaultEdgeOptions={{ animated: false }}
          fitView={laidOut}
          proOptions={{ hideAttribution: true }}
          nodesDraggable={false}
          nodesConnectable={false}
          elementsSelectable={true}
          style={{ opacity: laidOut ? 1 : 0,
                   transition: 'opacity 120ms ease-in' }}
        >
          <Background gap={16} size={1} color="var(--line-soft)" />
          <Controls showInteractive={false} />
          <MiniMap pannable zoomable
                   nodeColor={(n) => categoryColor(n.data?.category)}
                   style={{ background: 'var(--bg-2)' }} />
        </ReactFlow>
      </div>
    </ASTViewContext.Provider>
  );
}

export default function NumkitASTView(props) {
  return (
    <ReactFlowProvider>
      <NumkitASTViewInner {...props} />
    </ReactFlowProvider>
  );
}
