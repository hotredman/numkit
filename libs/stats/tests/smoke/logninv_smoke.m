clear

import compat.*

fprintf('=== logninv ===\n');
fprintf('  median LN(0,1)  : %g (expect 1)\n', logninv(0.5));
x = logninv([0.05 0.5 0.95], 0, 1);
fprintf('  vector q        : [%.4f %.4f %.4f]\n', x(1), x(2), x(3));
fprintf('  q=0 → %g, q=1 → %g (expect 0, Inf)\n', logninv(0), logninv(1));
fprintf('  edges: q<0 → %g, q>1 → %g, sigma=0 → %g (NaN)\n', logninv(-0.1), logninv(1.5), logninv(0.5,0,0));
