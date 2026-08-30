clear

fprintf('=== gpstat ===\n');
[m, v] = gpstat(0.3, 1, 0);
fprintf('  GP(0.3,1,0): m=%.4f v=%.4f\n', m, v);
[m, v] = gpstat([0.3 0 -0.3], 1, 0);
fprintf('  vec k     : m=[%g %g %g]\n', m(1), m(2), m(3));
[m, v] = gpstat(0.5, 1, 0);
fprintf('  k=0.5     : m=%g v=%g (var Inf)\n', m, v);
[m, v] = gpstat(1, 1, 0);
fprintf('  k=1       : m=%g (mean Inf)\n', m);
fprintf('  edges     : sigma=0 → %g, sigma<0 → %g (NaN)\n', gpstat(0.3,0,0), gpstat(0.3,-1,0));
