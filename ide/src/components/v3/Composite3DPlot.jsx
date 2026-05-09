/**
 * Composite3DPlot.jsx — WebGL renderer for 3-D figures via three.js.
 *
 * Mounts a <canvas> and drives a perspective Camera + OrbitControls
 * (mouse-drag → orbit, wheel → dolly, shift+drag → pan). Each 3-D
 * dataset (plot3 / scatter3 / stem3 / surf / mesh) becomes a Line or
 * Points geometry in a shared scene, plus a unit-cube AxesHelper for
 * orientation.
 *
 * Public contract mirrors CompositePlot: { figure, width, height,
 * fontScale, interactive, engine }. viewport / setViewport are
 * accepted but ignored — 3-D doesn't pan/zoom in (x, y) terms; the
 * camera holds the equivalent state internally.
 *
 * MVP scope:
 *   • plot3 / scatter3 / stem3 (latter is plot3 + scatter3 datasets)
 *   • surf / mesh (rendered as wireframe lines from the existing two-
 *     polyline emit format — no Z-matrix passthrough yet)
 *   • view(az, el) initialises the camera from figure.view
 *   • mouse interaction
 *
 * Polish (later commits):
 *   • C++ wire change: surf/mesh emits a Z-matrix → face-shaded surf
 *     with MeshLambertMaterial + DirectionalLight + colormap on Z
 *   • bar3 / waterfall / fill3 with raw 3D coords (today they
 *     pre-project through cabinet on the C++ side and stay in SVG)
 *   • Z-axis tick labels via HTML overlay
 */
import { useEffect, useRef, useState } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';

// Default azimuth / elevation if `view(az, el)` isn't called. Matches
// MATLAB's default 3-D view.
const DEFAULT_AZ_DEG = -37.5;
const DEFAULT_EL_DEG = 30;

/**
 * Convert MATLAB-style (az, el) in degrees to a unit camera direction
 * in three.js's right-handed Y-up world. We map data Z to world Y so
 * the conventional "up" axis sits vertical; data X stays world X, data
 * Y becomes world -Z (so increasing data-Y points away from the
 * default camera view).
 */
function azElToCameraOffset(azDeg, elDeg, dist) {
  const az = (azDeg * Math.PI) / 180;
  const el = (elDeg * Math.PI) / 180;
  // Camera position in world coords: spherical from origin.
  // Y is up (data-Z); horizontal plane is X / -Z (data-X / data-Y).
  const cosEl = Math.cos(el);
  return {
    x: dist * cosEl * Math.sin(az),
    y: dist * Math.sin(el),
    z: dist * cosEl * Math.cos(az),
  };
}

/**
 * Map data-space (x, y, z) → world-space (X, Y, Z) given the bbox.
 * We normalise each axis to [-1, 1] inside a unit cube so the camera
 * doesn't need per-figure tuning. Returns Float32Array suitable for
 * THREE.BufferAttribute.
 */
function buildVertices(xs, ys, zs, bbox) {
  const n = Math.min(xs.length, ys.length, zs.length);
  const out = new Float32Array(n * 3);
  const sx = bbox.xMax > bbox.xMin ? 2 / (bbox.xMax - bbox.xMin) : 1;
  const sy = bbox.yMax > bbox.yMin ? 2 / (bbox.yMax - bbox.yMin) : 1;
  const sz = bbox.zMax > bbox.zMin ? 2 / (bbox.zMax - bbox.zMin) : 1;
  for (let i = 0; i < n; i++) {
    const x = (xs[i] - bbox.xMin) * sx - 1;
    // Data-Y → world-Z so the camera looks down +Y (the up axis).
    const z = (ys[i] - bbox.yMin) * sy - 1;
    const y = (zs[i] - bbox.zMin) * sz - 1;
    out[i * 3 + 0] = Number.isFinite(x) ? x : NaN;
    out[i * 3 + 1] = Number.isFinite(y) ? y : NaN;
    out[i * 3 + 2] = Number.isFinite(z) ? z : NaN;
  }
  return out;
}

