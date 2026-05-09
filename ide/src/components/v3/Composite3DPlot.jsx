/**
 * Composite3DPlot.jsx — WebGL renderer for 3-D figures via three.js.
 *
 * Renders plot3 / scatter3 / stem3 / surf / mesh inside a perspective
 * camera with mouse-driven OrbitControls (drag → orbit, wheel → dolly,
 * shift+drag → pan). Around the data sits a tick-labelled axes box
 * with grid lines on the back faces — the standard MATLAB-style frame.
 *
 * Public contract mirrors CompositePlot: { figure, width, height,
 * fontScale, interactive, engine }. viewport / setViewport are
 * accepted but ignored — 3-D doesn't pan/zoom in (x, y) terms; the
 * camera holds the equivalent state internally.
 *
 * Coordinate convention:
 *   data-X → world-X
 *   data-Y → world-Z   (negated so positive data-Y points away from
 *                       the default camera azimuth)
 *   data-Z → world-Y   (the up axis under MATLAB's default view)
 *
 * Axis equal / vis3d:
 *   default mode normalises each axis to [-1, 1] independently — best
 *   for readability when ranges differ wildly.
 *   axis equal / axis vis3d apply a single scale = 2 / max(range) and
 *   centre the data, so 1 data-unit = 1 world-unit on every axis.
 */
import { useEffect, useRef, useState } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { CSS2DRenderer, CSS2DObject } from 'three/examples/jsm/renderers/CSS2DRenderer.js';

const DEFAULT_AZ_DEG = -37.5;
const DEFAULT_EL_DEG = 30;

/* ───────────── helpers (pure) ───────────── */

function azElToCameraOffset(azDeg, elDeg, dist) {
  const az = (azDeg * Math.PI) / 180;
  const el = (elDeg * Math.PI) / 180;
  const cosEl = Math.cos(el);
  return {
    x: dist * cosEl * Math.sin(az),
    y: dist * Math.sin(el),
    z: dist * cosEl * Math.cos(az),
  };
}

/** "Nice" tick generator — same algorithm as CompositePlot's. */
function niceTicks(min, max, target = 6) {
  const range = max - min;
  if (!Number.isFinite(range) || range <= 0) return [min];
  const rough = range / target;
  const pow = Math.pow(10, Math.floor(Math.log10(rough)));
  const norm = rough / pow;
  const step = norm < 1.5 ? pow : norm < 3 ? 2 * pow : norm < 7 ? 5 * pow : 10 * pow;
  const start = Math.ceil(min / step) * step;
  const arr = [];
  for (let v = start; v <= max + step * 1e-6; v += step) arr.push(+v.toFixed(12));
  return arr;
}

function fmtTick(v) {
  if (!Number.isFinite(v)) return '';
  if (v === 0) return '0';
  const a = Math.abs(v);
  if (a >= 1e5 || a < 1e-3) return v.toExponential(1);
  if (Number.isInteger(v)) return String(v);
  return v.toFixed(Math.max(0, 3 - Math.floor(Math.log10(a))));
}

