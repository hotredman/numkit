clear

x = [1 2 3 4 5]';

fprintf('=== gamlike ===\n');
[nL, av] = gamlike([2, 1], x);
fprintf('  nL      = %.6f (expect 10.212508)\n', nL);
fprintf('  av(1,1) = %.6f (expect  0.506414)\n', av(1,1));
fprintf('  av(1,2) = %.6f (expect -0.126603)\n', av(1,2));
fprintf('  av(2,2) = %.6f (expect  0.081651)\n', av(2,2));

fprintf('\n--- edges ---\n');
fprintf('  a=0   : %g (expect NaN)\n', gamlike([0, 1], x));
fprintf('  b=0   : %g (expect NaN)\n', gamlike([2, 0], x));
fprintf('  empty : %g (expect Inf)\n', gamlike([2, 1], []));
