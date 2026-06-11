clear

import compat.*

% find 2-output [r,c] and 3-output [r,c,v]. Bug fixed 2026-05-30: find
% returned only linear indices; [r,c]=find(X) errored. vs MATLAB R2025b.

fprintf('=== matrix [r,c] ===\n');
[r, c] = find([0 1; 1 0]);
fprintf('r=%s c=%s (expect [2;1] / [1;2])\n', mat2str(r), mat2str(c));

fprintf('\n=== matrix [r,c,v] (column-major value order) ===\n');
[r2, c2, v] = find([0 5; 7 0]);
fprintf('r=%s c=%s v=%s (expect [2;1] [1;2] [7;5])\n', mat2str(r2), mat2str(c2), mat2str(v));

fprintf('\n=== row-vector input -> row-vector outputs ===\n');
[rr, cc] = find([0 1 0 1]);
fprintf('r=%s c=%s (expect [1 1] / [2 4])\n', mat2str(rr), mat2str(cc));

fprintf('\n=== column-vector input ===\n');
[rc, cc2] = find([0; 3; 4]);
fprintf('r=%s c=%s (expect [2;3] / [1;1])\n', mat2str(rc), mat2str(cc2));

fprintf('\n=== single output (linear indices) unchanged ===\n');
fprintf('find([0;3;4]) = %s (expect [2;3])\n', mat2str(find([0; 3; 4])));
