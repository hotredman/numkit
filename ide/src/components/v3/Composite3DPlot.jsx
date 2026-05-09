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

/**
 * Pick a Three.js material constructor based on figure.lighting and
 * figure.material. Returns the material instance fully configured.
 *   lighting='flat'    → MeshLambertMaterial + flatShading=true
 *   lighting='gouraud' → MeshLambertMaterial (smooth, default)
 *   lighting='phong'   → MeshPhongMaterial   (adds specular)
 *   lighting='none'    → MeshBasicMaterial   (no shading at all)
 *   material='shiny'   → high specular / shininess (phong only)
 *   material='metal'   → mid specular, low ambient
 *   material='dull'    → no specular
 */
function buildMaterial(figure, opts) {
  const { vertexColors = true, color = 0xffffff,
          opacity = 1, doubleSide = true, flatHint = false } = opts || {};
  const lighting = figure.lighting || 'gouraud';
  const matPreset = figure.material || '';
  const transparent = opacity < 1;

  if (lighting === 'none') {
    return new THREE.MeshBasicMaterial({
      vertexColors, color: vertexColors ? 0xffffff : color,
      transparent, opacity,
      side: doubleSide ? THREE.DoubleSide : THREE.FrontSide,
    });
  }
  if (lighting === 'phong') {
    let specular = 0x111111, shininess = 30;
    if (matPreset === 'shiny')      { specular = 0x666666; shininess = 100; }
    else if (matPreset === 'metal') { specular = 0x444444; shininess = 25; }
    else if (matPreset === 'dull')  { specular = 0x000000; shininess = 1; }
    return new THREE.MeshPhongMaterial({
      vertexColors, color: vertexColors ? 0xffffff : color,
      transparent, opacity,
      side: doubleSide ? THREE.DoubleSide : THREE.FrontSide,
      specular, shininess,
      flatShading: flatHint,
    });
  }
  // 'flat' or 'gouraud' (default) → Lambert
  return new THREE.MeshLambertMaterial({
    vertexColors, color: vertexColors ? 0xffffff : color,
    transparent, opacity,
    side: doubleSide ? THREE.DoubleSide : THREE.FrontSide,
    flatShading: lighting === 'flat' || flatHint,
  });
}

/**
 * Build a face-shaded surface mesh from a regular grid (Xs[Nc],
 * Ys[Nr], Z[Nr][Nc]). Each (i, j) cell becomes two triangles whose
 * vertices carry per-vertex colors sampled from a viridis-like ramp
 * by Z. Lambert lighting comes from MeshLambertMaterial + the
 * directional light already in the scene.
 *
 * Cells with any non-finite corner are skipped so NaN data leaves
 * a "hole" in the surface instead of stretching a triangle through
 * garbage.
 */
