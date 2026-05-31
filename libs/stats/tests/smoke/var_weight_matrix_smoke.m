clear

import compat.*

% var/std with a weight VECTOR on a MATRIX — DEEP-PROBE 2026-05-31.
% Previously numkit threw: with a default dim it required the weight
% length to equal numel ("weight vector length must match number of
% elements"), and with an explicit dim it threw "weight vector with
% non-flat dim not yet supported". MATLAB applies the weight vector
% along the operating dimension, one weighted variance per slice
% (normalized by sum(w)). Reference: MATLAB R2025b.

Mw = [1 2; 3 4; 5 6];
wc = [1; 2; 3];

fprintf('=== var(Mw, wc) default dim 1 (per column) ===\n');
v = var(Mw, wc);
fprintf('v = %.6f %.6f   (expect 2.222222 2.222222)  size %dx%d (expect 1x2)\n', ...
        v(1), v(2), size(v,1), size(v,2));

fprintf('\n=== var(Mw, [1 3], 2) dim 2 (per row) ===\n');
v2 = var(Mw, [1 3], 2);
fprintf('v2 = %.6f %.6f %.6f   (expect 0.1875 x3)  size %dx%d (expect 3x1)\n', ...
        v2(1), v2(2), v2(3), size(v2,1), size(v2,2));

fprintf('\n=== std(Mw, wc) ===\n');
s = std(Mw, wc);
fprintf('s = %.12f %.12f   (expect 1.490711985)\n', s(1), s(2));

fprintf('\n=== omitnan with weights ([1 NaN 5] col1, w=[1;2;3]) ===\n');
Mn = [1 2; NaN 4; 5 6];
vn = var(Mn, [1;2;3], 1, 'omitnan');
fprintf('vn(1) = %.6f   (expect 3.0 — NaN sample + weight 2 dropped)\n', vn(1));

fprintf('\n=== vector weight still works ===\n');
fprintf('var([1 2 3 4 5], [1 1 1 1 4]) = %.6f   (expect 2.1875)\n', ...
        var([1 2 3 4 5], [1 1 1 1 4]));
