clear

import compat.*

fprintf('=== tpdf ===\n');
fprintf('  tpdf(0, 5)       : %.6f (expect 0.379607)\n', tpdf(0, 5));
fprintf('  tpdf(1, 5)       : %.6f (expect 0.219680)\n', tpdf(1, 5));
v = tpdf([-2 -1 0 1 2], 10);
fprintf('  vec x (nu=10)    : [%.4f %.4f %.4f %.4f %.4f]\n', v(1), v(2), v(3), v(4), v(5));
fprintf('  Gaussian limit @ x=0    : %.6f (expect 0.398942)\n', tpdf(0, Inf));
fprintf('  Gaussian limit @ x=1    : %.6f (expect 0.241971)\n', tpdf(1, Inf));
fprintf('  Large nu (1e10) @ x=0   : %.6f (~normpdf(0))\n', tpdf(0, 1e10));
fprintf('  edges: nu=0 → %g, nu<0 → %g, NaN → %g (NaN)\n', ...
    tpdf(0, 0), tpdf(0, -1), tpdf(NaN, 5));
