clear

import compat.*

fprintf('=== gampdf ===\n');
fprintf('  Gam(2,1) at 2 : %.6f (expect 0.270671)\n', gampdf(2, 2, 1));
y = gampdf([0 1 2 5], 2, 1);
fprintf('  vector x      : [%.4f %.4f %.4f %.4f]\n', y(1), y(2), y(3), y(4));
fprintf('  density at 0  : a<1 → %g (Inf), a=1 → %g (1), a>1 → %g (0)\n', gampdf(0,0.5,1), gampdf(0,1,1), gampdf(0,2,1));
fprintf('  edges         : a=0 → %g (0; degenerate), a<0 → %g, b=0 → %g (NaN)\n', gampdf(2,0,1), gampdf(2,-1,1), gampdf(2,2,0));
