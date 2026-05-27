clear

% colperm — column permutation by nonzero count.
% Reference: MATLAB R2025b.

A = [0 1 0 1; 1 1 1 0; 0 1 0 0; 1 0 0 0];
fprintf('nnz per col: %s\n', mat2str(sum(A ~= 0, 1)));
fprintf('colperm: %s (e [3 4 1 2])\n', mat2str(colperm(A)));

A2 = [-1 0 0; 2 -3 0; 0 4 5];
fprintf('\nnnz: %s   colperm: %s (e [3 1 2])\n', mat2str(sum(A2~=0, 1)), mat2str(colperm(A2)));

A3 = [1 0 1 0; 0 1 1 0];
fprintf('\nnon-square: nnz=%s   colperm: %s (e [4 1 2 3])\n', mat2str(sum(A3~=0, 1)), mat2str(colperm(A3)));

A4 = [0 1 0; 0 1 0];
fprintf('\nzero cols: colperm: %s (e [1 3 2])\n', mat2str(colperm(A4)));
