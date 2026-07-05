// @vitest-environment jsdom
import { describe, it, expect, afterEach, vi } from 'vitest';
import { render, cleanup, waitFor } from '@testing-library/react';

// jsdom has no ResizeObserver.
globalThis.ResizeObserver = globalThis.ResizeObserver
  || class { observe() {} unobserve() {} disconnect() {} };

// Shared spy for the store's updateNodeDimensions — the component drives it
// directly to work around React Flow never auto-measuring the AST nodes
// (see the re-measure effect in NumkitASTView). Hoisted so the vi.mock
// factory (also hoisted) can close over it.
const { updateNodeDimensionsSpy } = vi.hoisted(() => ({ updateNodeDimensionsSpy: vi.fn() }));

// Mock React Flow. jsdom does no real layout, so React Flow's own
// node-measurement (useNodesInitialized) could never fire; force it TRUE
// here so we can drive + assert the ELK-layout pipeline deterministically.
// The mock renders each node as a .react-flow__node div (with data-id, as
// the real renderer does) and applies the container `style` so we can read
// the laidOut → opacity gate.
vi.mock('reactflow', async () => {
  const React = await import('react');
  const RF = ({ children, nodes = [], style }) =>
    React.createElement(
      'div', { className: 'react-flow', style },
      nodes.map((n) => React.createElement('div', {
        key: n.id, className: 'react-flow__node', 'data-id': String(n.id),
        'data-x': String(n.position?.x ?? ''),
        'data-y': String(n.position?.y ?? ''),
      })),
      children,
    );
  const Null = () => null;
  return {
    __esModule: true,
    default: RF,
    Background: Null, Controls: Null, MiniMap: Null, Handle: Null,
    Position: { Top: 'top', Bottom: 'bottom', Left: 'left', Right: 'right' },
    ReactFlowProvider: ({ children }) => children,
    useNodesInitialized: () => true,
    useReactFlow: () => ({
      getNodes: () => [],
      getEdges: () => [],
      fitView: () => {},
      setCenter: () => {},
      getZoom: () => 1,
      getViewport: () => ({ x: 0, y: 0, zoom: 1 }),
      setViewport: () => {},
      project: (p) => p,
      screenToFlowPosition: (p) => p,
    }),
    useStoreApi: () => ({
      getState: () => ({
        domNode: globalThis.document,
        updateNodeDimensions: updateNodeDimensionsSpy,
      }),
    }),
  };
});
vi.mock('reactflow/dist/style.css', () => ({}));

// Mock ELK: lay each node out on a diagonal, synchronously-resolved.
vi.mock('elkjs/lib/elk.bundled.js', () => ({
  default: class {
    layout(g) {
      return Promise.resolve({
        ...g,
        children: (g.children || []).map((c, i) => ({
          ...c, x: i * 120, y: i * 40, width: 100, height: 30,
        })),
      });
    }
  },
}));

import NumkitASTView from './NumkitASTView';

afterEach(() => { cleanup(); updateNodeDimensionsSpy.mockClear(); });

const MOCK_AST = {
  type: 'Block', line: 1, col: 1, children: [
    { type: 'Assignment', line: 1, col: 1, children: [
      { type: 'Identifier', line: 1, col: 1 },
      { type: 'NumberLiteral', line: 1, col: 5 },
    ] },
    { type: 'Assignment', line: 2, col: 1, children: [
      { type: 'Identifier', line: 2, col: 1 },
      { type: 'BinaryOp', line: 2, col: 5, children: [
        { type: 'Identifier', line: 2, col: 5 },
        { type: 'NumberLiteral', line: 2, col: 9 },
      ] },
    ] },
  ],
};
const engine = { buildAST: () => MOCK_AST };

describe('NumkitASTView — layout pipeline', () => {
  it('renders nodes then reveals the tree (opacity 1) once measured + laid out', async () => {
    const { container } = render(
      <NumkitASTView source={'a=1;\nb=a+2;'} engine={engine} cursorLine={1} onNavigate={() => {}} />,
    );
    // Phase 1 — unmeasured nodes are in the DOM.
    await waitFor(() =>
      expect(container.querySelectorAll('.react-flow__node').length).toBeGreaterThan(0));
    // Phase 2 — after ELK, the opacity gate opens (laidOut === true).
    await waitFor(() => {
      const rf = container.querySelector('.react-flow');
      expect(rf?.style.opacity).toBe('1');
    }, { timeout: 2000 });
  });

  // Regression guard for the "blank AST pane" bug: React Flow never
  // auto-measures these static nodes (its ResizeObserver fires once, before
  // the container is recorded, then never again), so the component must force
  // updateNodeDimensions itself — otherwise useNodesInitialized() stays false
  // forever and the pane renders empty. If the re-measure effect is removed,
  // this fails.
  it('forces React Flow to (re)measure the rendered nodes', async () => {
    render(
      <NumkitASTView source={'a=1;\nb=a+2;'} engine={engine} cursorLine={1} onNavigate={() => {}} />,
    );
    await waitFor(() => expect(updateNodeDimensionsSpy).toHaveBeenCalled(), { timeout: 2000 });
    const lastCall = updateNodeDimensionsSpy.mock.calls.at(-1)[0];
    expect(Array.isArray(lastCall)).toBe(true);
    expect(lastCall.length).toBeGreaterThan(0);
    expect(lastCall[0]).toMatchObject({ forceUpdate: true });
  });
});
