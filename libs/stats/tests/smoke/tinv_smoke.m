clear

import compat.*

fprintf('=== tinv ===\n');
fprintf('  tinv(0.975, 5)   : %.6f (expect 2.570582)\n', tinv(0.975, 5));
v = tinv([0.05 0.5 0.975], 10);
fprintf('  vec p (nu=10)    : [%.4f %.4f %.4f]\n', v(1), v(2), v(3));
fprintf('  small nu=1       : %.4f (expect 12.7062 — Cauchy quartile)\n', tinv(0.975, 1));
fprintf('  Gaussian limit   : %.6f (expect 1.959964 — same as norminv(0.975))\n', tinv(0.975, Inf));
fprintf('  median nu=Inf    : %g (expect 0)\n', tinv(0.5, Inf));
fprintf('  p=0 → %g, p=1 → %g (expect -Inf, +Inf)\n', tinv(0, 5), tinv(1, 5));
fprintf('  edges: p<0 → %g, p>1 → %g, nu=0 → %g, nu<0 → %g (NaN)\n', ...
    tinv(-0.1, 5), tinv(1.5, 5), tinv(0.5, 0), tinv(0.5, -1));