/** Compute the data-space bounding box for 3-D layers. */
function computeBBox(layers, lims) {
  let xMin = Infinity, xMax = -Infinity;
  let yMin = Infinity, yMax = -Infinity;
  let zMin = Infinity, zMax = -Infinity;
  for (const ly of layers) {
    if (!ly || !ly.x || !ly.y || !ly.z) continue;
    const n = Math.min(ly.x.length, ly.y.length, ly.z.length);
    for (let i = 0; i < n; i++) {
      const x = ly.x[i], y = ly.y[i], z = ly.z[i];
      if (Number.isFinite(x)) { if (x < xMin) xMin = x; if (x > xMax) xMax = x; }
      if (Number.isFinite(y)) { if (y < yMin) yMin = y; if (y > yMax) yMax = y; }
      if (Number.isFinite(z)) { if (z < zMin) zMin = z; if (z > zMax) zMax = z; }
    }
  }
  if (!Number.isFinite(xMin)) { xMin = -1; xMax = 1; }
  if (!Number.isFinite(yMin)) { yMin = -1; yMax = 1; }
  if (!Number.isFinite(zMin)) { zMin = -1; zMax = 1; }
  if (xMax - xMin < 1e-9) { xMax += 0.5; xMin -= 0.5; }
  if (yMax - yMin < 1e-9) { yMax += 0.5; yMin -= 0.5; }
  if (zMax - zMin < 1e-9) { zMax += 0.5; zMin -= 0.5; }
  // User-set lims override the data extent.
  if (lims?.xlim) { xMin = lims.xlim[0]; xMax = lims.xlim[1]; }
  if (lims?.ylim) { yMin = lims.ylim[0]; yMax = lims.ylim[1]; }
  if (lims?.zlim) { zMin = lims.zlim[0]; zMax = lims.zlim[1]; }
  return { xMin, xMax, yMin, yMax, zMin, zMax };
}

/**
 * Compute per-axis world-space scale from a bbox + axisMode.
 *   default → each axis independently normalised to [-1, 1].
 *   equal/vis3d → single scale = 2 / max-range, plot centred.
 * Returns { sx, sy, sz, ox, oy, oz } such that
 *   worldX = (dataX - ox) * sx,  ditto Y/Z.
 * The cube edges always sit at world ±1.
 */
function computeScales(bbox, axisMode) {
  const dx = bbox.xMax - bbox.xMin;
  const dy = bbox.yMax - bbox.yMin;
  const dz = bbox.zMax - bbox.zMin;
  if (axisMode === 'equal' || axisMode === 'vis3d') {
    const m = Math.max(dx, dy, dz);
    const s = m > 0 ? 2 / m : 1;
    return {
      sx: s, sy: s, sz: s,
      ox: (bbox.xMin + bbox.xMax) / 2,
      oy: (bbox.yMin + bbox.yMax) / 2,
      oz: (bbox.zMin + bbox.zMax) / 2,
    };
  }
  return {
    sx: dx > 0 ? 2 / dx : 1,
    sy: dy > 0 ? 2 / dy : 1,
    sz: dz > 0 ? 2 / dz : 1,
    ox: (bbox.xMin + bbox.xMax) / 2,
    oy: (bbox.yMin + bbox.yMax) / 2,
    oz: (bbox.zMin + bbox.zMax) / 2,
  };
}

/** Map data (x, y, z) to world (X, Y, Z). World-Y is up = data-Z. */
function toWorld(x, y, z, scl) {
  return [
    (x - scl.ox) * scl.sx,
    (z - scl.oz) * scl.sz,
    -(y - scl.oy) * scl.sy,
  ];
}

/** Per-vertex world-coord packed Float32Array for a 1-D layer. */
function buildVertices(xs, ys, zs, scl) {
  const n = Math.min(xs.length, ys.length, zs.length);
  const out = new Float32Array(n * 3);
  for (let i = 0; i < n; i++) {
    const xi = xs[i], yi = ys[i], zi = zs[i];
    if (!Number.isFinite(xi) || !Number.isFinite(yi) || !Number.isFinite(zi)) {
      out[i * 3 + 0] = NaN;
      out[i * 3 + 1] = NaN;
      out[i * 3 + 2] = NaN;
      continue;
    }
    const [X, Y, Z] = toWorld(xi, yi, zi, scl);
    out[i * 3 + 0] = X;
    out[i * 3 + 1] = Y;
    out[i * 3 + 2] = Z;
  }
  return out;
}

