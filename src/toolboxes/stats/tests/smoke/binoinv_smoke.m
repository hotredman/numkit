clear

fprintf('=== binoinv ===\n');
fprintf('  median (10, 0.3) : %g (expect 3)\n', binoinv(0.5, 10, 0.3));
x = binoinv([0.05 0.5 0.95], 10, 0.3);
fprintf('  vector q         : [%g %g %g] (expect [1 3 5])\n', x(1), x(2), x(3));
fprintf('  q=0 → %g, q=1 → %g (expect 0, 10)\n', binoinv(0, 10, 0.3), binoinv(1, 10, 0.3));
fprintf('  p=0 → %g, p=1 → %g (expect 0, 10)\n', binoinv(0.5, 10, 0), binoinv(0.5, 10, 1));
fprintf('  edges: q<0 → %g, q>1 → %g, p<0 → %g, n<0 → %g, n=2.5 → %g (all NaN)\n', binoinv(-0.1,10,0.3), binoinv(1.5,10,0.3), binoinv(0.5,10,-0.1), binoinv(0.5,-1,0.3), binoinv(0.5,2.5,0.3));
