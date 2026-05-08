clear

import compat.*

fprintf('=== poissinv ===\n');
fprintf('  median Pois(2) : %g (expect 2)\n', poissinv(0.5, 2));
x = poissinv([0.05 0.5 0.95], 2);
fprintf('  vector q       : [%g %g %g]\n', x(1), x(2), x(3));
fprintf('  q=0 → %g, q=1 → %g (expect 0, Inf)\n', poissinv(0,2), poissinv(1,2));
fprintf('  edges: q<0 → %g, q>1 → %g, lam=0 → %g (0), lam<0 → %g (NaN)\n', poissinv(-0.1,2), poissinv(1.5,2), poissinv(0.5,0), poissinv(0.5,-1));
