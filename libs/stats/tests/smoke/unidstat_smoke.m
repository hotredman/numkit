clear

import compat.*

fprintf('=== unidstat ===\n');
[m, v] = unidstat(5);
fprintf('  N=5  : m=%g v=%g (expect 3 / 2)\n', m, v);
[m, v] = unidstat([3 5 10]);
fprintf('  vec  : m=[%g %g %g] v=[%g %g %g]\n', m(1), m(2), m(3), v(1), v(2), v(3));
fprintf('  edges: N=0 → %g, N<0 → %g, N=2.5 → %g (all expect NaN)\n', unidstat(0), unidstat(-1), unidstat(2.5));
