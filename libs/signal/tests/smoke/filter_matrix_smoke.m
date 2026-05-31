clear

import compat.*

% filter() per-column on a matrix (2026-05-30): MATLAB filter(b,a,X)
% filters along the first non-singleton dimension, resetting the delay
% state between signals. numkit previously filtered the whole column-major
% buffer as ONE signal, leaking state across column boundaries. vs MATLAB
% R2025b.

M = [1 2; 3 4; 5 6];

fprintf('=== FIR b=[1 1], a=1 on a 3x2 matrix (per column) ===\n');
disp(filter([1 1], 1, M));
fprintf('expect [1 2; 4 6; 8 10] (col1 [1;3;5]->[1;4;8], col2 [2;4;6]->[2;6;10])\n\n');

fprintf('=== column vector unchanged ===\n');
disp(filter([1 1], 1, [1; 3; 5]));
fprintf('expect [1; 4; 8]\n\n');

fprintf('=== row vector unchanged ===\n');
disp(filter([1 1], 1, [1 3 5]));
fprintf('expect [1 4 8]\n\n');

fprintf('=== [y, zf] returns a per-column final state ===\n');
[y, zf] = filter([1 0.5], 1, M);
disp(y);
fprintf('zf = %s (expect [2.5 3])\n\n', mat2str(zf));

fprintf('=== IIR a=[1 -0.5] per column ===\n');
[y2, zf2] = filter(1, [1 -0.5], M);
disp(y2);
fprintf('expect [1 2; 3.5 5; 6.75 8.5], zf2 = %s (expect [3.375 4.25])\n', mat2str(zf2));
