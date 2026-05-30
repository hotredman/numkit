clear

import compat.*

% sortrows direction strings/cells. Bug fixed 2026-05-30: sortrows threw
% "column spec must be numeric" on any string/cell direction argument.
% vs MATLAB R2025b. A = [3 1; 1 2; 3 0; 1 5].

A = [3 1; 1 2; 3 0; 1 5];

fprintf('=== direction string over all columns ===\n');
fprintf('descend = %s (expect [3 1;3 0;1 5;1 2])\n', mat2str(sortrows(A, 'descend')));
fprintf('ascend  = %s (expect [1 2;1 5;3 0;3 1])\n', mat2str(sortrows(A, 'ascend')));

fprintf('\n=== explicit columns + per-column direction cell ===\n');
fprintf('[1 2] {asc,desc} = %s (expect [1 5;1 2;3 1;3 0])\n', ...
        mat2str(sortrows(A, [1 2], {'ascend','descend'})));

fprintf('\n=== explicit columns + single direction (+ index) ===\n');
[B, ix] = sortrows(A, [1 2], 'descend');
fprintf('B  = %s (expect [3 1;3 0;1 5;1 2])\n', mat2str(B));
fprintf('ix = %s (expect [1;3;4;2])\n', mat2str(ix));

fprintf('\n=== direction cell standalone ===\n');
fprintf('{desc,asc} = %s (expect [3 0;3 1;1 2;1 5])\n', ...
        mat2str(sortrows(A, {'descend','ascend'})));

fprintf('\n=== scalar column + direction ===\n');
fprintf('col1 desc = %s (expect [3 1;3 0;1 2;1 5])\n', mat2str(sortrows(A, 1, 'descend')));

fprintf('\n=== numeric spec unchanged ===\n');
fprintf('[1 -2] = %s (expect [1 5;1 2;3 1;3 0])\n', mat2str(sortrows(A, [1 -2])));
