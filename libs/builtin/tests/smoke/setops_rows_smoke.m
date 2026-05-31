clear

import compat.*

% setdiff / intersect / union with the 'rows' flag — DEEP-PROBE 2026-05-31.
% These IGNORED 'rows' (flattening element-wise) or threw (setdiff). With
% 'rows' each ROW is treated as one element; the result is the sorted set of
% unique rows. Reference: MATLAB R2025b.

A = [1 2; 3 4; 5 6];
B = [3 4; 9 9; 1 2];

fprintf('=== setdiff rows (A-rows not in B) ===\n');
d = setdiff(A, B, 'rows');
fprintf('size %dx%d, d = [%g %g]  (expect 1x2, [5 6])\n', size(d,1), size(d,2), d(1,1), d(1,2));

fprintf('\n=== intersect rows (common rows, sorted) ===\n');
c = intersect(A, B, 'rows');
fprintf('size %dx%d, c = [%g %g; %g %g]  (expect 2x2, [1 2;3 4])\n', ...
        size(c,1), size(c,2), c(1,1), c(1,2), c(2,1), c(2,2));

fprintf('\n=== row-distinguishing: [1 2;3 4] vs [2 1;3 4] ===\n');
dd = intersect([1 2;3 4], [2 1;3 4], 'rows');
fprintf('size %dx%d, dd = [%g %g]  (expect 1x2 [3 4], NOT 1x4 — proves row-wise)\n', ...
        size(dd,1), size(dd,2), dd(1,1), dd(1,2));

fprintf('\n=== union rows (unique rows of [A;B], sorted) ===\n');
u = union(A, B, 'rows');
fprintf('size %dx%d (expect 4x2); last row = [%g %g] (expect [9 9])\n', ...
        size(u,1), size(u,2), u(4,1), u(4,2));

fprintf('\n=== non-rows element-wise path unchanged ===\n');
disp(setdiff([3 1 2 5 4], [2 5]));   % expect [1 3 4]
