clear

import compat.*

fprintf('=== evstat ===\n');
[m, v] = evstat(0, 1);
fprintf('  EV(0,1) : m=%.4f v=%.4f (expect -0.5772 / 1.6449)\n', m, v);
[m, v] = evstat([0 1 -2], [1 2 0.5]);
fprintf('  vec     : m=[%g %g %g]\n', m(1), m(2), m(3));
fprintf('  edges   : sigma=0 → %g, sigma<0 → %g (both NaN)\n', evstat(0,0), evstat(0,-1));
