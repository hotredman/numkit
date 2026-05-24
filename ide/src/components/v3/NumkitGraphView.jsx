/**
 * NumkitGraphView — script-graph visualizer (Phase 2a, read-only).
 *
 * Takes a .m source string, runs it through the WASM lowering pass
 * (`engine.buildScriptGraph`), then renders the resulting NodeGraph
 * IR via React Flow with ELK as the layout engine.
 *
 * Layout strategy: MEASURE then LAYOUT.
 *
 *   1. The graph IR is converted to React Flow nodes with `position:
 *      (0, 0)` and the ReactFlow container starts at `opacity: 0`.
 *   2. React Flow renders and measures every node's actual DOM size
 *      (via its internal ResizeObserver). The `useNodesInitialized()`
 *      hook fires when all measurements are in.
 *   3. We collect each node's measured width/height. For region
 *      compounds we additionally read the header bar's height from
 *      the DOM (`[data-region-header={id}]`) so ELK gets the correct
 *      top inset.
 *   4. ELK runs `layered` layout with those real sizes.
 *   5. We apply ELK positions (and region sizes), then fade-in.
 *
 *   No magic numbers. CSS owns appearance entirely — change padding,
 *   font, port-label style freely; JS just reads DOM.
 *
 * Custom node kinds — one component per NodeKind family:
 *   • AssignmentNode / ExprStmtNode — code-block body + input/output
 *     handles. No header bar (the source line is the title).
 *   • OpaqueNode — GlobalDecl / PersistentDecl etc. Kind tag + body.
 *   • RegionNode — IfRegion / ForRegion / WhileRegion / SwitchRegion /
 *     TryRegion / FunctionDef. Header bar shows the source slice;
 *     children land inside via ReactFlow's `parentNode`/`extent`.
 *   • JumpNode — break / continue / return pill.
 *   • MergeNode — Phase-2c placeholder.
 *
 * Props:
 *   source : string  — the .m text to visualize. Empty → empty graph.
 *   engine : object  — must expose buildScriptGraph(text) → graph JSON
 *                      (or { error: '...' }).
 */

import { useEffect, useRef, useState } from 'react';
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

// ── Node kind taxonomy ──────────────────────────────────────────────

const REGION_KINDS = new Set([
  'IfRegion', 'ForRegion', 'WhileRegion', 'SwitchRegion',
  'TryRegion', 'FunctionDef',
]);
const JUMP_KINDS   = new Set(['JumpBreak', 'JumpContinue', 'JumpReturn']);

const isRegionKind = (k) => REGION_KINDS.has(k);
const isJumpKind   = (k) => JUMP_KINDS.has(k);

function kindLabel(k) {
  switch (k) {
    case 'IfRegion':       return 'if';
    case 'ForRegion':      return 'for';
    case 'WhileRegion':    return 'while';
    case 'SwitchRegion':   return 'switch';
    case 'TryRegion':      return 'try';
    case 'FunctionDef':    return 'function';
    case 'JumpBreak':      return 'break';
    case 'JumpContinue':   return 'continue';
    case 'JumpReturn':     return 'return';
    case 'GlobalDecl':     return 'global';
    case 'PersistentDecl': return 'persistent';
    case 'Merge':          return 'merge';
    default:               return k;
  }
}

// ── Custom node renderers ───────────────────────────────────────────
//
// Port handles + labels render in a horizontal flex row UNDER the
// code body. Each port row is PORT_STEP tall so ELK gets sensible
// padding-free sizes. There are no inline pixel widths anywhere —
// CSS sizes everything from content, and JS just measures the result.

const PORT_STEP = 16;
const PORT_PAD  = 6;

