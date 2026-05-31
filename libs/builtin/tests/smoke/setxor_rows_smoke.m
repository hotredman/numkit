clear

import compat.*

% setxor(A, B, 'rows') — DEEP-PROBE 2026-05-31. This IGNORED the 'rows'
% flag and flattened element-wise to a 1xN vector. With 'rows' each ROW is
% one element; the result is the sorted set of rows present in EXACTLY one
% of A or B (symmetric difference). Reference: MATLAB R2025b.

A = [1 2; 3 4; 5 6];
B = [3 4; 9 9; 1 2];

fprintf('=== setxor rows: symmetric difference ===\n');
x = setxor(A, B, 'rows');
fprintf('x size %dx%d = [%g %g; %g %g]  (expect 2x2, [5 6;9 9])\n', ...
        size(x,1), size(x,2), x(1,1), x(1,2), x(2,1), x(2,2));

fprintf('\n=== interleaved only-in-A / only-in-B, sorted ===\n');
y = setxor([5 6; 1 1], [1 1; 2 2; 7 8], 'rows');
fprintf('y size %dx%d, col1 = [%g %g %g]  (expect 3x2, [2;5;7])\n', ...
        size(y,1), size(y,2), y(1,1), y(2,1), y(3,1));

fprintf('\n=== element-wise (non-rows) path unchanged ===\n');
disp(setxor([1 2 3 4], [3 4 5 6]));   % expect [1 2 5 6]
