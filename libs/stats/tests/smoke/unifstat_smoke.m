clear

import compat.*

fprintf('=== unifstat ===\n');
[m, v] = unifstat(0, 1);
fprintf('  U(0,1) : m=%g v=%g (expect 0.5 / 0.0833)\n', m, v);
[m, v] = unifstat([0 -2], [1 5]);
fprintf('  vec    : m=[%g %g]\n', m(1), m(2));
fprintf('  edges  : a=b → %g, a>b → %g (both NaN)\n', unifstat(1, 1), unifstat(2, 1));
