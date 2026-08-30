clear

x = [1 2 3 4 5]';

fprintf('=== gevlike ===\n');
[nL, ac] = gevlike([0.5, 1, 0], x);
fprintf('  k>0 nL      = %.6f (expect 14.146023)\n', nL);
fprintf('  k>0 ac(1,1) = %.6f (expect  0.732324)\n', ac(1,1));
fprintf('  k>0 ac(2,2) = %.6f (expect  0.384193)\n', ac(2,2));
fprintf('  k>0 ac(3,3) = %.6f (expect -1.235779)\n', ac(3,3));

nL_g = gevlike([0, 1, 0], x);
fprintf('\n  k=0 nL      = %.6f (expect 15.578055; Gumbel-MAX limit)\n', nL_g);
fprintf('  (note: at exactly k=0 ACOV uses FD value, not MATLAB analytical)\n');

fprintf('\n--- edges ---\n');
fprintf('  OOS (k=0.5)  : %g (expect NaN)\n', gevlike([0.5, 1, 0], [-100; -1; 0]));
fprintf('  sigma<=0     : %g (expect NaN)\n', gevlike([0.5, -1, 0], x));
fprintf('  empty        : %g (expect Inf)\n', gevlike([0.5, 1, 0], []));