/**
 * Compute the (x, y, z) bounding box across all 3-D layers. NaN-safe.
 * Empty layers return a degenerate [-1, 1] cube so the camera still
 * has a finite frame to look at.
 */
function computeBBox(layers) {
  let xMin = Infinity, xMax = -Infinity;
  let yMin = Infinity, yMax = -Infinity;
  let zMin = Infinity, zMax = -Infinity;
  for (const ly of layers) {
    if (!ly.x || !ly.y || !ly.z) continue;
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
  // Guard against degenerate (zero-extent) axes — pad them so the
  // unit-cube mapping doesn't NaN out.
  if (xMax - xMin < 1e-9) { xMax += 0.5; xMin -= 0.5; }
  if (yMax - yMin < 1e-9) { yMax += 0.5; yMin -= 0.5; }
  if (zMax - zMin < 1e-9) { zMax += 0.5; zMin -= 0.5; }
  return { xMin, xMax, yMin, yMax, zMin, zMax };
}

/**
 * Build a Three.js Line for a polyline-style layer (plot3 / surf
 * wireframe) handling NaN segment breaks: a NaN in x/y/z splits the
 * line so unrelated segments don't connect through the origin. Three's
 * Line draws gl.LINE_STRIP across consecutive indices, so we feed it
 * an indexed geometry where index buffer breaks at each NaN run.
 */
function buildLineSegments(positions, color) {
  // Split into runs of consecutive finite vertices.
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
  // Filter out NaN vertices; Points geometry treats them as "draw
  // huge weird artefacts" otherwise.
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

export default function Composite3DPlot({
  figure, width, height,
  fontScale = 1,
  interactive = true,
}) {
  const containerRef = useRef(null);
  const canvasRef = useRef(null);
  // Renderer / scene / camera / controls live in a single ref so the
  // useEffect cleanup can dispose them. We avoid storing them in
  // state to dodge React-driven re-renders on every camera tweak.
  const ctxRef = useRef(null);
  // Frame counter exposed for e2e tests — no other purpose.
  const [frameCount, setFrameCount] = useState(0);

  // Initial mount: build renderer + scene + camera + controls.
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return undefined;

    const renderer = new THREE.WebGLRenderer({
      canvas,
      antialias: true,
      alpha: true,
      preserveDrawingBuffer: process.env.NUMKIT_E2E === '1',
    });
    renderer.setPixelRatio(window.devicePixelRatio || 1);
    renderer.setClearColor(0x0d1117, 1);

    const scene = new THREE.Scene();

    const camera = new THREE.PerspectiveCamera(45, 1, 0.01, 100);
    // Default view from MATLAB-style (az, el).
    const initialView = Array.isArray(figure?.view) && figure.view.length === 2
      ? figure.view
      : [DEFAULT_AZ_DEG, DEFAULT_EL_DEG];
    const off = azElToCameraOffset(initialView[0], initialView[1], 4);
    camera.position.set(off.x, off.y, off.z);
    camera.lookAt(0, 0, 0);

    // Soft fill + key light so wireframes get a tiny depth cue from
    // future Lambert materials (face shading lands in a follow-up).
    scene.add(new THREE.AmbientLight(0xffffff, 0.45));
    const key = new THREE.DirectionalLight(0xffffff, 0.85);
    key.position.set(2, 3, 4);
    scene.add(key);

    // Axes helper: 1-unit RGB lines along world X / Y / Z. Sits inside
    // the [-1, 1] cube where data is normalised, so it always frames
    // the geometry.
    const axesGroup = new THREE.Group();
    const axesHelper = new THREE.AxesHelper(1.05);
    axesGroup.add(axesHelper);
    // Wire cube for the bounding-box outline (12 edges).
    const cubeGeom = new THREE.BoxGeometry(2, 2, 2);
    const cubeEdges = new THREE.EdgesGeometry(cubeGeom);
    const cubeLine = new THREE.LineSegments(
      cubeEdges,
      new THREE.LineBasicMaterial({ color: 0x444c56, transparent: true, opacity: 0.6 }));
    axesGroup.add(cubeLine);
    scene.add(axesGroup);

    const controls = new OrbitControls(camera, canvas);
    controls.enableDamping = true;
    controls.dampingFactor = 0.1;
    controls.enabled = !!interactive;

    // Layer group — wiped + rebuilt whenever `figure` changes.
    const layerGroup = new THREE.Group();
    scene.add(layerGroup);

    let raf = 0;
    let frames = 0;
    const tick = () => {
      controls.update();
      renderer.render(scene, camera);
      frames++;
      // Don't trigger React re-renders every frame — only every 30
      // (≈ 0.5 s @ 60 fps) so the test counter advances visibly
      // without flooding state.
      if (frames % 30 === 0) setFrameCount(frames);
      raf = requestAnimationFrame(tick);
    };

    ctxRef.current = { renderer, scene, camera, controls, layerGroup };
    raf = requestAnimationFrame(tick);

    // Mark the canvas so e2e tests can locate it.
    canvas.setAttribute('data-numkit-3d', '1');
    // Log once for the e2e mode-check.
    try {
      const gl = renderer.getContext();
      if (gl) console.log('[numkit-3d] gl context ok', gl.getParameter(gl.VERSION));
    } catch (e) { /* ignore */ }

    return () => {
      cancelAnimationFrame(raf);
      controls.dispose();
      // Dispose all geometries + materials on the scene.
      scene.traverse((obj) => {
        if (obj.geometry) obj.geometry.dispose();
        if (obj.material) {
          if (Array.isArray(obj.material)) obj.material.forEach((m) => m.dispose());
          else obj.material.dispose();
        }
      });
      renderer.dispose();
      ctxRef.current = null;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Resize → match canvas size to the prop dims.
  useEffect(() => {
    const c = ctxRef.current;
    if (!c || !width || !height) return;
    c.renderer.setSize(width, height, false);
    c.camera.aspect = width / height;
    c.camera.updateProjectionMatrix();
  }, [width, height]);

  // Figure data → rebuild scene layers. Runs whenever the figure
  // identity OR layer list changes.
  useEffect(() => {
    const c = ctxRef.current;
    if (!c || !figure) return;
    // Wipe previous layers.
    while (c.layerGroup.children.length) {
      const child = c.layerGroup.children[0];
      c.layerGroup.remove(child);
      child.traverse((obj) => {
        if (obj.geometry) obj.geometry.dispose();
        if (obj.material) {
          if (Array.isArray(obj.material)) obj.material.forEach((m) => m.dispose());
          else obj.material.dispose();
        }
      });
    }
    // For 3-D rendering use the raw (pre-cabinet) coords from the
    // adapter: xRaw / yRaw / z. Filter out layers without z — those
    // are 2-D-only types that snuck through (shouldn't happen for
    // composite3d figures, but keep it safe).
    const layers = (figure.layers || []).filter((ly) =>
      ly && ly.kind === 'series' && ly.xRaw && ly.yRaw && ly.z);
    if (layers.length === 0) return;

    const bbox = computeBBox(layers.map((ly) => ({
      x: ly.xRaw, y: ly.yRaw, z: ly.z,
    })));

    for (const ly of layers) {
      const positions = buildVertices(ly.xRaw, ly.yRaw, ly.z, bbox);
      const color = new THREE.Color(ly.color || '#1f77b4');
      const mode = ly.mode || 'line';
      if (mode === 'scatter') {
        const pts = buildPoints(positions, color, ly.size || 3);
        if (pts) c.layerGroup.add(pts);
      } else {
        // Default: line / stairs / etc. Treat as polyline (3-D wire).
        c.layerGroup.add(buildLineSegments(positions, color));
      }
    }
  }, [figure]);

  // Honour the interactive flag on toggle.
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