function leafBody({ title, body, kindClass, inputs, outputs }) {
  const portRows = Math.max(inputs.length, outputs.length);
  const footerH  = portRows > 0 ? PORT_PAD * 2 + portRows * PORT_STEP : 0;
  return (
    <div className={`ng-node ${kindClass}${title ? '' : ' ng-node-notitle'}`}>
      {title && <div className="ng-node-title">{title}</div>}
      <div className="ng-node-body">{body}</div>
      {portRows > 0 && (
        <div className="ng-node-ports-footer" style={{ height: footerH }}>
          {inputs.map((name, i) => {
            const y = PORT_PAD + i * PORT_STEP + PORT_STEP / 2;
            return (
              <span key={`in-${i}`}>
                <Handle type="target" position={Position.Left}
                        id={`in-${i}`}
                        style={{ top: y }}
                        title={name} />
                <span className="ng-port-inline ng-port-in"
                      style={{ top: y - 6 }}>{name}</span>
              </span>
            );
          })}
          {outputs.map((name, i) => {
            const y = PORT_PAD + i * PORT_STEP + PORT_STEP / 2;
            return (
              <span key={`out-${i}`}>
                <Handle type="source" position={Position.Right}
                        id={`out-${i}`}
                        style={{ top: y }}
                        title={name} />
                <span className="ng-port-inline ng-port-out"
                      style={{ top: y - 6 }}>{name}</span>
              </span>
            );
          })}
        </div>
      )}
    </div>
  );
}

function AssignmentNode({ data }) {
  return leafBody({
    title: '',
    body: data.sourceText || '',
    kindClass: 'ng-node-assignment',
    inputs: data.inputs || [],
    outputs: data.outputs || [],
  });
}

function ExprStmtNode({ data }) {
  return leafBody({
    title: '',
    body: data.sourceText || '',
    kindClass: 'ng-node-exprstmt',
    inputs: data.inputs || [],
    outputs: [],
  });
}

function OpaqueNode({ data }) {
  return leafBody({
    title: kindLabel(data.kind),
    body: data.sourceText || '',
    kindClass: 'ng-node-opaque',
    inputs: data.inputs || [],
    outputs: data.outputs || [],
  });
}

/** Compound node for control-flow regions. The header is tagged with
 *  `data-region-header={id}` so the layout effect can read its
 *  measured height and feed it to ELK as the top inset. */
function RegionNode({ id, data }) {
  const inputs  = data.inputs  || [];
  const outputs = data.outputs || [];
  return (
    <div className={`ng-region ng-region-${data.kind.toLowerCase()}`}>
      <div className="ng-region-header" data-region-header={id}>
        <span className="ng-region-source">{data.sourceText || ''}</span>
      </div>
      {inputs.map((name, i) => {
        const y = 14 + i * PORT_STEP + PORT_STEP / 2;
        return (
          <span key={`rin-${i}`}>
            <Handle type="target" position={Position.Left}
                    id={`in-${i}`} style={{ top: y }} title={name} />
            <span className="ng-port-inline ng-port-in"
                  style={{ top: y - 6, left: 6 }}>{name}</span>
          </span>
        );
      })}
      {outputs.map((name, i) => {
        const y = 14 + i * PORT_STEP + PORT_STEP / 2;
        return (
          <span key={`rout-${i}`}>
            <Handle type="source" position={Position.Right}
                    id={`out-${i}`} style={{ top: y }} title={name} />
            <span className="ng-port-inline ng-port-out"
                  style={{ top: y - 6, right: 6 }}>{name}</span>
          </span>
        );
      })}
    </div>
  );
}

function JumpNode({ data }) {
  return (
    <div className={`ng-node ng-node-jump ng-node-jump-${data.kind.toLowerCase()}`}>
      <div className="ng-node-jump-label">{kindLabel(data.kind)}</div>
    </div>
  );
}

/** Phase-2c merge node: fans in writers from every branch of an
 *  if/switch/try into a single output. The hexagonal shape (clip-
 *  path chevron on each side) is the flowchart-style merge visual —
 *  no jargon labels (`φ`), the shape itself says "this is a join".
 *  Only the merged variable name is shown inside. */
