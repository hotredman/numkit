clear

% symrcm — symmetric reverse Cuthill-McKee bandwidth reduction.
% Reference: MATLAB R2025b.

A = [1 1 0 0 1 0;
     1 1 1 0 0 0;
     0 1 1 1 0 0;
     0 0 1 1 1 1;
     1 0 0 1 1 0;
     0 0 0 1 0 1];
p = symrcm(A);
fprintf('6x6: p = %s (e [1 2 5 3 4 6])\n', mat2str(p));
B = A(p, p);
[ri, rj] = find(A); bw_before = max(abs(ri - rj));
[ni, nj] = find(B); bw_after  = max(abs(ni - nj));
fprintf('  bandwidth: %d → %d\n', bw_before, bw_after);

% Tridiag self-reversal
A = full([1 1 0 0 0; 1 1 1 0 0; 0 1 1 1 0; 0 0 1 1 1; 0 0 0 1 1]);
fprintf('tridiag: %s (e [5 4 3 2 1])\n', mat2str(symrcm(A)));

% Block-diagonal disconnected components
A = [1 1 0 0; 1 1 0 0; 0 0 1 1; 0 0 1 1];
fprintf('disconn: %s (e [2 1 4 3])\n', mat2str(symrcm(A)));
