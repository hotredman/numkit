clear

x = [1 2 3 4 5]';

fprintf('=== gplike ===\n');
[nL, ac] = gplike([0.5, 1], x);
fprintf('  k>0  nL      = %.6f (expect 13.098835)\n', nL);
fprintf('  k>0  ac(1,1) = %.6f (expect  0.535756)\n', ac(1,1));
fprintf('  k>0  ac(2,2) = %.6f (expect  0.368925)\n', ac(2,2));

[nL, ac] = gplike([0, 1], x);
fprintf('\n  k=0  nL      = %.6f (expect 15.0; exponential limit)\n', nL);
fprintf('  k=0  ac(1,1) = %.6f (expect  0.032258)\n', ac(1,1));
fprintf('  k=0  ac(2,2) = %.6f (expect  0.122581)\n', ac(2,2));

fprintf('\n--- edges (asymmetric vs gevlike — match MATLAB R2025b) ---\n');
fprintf('  x<0 OK   : %.6f (expect 1.216395; per-point support holds)\n', gplike([0.5, 1], [-1 1 2]'));
fprintf('  OOS      : %g (expect Inf)\n', gplike([-1, 1], 10));
fprintf('  sigma<0  : %g (expect -Inf)\n', gplike([0.5, -1], x));
fprintf('  sigma=0  : %g (expect NaN)\n', gplike([0.5, 0], x));
fprintf('  empty    : %g (expect Inf)\n', gplike([0.5, 1], []));
