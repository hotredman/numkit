/**
 * NumkitGraphView — script-graph visualizer (Phase 1b, read-only).
 *
 * Takes a .m source string, runs it through the WASM lowering pass
 * (`engine.buildScriptGraph`), then renders the resulting NodeGraph
 * IR via React Flow. Layout is computed with dagre (LR direction,
 * compound when regions land in Phase 2).
 *
 * Custom node types (one per NodeKind):
 *   • AssignmentNode  — LHS-name pill on top, RHS text, input handles
 *     on the left (one per inputs[]), output handles on the right
 *     (one per outputs[]).
 *   • ExprStmtNode    — same layout but outputs side is empty (no LHS).
 *   • OpaqueNode      — Phase-1 placeholder for control-flow / jumps /
 *     decls (rendered with a muted style so they're visibly distinct).
 *
 * Phase-2 will add IfRegion / ForRegion / WhileRegion / etc. as
 * compound nodes via React Flow's `parentNode` + `extent: 'parent'`.
 *
 * Props:
 *   source : string  — the .m text to visualize. Empty → empty graph.
 *   engine : object  — must expose buildScriptGraph(text) → graph JSON
 *                      (or { error: '...' }).
 */

import { useEffect, useMemo, useRef, useState } from 'react';
import ReactFlow, {
  Background,
  Controls,
  MiniMap,
  ReactFlowProvider,
  Handle,
  Position,
} from 'reactflow';
import dagre from '@dagrejs/dagre';
import 'reactflow/dist/style.css';

// ── Custom node renderers ───────────────────────────────────────────

/** Header + body + ports footer shared by all node kinds. `title`
 *  may be empty — in that case the header row is dropped so terminal
 *  statements (plot, clear, …) read as a clean source line without a
 *  redundant bullet placeholder.
 *
 *  Ports footer:
 *    Each input is one row: handle dot ON the left edge of the card,
 *    variable name as a tiny label JUST INSIDE the edge.
 *    Outputs mirror on the right edge.
 *    Rows stack at PORT_STEP (16 px) so handle Y aligns with label Y.
 *    Footer height = max(inputs, outputs) × PORT_STEP + a top/bottom
 *    breathing strip, so the card auto-grows for many ports. */
const PORT_STEP = 16;
const PORT_PAD  = 6;  // breathing room above the first port row

function nodeBody({ title, body, kindClass, inputs, outputs }) {
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
  return nodeBody({
    title: data.outputs?.join(', ') || '?',
    body: data.sourceText || '',
    kindClass: 'ng-node-assignment',
    inputs: data.inputs || [],
    outputs: data.outputs || [],
  });
}

function ExprStmtNode({ data }) {
  // No LHS → no title row. Source line speaks for itself
  // (`plot(y)`, `clear`, `disp(x)`).
  return nodeBody({
    title: '',
    body: data.sourceText || '',
    kindClass: 'ng-node-exprstmt',
    inputs: data.inputs || [],
    outputs: [],
  });
}

function OpaqueNode({ data }) {
  // Phase-1 stubs: GlobalDecl, PersistentDecl, IfRegion, etc. Keep
  // the kind tag as title so the user can tell it's a placeholder.
  return nodeBody({
    title: data.kind,
    body: data.sourceText || '',
    kindClass: 'ng-node-opaque',
    inputs: data.inputs || [],
    outputs: data.outputs || [],
  });
}

const nodeTypes = {
  assignment: AssignmentNode,
  exprstmt:   ExprStmtNode,
  opaque:     OpaqueNode,
};

// ── Layout (dagre, LR) ──────────────────────────────────────────────

const NODE_W = 220;
const NODE_H = 80;

