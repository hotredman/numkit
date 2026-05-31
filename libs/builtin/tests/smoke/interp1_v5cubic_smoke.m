clear

import compat.*

% interp1 'v5cubic' / 'cubic' (2026-05-30): MATLAB's classic Keys cubic
% convolution (a=-0.5) on a uniformly-spaced grid. 'cubic' and 'v5cubic'
% are the same method here. On a non-uniform grid MATLAB switches to
% 'spline'; out-of-range returns NaN (NOT extrapolated, unlike
% spline/pchip/makima). numkit previously errored "unknown method" for
% both. vs MATLAB R2025b.

x = 0:5; y = [0 1 8 27 64 125];

fprintf('=== uniform grid (cubic convolution) ===\n');
fprintf('v5cubic 2.3 = %.10f  (expect 12.251)\n',  interp1(x, y, 2.3, 'v5cubic'));
fprintf('cubic   2.3 = %.10f  (expect 12.251)\n',  interp1(x, y, 2.3, 'cubic'));
fprintf('v5cubic 4.7 = %.10f  (expect 104.18)\n',  interp1(x, y, 4.7, 'v5cubic'));

fprintf('\n=== non-polynomial data (distinct from spline) ===\n');
fprintf('v5cubic 2.4 = %.10f  (expect 2.896)\n', interp1(0:5, [2 1 4 1 5 9], 2.4, 'v5cubic'));

fprintf('\n=== out of range -> NaN (no extrapolation) ===\n');
fprintf('v5cubic 5.5  = %.4f  (expect NaN)\n', interp1(x, y, 5.5, 'v5cubic'));
fprintf('cubic  -0.5  = %.4f  (expect NaN)\n', interp1(x, y, -0.5, 'cubic'));

fprintf('\n=== non-uniform grid -> spline fallback ===\n');
fprintf('v5cubic = %.10f  (expect spline 9.4411250000)\n', interp1([0 1 2 4 5 6], y, 2.3, 'v5cubic'));
