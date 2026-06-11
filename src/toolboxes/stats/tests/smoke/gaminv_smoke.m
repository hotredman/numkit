clear

import compat.*

fprintf('=== gaminv ===\n');
fprintf('  median Gam(2,1)  : %.4f (expect 1.6783)\n', gaminv(0.5, 2, 1));
x = gaminv([0.05 0.5 0.95], 2, 1);
fprintf('  vector q         : [%.4f %.4f %.4f]\n', x(1), x(2), x(3));
fprintf('  q=0 → %g, q=1 → %g (expect 0, Inf)\n', gaminv(0, 2, 1), gaminv(1, 2, 1));
fprintf('  edges: q<0 → %g, q>1 → %g, a=0 → %g (0 deg), a<0 → %g (NaN), b=0 → %g (NaN)\n', gaminv(-0.1,2,1), gaminv(1.5,2,1), gaminv(0.5,0,1), gaminv(0.5,-1,1), gaminv(0.5,2,0));
