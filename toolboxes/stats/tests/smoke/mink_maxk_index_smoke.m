clear

import compat.*

% mink/maxk second output (indices). Bug fixed 2026-05-30: [M,I] = mink/maxk
% did not return the index of each returned element. vs MATLAB R2025b.

fprintf('=== vector indices ===\n');
[m, ix] = mink([5 2 8 1 9], 2);
fprintf('mink m=%s ix=%s (expect [1 2] / [4 2])\n', mat2str(m), mat2str(ix));
[mx, jx] = maxk([5 2 8 1 9], 2);
fprintf('maxk m=%s jx=%s (expect [9 8] / [5 3])\n', mat2str(mx), mat2str(jx));

fprintf('\n=== ties keep the lower position (stable) ===\n');
[mt, it] = mink([3 1 3], 3);
fprintf('mink m=%s it=%s (expect [1 3 3] / [2 1 3])\n', mat2str(mt), mat2str(it));

fprintf('\n=== matrix, default dim 1 (row positions per column) ===\n');
[mm, im] = mink([3 6; 1 4; 2 5], 2);
fprintf('m=%s\nim=%s (expect [1 4;2 5] / [2 2;3 3])\n', mat2str(mm), mat2str(im));

fprintf('\n=== matrix, dim 2 (col positions per row) ===\n');
[md, id] = maxk([1 5 2; 8 3 9], 2, 2);
fprintf('m=%s\nid=%s (expect [5 2;9 8] / [2 3;3 1])\n', mat2str(md), mat2str(id));

fprintf('\n=== single output still works ===\n');
fprintf('mink=%s maxk=%s\n', mat2str(mink([5 2 8 1 9],2)), mat2str(maxk([5 2 8 1 9],2)));
