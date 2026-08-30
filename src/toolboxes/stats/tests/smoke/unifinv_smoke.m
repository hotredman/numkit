clear

fprintf('=== unifinv ===\n');
fprintf('  median U(0,1)    : %g (expect 0.5)\n', unifinv(0.5));
fprintf('  unif(1,5) at 0.25 : %g (expect 2)\n', unifinv(0.25, 1, 5));
v = unifinv([0.05 0.5 0.95], 0, 10);
fprintf('  vec p (0..10)    : [%g %g %g]\n', v(1), v(2), v(3));
fprintf('  p=0 → %g, p=1 → %g (expect a, b)\n', unifinv(0, 1, 5), unifinv(1, 1, 5));
fprintf('  p out: -0.1 → %g, 1.5 → %g (NaN)\n', unifinv(-0.1), unifinv(1.5));
fprintf('  bad params: b<a → %g, b<a → %g, b=a → %g (NaN)\n', ...
    unifinv(0.5, 1, 0), unifinv(0.5, 5, 1), unifinv(0.5, 1, 1));
fprintf('  NaN: p=NaN → %g, a=NaN → %g (NaN)\n', ...
    unifinv(NaN, 0, 1), unifinv(0.5, NaN, 1));
