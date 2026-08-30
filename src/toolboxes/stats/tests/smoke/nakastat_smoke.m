clear

fprintf('=== nakastat ===\n');
[m, v] = nakastat(1, 1);
fprintf('  Naka(1,1) : m=%.4f v=%.4f (Naka(1,1)≡Rayleigh)\n', m, v);
[m, v] = nakastat([0.5 1 2], 1);
fprintf('  vec mu    : m=[%g %g %g]\n', m(1), m(2), m(3));
fprintf('  edges     : mu=0 → %g, omega=0 → %g, mu<0 → %g (all NaN)\n', nakastat(0,1), nakastat(1,0), nakastat(-1,1));