function buildLineSegments(positions, color) {
  const runs = [];
  let cur = [];
  for (let i = 0; i < positions.length; i += 3) {
    const ok = Number.isFinite(positions[i])
            && Number.isFinite(positions[i + 1])
            && Number.isFinite(positions[i + 2]);
    if (!ok) { if (cur.length) { runs.push(cur); cur = []; } continue; }
    cur.push(i / 3);
  }
  if (cur.length) runs.push(cur);
  const group = new THREE.Group();
  for (const run of runs) {
    if (run.length < 2) continue;
    const pts = new Float32Array(run.length * 3);
    for (let k = 0; k < run.length; k++) {
      const idx = run[k] * 3;
      pts[k * 3 + 0] = positions[idx + 0];
      pts[k * 3 + 1] = positions[idx + 1];
      pts[k * 3 + 2] = positions[idx + 2];
    }
    const geom = new THREE.BufferGeometry();
    geom.setAttribute('position', new THREE.BufferAttribute(pts, 3));
    const mat = new THREE.LineBasicMaterial({ color, linewidth: 1.5 });
    group.add(new THREE.Line(geom, mat));
  }
  return group;
}

function buildPoints(positions, color, size) {
  const finite = [];
  for (let i = 0; i < positions.length; i += 3) {
    if (Number.isFinite(positions[i])
     && Number.isFinite(positions[i + 1])
     && Number.isFinite(positions[i + 2])) {
      finite.push(positions[i], positions[i + 1], positions[i + 2]);
    }
  }
  if (finite.length === 0) return null;
  const geom = new THREE.BufferGeometry();
  geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(finite), 3));
  const mat = new THREE.PointsMaterial({ color, size: size * 0.02, sizeAttenuation: true });
  return new THREE.Points(geom, mat);
}

/**
 * Build the full axes frame: cube edges, grid lines on three back
 * faces, tick mark lines, and CSS2D label objects. Returns a Group +
 * an array of CSS2DObjects to attach to the scene. Caller is
 * responsible for disposing geometries when this group is replaced.
 *
 * Tick labels live on the data-X / data-Y / data-Z axes that meet at
 * the (xMin, yMin, zMin) corner — a convention close enough to MATLAB
 * to feel familiar (we don't currently re-pick the visible corner as
 * the camera orbits; that's a follow-up).
 */