function MergeNode({ data }) {
  const inputs  = data.inputs  || [];
  const outputs = data.outputs || [];
  // Grow the chip vertically when there are many incoming branches
  // so the input handles don't pile up on top of each other.
  const minH = Math.max(inputs.length, 1) * PORT_STEP + PORT_PAD * 2;
  return (
    <div className="ng-node-merge" style={{ minHeight: minH }}>
      <span className="ng-node-merge-name">{outputs[0] || data.sourceText || ''}</span>
      {inputs.map((name, i) => {
        const y = PORT_PAD + i * PORT_STEP + PORT_STEP / 2;
        return (
          <Handle key={`in-${i}`} type="target" position={Position.Left}
                  id={`in-${i}`} style={{ top: y }} title={name} />
        );
      })}
      {outputs.map((_, i) => (
        <Handle key={`out-${i}`} type="source" position={Position.Right}
                id={`out-${i}`} style={{ top: '50%' }} />
      ))}
    </div>
  );
}

const nodeTypes = {
  assignment: AssignmentNode,
  exprstmt:   ExprStmtNode,
  opaque:     OpaqueNode,
  region:     RegionNode,
  jump:       JumpNode,
  merge:      MergeNode,
};

function nodeTypeFor(kind) {
  if (kind === 'Assignment')   return 'assignment';
  if (kind === 'ExprStmt')     return 'exprstmt';
  if (isRegionKind(kind))      return 'region';
  if (isJumpKind(kind))        return 'jump';
  if (kind === 'Merge')        return 'merge';
  return 'opaque';
}

// ── Phase 1: graph IR → unmeasured React Flow nodes ─────────────────

function buildInitialNodes(graph) {
  if (!graph || !Array.isArray(graph.nodes)) return [];
  // Bucket by parentRegionId so parents come before children in the
  // array — React Flow requires parent-first ordering.
  const byParent = new Map();
  for (const n of graph.nodes) {
    const key = n.parentRegionId == null ? null : n.parentRegionId;
    if (!byParent.has(key)) byParent.set(key, []);
    byParent.get(key).push(n);
  }
  const out = [];
  function emit(parentId) {
    const kids = byParent.get(parentId) || [];
    for (const n of kids) {
      const node = {
        id: String(n.id),
        type: nodeTypeFor(n.kind),
        position: { x: 0, y: 0 },
        data: {
          kind:       n.kind,
          sourceText: n.sourceText || '',
          sourceLine: n.sourceLine,
          inputs:     n.inputs  || [],
          outputs:    n.outputs || [],
        },
      };
      if (n.parentRegionId != null) {
        node.parentNode = String(n.parentRegionId);
        node.extent     = 'parent';
      }
      // Regions need a placeholder size before ELK runs — otherwise
      // React Flow can't allocate space for children. Replaced after
      // layout with the ELK-computed size.
      if (isRegionKind(n.kind)) {
        node.style = { width: 320, height: 120 };
      }
      out.push(node);
      if (isRegionKind(n.kind)) emit(n.id);
    }
  }
  emit(null);
  return out;
}

function buildEdges(graph) {
  return (graph.edges || []).map((e, i) => ({
    id: `e${i}`,
    source: String(e.source.nodeId),
    target: String(e.target.nodeId),
    sourceHandle: `out-${e.source.portIndex}`,
    targetHandle: `in-${e.target.portIndex}`,
    label: e.varName,
    type: 'default',
    className: `ng-edge ng-edge-${(e.kind || 'Data').toLowerCase()}`,
  }));
}

// ── Phase 2: build ELK graph from MEASURED dimensions ───────────────