function buildSurfaceMesh(grid, scl, bbox, figure) {
  const Xs = grid.Xs, Ys = grid.Ys, Z = grid.Z;
  const Nc = Xs.length, Nr = Ys.length;
  if (Nc < 2 || Nr < 2) return null;

  // Viridis-ish: blue → cyan → green → yellow → red ramp via HSL
  // (240° hue at zMin, 0° at zMax). Cheap and good enough — a real
  // LUT swap is a follow-up.
  const colorAt = (t) => {
    const h = (1 - Math.max(0, Math.min(1, t))) * 240 / 360;
    const c = new THREE.Color();
    c.setHSL(h, 0.6, 0.5);
    return c;
  };
  const zSpan = bbox.zMax - bbox.zMin;
  const norm = (v) => (zSpan > 0 ? (v - bbox.zMin) / zSpan : 0.5);

  const vertCount = Nr * Nc;
  const positions = new Float32Array(vertCount * 3);
  const colors    = new Float32Array(vertCount * 3);
  for (let r = 0; r < Nr; r++) {
    for (let c = 0; c < Nc; c++) {
      const idx = r * Nc + c;
      const xv = Xs[c], yv = Ys[r], zv = Z[r] ? Z[r][c] : NaN;
      const finite = Number.isFinite(xv) && Number.isFinite(yv)
                  && Number.isFinite(zv);
      if (finite) {
        const [X, Y, Zw] = toWorld(xv, yv, zv, scl);
        positions[idx * 3 + 0] = X;
        positions[idx * 3 + 1] = Y;
        positions[idx * 3 + 2] = Zw;
        const col = colorAt(norm(zv));
        colors[idx * 3 + 0] = col.r;
        colors[idx * 3 + 1] = col.g;
        colors[idx * 3 + 2] = col.b;
      } else {
        // Mark as NaN — used to skip triangles that touch this vertex.
        positions[idx * 3 + 0] = NaN;
        positions[idx * 3 + 1] = NaN;
        positions[idx * 3 + 2] = NaN;
        colors[idx * 3 + 0] = 0;
        colors[idx * 3 + 1] = 0;
        colors[idx * 3 + 2] = 0;
      }
    }
  }

  // Index buffer: two triangles per cell, skipped if any corner NaN.
  const indices = [];
  const isFinite3 = (i) => Number.isFinite(positions[i * 3])
                        && Number.isFinite(positions[i * 3 + 1])
                        && Number.isFinite(positions[i * 3 + 2]);
  for (let r = 0; r < Nr - 1; r++) {
    for (let c = 0; c < Nc - 1; c++) {
      const a = r * Nc + c;
      const b = a + 1;
      const cIdx = a + Nc;
      const dIdx = cIdx + 1;
      if (!isFinite3(a) || !isFinite3(b) || !isFinite3(cIdx) || !isFinite3(dIdx))
        continue;
      indices.push(a, b, dIdx, a, dIdx, cIdx);
    }
  }
  if (indices.length === 0) return null;

  // Replace any leftover NaN positions with origin so three doesn't
  // fold the bounding sphere; triangles touching them are already
  // excluded above.
  for (let i = 0; i < positions.length; i++) {
    if (!Number.isFinite(positions[i])) positions[i] = 0;
  }

  const geom = new THREE.BufferGeometry();
  geom.setAttribute('position', new THREE.BufferAttribute(positions, 3));
  geom.setAttribute('color',    new THREE.BufferAttribute(colors,    3));
  geom.setIndex(new THREE.BufferAttribute(new Uint32Array(indices), 1));
  geom.computeVertexNormals();

  const mat = buildMaterial(figure || {}, { vertexColors: true });
  return new THREE.Mesh(geom, mat);
}

/**
 * Build 3-D bars from a regular grid. Each Z(i, j) ≠ 0 becomes an
 * axis-aligned cuboid centred at (j+1, i+1) with height z. Single
 * indexed mesh — far cheaper than one BufferGeometry per bar. Per-
 * vertex colors driven by Z to match the colormap on surf.
 */
function buildBars3D(grid, scl, bbox, figure) {
  const Xs = grid.Xs, Ys = grid.Ys, Z = grid.Z;
  const Nc = Xs.length, Nr = Ys.length;
  if (Nc < 1 || Nr < 1) return null;

  const colorAt = (t) => {
    const h = (1 - Math.max(0, Math.min(1, t))) * 240 / 360;
    const c = new THREE.Color();
    c.setHSL(h, 0.6, 0.55);
    return c;
  };
  const zSpan = bbox.zMax - bbox.zMin;
  const norm = (v) => (zSpan > 0 ? (v - bbox.zMin) / zSpan : 0.5);

  // Bar half-width in DATA units. We pick 0.4 so neighbouring bars
  // (at data-step 1) leave a small gap.
  const half = 0.4;

  const positions = [];
  const colors = [];
  const indices = [];

  for (let r = 0; r < Nr; r++) {
    for (let c = 0; c < Nc; c++) {
      const zh = Z[r] ? Z[r][c] : NaN;
      if (!Number.isFinite(zh) || zh === 0) continue;
      const xc = Xs[c], yc = Ys[r];
      const x0 = xc - half, x1 = xc + half;
      const y0 = yc - half, y1 = yc + half;
      // 8 corners (data → world). Order: (x*, y*, z) bit pattern xyz.
      const corners = [
        [x0, y0, 0],   [x1, y0, 0],   [x1, y1, 0],   [x0, y1, 0],
        [x0, y0, zh],  [x1, y0, zh],  [x1, y1, zh],  [x0, y1, zh],
      ];
      const baseIdx = positions.length / 3;
      const col = colorAt(norm(zh));
      for (const [dx, dy, dz] of corners) {
        const [X, Y, Zw] = toWorld(dx, dy, dz, scl);
        positions.push(X, Y, Zw);
        colors.push(col.r, col.g, col.b);
      }
      // 6 faces × 2 triangles. Winding chosen so face normals point
      // outward; computeVertexNormals will average them at shared
      // verts but each cuboid corner is only shared inside its bar so
      // we get crisp lighting per-bar.
      const faces = [
        [0, 1, 5, 4],     // -y face (front when default cam)
        [1, 2, 6, 5],     // +x face
        [2, 3, 7, 6],     // +y face
        [3, 0, 4, 7],     // -x face
        [4, 5, 6, 7],     // top
        [3, 2, 1, 0],     // bottom (inward, hidden under bar)
      ];
      for (const [a, b, cc, d] of faces) {
        indices.push(baseIdx + a, baseIdx + b, baseIdx + cc);
        indices.push(baseIdx + a, baseIdx + cc, baseIdx + d);
      }
    }
  }
  if (indices.length === 0) return null;

  const geom = new THREE.BufferGeometry();
  geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(positions), 3));
  geom.setAttribute('color',    new THREE.BufferAttribute(new Float32Array(colors),    3));
  geom.setIndex(new THREE.BufferAttribute(new Uint32Array(indices), 1));
  geom.computeVertexNormals();
  const mat = buildMaterial(figure || {}, { vertexColors: true, flatHint: true });
  return new THREE.Mesh(geom, mat);
}