function buildAxesFrame(bbox, scl, opts) {
  const { showGrid = true, showBox = true, fontScale = 1,
          xLabel = '', yLabel = '', zLabel = '' } = opts || {};

  const group = new THREE.Group();
  group.name = 'axes-frame';
  const labels = [];   // CSS2DObjects collected here, attached to group

  // Cube edges at world ±1 — drawn last so it overdraws grid lines.
  if (showBox) {
    const cubeGeom = new THREE.BoxGeometry(2, 2, 2);
    const cubeEdges = new THREE.EdgesGeometry(cubeGeom);
    cubeGeom.dispose();
    const cubeLine = new THREE.LineSegments(
      cubeEdges,
      new THREE.LineBasicMaterial({
        color: 0x6e7681, transparent: true, opacity: 0.7,
      }));
    group.add(cubeLine);
  }

  const xTicks = niceTicks(bbox.xMin, bbox.xMax, 6);
  const yTicks = niceTicks(bbox.yMin, bbox.yMax, 6);
  const zTicks = niceTicks(bbox.zMin, bbox.zMax, 6);

  const tickW = (v, axis) => {
    if (axis === 'x') return (v - scl.ox) * scl.sx;
    if (axis === 'y') return -(v - scl.oy) * scl.sy;
    return (v - scl.oz) * scl.sz;
  };

  // Grid lines on three "back" faces. Without knowing camera azimuth
  // up-front we draw the grid on the faces opposite the default
  // camera (data-Y positive face = world-Z = -1; data-X positive
  // face = world-X = +1; data-Z negative = world-Y = -1). Cheap and
  // good enough; a follow-up will reposition them per-frame.
  if (showGrid) {
    const gridMat = new THREE.LineBasicMaterial({
      color: 0x484f58, transparent: true, opacity: 0.4,
    });
    // X-Y plane at zMin (world-Y = -1).
    for (const xv of xTicks) {
      const wx = tickW(xv, 'x');
      const g = new THREE.BufferGeometry();
      g.setAttribute('position', new THREE.BufferAttribute(new Float32Array([
        wx, -1, -1,  wx, -1, 1,
      ]), 3));
      group.add(new THREE.LineSegments(g, gridMat));
    }
    for (const yv of yTicks) {
      const wz = tickW(yv, 'y');
      const g = new THREE.BufferGeometry();
      g.setAttribute('position', new THREE.BufferAttribute(new Float32Array([
        -1, -1, wz,  1, -1, wz,
      ]), 3));
      group.add(new THREE.LineSegments(g, gridMat));
    }
    // X-Z plane at yMax (world-Z = -1, the back face).
    for (const xv of xTicks) {
      const wx = tickW(xv, 'x');
      const g = new THREE.BufferGeometry();
      g.setAttribute('position', new THREE.BufferAttribute(new Float32Array([
        wx, -1, -1,  wx, 1, -1,
      ]), 3));
      group.add(new THREE.LineSegments(g, gridMat));
    }
    for (const zv of zTicks) {
      const wy = tickW(zv, 'z');
      const g = new THREE.BufferGeometry();
      g.setAttribute('position', new THREE.BufferAttribute(new Float32Array([
        -1, wy, -1,  1, wy, -1,
      ]), 3));
      group.add(new THREE.LineSegments(g, gridMat));
    }
    // Y-Z plane at xMin (world-X = -1).
    for (const yv of yTicks) {
      const wz = tickW(yv, 'y');
      const g = new THREE.BufferGeometry();
      g.setAttribute('position', new THREE.BufferAttribute(new Float32Array([
        -1, -1, wz,  -1, 1, wz,
      ]), 3));
      group.add(new THREE.LineSegments(g, gridMat));
    }
    for (const zv of zTicks) {
      const wy = tickW(zv, 'z');
      const g = new THREE.BufferGeometry();
      g.setAttribute('position', new THREE.BufferAttribute(new Float32Array([
        -1, wy, -1,  -1, wy, 1,
      ]), 3));
      group.add(new THREE.LineSegments(g, gridMat));
    }
  }

  // Tick labels on the front-bottom edge for X, side-bottom for Y,
  // front-side for Z.
  const makeLabel = (text, x, y, z) => {
    if (!text) return null;
    const div = document.createElement('div');
    div.textContent = text;
    div.style.cssText = `font-family: monospace; font-size: ${10 * fontScale}px; ` +
                        `color: var(--plot-text, #d4d4f0); ` +
                        `pointer-events: none; opacity: 0.85; ` +
                        `text-shadow: 0 0 3px var(--plot-bg, #0d1117);`;
    const obj = new CSS2DObject(div);
    obj.position.set(x, y, z);
    return obj;
  };

  for (const xv of xTicks) {
    const wx = tickW(xv, 'x');
    const obj = makeLabel(fmtTick(xv), wx, -1.05, 1.05);
    if (obj) { group.add(obj); labels.push(obj); }
  }
  for (const yv of yTicks) {
    const wz = tickW(yv, 'y');
    const obj = makeLabel(fmtTick(yv), 1.05, -1.05, wz);
    if (obj) { group.add(obj); labels.push(obj); }
  }
  for (const zv of zTicks) {
    const wy = tickW(zv, 'z');
    const obj = makeLabel(fmtTick(zv), -1.1, wy, 1.05);
    if (obj) { group.add(obj); labels.push(obj); }
  }

  // Axis name labels at the midpoint of each axis.
  if (xLabel) {
    const obj = makeLabel(xLabel, 0, -1.2, 1.2);
    if (obj) {
      obj.element.style.fontSize = `${12 * fontScale}px`;
      obj.element.style.fontWeight = '600';
      group.add(obj); labels.push(obj);
    }
  }
  if (yLabel) {
    const obj = makeLabel(yLabel, 1.2, -1.2, 0);
    if (obj) {
      obj.element.style.fontSize = `${12 * fontScale}px`;
      obj.element.style.fontWeight = '600';
      group.add(obj); labels.push(obj);
    }
  }
  if (zLabel) {
    const obj = makeLabel(zLabel, -1.25, 0, 1.2);
    if (obj) {
      obj.element.style.fontSize = `${12 * fontScale}px`;
      obj.element.style.fontWeight = '600';
      group.add(obj); labels.push(obj);
    }
  }

  return { group, labels };
}

