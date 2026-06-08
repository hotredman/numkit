clear

import compat.*

fprintf('=== raylinv ===\n');
fprintf('  median Rayl(b=1) : %.6f (expect 1.177410)\n', raylinv(0.5, 1));
x = raylinv([0.05 0.5 0.95], 1);
fprintf('  vector q         : [%.4f %.4f %.4f]\n', x(1), x(2), x(3));
fprintf('  q=0 → %g, q=1 → %g (expect 0, Inf)\n', raylinv(0, 1), raylinv(1, 1));
fprintf('  edges: q<0 → %g, q>1 → %g, b=0 → %g, b<0 → %g (NaN)\n', ...
    raylinv(-0.1, 1), raylinv(1.5, 1), raylinv(0.5, 0), raylinv(0.5, -1));