/**
 * Build per-row ribbons for waterfall. Each row r becomes a closed
 * polygon strip: top edge along Z(r, :), bottom along z=0, left/right
 * caps. Triangles in two strips per row.
 */
function buildWaterfall(grid, scl, bbox, figure) {
  const Xs = grid.Xs, Ys = grid.Ys, Z = grid.Z;
  const Nc = Xs.length, Nr = Ys.length;
  if (Nc < 2 || Nr < 1) return null;

  const colorAt = (t) => {
    const h = (1 - Math.max(0, Math.min(1, t))) * 240 / 360;
    const c = new THREE.Color();
    c.setHSL(h, 0.55, 0.5);
    return c;
  };
  const zSpan = bbox.zMax - bbox.zMin;
  const norm = (v) => (zSpan > 0 ? (v - bbox.zMin) / zSpan : 0.5);

  const positions = [];
  const colors = [];
  const indices = [];

  for (let r = 0; r < Nr; r++) {
    const yc = Ys[r];
    for (let c = 0; c < Nc; c++) {
      const zv = Z[r] && Number.isFinite(Z[r][c]) ? Z[r][c] : 0;
      // top vertex
      const [Xt, Yt, Zt] = toWorld(Xs[c], yc, zv, scl);
      positions.push(Xt, Yt, Zt);
      const col = colorAt(norm(zv));
      colors.push(col.r, col.g, col.b);
      // bottom vertex (z=0)
      const [Xb, Yb, Zb] = toWorld(Xs[c], yc, 0, scl);
      positions.push(Xb, Yb, Zb);
      colors.push(col.r * 0.4, col.g * 0.4, col.b * 0.4);
    }
    // Build the strip: for each (c, c+1), two triangles.
    const baseRow = r * Nc * 2;
    for (let c = 0; c < Nc - 1; c++) {
      const i0 = baseRow + c * 2;       // top c
      const i1 = baseRow + c * 2 + 1;   // bottom c
      const i2 = baseRow + (c + 1) * 2;
      const i3 = baseRow + (c + 1) * 2 + 1;
      indices.push(i0, i2, i3);
      indices.push(i0, i3, i1);
    }
  }
  if (indices.length === 0) return null;

  const geom = new THREE.BufferGeometry();
  geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(positions), 3));
  geom.setAttribute('color',    new THREE.BufferAttribute(new Float32Array(colors),    3));
  geom.setIndex(new THREE.BufferAttribute(new Uint32Array(indices), 1));
  geom.computeVertexNormals();
  const mat = buildMaterial(figure || {}, { vertexColors: true });
  return new THREE.Mesh(geom, mat);
}

/**
 * Build 3-D filled polygon(s). xRaw / yRaw / z come with NaN markers
 * between polygon groups (matching the 2-D polygon path); each
 * sub-polygon is triangulated as a fan from its first vertex.
 * Caller-provided color is applied uniformly.
 */
