clear

import compat.*

% ismember(A, B, 'rows') — DEEP-PROBE 2026-05-31. This IGNORED the 'rows'
% flag and did element-wise membership (returned a MATRIX the size of A).
% With 'rows' each ROW is one element; tf and loc are COLUMN vectors of
% height size(A,1). Reference: MATLAB R2025b.

A = [1 2; 5 6; 3 4];
B = [3 4; 1 2; 7 8];

fprintf('=== ismember rows: tf + loc ===\n');
[tf, loc] = ismember(A, B, 'rows');
fprintf('tf size %dx%d = [%g %g %g]  (expect 3x1, [1 0 1])\n', ...
        size(tf,1), size(tf,2), tf(1), tf(2), tf(3));
fprintf('loc = [%g %g %g]  (expect [2 0 1])\n', loc(1), loc(2), loc(3));

fprintf('\n=== duplicate row in B -> lowest index ===\n');
[~, l2] = ismember([2 2; 1 1], [1 1; 3 3; 1 1; 2 2], 'rows');
fprintf('l2 = [%g %g]  (expect [4 1])\n', l2(1), l2(2));

fprintf('\n=== single-output scalar form ===\n');
t = ismember([10 20], [10 20; 1 2], 'rows');
fprintf('t = %g  (expect 1, scalar)\n', t);

fprintf('\n=== NaN-containing row never matches ===\n');
tn = ismember([NaN 2], [NaN 2; 1 2], 'rows');
fprintf('tn = %g  (expect 0)\n', tn);

fprintf('\n=== element-wise (non-rows) path unchanged ===\n');
[te, le] = ismember([1 5 3 2], [3 4 1]);
fprintf('tf=%s loc=%s  (expect [1 0 1 0], [3 0 1 0])\n', mat2str(te), mat2str(le));
