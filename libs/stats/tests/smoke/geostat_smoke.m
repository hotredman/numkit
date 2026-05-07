clear

import compat.*

fprintf('=== geostat ===\n');
[m, v] = geostat(0.3);
fprintf('  p=0.3 : m=%.4f v=%.4f (expect 2.3333 / 7.7778)\n', m, v);
[m, v] = geostat([0.1 0.5 0.9]);
fprintf('  vec   : m=[%g %g %g] v=[%g %g %g]\n', m(1), m(2), m(3), v(1), v(2), v(3));
[m, v] = geostat(1);
fprintf('  p=1   : m=%g v=%g (expect 0 / 0)\n', m, v);
fprintf('  edges : geostat(0)=%g, geostat(-0.1)=%g, geostat(1.5)=%g (all NaN)\n', geostat(0), geostat(-0.1), geostat(1.5));