function buildPolygon3D(layer, scl, figure) {
  const xs = layer.xRaw, ys = layer.yRaw, zs = layer.z;
  const polys = [];
  let cur = [];
  const n = Math.min(xs.length, ys.length, zs.length);
  for (let i = 0; i < n; i++) {
    const fin = Number.isFinite(xs[i]) && Number.isFinite(ys[i]) && Number.isFinite(zs[i]);
    if (!fin) {
      if (cur.length >= 3) polys.push(cur);
      cur = [];
      continue;
    }
    cur.push(i);
  }
  if (cur.length >= 3) polys.push(cur);
  if (polys.length === 0) return null;

  const positions = [];
  const indices = [];
  for (const poly of polys) {
    const baseIdx = positions.length / 3;
    for (const i of poly) {
      const [X, Y, Z] = toWorld(xs[i], ys[i], zs[i], scl);
      positions.push(X, Y, Z);
    }
    // Fan triangulation: assumes convex polygon, which is the common
    // user-built case (rectangles, triangles, hand-typed quads).
    for (let k = 1; k < poly.length - 1; k++) {
      indices.push(baseIdx, baseIdx + k, baseIdx + k + 1);
    }
  }
  if (indices.length === 0) return null;

  const geom = new THREE.BufferGeometry();
  geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(positions), 3));
  geom.setIndex(new THREE.BufferAttribute(new Uint32Array(indices), 1));
  geom.computeVertexNormals();
  const mat = buildMaterial(figure || {}, {
    vertexColors: false,
    color: new THREE.Color(layer.color || '#9467bd'),
    opacity: Number.isFinite(layer.fillOpacity) ? layer.fillOpacity : 0.7,
  });
  return new THREE.Mesh(geom, mat);
}

/**
 * Build 3-D arrows for quiver3. Each arrow is a 2-vertex line from
 * (x, y, z) to (x+s·u, y+s·v, z+s·w). Cone-shaped arrowheads are a
 * follow-up; for now plain LineSegments give the right shape.
 */
function buildQuiver3D(layer, scl) {
  const xs = layer.xRaw, ys = layer.yRaw, zs = layer.z;
  const u = layer.u, v = layer.v, w = layer.w;
  const s = Number.isFinite(layer.scale) ? layer.scale : 1;
  const N = Math.min(xs.length, ys.length, zs.length, u.length, v.length, w.length);
  if (N === 0) return null;

  const positions = new Float32Array(N * 6);   // 2 vertices per arrow
  for (let i = 0; i < N; i++) {
    const xi = xs[i], yi = ys[i], zi = zs[i];
    const ui = u[i], vi = v[i], wi = w[i];
    if (!Number.isFinite(xi + yi + zi + ui + vi + wi)) {
      // Skip non-finite arrow — collapse to origin.
      for (let k = 0; k < 6; k++) positions[i * 6 + k] = 0;
      continue;
    }
    const [X0, Y0, Z0] = toWorld(xi, yi, zi, scl);
    const [X1, Y1, Z1] = toWorld(xi + s * ui, yi + s * vi, zi + s * wi, scl);
    positions[i * 6 + 0] = X0;
    positions[i * 6 + 1] = Y0;
    positions[i * 6 + 2] = Z0;
    positions[i * 6 + 3] = X1;
    positions[i * 6 + 4] = Y1;
    positions[i * 6 + 5] = Z1;
  }

  const geom = new THREE.BufferGeometry();
  geom.setAttribute('position', new THREE.BufferAttribute(positions, 3));
  const mat = new THREE.LineBasicMaterial({
    color: new THREE.Color(layer.color || '#9467bd'),
  });
  return new THREE.LineSegments(geom, mat);
}

/**
 * Build contour lines that ride on the Z-surface. Standard marching
 * squares per cell, but each segment carries the level Z so it sits
 * on top of the surface. levels = explicit array; otherwise n equally
 * spaced strictly inside (zMin, zMax).
 */
