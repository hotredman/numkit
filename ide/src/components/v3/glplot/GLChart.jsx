import { useEffect, useRef } from 'react';
import { createGL, resizeToDisplay, glViewportRect } from './glcontext';
import { GLPlotRenderer } from './GLPlotRenderer';

// Canvas overlay that draws the heavy line layers on WebGL. Sits behind the
// SVG (axes / grid / overlays stay SVG), shares the SVG viewBox, and ignores
// pointer events so the SVG keeps handling the mouse. Inert (draws nothing,
// no throw) when WebGL is unavailable — the parent then keeps those layers
// on the SVG path. pan / zoom = a projection change → one O(1) draw.
//
// Props:
//   series   — [{ data: Float32Array (interleaved x,y), segments, color:[r,g,b,a] }]
//   proj     — makeProjection output (data → clip)
//   plotRect — { x, y, w, h } in viewBox units (the axes box)
//   width, height — viewBox size (shared with the SVG)
//   dpr      — devicePixelRatio
export default function GLChart({ series, proj, plotRect, width, height, dpr = 1, clip = null }) {
  const canvasRef = useRef(null);
  const rendRef = useRef(null);

  // Create the GL context + renderer once. Null context (no WebGL) → inert.
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return undefined;
    const gl = createGL(canvas);
    if (!gl) return undefined;
    rendRef.current = new GLPlotRenderer(gl);
    return () => { rendRef.current?.dispose(); rendRef.current = null; };
  }, []);

  // Upload series whenever the data changes.
  useEffect(() => {
    rendRef.current?.setSeries(series || []);
  }, [series]);

  // Draw on any projection / size / data change. The viewport+scissor pin
  // the GL output to the plot rect under the SVG axes.
  useEffect(() => {
    const rend = rendRef.current;
    const canvas = canvasRef.current;
    if (!rend || !canvas) return;
    const gl = rend.gl;
    const cssW = canvas.clientWidth || width;
    const cssH = canvas.clientHeight || height;
    resizeToDisplay(canvas, cssW, cssH, dpr);
    const vp = glViewportRect(plotRect, { w: width, h: height },
                              { w: canvas.width, h: canvas.height });
    gl.viewport(vp.x, vp.y, vp.w, vp.h);
    gl.enable(gl.SCISSOR_TEST);
    gl.scissor(vp.x, vp.y, vp.w, vp.h);
    rend.setProjection(proj);
    rend.setPixelRatio(dpr);   // scatter marker size = radius × dpr (framebuffer px)
    rend.setClip(clip);        // polar disc clip (cx,cy,radius); null = cartesian
    rend.draw();
  }, [series, proj, plotRect, width, height, dpr, clip]);

  return (
    <canvas
      ref={canvasRef}
      style={{ position: 'absolute', inset: 0, width: '100%', height: '100%',
               pointerEvents: 'none' }}
    />
  );
}