function layoutNodes(rawNodes, rawEdges) {
  const g = new dagre.graphlib.Graph();
  g.setGraph({ rankdir: 'LR', nodesep: 24, ranksep: 60, marginx: 12, marginy: 12 });
  g.setDefaultEdgeLabel(() => ({}));

  for (const n of rawNodes) g.setNode(n.id, { width: NODE_W, height: NODE_H });
  for (const e of rawEdges) g.setEdge(e.sourceId, e.targetId);
  dagre.layout(g);

  return rawNodes.map((n) => {
    const p = g.node(n.id);
    return { ...n, position: { x: p.x - NODE_W / 2, y: p.y - NODE_H / 2 } };
  });
}

// ── Graph JSON → React Flow nodes/edges ─────────────────────────────

/** Bucket a NodeKind string into one of our React-Flow custom types. */
function nodeTypeFor(kind) {
  if (kind === 'Assignment') return 'assignment';
  if (kind === 'ExprStmt')   return 'exprstmt';
  return 'opaque';
}

function buildFlow(graph) {
  if (!graph || !Array.isArray(graph.nodes)) {
    return { nodes: [], edges: [] };
  }
  const rfNodes = graph.nodes.map((n) => ({
    id: String(n.id),
    type: nodeTypeFor(n.kind),
    position: { x: 0, y: 0 },  // overwritten by dagre below
    data: {
      kind: n.kind,
      sourceText: n.sourceText,
      sourceLine: n.sourceLine,
      inputs:  n.inputs  || [],
      outputs: n.outputs || [],
    },
  }));
  const rfEdges = (graph.edges || []).map((e, i) => ({
    id: `e${i}`,
    source: String(e.source.nodeId),
    target: String(e.target.nodeId),
    sourceHandle: `out-${e.source.portIndex}`,
    targetHandle: `in-${e.target.portIndex}`,
    label: e.varName,
    type: 'default',
    className: `ng-edge ng-edge-${(e.kind || 'Data').toLowerCase()}`,
  }));
  const positioned = layoutNodes(
    rfNodes.map((n) => ({ id: n.id })),
    rfEdges.map((e) => ({ sourceId: e.source, targetId: e.target })),
  );
  // Merge positions back into rfNodes.
  const posMap = new Map(positioned.map((p) => [p.id, p.position]));
  return {
    nodes: rfNodes.map((n) => ({ ...n, position: posMap.get(n.id) || { x: 0, y: 0 } })),
    edges: rfEdges,
  };
}

// ── Component ───────────────────────────────────────────────────────

function NumkitGraphViewInner({ source, engine }) {
  const [graph, setGraph]   = useState(null);
  const [error, setError]   = useState(null);
  // Bump on every source-string change to force a fresh layout pass.
  const buildIdRef = useRef(0);

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
    buildIdRef.current += 1;
  }, [source, engine]);

  const flow = useMemo(() => buildFlow(graph), [graph]);

  if (error) {
    return (
      <div className="numkit-graph-view ng-empty">
        <div className="ng-empty-title">Graph unavailable</div>
        <div className="ng-empty-msg">{error}</div>
      </div>
    );
  }
  if (!graph || (graph.nodes && graph.nodes.length === 0)) {
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
        nodes={flow.nodes}
        edges={flow.edges}
        nodeTypes={nodeTypes}
        defaultEdgeOptions={{ animated: false }}
        fitView
        proOptions={{ hideAttribution: true }}
        nodesDraggable={false}
        nodesConnectable={false}
        elementsSelectable={true}
      >
        <Background gap={16} size={1} color="var(--line-soft)" />
        <Controls showInteractive={false} />
        <MiniMap pannable zoomable
                 nodeColor={(n) => n.type === 'assignment' ? '#7fd99a'
                                : n.type === 'exprstmt'   ? '#9b8cf2'
                                                          : '#888'}
                 style={{ background: 'var(--bg-2)' }} />
      </ReactFlow>
    </div>
  );
}

export default function NumkitGraphView(props) {
  // React Flow needs its Provider in the tree for hooks (zoom, fitView).
  return (
    <ReactFlowProvider>
      <NumkitGraphViewInner {...props} />
    </ReactFlowProvider>
  );
}
