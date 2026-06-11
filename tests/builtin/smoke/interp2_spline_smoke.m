clear

import compat.*

% interp2 'spline' method (2026-05-30): separable tensor-product cubic
% spline. The cubic spline is a LINEAR interpolation operator, so the 2-D
% result equals interpolating along x for each row then along y -- which
% reproduces MATLAB exactly (including out-of-range extrapolation and
% non-uniform grids). numkit previously errored "'spline' not yet
% supported". vs MATLAB R2025b. ('makima'/'pchip' for interp2 stay
% deferred -- they are nonlinear and need a full bicubic-Hermite tensor
% product with cross derivatives.)

x = 1:4; y = 1:4;
Z = [1 2 4 8; 3 5 9 15; 6 10 16 24; 11 17 25 35];

fprintf('=== interior query (2.4, 3.1) ===\n');
fprintf('spline = %.10f  (expect 12.851056)\n', interp2(x, y, Z, 2.4, 3.1, 'spline'));
fprintf('cubic  = %.10f  (expect 12.85)\n',     interp2(x, y, Z, 2.4, 3.1, 'cubic'));

fprintf('\n=== exact at a grid node (2,3) -> Z(3,2)=10 ===\n');
fprintf('spline = %.10f\n', interp2(x, y, Z, 2, 3, 'spline'));

fprintf('\n=== extrapolates out of range (5,2) ===\n');
fprintf('spline = %.10f  (expect 23, linear would be NaN)\n', interp2(x, y, Z, 5, 2, 'spline'));

fprintf('\n=== non-uniform grid (cubic rejects this) ===\n');
g = [1 2 4 7];
fprintf('spline = %.10f  (expect 10.3471604938)\n', interp2(g, g, Z, 3, 3, 'spline'));

fprintf('\n=== 2x2 grid falls back to bilinear ===\n');
fprintf('spline = %.10f  (expect 2.5)\n', interp2([1 2], [1 2], [1 2; 3 4], 1.5, 1.5, 'spline'));
