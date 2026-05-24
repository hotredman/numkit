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
} from 'reactflow';
import ELK from 'elkjs/lib/elk.bundled.js';
import 'reactflow/dist/style.css';

const elk = new ELK();

// ── NodeType taxonomy → display category ──────────────────────────

const CATEGORIES = [
  {
    key:    'container',
    label:  'Containers',
    types:  ['BLOCK', 'EXPR_STMT'],
    color:  '#7a8390',
    /* off by default — BLOCK / EXPR_STMT add noise without much info,
       the user can switch them back on if they want a literal view. */
    defaultOn: false,
  },
  { key: 'literal',  label: 'Literals',
    types: ['NUMBER_LITERAL', 'IMAG_LITERAL', 'STRING_LITERAL',
            'DQSTRING_LITERAL', 'BOOL_LITERAL', 'MATRIX_LITERAL',
            'CELL_LITERAL'],
    color: '#7fd99a', defaultOn: true },
  { key: 'ident',    label: 'Identifiers',
    types: ['IDENTIFIER', 'END_VAL'],
    color: '#7fd0e0', defaultOn: true },
  { key: 'operator', label: 'Operators',
    types: ['BINARY_OP', 'UNARY_OP', 'COLON_EXPR'],
    color: '#9b8cf2', defaultOn: true },
  { key: 'access',   label: 'Calls & access',
    types: ['CALL', 'COMMAND_CALL', 'INDEX', 'CELL_INDEX',
            'FIELD_ACCESS', 'DYNAMIC_FIELD_ACCESS', 'ANON_FUNC'],
    color: '#f0b97a', defaultOn: true },
  { key: 'assign',   label: 'Assignments',
    types: ['ASSIGN', 'MULTI_ASSIGN', 'DELETE_ASSIGN'],
    color: '#5fb87a', defaultOn: true },
  { key: 'control',  label: 'Control flow',
    types: ['IF_STMT', 'FOR_STMT', 'WHILE_STMT', 'SWITCH_STMT',
            'BREAK_STMT', 'CONTINUE_STMT', 'RETURN_STMT', 'TRY_STMT'],
    color: '#d97c7c', defaultOn: true },
  { key: 'decl',     label: 'Declarations',
    types: ['FUNCTION_DEF', 'GLOBAL_STMT', 'PERSISTENT_STMT'],
    color: '#c98cf2', defaultOn: true },
];

/** Build a constant Map<typeName, categoryKey> for O(1) lookup. */
const TYPE_TO_CAT = (() => {
  const m = new Map();
  for (const cat of CATEGORIES) for (const t of cat.types) m.set(t, cat.key);
  return m;
})();

function categoryOf(type)  { return TYPE_TO_CAT.get(type) || 'other'; }
function defaultFilters()  {
  const f = {};
  for (const cat of CATEGORIES) f[cat.key] = cat.defaultOn;
  f.other = true;
  return f;
}

// ── AST tree → flat React Flow nodes/edges ────────────────────────

/** True iff this AST node has at least one descendant to show.
 *  Used to render the collapse chevron only when there's something
 *  to collapse. */
function hasAnyChildren(node) {
  if (!node) return false;
  if (node.children && node.children.length > 0) return true;
  if (node.branches && node.branches.length > 0) return true;
  if (node.elseBranch) return true;
  return false;
}

/** Compact one-line value preview shown next to the type chip. */
function valueText(node) {
  if (!node) return '';
  switch (node.type) {
    case 'IDENTIFIER':         return node.strValue || '';
    case 'NUMBER_LITERAL':     return String(node.numValue ?? '');
    case 'IMAG_LITERAL':       return `${node.numValue ?? 0}i`;
    case 'STRING_LITERAL':     return `'${node.strValue ?? ''}'`;
    case 'DQSTRING_LITERAL':   return `"${node.strValue ?? ''}"`;
    case 'BOOL_LITERAL':       return node.boolValue ? 'true' : 'false';
    case 'BINARY_OP':
    case 'UNARY_OP':           return node.strValue || '';
    case 'FUNCTION_DEF':       return node.strValue || '';
    case 'CALL':
    case 'COMMAND_CALL':       return node.strValue || '';
    case 'FIELD_ACCESS':
    case 'DYNAMIC_FIELD_ACCESS': return `.${node.strValue || ''}`;
    case 'FOR_STMT':           return node.strValue || '';   // iter var
    case 'TRY_STMT':           return node.strValue || '';   // catch var
    default:                   return '';
  }
}

function flattenAST(astRoot, collapsedSet, filters) {
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
    const kids = node.children || [];
    for (let i = 0; i < kids.length; ++i) {
      visit(kids[i], `${path}/c${i}`, parentForChildren);
    }
    const branches = node.branches || [];
    for (let i = 0; i < branches.length; ++i) {
      visit(branches[i].cond, `${path}/b${i}/cond`, parentForChildren);
      visit(branches[i].body, `${path}/b${i}/body`, parentForChildren);
    }
    if (node.elseBranch) {
      visit(node.elseBranch, `${path}/else`, parentForChildren);
    }
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
  return (
    <div className={`ng-ast-node ng-ast-cat-${data.category}`}
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

function ASTFilterBar({ filters, onChange }) {
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
    </div>
  );
}

// ── ELK layout (mrtree, top-down) ─────────────────────────────────

function buildElkAST(rfNodes, rfEdges, sizeById) {
  return {
    id: 'root',
    layoutOptions: {
      'elk.algorithm':                              'mrtree',
      'elk.direction':                              'DOWN',
      'elk.mrtree.searchOrder':                     'DFS',
      'elk.spacing.nodeNode':                       '20',
      'elk.mrtree.spacing.nodeNode':                '24',
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

function NumkitASTViewInner({ source, engine, onNavigate }) {
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

  // Rebuild flat node list on AST / filters / collapsed change.
  const { rfNodes: flatNodes, rfEdges: flatEdges } = useMemo(() => {
    return flattenAST(ast, collapsed, filters);
  }, [ast, collapsed, filters]);

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
      <div className="numkit-ast-view">
        <ASTFilterBar filters={filters} onChange={setFilters} />
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
                   nodeColor={(n) => {
                     const cat = n.data?.category;
                     const found = CATEGORIES.find((c) => c.key === cat);
                     return found?.color || '#888';
                   }}
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