function buildElkGraph(graph, sizeById, headerHById) {
  const byParent = new Map();
  for (const n of graph.nodes) {
    const key = n.parentRegionId == null ? null : n.parentRegionId;
    if (!byParent.has(key)) byParent.set(key, []);
    byParent.get(key).push(n);
  }
  function subtree(parentId) {
    const kids = byParent.get(parentId) || [];
    return kids.map((n) => {
      const id = String(n.id);
      if (isRegionKind(n.kind)) {
        // Region: ELK auto-sizes from children + padding. Top inset
        // is the MEASURED header height (+ a few px breathing room).
        const headerH = headerHById[id] || 36;
        return {
          id,
          layoutOptions: {
            'elk.algorithm':                              'layered',
            'elk.direction':                              'RIGHT',
            'elk.padding':                                `[top=${headerH + 10},left=14,bottom=14,right=14]`,
            'elk.spacing.nodeNode':                       '18',
            'elk.layered.spacing.nodeNodeBetweenLayers':  '40',
          },
          children: subtree(n.id),
        };
      }
      // Leaf / jump: take the measured size verbatim from the DOM.
      // Fallback values are only used if measurement somehow missed
      // (shouldn't happen because useNodesInitialized gates this).
      const m = sizeById[id];
      return { id, width: m?.width ?? 200, height: m?.height ?? 60 };
    });
  }
  const elkEdges = (graph.edges || []).map((e, i) => ({
    id: `e${i}`,
    sources: [String(e.source.nodeId)],
    targets: [String(e.target.nodeId)],
  }));
  return {
    id: 'root',
    layoutOptions: {
      'elk.algorithm':                              'layered',
      'elk.direction':                              'RIGHT',
      'elk.spacing.nodeNode':                       '32',
      'elk.layered.spacing.nodeNodeBetweenLayers':  '70',
      'elk.hierarchyHandling':                      'INCLUDE_CHILDREN',
    },
    children: subtree(null),
    edges: elkEdges,
  };
}

/** Walk ELK result, return { id → {x, y, width, height} }. */
function collectElkLayout(elkResult) {
  const out = new Map();
  function walk(node) {
    if (node.id !== 'root') {
      out.set(node.id, {
        x:      node.x      ?? 0,
        y:      node.y      ?? 0,
        width:  node.width,
        height: node.height,
      });
    }
    for (const c of node.children || []) walk(c);
  }
  walk(elkResult);
  return out;
}

// ── Component ───────────────────────────────────────────────────────

