clear

x = [0.1 0.3 0.5 0.7 0.9]';

fprintf('=== betalike ===\n');
[nL, av] = betalike([2, 2], x);
fprintf('  nL      = %.6f (expect 0.364684)\n', nL);
fprintf('  av(1,1) = %.6f (expect 0.923439)\n', av(1,1));
fprintf('  av(1,2) = %.6f (expect 0.743120)\n', av(1,2));
fprintf('  av(2,2) = %.6f (expect 0.923439)\n', av(2,2));
fprintf('  (note: AVAR uses BHHH, not the Hessian — matches MATLAB)\n');

fprintf('\n--- edges ---\n');
fprintf('  a=0      : %g (expect NaN)\n', betalike([0, 2], x));
fprintf('  x>1      : %g (expect NaN)\n', betalike([2, 2], [0.5 1.5]'));
fprintf('  x=0      : %g (expect NaN)\n', betalike([2, 2], [0.0 0.5]'));
fprintf('  empty    : %g (expect Inf)\n', betalike([2, 2], []));
