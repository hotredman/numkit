# graphics.subplot — `subplot(m,n,p)` never reuses/replaces the pre-existing default axes; gcf.Children exposes the ghost

- **Status:** ✅ FIXED (portion 31, commit 1972cd76d)
- **Severity:** P3 (invisible until a handle-tree walk: extra ghost axes in
  the figure; render unaffected — the ghost has no subplotIndex)
- **Kind:** bug
- **Found:** 2026-09-11 portion 31 — `a2.Parent.Children` returned 3 axes
  after plot + subplot(2,1,1) + subplot(2,1,2); MATLAB returns 2.

## Symptom

`setSubplot` only searches for an axes with the exact `subplotIndex == p`;
a pre-existing default axes (subplotIndex 0, created by an earlier plot
before any subplot call) never matches, so every subplot call ADDS an
axes. MATLAB deletes/replaces the overlapping default axes: after
`plot(1:5); subplot(2,1,1); subplot(2,1,2);` MATLAB's figure has exactly
2 axes.

## Repro (self-contained)

```matlab
clear;
plot(1:5);            % creates the default axes (subplotIndex 0)
subplot(2,1,1);
plot(1:5);
subplot(2,1,2);
plot(1:5);
ch = gca.Parent.Children;
disp(numel(ch))
% numkit:  3   (ghost default axes kept)
% MATLAB R2025b: 2   (default axes replaced by subplot(2,1,1))
```

## Fix (portion 31)

`setSubplot` (figure_manager.hpp) now ADOPTS the lone default axes
(subplotIndex 0) for the first subplot cell instead of appending a new
one: the default axes' state is reset and its handle records invalidated
(scoped sweep), mirroring MATLAB's delete-overlapping semantics. Found
broader than first filed: the ghost appeared even for a CLEAN
subplot-first script, because `setSubplot` itself materialises the
default axes via `current()` before appending.

## References

- **Guard:** `SubplotAdoptsDefaultAxes` (dual-engine, enabled) +
  `FigureChildrenIsFullAxesArray` in
  `src/graphics/tests/graphics_handle_test.cpp`.
