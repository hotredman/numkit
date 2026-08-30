clear

x    = [1 2 3 4 5 6 7 8 9 10]';
cens = [0 0 0 0 0 0 0 1 1 1]';
freq = [2 2 2 1 1 1 1 1 1 1]';

fprintf('=== lognlike ===\n');
[nL, av] = lognlike([0, 1], x);
fprintf('  basic    nL=%.6f av=[%.4f %.4f; %.4f %.4f]\n', nL, av(1,1), av(1,2), av(2,1), av(2,2));
fprintf('   expect  nL=38.118920 av=[-0.3985 0.1650; 0.1650 -0.0546]\n');
fprintf('   (note: aVar can be non-PD at non-MLE params — observed Fisher)\n');

fprintf('\n  cens     = %.6f (expect 34.341116)\n', lognlike([0, 1], x, cens));
fprintf('  freq     = %.6f (expect 43.511196)\n', lognlike([0, 1], x, [], freq));
fprintf('  cens+freq= %.6f (expect 39.733392)\n', lognlike([0, 1], x, cens, freq));

fprintf('\n--- edges ---\n');
fprintf('  sigma=0  : %g (expect NaN)\n', lognlike([0, 0], [1 2 3 4 5]'));
fprintf('  x<=0     : %g (expect NaN)\n', lognlike([0, 1], [-1 2 3]'));
fprintf('  empty    : %g (expect 0)\n', lognlike([0, 1], []));
