clear

import compat.*

fprintf('=== nbinstat ===\n');
[m, v] = nbinstat(5, 0.3);
fprintf('  NB(5,0.3) : m=%.4f v=%.4f (expect 11.6667 / 38.8889)\n', m, v);
[m, v] = nbinstat([3 5 10], 0.5);
fprintf('  vec r     : m=[%g %g %g]\n', m(1), m(2), m(3));
[m, v] = nbinstat(5, 1);
fprintf('  p=1       : m=%g v=%g (expect 0 / 0)\n', m, v);
[m, v] = nbinstat(2.5, 0.5);
fprintf('  r=2.5     : m=%g v=%g (Pólya generalisation, r need not be integer)\n', m, v);
fprintf('  edges     : p=0 → %g, r=0 → %g, r<0 → %g, p>1 → %g (all NaN)\n', nbinstat(5,0), nbinstat(0,0.5), nbinstat(-1,0.5), nbinstat(5,1.5));
