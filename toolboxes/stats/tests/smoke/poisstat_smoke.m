clear

import compat.*

fprintf('=== poisstat ===\n');
[m, v] = poisstat(2);
fprintf('  lam=2 : m=%g v=%g (expect 2 / 2)\n', m, v);
[m, v] = poisstat([1 2 5 10]);
fprintf('  vec   : m=[%g %g %g %g] v=[%g %g %g %g]\n', m(1), m(2), m(3), m(4), v(1), v(2), v(3), v(4));
fprintf('  edges : poisstat(0)=%g poisstat(-1)=%g (both expect NaN)\n', poisstat(0), poisstat(-1));
