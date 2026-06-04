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
import { forwardRef, useEffect, useImperativeHandle, useRef, useState } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { CSS2DRenderer } from 'three/examples/jsm/renderers/CSS2DRenderer.js';
import {
  azElToCameraOffset, fmtTick, computeBBox, computeScales,
  buildVertices, buildLineSegments, buildSurfaceMesh, buildBars3D,
  buildWaterfall, buildPolygon3D, buildQuiver3D, buildContour3D,
  buildPoints, buildAxesFrame, disposeTree, FACE_NORMALS, ALL_FACES,
} from './composite3d.helpers';

const DEFAULT_AZ_DEG = -37.5;
const DEFAULT_EL_DEG = 30;


/* ───────────── component ───────────── */

function Composite3DPlot({
  figure, width, height,
  fontScale = 1,
  interactive = true,
  // Grid toggles forwarded by FigureWindow's toolbar buttons. Both
  // accept booleans; falsy values fall back to the figure's own
  // grid / gridMinor strings ("on" / "off"). Same prop names as
  // CompositePlot uses for 2-D so the parent can pass one set.
  major,
  minor,
  // Visibility toggles from display ▾. All default true; FigureWindow
  // flips to false when the user un-ticks the corresponding row.
  showTitle  = true,
  showXLabel = true,
  showYLabel = true,
  showZLabel = true,
  viewport3d = null,         // optional override of figure.xlim/ylim/zlim
  onBBox = null,             // (bbox) => void — fired on each rebuild
}, ref) {
  const effectiveMajor = (typeof major === 'boolean') ? major : (figure?.grid === 'on');
  const effectiveMinor = (typeof minor === 'boolean') ? minor : (figure?.gridMinor === 'on');
  const containerRef = useRef(null);
  const canvasRef = useRef(null);
  const labelLayerRef = useRef(null);
  const ctxRef = useRef(null);
  const [frameCount, setFrameCount] = useState(0);
  const [tip, setTip] = useState(null);   // { x, y, z, screenX, screenY }

  // Initial mount.
  useEffect(() => {
    const canvas = canvasRef.current;
    const labelLayer = labelLayerRef.current;
    if (!canvas || !labelLayer) return undefined;

    const renderer = new THREE.WebGLRenderer({
      canvas,
      antialias: true,
      alpha: true,
      preserveDrawingBuffer: import.meta.env.VITE_NUMKIT_E2E === '1',
    });
    renderer.setPixelRatio(window.devicePixelRatio || 1);
    // Transparent canvas — the wrapper <div> below carries the actual
    // background through `var(--plot-bg)`, which the browser updates
    // automatically on theme switch. Setting a fixed clear-color here
    // would freeze the dark fallback onto preview cards even after the
    // user switched to a light theme.
    renderer.setClearColor(0x000000, 0);

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
    // Optional camlight — repositioned per-frame based on the camera
    // pose so it always lights the visible side. `null` means
    // "no extra cam-attached light".
    const camLight = new THREE.DirectionalLight(0xffffff, 0.9);
    camLight.visible = false;
    scene.add(camLight);

    const controls = new OrbitControls(camera, canvas);
    controls.enableDamping = true;
    controls.dampingFactor = 0.1;
    controls.enabled = !!interactive;

    // Raycaster for data-tip on hover. We test against the data layer
    // group only — axes / grid / cube don't show tooltips.
    const raycaster = new THREE.Raycaster();
    raycaster.params.Points = { threshold: 0.04 };  // tolerate ~4% pick radius
    raycaster.params.Line   = { threshold: 0.02 };
    const ndc = new THREE.Vector2();
    let bbox = null;             // captured at figure-build time below
    let scl  = null;             // ditto

    const onMove = (ev) => {
      const rect = canvas.getBoundingClientRect();
      ndc.x =  ((ev.clientX - rect.left) / rect.width)  * 2 - 1;
      ndc.y = -((ev.clientY - rect.top)  / rect.height) * 2 + 1;
      raycaster.setFromCamera(ndc, camera);
      const hits = raycaster.intersectObjects(layerGroup.children, true);
      if (hits.length === 0 || !scl) {
        setTip(null);
        return;
      }
      // Closest hit; convert world coords back to data coords.
      const p = hits[0].point;
      // Inverse of toWorld: world (X, Y, Z) → data (x, y, z).
      const dataX = p.x / scl.sx + scl.ox;
      const dataY = -(p.z) / scl.sy + scl.oy;
      const dataZ = p.y / scl.sz + scl.oz;
      setTip({
        x: dataX, y: dataY, z: dataZ,
        screenX: ev.clientX - rect.left,
        screenY: ev.clientY - rect.top,
      });
    };
    const onLeave = () => setTip(null);
    canvas.addEventListener('mousemove', onMove);
    canvas.addEventListener('mouseleave', onLeave);

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
    const tmpV = new THREE.Vector3();
    const tick = () => {
      controls.update();
      // Reposition the camlight (if active) so it tracks the camera.
      // headlight  → light at camera position, target world origin.
      // left/right → offset to one side of the camera direction.
      if (camLight.visible) {
        const camPos = camera.position;
        if (camLight.userData.pos === 'headlight') {
          camLight.position.copy(camPos);
        } else {
          // Compute camera right vector for L/R offsets.
          camera.getWorldDirection(tmpV);
          const right = new THREE.Vector3().crossVectors(tmpV, camera.up).normalize();
          const sign = camLight.userData.pos === 'left' ? -1 : 1;
          camLight.position.copy(camPos).addScaledVector(right, sign * 1.2);
        }
        camLight.target.position.set(0, 0, 0);
        camLight.target.updateMatrixWorld();
      }
      // Per-frame back-face grid visibility. dot(camera.position,
      // outward face normal) < 0 ⇒ camera is on the opposite side ⇒
      // face is behind the data ⇒ grid drawn there doesn't overdraw.
      // BUG #39b fix: was hard-coded to three faces at build time.
      const ctx = ctxRef.current;
      if (ctx && (ctx.gridMajorByFace || ctx.gridMinorByFace)) {
        const cam = camera.position;
        const wantMajor = !!ctx.wantMajorRef?.current;
        const wantMinor = !!ctx.wantMinorRef?.current;
        for (const face of ALL_FACES) {
          const n = FACE_NORMALS[face];
          const dot = cam.x * n[0] + cam.y * n[1] + cam.z * n[2];
          const isBack = dot < 0;
          const major = ctx.gridMajorByFace && ctx.gridMajorByFace[face];
          const minor = ctx.gridMinorByFace && ctx.gridMinorByFace[face];
          if (major) major.visible = wantMajor && isBack;
          if (minor) minor.visible = wantMinor && isBack;
        }
      }
      renderer.render(scene, camera);
      css2d.render(scene, camera);
      frames++;
      if (frames % 30 === 0) setFrameCount(frames);
      raf = requestAnimationFrame(tick);
    };

    ctxRef.current = { renderer, css2d, scene, camera, controls,
                       layerGroup, axesGroup, camLight,
                       setBbox: (b) => { bbox = b; },
                       setScl:  (s) => { scl  = s; },
                       gridMajorByFace: {}, gridMinorByFace: {},
                       // Refs read by the tick loop; kept on ctx so the
                       // grid-toggle effect updates without retriggering
                       // a full scene rebuild (BUG #39a fix).
                       wantMajorRef: { current: false },
                       wantMinorRef: { current: false } };
    raf = requestAnimationFrame(tick);

    canvas.setAttribute('data-numkit-3d', '1');
    // Test inspection hook — exposes the live three.js context so
    // Playwright specs can read camera.position / count visible grid
    // faces. The cleanup on unmount drops it; minimal leak risk.
    canvas.__numkit3dCtx = ctxRef;

    return () => {
      cancelAnimationFrame(raf);
      canvas.removeEventListener('mousemove', onMove);
      canvas.removeEventListener('mouseleave', onLeave);
      controls.dispose();
      disposeTree(scene);
      renderer.dispose();
      try { delete canvas.__numkit3dCtx; } catch (e) { /* ignore */ }
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

  // Track the last `figure.view` we applied to the camera so a rebuild
  // doesn't re-apply the same value and clobber a user orbit. Compared
  // by JSON to handle [az, el] tuples cheaply (BUG #39a).
  const lastViewRef = useRef(null);

  // Figure data → rebuild data-layer group AND axes-frame group.
  // NB: grid toggles do NOT trigger this — they live in their own
  // effect below that flips visibility via refs without rebuilding.
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
    // Drop refs to the destroyed face groups so the tick loop's
    // visibility update no-ops until the new ones land below.
    c.gridMajorByFace = {};
    c.gridMinorByFace = {};

    const layers = (figure.layers || []).filter((ly) =>
      ly && ly.kind === 'series' && ly.xRaw && ly.yRaw && ly.z);
    if (layers.length === 0) return;

    // viewport3d (from FigureWindow's X/Y/Z inputs) wins over the
    // figure's own xlim/ylim/zlim. Each axis falls back independently
    // so the user can override only Z while leaving X/Y at data
    // extent, etc.
    const lims = {
      xlim: viewport3d?.x ?? figure.xlim,
      ylim: viewport3d?.y ?? figure.ylim,
      zlim: viewport3d?.z ?? figure.zlim,
    };
    const bbox = computeBBox(
      layers.map((ly) => ({ x: ly.xRaw, y: ly.yRaw, z: ly.z })),
      lims
    );
    if (typeof onBBox === 'function') onBBox(bbox);
    const scl = computeScales(bbox, figure.axisMode || '');
    // Hand the (bbox, scl) to the raycaster closure so it can map
    // world-space hits back to data coords for the tooltip.
    if (c.setBbox) c.setBbox(bbox);
    if (c.setScl)  c.setScl(scl);
    // Cache for the imperative getBBox() handle.
    c.bbox = bbox;
    c.scl  = scl;

    // Axes frame. Build all six grid faces unconditionally — the tick
    // loop picks back-faces per-frame. effectiveMajor/Minor are read
    // from the refs the grid-toggle effect maintains; this effect
    // doesn't depend on them, so the frame survives grid toggles.
    const { group: axesFrame, axisLabels, gridMajorByFace, gridMinorByFace }
      = buildAxesFrame(bbox, scl, {
          showBox: true, fontScale,
          xLabel: figure.xLabel || '',
          yLabel: figure.yLabel || '',
          zLabel: figure.zLabel || '',
        });
    c.axesGroup.add(axesFrame);
    c.axisLabels = axisLabels;
    c.gridMajorByFace = gridMajorByFace;
    c.gridMinorByFace = gridMinorByFace;

    // Data layers.
    for (const ly of layers) {
      const mode = ly.mode || 'line';
      if (mode === 'surface' && ly.surfaceGrid) {
        const mesh = buildSurfaceMesh(ly.surfaceGrid, scl, bbox, figure);
        if (mesh) c.layerGroup.add(mesh);
        continue;
      }
      if (mode === 'bar3' && ly.surfaceGrid) {
        const mesh = buildBars3D(ly.surfaceGrid, scl, bbox, figure);
        if (mesh) c.layerGroup.add(mesh);
        continue;
      }
      if (mode === 'waterfall' && ly.surfaceGrid) {
        const mesh = buildWaterfall(ly.surfaceGrid, scl, bbox, figure);
        if (mesh) c.layerGroup.add(mesh);
        continue;
      }
      if (mode === 'polygon3d') {
        const mesh = buildPolygon3D(ly, scl, figure);
        if (mesh) c.layerGroup.add(mesh);
        continue;
      }
      if (mode === 'quiver3') {
        const arrows = buildQuiver3D(ly, scl);
        if (arrows) c.layerGroup.add(arrows);
        continue;
      }
      if (mode === 'contour3' && ly.surfaceGrid) {
        const lines = buildContour3D(ly, scl, bbox);
        if (lines) c.layerGroup.add(lines);
        continue;
      }
      const positions = buildVertices(ly.xRaw, ly.yRaw, ly.z, scl);
      const color = new THREE.Color(ly.color || '#1f77b4');
      if (mode === 'scatter') {
        const pts = buildPoints(positions, color, ly.size || 3);
        if (pts) c.layerGroup.add(pts);
      } else {
        c.layerGroup.add(buildLineSegments(positions, color));
      }
    }

    // Apply figure.view to the camera ONLY if it changed since the
    // last rebuild. Otherwise we'd clobber the user's orbit on every
    // figure prop tick (BUG #39a fix). lastViewRef stores a JSON
    // serialisation so [az, el] tuples compare by value.
    if (Array.isArray(figure.view) && figure.view.length === 2) {
      const sig = `${figure.view[0]},${figure.view[1]}`;
      if (sig !== lastViewRef.current) {
        const off = azElToCameraOffset(figure.view[0], figure.view[1], 4);
        c.camera.position.set(off.x, off.y, off.z);
        c.camera.lookAt(0, 0, 0);
        c.controls.update();
        lastViewRef.current = sig;
      }
    }

    // Toggle the camlight per figure.camlight.
    const camPos = figure.camlight || '';
    if (camPos === 'left' || camPos === 'right' || camPos === 'headlight') {
      c.camLight.visible = true;
      c.camLight.userData.pos = camPos;
    } else {
      c.camLight.visible = false;
    }

    // OrbitControls per-axis toggles. '' = default (all enabled).
    c.controls.enableRotate = figure.rotate3d !== 'off';
    c.controls.enablePan    = figure.pan3d    !== 'off';
    c.controls.enableZoom   = figure.zoom3d   !== 'off';
  }, [figure, fontScale, viewport3d]);

  // Grid major/minor toggle — touches only refs that the tick loop
  // reads next frame. No scene rebuild, no camera reset (BUG #39a).
  useEffect(() => {
    const c = ctxRef.current;
    if (!c) return;
    if (c.wantMajorRef) c.wantMajorRef.current = !!effectiveMajor;
    if (c.wantMinorRef) c.wantMinorRef.current = !!effectiveMinor;
  }, [effectiveMajor, effectiveMinor]);

  // Axis-name label visibility — flip CSS2DObject .visible directly.
  // Cheap (no rebuild). The objects are tagged in buildAxesFrame and
  // collected into ctx.axisLabels. Missing labels (script never set
  // xlabel/etc) are simply absent from axisLabels so we no-op.
  useEffect(() => {
    const c = ctxRef.current;
    if (!c || !c.axisLabels) return;
    if (c.axisLabels.x) c.axisLabels.x.visible = !!showXLabel;
    if (c.axisLabels.y) c.axisLabels.y.visible = !!showYLabel;
    if (c.axisLabels.z) c.axisLabels.z.visible = !!showZLabel;
  }, [showXLabel, showYLabel, showZLabel, frameCount]);

  useEffect(() => {
    const c = ctxRef.current;
    if (c && c.controls) c.controls.enabled = !!interactive;
  }, [interactive]);

  // Imperative handle exposed to FigureWindow for the toolbar's Fit
  // menu, X/Y/Z inputs, and Save/Export popups. Everything reads
  // straight off ctxRef so callers always see the live state.
  useImperativeHandle(ref, () => ({
    /** Current data-space bbox (after computeBBox). null until first
     *  figure rebuild runs. */
    getBBox: () => ctxRef.current?.bbox || null,

    /** Live canvas element (raw DOM). Callers compute pixel dims off
     *  it for high-DPI export. Returns null pre-mount. */
    getCanvas: () => canvasRef.current || null,

    /** Current CSS pixel size of the canvas — what the user sees. The
     *  drawing-buffer size may differ when device pixel ratio != 1.
     *  Used by exportPngPrint to derive the right scale factor for a
     *  target physical-mm width. */
    getCanvasCssSize: () => {
      const cv = canvasRef.current;
      if (!cv) return null;
      return { width: cv.clientWidth || cv.width, height: cv.clientHeight || cv.height };
    },

    /** Render once at `scale` and return a PNG data URL. Used by the
     *  FigureWindow Save/Export menu instead of the SVG pipeline (3-D
     *  geometry lives in canvas).
     *
     *  scale > 1 grows the renderer's drawing buffer to scale × current
     *  CSS size, renders one frame at that resolution, snapshots, and
     *  restores the original size. The CSS size of the canvas element
     *  itself doesn't change — the user's modal layout is untouched.
     */
    getCanvasDataURL: (scale = 1) => {
      const c = ctxRef.current;
      const canvas = canvasRef.current;
      if (!c || !canvas) return null;
      const s = Math.max(1, Number(scale) || 1);
      if (s === 1) {
        // Common path — just snapshot the live buffer.
        c.renderer.render(c.scene, c.camera);
        c.css2d.render(c.scene, c.camera);
        return canvas.toDataURL('image/png');
      }
      // Higher-resolution path: grow the drawing buffer, render once,
      // capture, restore. setSize(w, h, false) leaves the CSS size
      // alone (the third arg is `updateStyle`); we change only the
      // backing pixel count + pixel ratio so the captured PNG carries
      // s× linear pixels.
      const cssW = canvas.clientWidth || canvas.width;
      const cssH = canvas.clientHeight || canvas.height;
      const prevPR = c.renderer.getPixelRatio();
      try {
        c.renderer.setPixelRatio((window.devicePixelRatio || 1) * s);
        // setSize keeps the same CSS coords; we just flag updateStyle=false.
        c.renderer.setSize(cssW, cssH, false);
        c.renderer.render(c.scene, c.camera);
        c.css2d.render(c.scene, c.camera);
        return canvas.toDataURL('image/png');
      } finally {
        c.renderer.setPixelRatio(prevPR);
        c.renderer.setSize(cssW, cssH, false);
        c.renderer.render(c.scene, c.camera);
      }
    },

    /** Pull (name, x[], y[], z[]) per layer for CSV / TSV / JSON
     *  export. Surface layers expand the matrix into a flat list. */
    getCsvData: () => {
      const out = [];
      for (const ly of (figure?.layers || [])) {
        if (!ly || ly.kind !== 'series') continue;
        const xr = ly.xRaw, yr = ly.yRaw, zr = ly.z;
        if (!xr || !yr) continue;
        out.push({
          name: ly.name || 'series',
          x: Array.from(xr),
          y: Array.from(yr),
          z: Array.isArray(zr) ? Array.from(zr) : null,
        });
      }
      return out;
    },

    /** Snap camera to (az, el) degrees — same code path as
     *  view(az, el) on figure mount. */
    setView: (az, el) => {
      const c = ctxRef.current;
      if (!c) return;
      const off = azElToCameraOffset(az, el, 4);
      c.camera.position.set(off.x, off.y, off.z);
      c.camera.lookAt(0, 0, 0);
      c.controls.update();
    },
  }), [figure]);

  return (
    <div ref={containerRef}
         style={{
           // Wrapper carries the actual figure background through a CSS
           // variable so the WebGL canvas can stay transparent. Browser
           // re-applies the color on theme switch — no imperative
           // refresh needed in the renderer.
           position: 'relative', width, height,
           background: 'var(--plot-bg, var(--bg-1, #0d1117))',
         }}>
      <canvas ref={canvasRef}
              style={{
                display: 'block', width, height,
                background: 'transparent',
              }}
              data-numkit-3d-frames={frameCount} />
      {/* CSS2D overlay: HTML labels positioned in 3-D space by three. */}
      <div ref={labelLayerRef}
           style={{
             position: 'absolute', top: 0, left: 0,
             width: '100%', height: '100%',
             pointerEvents: 'none', overflow: 'hidden',
           }} />
      {showTitle && figure?.title && (
        <div style={{
          position: 'absolute', top: 8, left: 0, right: 0,
          textAlign: 'center', fontSize: 12 * fontScale,
          color: 'var(--plot-text-strong)', pointerEvents: 'none',
        }}>{figure.title}</div>
      )}
      {/* PNG export — small button bottom-right, only when interactive
          (hidden in preview cards which set interactive=false). */}
      {interactive && (
        <button
          type="button"
          onClick={() => {
            const canvas = canvasRef.current;
            if (!canvas) return;
            // Force a fresh render to populate the drawing buffer in
            // case preserveDrawingBuffer is off — call render(scene,
            // camera) once before grabbing dataURL.
            const c = ctxRef.current;
            if (c) c.renderer.render(c.scene, c.camera);
            const url = canvas.toDataURL('image/png');
            const a = document.createElement('a');
            a.href = url;
            a.download = `figure_${figure?.id || '3d'}.png`;
            a.click();
          }}
          style={{
            position: 'absolute',
            right: 8, bottom: 8,
            padding: '3px 8px', fontSize: 10,
            background: 'var(--bg-3, #2d333b)',
            color: 'var(--fg-1, #d0d4dc)',
            border: '1px solid var(--line, #444c56)',
            borderRadius: 3, cursor: 'pointer',
            opacity: 0.7,
          }}
          onMouseEnter={(e) => { e.target.style.opacity = '1'; }}
          onMouseLeave={(e) => { e.target.style.opacity = '0.7'; }}
        >PNG</button>
      )}

      {tip && (
        <div style={{
          position: 'absolute',
          left: Math.min(tip.screenX + 12, (width || 320) - 140),
          top:  Math.max(8, tip.screenY - 60),
          padding: '4px 8px',
          background: 'var(--plot-tip-bg, rgba(20,24,30,0.92))',
          color: 'var(--plot-tip-text, #d4d4f0)',
          border: '1px solid var(--plot-cross, #6e7681)',
          borderRadius: 3,
          fontFamily: 'monospace', fontSize: 10 * fontScale,
          pointerEvents: 'none',
          whiteSpace: 'nowrap',
        }}>
          x = {fmtTick(tip.x)}<br/>
          y = {fmtTick(tip.y)}<br/>
          z = {fmtTick(tip.z)}
        </div>
      )}
    </div>
  );
}

// forwardRef so FigureWindow can grab the imperative handle (getBBox,
// getCanvasDataURL, getCsvData, setView) without lifting state out of
// this component.
export default forwardRef(Composite3DPlot);