function NumkitGraphViewInner({ source, engine }) {
  const [graph, setGraph]     = useState(null);
  const [error, setError]     = useState(null);
  const [nodes, setNodes]     = useState([]);
  const [edges, setEdges]     = useState([]);
  // `laidOut` = ELK has positioned everything for the CURRENT graph.
  // We keep the canvas hidden (opacity 0) until this is true so the
  // user never sees nodes stacked at (0, 0) during the measure pass.
  const [laidOut, setLaidOut] = useState(false);
  // Bumped on every graph change so a stale ELK promise can detect
  // it's no longer the active layout and bail out.
  const tokenRef = useRef(0);

  const nodesInitialized = useNodesInitialized();
  const reactFlow        = useReactFlow();

  // ── Source → graph IR ───────────────────────────────────────────
  useEffect(() => {
    if (!engine || typeof engine.buildScriptGraph !== 'function') {
      setError('engine.buildScriptGraph not available');
      setGraph(null);
      return;
    }
    if (!source || source.trim() === '') {
      setGraph({ nodes: [], edges: [] });
      setError(null);
      return;
    }
    const result = engine.buildScriptGraph(source);
    if (result && result.error) {
      setError(result.error);
      setGraph(null);
    } else {
      setGraph(result);
      setError(null);
    }
  }, [source, engine]);

  // ── Phase 1: graph → unmeasured nodes (rendered with opacity 0) ──
  useEffect(() => {
    tokenRef.current += 1;
    setLaidOut(false);
    if (!graph || !graph.nodes || graph.nodes.length === 0) {
      setNodes([]);
      setEdges([]);
      return;
    }
    setNodes(buildInitialNodes(graph));
    setEdges(buildEdges(graph));
  }, [graph]);

  // ── Phase 2: nodes measured → ELK layout → apply positions ───────
  useEffect(() => {
    if (!nodesInitialized || !graph || laidOut) return;
    if (!nodes.length) return;

    const token = tokenRef.current;

    // Collect MEASURED sizes from React Flow's internal store. Leaf
    // and jump nodes have correct DOM widths/heights here; regions
    // still carry the placeholder size from buildInitialNodes (their
    // real size comes back from ELK).
    const sizeById = {};
    for (const rf of reactFlow.getNodes()) {
      const kind = rf.data?.kind;
      if (!kind || isRegionKind(kind)) continue;  // regions sized by ELK
      if (rf.width != null && rf.height != null) {
        sizeById[rf.id] = { width: rf.width, height: rf.height };
      }
    }

    // Measure region headers directly from the DOM — these tell ELK
    // how much top padding to leave inside each compound. Using
    // offsetHeight keeps us in CSS pixels (zoom-independent).
    const headerHById = {};
    for (const n of graph.nodes) {
      if (!isRegionKind(n.kind)) continue;
      const el = document.querySelector(`[data-region-header="${n.id}"]`);
      if (el) headerHById[String(n.id)] = el.offsetHeight;
    }

    const elkInput = buildElkGraph(graph, sizeById, headerHById);
    elk.layout(elkInput).then((result) => {
      if (tokenRef.current !== token) return;  // a newer graph won
      const layoutById = collectElkLayout(result);
      setNodes((prev) => prev.map((n) => {
        const lay = layoutById.get(n.id);
        if (!lay) return n;
        const next = { ...n, position: { x: lay.x, y: lay.y } };
        // Regions: ELK computed their final size — apply it.
        // Leaves/jumps: keep their natural (DOM-driven) size, do not
        // override style.width/height.
        if (isRegionKind(n.data.kind)
         && lay.width != null && lay.height != null) {
          next.style = { ...(n.style || {}), width: lay.width, height: lay.height };
        }
        return next;
      }));
      setLaidOut(true);
    }).catch((err) => {
      if (tokenRef.current !== token) return;
      // eslint-disable-next-line no-console
      console.error('ELK layout failed:', err);
      setError(`Layout failed: ${err.message || err}`);
    });
  }, [nodesInitialized, graph, nodes, laidOut, reactFlow]);

  if (error) {
    return (
      <div className="numkit-graph-view ng-empty">
        <div className="ng-empty-title">Graph unavailable</div>
        <div className="ng-empty-msg">{error}</div>
      </div>
    );
  }
  if (!graph || !graph.nodes || graph.nodes.length === 0) {
    return (
      <div className="numkit-graph-view ng-empty">
        <div className="ng-empty-title">Empty graph</div>
        <div className="ng-empty-msg">
          Load a script in the editor to see its data-flow representation here.
        </div>
      </div>
    );
  }

  return (
    <div className="numkit-graph-view">
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
        style={{
          opacity: laidOut ? 1 : 0,
          transition: 'opacity 150ms ease-in',
        }}
      >
        <Background gap={16} size={1} color="var(--line-soft)" />
        <Controls showInteractive={false} />
        <MiniMap pannable zoomable
                 nodeColor={(n) => n.type === 'assignment' ? '#7fd99a'
                                 : n.type === 'exprstmt'   ? '#9b8cf2'
                                 : n.type === 'region'     ? '#5b6470'
                                 : n.type === 'jump'       ? '#d97c7c'
                                                           : '#888'}
                 style={{ background: 'var(--bg-2)' }} />
      </ReactFlow>
    </div>
  );
}

export default function NumkitGraphView(props) {
  return (
    <ReactFlowProvider>
      <NumkitGraphViewInner {...props} />
    </ReactFlowProvider>
  );
}
