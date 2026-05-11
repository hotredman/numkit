// 3d-view-presets.spec.js — toolbar "view ▾" preset menu in
// FigureWindow for 3-D figures. Each preset calls Composite3DPlot's
// setView imperative method to snap the camera to a standard
// orientation (top, bottom, front, back, left, right, iso).
//
// Verified end-to-end via the canvas.__numkit3dCtx test hook.

import { test, expect } from '../../helpers/shared.js';

test.describe('3-D view-preset toolbar', () => {
  async function openSurfModal(ide) {
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
    return canvas;
  }

  test('view ▾ button visible only on 3-D figures', async ({ ide, page }) => {
    await openSurfModal(ide);
    const viewBtn = ide.figureWindow.locator('button', { hasText: /^view ▾$/ });
    expect(await viewBtn.count()).toBe(1);
  });

  test('top preset (az=0, el=90) puts camera above origin', async ({ ide, page }) => {
    const canvas = await openSurfModal(ide);
    const viewBtn = ide.figureWindow.locator('button', { hasText: /^view ▾$/ });
    await viewBtn.click();
    await ide.figureWindow.locator('button[data-fw-view="top"]').click();
    await page.waitForTimeout(50);
    const pos = await canvas.evaluate((cv) => {
      const ctx = cv.__numkit3dCtx?.current;
      if (!ctx) return null;
      const p = ctx.camera.position;
      return { x: p.x, y: p.y, z: p.z };
    });
    expect(pos).not.toBeNull();
    // top → camera world-Y >> 0, world-X / world-Z ≈ 0.
    expect(pos.y).toBeGreaterThan(2);
    expect(Math.abs(pos.x)).toBeLessThan(0.6);
    expect(Math.abs(pos.z)).toBeLessThan(0.6);
  });

  test('front preset (az=0, el=0) puts camera along +Z', async ({ ide, page }) => {
    const canvas = await openSurfModal(ide);
    await ide.figureWindow.locator('button', { hasText: /^view ▾$/ }).click();
    await ide.figureWindow.locator('button[data-fw-view="front"]').click();
    await page.waitForTimeout(50);
    const pos = await canvas.evaluate((cv) => {
      const ctx = cv.__numkit3dCtx?.current;
      if (!ctx) return null;
      const p = ctx.camera.position;
      return { x: p.x, y: p.y, z: p.z };
    });
    expect(pos.z).toBeGreaterThan(2);
    expect(Math.abs(pos.x)).toBeLessThan(0.6);
    expect(Math.abs(pos.y)).toBeLessThan(0.6);
  });

  test('iso preset (default) restores -37.5° / 30°', async ({ ide, page }) => {
    const canvas = await openSurfModal(ide);
    // First snap to top, then back to iso.
    await ide.figureWindow.locator('button', { hasText: /^view ▾$/ }).click();
    await ide.figureWindow.locator('button[data-fw-view="top"]').click();
    await page.waitForTimeout(30);
    await ide.figureWindow.locator('button', { hasText: /^view ▾$/ }).click();
    await ide.figureWindow.locator('button[data-fw-view="iso"]').click();
    await page.waitForTimeout(30);
    const pos = await canvas.evaluate((cv) => {
      const ctx = cv.__numkit3dCtx?.current;
      if (!ctx) return null;
      const p = ctx.camera.position;
      return { x: p.x, y: p.y, z: p.z };
    });
    // Iso in our convention: az=-37.5°, el=30° at radius 4 →
    // x = 4 cos30° sin(-37.5°), y = 4 sin30°, z = 4 cos30° cos(-37.5°)
    expect(pos.y).toBeCloseTo(4 * Math.sin(30 * Math.PI / 180), 1);
    expect(pos.z).toBeGreaterThan(0);   // forward of origin
    expect(pos.x).toBeLessThan(0);      // left of origin
  });

  test('view-preset toggles produce no console errors', async ({ ide, page }) => {
    await openSurfModal(ide);
    const presets = ['top', 'bottom', 'front', 'back', 'right', 'left', 'iso'];
    for (const p of presets) {
      await ide.figureWindow.locator('button', { hasText: /^view ▾$/ }).click();
      await ide.figureWindow.locator(`button[data-fw-view="${p}"]`).click();
      await page.waitForTimeout(20);
    }
    expect(ide.devErrors().filter((e) =>
      !/Autofill\.enable/i.test(e) && !/\[hmr\]/i.test(e)
    )).toEqual([]);
  });
});