/** Recursively dispose every disposable in a subtree. */
function disposeTree(root) {
  root.traverse((obj) => {
    if (obj.geometry) obj.geometry.dispose();
    if (obj.material) {
      if (Array.isArray(obj.material)) obj.material.forEach((m) => m.dispose());
      else obj.material.dispose();
    }
    // CSS2DObject: detach the DOM element.
    if (obj.element && obj.element.parentNode) {
      obj.element.parentNode.removeChild(obj.element);
    }
  });
}

/* ───────────── component ───────────── */

export default function Composite3DPlot({
  figure, width, height,
  fontScale = 1,
  interactive = true,
}) {
  const containerRef = useRef(null);
  const canvasRef = useRef(null);
  const labelLayerRef = useRef(null);
  const ctxRef = useRef(null);
  const [frameCount, setFrameCount] = useState(0);

  // Initial mount.
  useEffect(() => {
    const canvas = canvasRef.current;
    const labelLayer = labelLayerRef.current;
    if (!canvas || !labelLayer) return undefined;

    const renderer = new THREE.WebGLRenderer({
      canvas,
      antialias: true,
      alpha: true,
      preserveDrawingBuffer: process.env.NUMKIT_E2E === '1',
    });
    renderer.setPixelRatio(window.devicePixelRatio || 1);
    renderer.setClearColor(0x0d1117, 1);

    // CSS2D renderer for HTML overlays (tick labels, axis labels).
    const css2d = new CSS2DRenderer({ element: labelLayer });
    css2d.setSize(width || 320, height || 240);

    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(45, 1, 0.01, 100);
    const initialView = Array.isArray(figure?.view) && figure.view.length === 2
      ? figure.view
      : [DEFAULT_AZ_DEG, DEFAULT_EL_DEG];
    const off = azElToCameraOffset(initialView[0], initialView[1], 4);
    camera.position.set(off.x, off.y, off.z);
    camera.lookAt(0, 0, 0);

    scene.add(new THREE.AmbientLight(0xffffff, 0.45));
    const key = new THREE.DirectionalLight(0xffffff, 0.85);
    key.position.set(2, 3, 4);
    scene.add(key);

    const controls = new OrbitControls(camera, canvas);
    controls.enableDamping = true;
    controls.dampingFactor = 0.1;
    controls.enabled = !!interactive;

    // Two top-level groups: data (rebuilt per figure) + axes (rebuilt
    // per figure too, since ticks depend on bbox).
    const layerGroup = new THREE.Group();
    layerGroup.name = 'data-layers';
    scene.add(layerGroup);
    const axesGroup = new THREE.Group();
    axesGroup.name = 'axes';
    scene.add(axesGroup);

    let raf = 0;
    let frames = 0;
    const tick = () => {
      controls.update();
      renderer.render(scene, camera);
      css2d.render(scene, camera);
      frames++;
      if (frames % 30 === 0) setFrameCount(frames);
      raf = requestAnimationFrame(tick);
    };

    ctxRef.current = { renderer, css2d, scene, camera, controls,
                       layerGroup, axesGroup };
    raf = requestAnimationFrame(tick);

    canvas.setAttribute('data-numkit-3d', '1');
    try {
      const gl = renderer.getContext();
      if (gl) console.log('[numkit-3d] gl context ok', gl.getParameter(gl.VERSION));
    } catch (e) { /* ignore */ }

    return () => {
      cancelAnimationFrame(raf);
      controls.dispose();
      disposeTree(scene);
      renderer.dispose();
      ctxRef.current = null;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Resize.
  useEffect(() => {
    const c = ctxRef.current;
    if (!c || !width || !height) return;
    c.renderer.setSize(width, height, false);
    c.css2d.setSize(width, height);
    c.camera.aspect = width / height;
    c.camera.updateProjectionMatrix();
  }, [width, height]);

  // Figure data → rebuild data-layer group AND axes-frame group.
  useEffect(() => {
    const c = ctxRef.current;
    if (!c || !figure) return;

    // Wipe old layers.
    while (c.layerGroup.children.length) {
      const child = c.layerGroup.children[0];
      c.layerGroup.remove(child);
      disposeTree(child);
    }
    while (c.axesGroup.children.length) {
      const child = c.axesGroup.children[0];
      c.axesGroup.remove(child);
      disposeTree(child);
    }

    const layers = (figure.layers || []).filter((ly) =>
      ly && ly.kind === 'series' && ly.xRaw && ly.yRaw && ly.z);
    if (layers.length === 0) return;

    const bbox = computeBBox(
      layers.map((ly) => ({ x: ly.xRaw, y: ly.yRaw, z: ly.z })),
      { xlim: figure.xlim, ylim: figure.ylim, zlim: figure.zlim }
    );
    const scl = computeScales(bbox, figure.axisMode || '');

    // Axes frame.
    const showGrid = figure.grid !== '' && figure.grid !== 'off';
    const { group: axesFrame } = buildAxesFrame(bbox, scl, {
      showGrid, showBox: true, fontScale,
      xLabel: figure.xLabel || '',
      yLabel: figure.yLabel || '',
      zLabel: figure.zLabel || '',
    });
    c.axesGroup.add(axesFrame);

    // Data layers.
    for (const ly of layers) {
      const positions = buildVertices(ly.xRaw, ly.yRaw, ly.z, scl);
      const color = new THREE.Color(ly.color || '#1f77b4');
      const mode = ly.mode || 'line';
      if (mode === 'scatter') {
        const pts = buildPoints(positions, color, ly.size || 3);
        if (pts) c.layerGroup.add(pts);
      } else {
        c.layerGroup.add(buildLineSegments(positions, color));
      }
    }

    // If view changed, snap camera to the new (az, el).
    if (Array.isArray(figure.view) && figure.view.length === 2) {
      const off = azElToCameraOffset(figure.view[0], figure.view[1], 4);
      c.camera.position.set(off.x, off.y, off.z);
      c.camera.lookAt(0, 0, 0);
      c.controls.update();
    }
  }, [figure, fontScale]);

  useEffect(() => {
    const c = ctxRef.current;
    if (c && c.controls) c.controls.enabled = !!interactive;
  }, [interactive]);

  return (
    <div ref={containerRef}
         style={{ position: 'relative', width, height, background: 'var(--bg-1)' }}>
      <canvas ref={canvasRef}
              style={{ display: 'block', width, height }}
              data-numkit-3d-frames={frameCount} />
      {/* CSS2D overlay: HTML labels positioned in 3-D space by three. */}
      <div ref={labelLayerRef}
           style={{
             position: 'absolute', top: 0, left: 0,
             width: '100%', height: '100%',
             pointerEvents: 'none', overflow: 'hidden',
           }} />
      {figure?.title && (
        <div style={{
          position: 'absolute', top: 8, left: 0, right: 0,
          textAlign: 'center', fontSize: 12 * fontScale,
          color: 'var(--plot-text-strong)', pointerEvents: 'none',
        }}>{figure.title}</div>
      )}
    </div>
  );
}
