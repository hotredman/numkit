// 3d-grid-camera.spec.js — BUG #39 regression guard.
//
// 39a — toggling the major-grid button must NOT reset the camera.
// 39b — major-grid lines must hide on the three "front" faces of
//       the bounding cube and stay visible only on back faces.
//
// We hook into the live three.js context via `canvas.__numkit3dCtx`,
// a debug-only ref attached at component mount. Tests read camera
// position and per-face `LineSegments.visible` flags directly.

import { test, expect } from '../../helpers/shared.js';

test.describe('3-D grid + camera — BUG #39', () => {
  test('39a: toggling grid button keeps camera position', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3 4 5; 2 4 6 4 2; 3 6 9 6 3; 2 4 6 4 2; 1 2 3 2 1];\n'
      + 'surf(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    // Wait for the canvas + three.js ctx to mount.
    const canvas = ide.figureWindow.locator('canvas[data-numkit-3d]').first();
    await expect(canvas).toBeVisible({ timeout: 5_000 });
    // Read camera position before toggling.
    const before = await canvas.evaluate((cv) => {
      const ctx = cv.__numkit3dCtx?.current;
      if (!ctx) return null;
      const p = ctx.camera.position;
      return { x: p.x, y: p.y, z: p.z };
    });
    expect(before).not.toBeNull();
    // Toggle grid off then on through the toolbar.
    const gridBtn = ide.figureWindow.locator('button', { hasText: /^grid$/ });
    await gridBtn.click();
    await page.waitForTimeout(50);
    await gridBtn.click();
    await page.waitForTimeout(50);
    const after = await canvas.evaluate((cv) => {
      const ctx = cv.__numkit3dCtx?.current;
      if (!ctx) return null;
      const p = ctx.camera.position;
      return { x: p.x, y: p.y, z: p.z };
    });
    expect(after).not.toBeNull();
    // Should match exactly — no rebuild, no view re-application.
    expect(after.x).toBeCloseTo(before.x, 6);
    expect(after.y).toBeCloseTo(before.y, 6);
    expect(after.z).toBeCloseTo(before.z, 6);
  });

  test('39b: major grid built on all six faces, exactly three visible', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3; 2 4 6; 3 6 9];\n'
      + 'surf(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const canvas = ide.figureWindow.locator('canvas[data-numkit-3d]').first();
    await expect(canvas).toBeVisible({ timeout: 5_000 });
    // Allow the first tick to settle visibility flags.
    await page.waitForTimeout(100);
    const stats = await canvas.evaluate((cv) => {
      const ctx = cv.__numkit3dCtx?.current;
      if (!ctx) return null;
      const major = ctx.gridMajorByFace || {};
      const faces = Object.keys(major);
      const visible = faces.filter((f) => major[f].visible);
      return { faceCount: faces.length, visibleCount: visible.length };
    });
    expect(stats).not.toBeNull();
    // Builder emits one LineSegments per face → 6 entries.
    expect(stats.faceCount).toBe(6);
    // Tick loop hides front 3 → exactly 3 are visible.
    expect(stats.visibleCount).toBe(3);
  });

  test('39b: orbiting camera flips which faces are visible', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'Z = [1 2 3; 2 4 6; 3 6 9];\n'
      + 'surf(Z);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const canvas = ide.figureWindow.locator('canvas[data-numkit-3d]').first();
    await expect(canvas).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(100);
    const result = await canvas.evaluate((cv) => {
      const ctx = cv.__numkit3dCtx?.current;
      if (!ctx) return null;
      const sample = () => {
        const m = ctx.gridMajorByFace;
        return Object.fromEntries(Object.keys(m).map((f) => [f, m[f].visible]));
      };
      const before = sample();
      // Flip camera to the opposite octant — every face's "isBack"
      // flips, so the visibility pattern must invert.
      const p = ctx.camera.position;
      ctx.camera.position.set(-p.x, -p.y, -p.z);
      ctx.camera.lookAt(0, 0, 0);
      ctx.controls.update();
      // Force one extra tick so the visibility update lands.
      return new Promise((res) => {
        setTimeout(() => {
          const after = sample();
          res({ before, after });
        }, 50);
      });
    });
    expect(result).not.toBeNull();
    // Every face that was visible before must now be hidden, and
    // vice-versa.
    for (const face of Object.keys(result.before)) {
      expect(result.after[face]).toBe(!result.before[face]);
    }
  });

  test('39: toggle grid then toggle back yields no console errors', async ({ ide, page }) => {
    await ide.runScript(
      'import compat.*;\n'
      + 'plot3([0 1 2 3], [0 1 0 1], [0 1 4 9]);\n'
    );
    await expect(ide.figureCards).toHaveCount(1, { timeout: 10_000 });
    await ide.figureCards.first().click();
    await expect(ide.figureWindow).toBeVisible({ timeout: 5_000 });
    const gridBtn = ide.figureWindow.locator('button', { hasText: /^grid$/ });
    const minorBtn = ide.figureWindow.locator('button', { hasText: /^minor$/ });
    // Cycle through several toggles.
    for (let i = 0; i < 3; i++) {
      await gridBtn.click();
      await minorBtn.click();
      await page.waitForTimeout(30);
    }
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
