clear

import compat.*

fprintf('=== lognstat ===\n');
[m, v] = lognstat(0, 1);
fprintf('  LN(0,1) : m=%.4f v=%.4f (expect 1.6487 / 4.6708)\n', m, v);
[m, v] = lognstat([0 1 2], [1 0.5 1]);
fprintf('  vec     : m=[%g %g %g]\n', m(1), m(2), m(3));
fprintf('  edges   : sigma=0 → %g, sigma<0 → %g (both NaN)\n', lognstat(0, 0), lognstat(0, -1));
