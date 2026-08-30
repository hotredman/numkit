clear

fprintf('=== unifpdf ===\n');
fprintf('  default (a=0,b=1) at 0.5 : %g (expect 1)\n', unifpdf(0.5));
fprintf('  default at -0.1, 1.1     : %g %g (expect 0 0)\n', unifpdf(-0.1), unifpdf(1.1));
fprintf('  unif(1,5) at 2           : %g (expect 0.25)\n', unifpdf(2, 1, 5));
y = unifpdf([-0.5 0 0.3 1 1.5], 0, 1);
fprintf('  vec x                    : [%g %g %g %g %g]\n', y(1), y(2), y(3), y(4), y(5));
fprintf('  bad params: b<a → %g, b<a → %g, b=a → %g (NaN)\n', ...
    unifpdf(0.5, 1, 0), unifpdf(0.5, 5, 1), unifpdf(0.5, 1, 1));
fprintf('  NaN x → %g (NaN), NaN a → %g (0 — comparison-false)\n', ...
    unifpdf(NaN, 0, 1), unifpdf(0.5, NaN, 1));
