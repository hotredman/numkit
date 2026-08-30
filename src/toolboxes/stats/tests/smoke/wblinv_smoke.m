clear

fprintf('=== wblinv ===\n');
fprintf('  default a=b=1 (=exponential):\n');
fprintf('    median  : %.6f (expect 0.693147 = ln 2)\n', wblinv(0.5));
fprintf('    p=0.05  : %.6f (expect 0.051293)\n', wblinv(0.05));
fprintf('    p=0.95  : %.6f (expect 2.995732)\n', wblinv(0.95));
fprintf('    p=0     : %g (expect 0)\n', wblinv(0));
fprintf('    p=1     : %g (expect Inf)\n', wblinv(1));
fprintf('  scaled (a=2, b=3) median : %.6f (expect 1.769994)\n', wblinv(0.5, 2, 3));
v = wblinv([0.05 0.5 0.95], 1, 2);
fprintf('  vec p (a=1,b=2)  : [%.4f %.4f %.4f]\n', v(1), v(2), v(3));
fprintf('  p out: -0.1 → %g, 1.5 → %g (NaN)\n', wblinv(-0.1), wblinv(1.5));
fprintf('  bad: a<=0 → %g, b<=0 → %g, NaN a → %g (NaN)\n', ...
    wblinv(0.5, 0, 1), wblinv(0.5, 1, 0), wblinv(0.5, NaN, 1));