function buildContour3D(layer, scl, bbox) {
  const Xs = layer.surfaceGrid?.Xs || [];
  const Ys = layer.surfaceGrid?.Ys || [];
  const Z  = layer.surfaceGrid?.Z  || [];
  const Nc = Xs.length, Nr = Ys.length;
  if (Nc < 2 || Nr < 2) return null;

  let zmn = Infinity, zmx = -Infinity;
  for (let r = 0; r < Nr; r++) for (let c = 0; c < Nc; c++) {
    const v = Z[r] ? Z[r][c] : NaN;
    if (Number.isFinite(v)) { if (v < zmn) zmn = v; if (v > zmx) zmx = v; }
  }
  if (!Number.isFinite(zmn)) return null;

  let levels = layer.levels;
  if (!Array.isArray(levels) || levels.length === 0) {
    const n = layer.n || 10;
    const step = (zmx - zmn) / (n + 1);
    levels = [];
    for (let i = 1; i <= n; i++) levels.push(zmn + i * step);
  }

  const interp = (a, b, va, vb, L) => {
    if (Math.abs(vb - va) < 1e-15) return a;
    return a + (L - va) / (vb - va) * (b - a);
  };

  const colorAt = (t) => {
    const h = (1 - Math.max(0, Math.min(1, t))) * 240 / 360;
    const c = new THREE.Color();
    c.setHSL(h, 0.6, 0.5);
    return c;
  };
  const zSpan = zmx - zmn;
  const norm = (v) => (zSpan > 0 ? (v - zmn) / zSpan : 0.5);

  const positions = [];
  const colors = [];
  for (const L of levels) {
    const col = colorAt(norm(L));
    for (let r = 0; r + 1 < Nr; r++) {
      for (let c = 0; c + 1 < Nc; c++) {
        const vTL = Z[r][c], vTR = Z[r][c + 1];
        const vBL = Z[r + 1][c], vBR = Z[r + 1][c + 1];
        if (!Number.isFinite(vTL) || !Number.isFinite(vTR)
         || !Number.isFinite(vBL) || !Number.isFinite(vBR)) continue;
        let code = 0;
        if (vTL > L) code |= 1;
        if (vTR > L) code |= 2;
        if (vBR > L) code |= 4;
        if (vBL > L) code |= 8;
        if (code === 0 || code === 15) continue;

        const xL = Xs[c], xR = Xs[c + 1];
        const yT = Ys[r], yB = Ys[r + 1];
        const T  = [interp(xL, xR, vTL, vTR, L), yT, L];
        const RE = [xR, interp(yT, yB, vTR, vBR, L), L];
        const B  = [interp(xL, xR, vBL, vBR, L), yB, L];
        const LE = [xL, interp(yT, yB, vTL, vBL, L), L];
        const segs = [];
        switch (code) {
          case 1: case 14: segs.push([LE, T]); break;
          case 2: case 13: segs.push([T, RE]); break;
          case 3: case 12: segs.push([LE, RE]); break;
          case 4: case 11: segs.push([RE, B]); break;
          case 6: case 9:  segs.push([T, B]); break;
          case 7: case 8:  segs.push([LE, B]); break;
          case 5:  segs.push([LE, T], [RE, B]); break;
          case 10: segs.push([LE, B], [T, RE]); break;
        }
        for (const [a, b] of segs) {
          const [X0, Y0, Z0] = toWorld(a[0], a[1], a[2], scl);
          const [X1, Y1, Z1] = toWorld(b[0], b[1], b[2], scl);
          positions.push(X0, Y0, Z0, X1, Y1, Z1);
          colors.push(col.r, col.g, col.b, col.r, col.g, col.b);
        }
      }
    }
  }
  if (positions.length === 0) return null;

  const geom = new THREE.BufferGeometry();
  geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(positions), 3));
  geom.setAttribute('color',    new THREE.BufferAttribute(new Float32Array(colors),    3));
  const mat = new THREE.LineBasicMaterial({ vertexColors: true });
  return new THREE.LineSegments(geom, mat);
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
      renderer.render(scene, camera);
      css2d.render(scene, camera);
      frames++;
      if (frames % 30 === 0) setFrameCount(frames);
      raf = requestAnimationFrame(tick);
    };

    ctxRef.current = { renderer, css2d, scene, camera, controls,
                       layerGroup, axesGroup, camLight };
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

    // If view changed, snap camera to the new (az, el).
    if (Array.isArray(figure.view) && figure.view.length === 2) {
      const off = azElToCameraOffset(figure.view[0], figure.view[1], 4);
      c.camera.position.set(off.x, off.y, off.z);
      c.camera.lookAt(0, 0, 0);
      c.controls.update();
    }

    // Toggle the camlight per figure.camlight.
    const camPos = figure.camlight || '';
    if (camPos === 'left' || camPos === 'right' || camPos === 'headlight') {
      c.camLight.visible = true;
      c.camLight.userData.pos = camPos;
    } else {
      c.camLight.visible = false;
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
