clear

import compat.*

fprintf('=== ricestat ===\n');
[m, v] = ricestat(2, 1);
fprintf('  Rice(2,1) : m=%.4f v=%.4f\n', m, v);
[m, v] = ricestat(0, 1);
fprintf('  Rice(0,1) : m=%.4f v=%.4f (≡Rayleigh(σ=1))\n', m, v);
[m, v] = ricestat([0 1 2], 1);
fprintf('  vec s     : m=[%g %g %g]\n', m(1), m(2), m(3));
fprintf('  edges     : sigma=0 → %g, sigma<0 → %g, s<0 → %g (all NaN)\n', ricestat(2,0), ricestat(2,-1), ricestat(-1,1));
