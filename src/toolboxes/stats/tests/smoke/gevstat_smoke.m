clear

fprintf('=== gevstat ===\n');
[m, v] = gevstat(0.3, 1, 0);
fprintf('  GEV(0.3,1,0) : m=%.4f v=%.4f\n', m, v);
[m, v] = gevstat(0, 1, 0);
fprintf('  k=0 (Gumbel) : m=%.4f v=%.4f (= γ_E / π²/6)\n', m, v);
[m, v] = gevstat(0.5, 1, 0);
fprintf('  k=0.5        : m=%g v=%g (var Inf at k≥0.5)\n', m, v);
[m, v] = gevstat(1, 1, 0);
fprintf('  k=1          : m=%g (mean Inf at k≥1)\n', m);
[m, v] = gevstat([0.3 0 -0.3], 1, 0);
fprintf('  vector k     : m=[%g %g %g]\n', m(1), m(2), m(3));
fprintf('  edges        : sigma=0 → %g, sigma<0 → %g (NaN)\n', gevstat(0.3,0,0), gevstat(0.3,-1,0));
